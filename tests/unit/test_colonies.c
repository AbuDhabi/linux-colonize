#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/ai_popup.h"
#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/ss.h"
#include "core/units_cargo.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"
#include "../common/test_runner.h"

static int failures = 0;

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      fprintf(stderr, "FAIL: %s\n", (msg)); \
      failures++; \
    } else { \
      printf("OK: %s\n", (msg)); \
    } \
  } while (0)


/* Col1 +0x98: BUY construction accumulates hammers_purchased remainder. */
static int unit_hammers_purchased_buy(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Stockade");
  pool.building_types[0].hammers = 64;
  pool.building_types[0].tools_cost = 0;
  pool.building_type_count = 1;
  ColonizeColony* c = &pool.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->building_in_production = 0;
  c->hammers = 14;
  c->hammers_purchased = 0;
  pool.colony_count = 1;
  int gold = 2000;
  /* FUN_2f2b_5e44: hammers_deficit(50) * 13, no doubling (colony->hammers
   * != 0 before the buy), no tools term (tools_cost 0). */
  const int deficit = 64 - 14;
  const int expect_cost = deficit * 13;
  if (!colonies_buy_construction(&pool, 0, /*difficulty=*/0, &gold)) {
    fprintf(stderr, "hammers_purchased: buy failed\n");
    return 1;
  }
  if (c->hammers_purchased != (uint16_t)deficit || gold != 2000 - expect_cost) {
    fprintf(stderr, "hammers_purchased=%u gold=%d expect_deficit=%d expect_cost=%d\n",
            (unsigned)c->hammers_purchased, gold, deficit, expect_cost);
    return 1;
  }
  /* Buy only tops hammers up to the threshold — it does NOT complete the
   * project (player-corrected: completion happens next turn). */
  if (c->hammers != 64 || c->has_building[0]) {
    fprintf(stderr, "expected hammers topped (64) and NOT completed yet: hammers=%d has_building=%d\n",
            c->hammers, c->has_building[0]);
    return 1;
  }
  fprintf(stderr, "unit_colonies: hammers_purchased buy ok\n");
  return 0;
}

/* Col1 +0x95/+0x96: warehouse_level drives 100*(1+level); capitol INC on complete. */
/*
 * Smell audit #70: Col1 colony +0x1c bit 0x80. FUN_364b_0114 ORs it in when a
 * project completes (~56925 / ~56935); FUN_5952_0214 / _02f4 clear it again
 * when a project is assigned or the queue is cleared (~93714 / ~93754).
 */
static int unit_build_complete_latch(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Docks");
  pool.building_types[0].hammers = 10;
  pool.building_types[0].tools_cost = 0;
  snprintf(pool.building_types[1].name, sizeof(pool.building_types[1].name), "Stable");
  pool.building_types[1].hammers = 10;
  pool.building_types[1].tools_cost = 0;
  pool.building_type_count = 2;
  ColonizeColony* c = &pool.colonies[0];
  memset(c, 0, sizeof(*c));
  c->id = 0;
  c->active = true;
  c->population = 1;
  c->colonist_count = 1;
  c->building_in_production = -1;
  pool.colony_count = 1;

  if ((c->colony_flags & COLONIZE_COLONY_FLAG_BUILD_COMPLETE) != 0) {
    fprintf(stderr, "build-complete latch set on a fresh colony\n");
    return 1;
  }
  if (!colonies_set_construction(&pool, 0, 0)) {
    fprintf(stderr, "build-complete latch: set_construction(Docks) failed\n");
    return 1;
  }
  c->hammers = 10;
  if (!colonies_try_complete_building(&pool, 0)) {
    fprintf(stderr, "build-complete latch: Docks did not complete\n");
    return 1;
  }
  if ((c->colony_flags & COLONIZE_COLONY_FLAG_BUILD_COMPLETE) == 0) {
    fprintf(stderr, "build-complete latch not set after completion\n");
    return 1;
  }
  /* Assigning the next project spends the latch (FUN_5952_0214). */
  if (!colonies_set_construction(&pool, 0, 1)) {
    fprintf(stderr, "build-complete latch: set_construction(Stable) failed\n");
    return 1;
  }
  if ((c->colony_flags & COLONIZE_COLONY_FLAG_BUILD_COMPLETE) != 0) {
    fprintf(stderr, "build-complete latch survived a new assignment\n");
    return 1;
  }
  /* So does clearing the queue (FUN_5952_02f4). */
  c->colony_flags |= COLONIZE_COLONY_FLAG_BUILD_COMPLETE;
  if (!colonies_clear_construction(&pool, 0) ||
      (c->colony_flags & COLONIZE_COLONY_FLAG_BUILD_COMPLETE) != 0) {
    fprintf(stderr, "build-complete latch survived clear_construction\n");
    return 1;
  }
  return 0;
}

/*
 * Smell audit #71: colony_craft_preview must clamp the scratch stock exactly
 * like the live tick's colony_craft_clamp, or a near-full warehouse previews
 * a figure the tick never produces.
 */
static int unit_craft_preview_clamps(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Rum Distillery");
  pool.building_types[0].hammers = 0;
  pool.building_type_count = 1;
  ColonizeColony* c = &pool.colonies[0];
  memset(c, 0, sizeof(*c));
  c->id = 0;
  c->active = true;
  c->population = 1;
  c->colonist_count = 1;
  c->building_in_production = -1;
  c->colonists[0].active = true;
  c->colonists[0].field_job = -1;
  c->colonists[0].building_type = 0;
  c->colonists[0].profession = COLONIZE_PROF_DISTILLER;
  c->stock[COLONIZE_CARGO_SUGAR] = 100;
  c->stock[COLONIZE_CARGO_RUM] = 65530;
  pool.colony_count = 1;

  ColonizeColony scratch = *c;
  int shortfall[COLONIZE_CARGO_COUNT];
  ColonizeColonyProdDelta delta;
  memset(&delta, 0, sizeof(delta));
  colony_craft_preview(&pool, &scratch, shortfall, &delta, 0, NULL, NULL);
  if (scratch.stock[COLONIZE_CARGO_RUM] > 65535) {
    fprintf(
      stderr, "craft preview stock unclamped: rum=%d\n", scratch.stock[COLONIZE_CARGO_RUM]
    );
    return 1;
  }
  /* And it agrees with the live tick on the same colony. */
  ColonizeColonyProdDelta live_delta;
  memset(&live_delta, 0, sizeof(live_delta));
  colony_craft_one_colony(&pool, c, &live_delta, 0);
  if (c->stock[COLONIZE_CARGO_RUM] != scratch.stock[COLONIZE_CARGO_RUM]) {
    fprintf(
      stderr,
      "craft preview/tick disagree on clamped rum: preview=%d tick=%d\n",
      scratch.stock[COLONIZE_CARGO_RUM],
      c->stock[COLONIZE_CARGO_RUM]
    );
    return 1;
  }
  return 0;
}

static int unit_warehouse_capitol_levels(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Warehouse");
  pool.building_types[0].hammers = 10;
  pool.building_types[0].tools_cost = 0;
  snprintf(pool.building_types[1].name, sizeof(pool.building_types[1].name), "Capitol");
  pool.building_types[1].hammers = 10;
  pool.building_types[1].tools_cost = 0;
  pool.building_type_count = 2;
  ColonizeColony* c = &pool.colonies[0];
  memset(c, 0, sizeof(*c));
  c->id = 0;
  c->active = true;
  c->building_in_production = -1;
  pool.colony_count = 1;

  if (colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS) != 100) {
    fprintf(stderr, "warehouse_level0 cap want 100\n");
    return 1;
  }
  c->warehouse_level = 1;
  if (colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS) != 200) {
    fprintf(stderr, "warehouse_level1 cap want 200 got %d\n",
            colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS));
    return 1;
  }
  c->warehouse_level = 0;
  c->building_in_production = 0;
  c->hammers = 10;
  /*
   * DOS DS:0x34a — the just-finished building the colony screen reveals
   * (and the only thing that lets FUN_2f2b_6cd4 push event 0x54). Idle
   * colonies must carry "none" so opening the screen is silent (bugs.md:
   * celebratory SFX on every colony open).
   */
  if (c->pending_build_reveal != 0) {
    fprintf(stderr, "pending_build_reveal want 0 (none) before completion, got %d\n",
            c->pending_build_reveal);
    return 1;
  }
  if (!colonies_try_complete_building(&pool, 0) || c->warehouse_level != 1) {
    fprintf(stderr, "Warehouse complete warehouse_level=%u\n",
            (unsigned)c->warehouse_level);
    return 1;
  }
  if (c->pending_build_reveal != 1) {
    fprintf(stderr, "pending_build_reveal want 1 (building 0 + 1), got %d\n",
            c->pending_build_reveal);
    return 1;
  }
  c->pending_build_reveal = 0; /* colony screen consumes it on open */
  if (colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS) != 200) {
    fprintf(stderr, "after Warehouse cap want 200\n");
    return 1;
  }
  c->building_in_production = 1;
  c->hammers = 10;
  if (!colonies_try_complete_building(&pool, 0) || c->capitol_level != 1) {
    fprintf(stderr, "Capitol complete capitol_level=%u\n", (unsigned)c->capitol_level);
    return 1;
  }
  fprintf(stderr, "unit_colonies: warehouse/capitol levels ok\n");
  return 0;
}

/* Helper: @SEACOLONY / @NOPORT GAME.TXT + inland vs coastal found tiles. */
static int unit_found_chrome(void) {
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "found: GAME.TXT load failed\n");
    return 1;
  }
  char body[512];
  popup_msg_fill(
    &game_txt,
    "SEACOLONY",
    NULL,
    "Colonies cannot be built at sea.",
    body,
    sizeof(body)
  );
  if (strstr(body, "sea") == NULL && strstr(body, "Sea") == NULL) {
    fprintf(stderr, "found: SEACOLONY body weak '%s'\n", body);
    assets_msg_free(&game_txt);
    return 1;
  }

  popup_msg_fill(
    &game_txt,
    "NOPORT",
    NULL,
    "This square does not have access to the ocean.",
    body,
    sizeof(body)
  );
  if ((strstr(body, "ocean") == NULL && strstr(body, "Ocean") == NULL) ||
      (strstr(body, "port") == NULL && strstr(body, "Port") == NULL &&
       strstr(body, "wagon") == NULL && strstr(body, "Wagon") == NULL)) {
    fprintf(stderr, "found: NOPORT body weak '%s'\n", body);
    assets_msg_free(&game_txt);
    return 1;
  }
  char choices[4][POPUP_MSG_CHOICE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(&game_txt, "NOPORT");
  const int nch = popup_msg_choices(sec, choices, 4);
  if (nch < 2) {
    fprintf(stderr, "found: NOPORT want 2 choices got %d\n", nch);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strstr(choices[0], "forgot") == NULL) {
    fprintf(stderr, "found: NOPORT choice0 unexpected '%s'\n", choices[0]);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strstr(choices[1], "mind") == NULL && strstr(choices[1], "exactly") == NULL) {
    fprintf(stderr, "found: NOPORT choice1 unexpected '%s'\n", choices[1]);
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char map_err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, map_err, sizeof(map_err))) {
    fprintf(stderr, "found: AMER2 load failed: %s\n", map_err);
    return 1;
  }
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);

  int water = 0, inland = 0, coastal = 0;
  for (int y = 0; y < (int)map.height; ++y) {
    for (int x = 0; x < (int)map.width; ++x) {
      if (!map_tile_is_land(&map, x, y)) {
        water++;
        continue;
      }
      if (!colonies_can_found(&pool, &map, x, y)) {
        continue;
      }
      if (map_tile_is_coastal(&map, x, y)) {
        coastal++;
      } else {
        inland++;
      }
    }
  }
  map_free(&map);
  if (water < 1 || inland < 1 || coastal < 1) {
    fprintf(
      stderr,
      "found: tile classes water=%d inland=%d coastal=%d\n",
      water,
      inland,
      coastal
    );
    return 1;
  }
  fprintf(
    stderr,
    "unit_colonies: found chrome ok (water=%d inland=%d coastal=%d)\n",
    water,
    inland,
    coastal
  );
  return 0;
}

/* Helper: @FULL chrome when colony is at population cap. */
static int unit_full_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Jamestown");
  col->colonist_count = COLONIZE_COLONY_POP_MAX;
  col->population = COLONIZE_COLONY_POP_MAX;
  pool.colony_count = 1;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "full: GAME.TXT load failed\n");
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_full_chrome(col, &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Jamestown") == NULL &&
       strstr(pops.queue[0].body, "crowded") == NULL &&
       strstr(pops.queue[0].body, "immigrants") == NULL)) {
    fprintf(
      stderr,
      "full: FULL popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_colonies: FULL chrome ok\n");
  return 0;
}

/* Helper: @ALREADYHAVE / @NOMOREWAREHOUSE when construction already owned. */
static int unit_alreadyhave_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Williamsburg");
  pool.colony_count = 1;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "alreadyhave: GAME.TXT load failed\n");
    return 1;
  }

  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_already_have_chrome(col, "Printing Press", &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Williamsburg") == NULL &&
       strstr(pops.queue[0].body, "already") == NULL &&
       strstr(pops.queue[0].body, "Printing") == NULL)) {
    fprintf(
      stderr,
      "alreadyhave: ALREADYHAVE popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }

  ai_popup_init(&pops);
  colonies_emit_already_have_chrome(col, "Warehouse Expansion", &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Williamsburg") == NULL &&
       strstr(pops.queue[0].body, "expansion") == NULL &&
       strstr(pops.queue[0].body, "Expansion") == NULL &&
       strstr(pops.queue[0].body, "one") == NULL &&
       strstr(pops.queue[0].body, "One") == NULL)) {
    fprintf(
      stderr,
      "alreadyhave: NOMOREWAREHOUSE popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }

  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_colonies: ALREADYHAVE/NOMOREWAREHOUSE chrome ok\n");
  return 0;
}

/* Helper: unskilled school assign → @NOTEACHER; Teacher may assign. */
static int unit_noteacher_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Harvard");
  col->has_building[0] = true;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[0].building_type = -1;
  col->colonists[0].field_job = -1;
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  if (colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "noteacher: Free Colonist should not assign to Schoolhouse\n");
    return 1;
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "noteacher: GAME.TXT load failed\n");
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_noteacher_chrome(&pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "mastered") == NULL &&
       strstr(pops.queue[0].body, "teach") == NULL &&
       strstr(pops.queue[0].body, "profession") == NULL)) {
    fprintf(
      stderr,
      "noteacher: NOTEACHER popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);

  /* bugs.md #380: @JOB 18 "Teacher" is level 4 and may NOT teach in DOS. */
  col->colonists[0].profession = COLONIZE_PROF_TEACHER;
  if (colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "noteacher: @JOB 18 Teacher must not assign to Schoolhouse\n");
    return 1;
  }
  col->colonists[0].profession = COLONIZE_JOB_FARMER; /* @JOB level 1 */
  if (!colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "noteacher: Expert Farmer should assign to Schoolhouse\n");
    return 1;
  }
  if (col->colonists[0].building_type != 0) {
    fprintf(
      stderr, "noteacher: teacher building_type want 0 got %d\n", col->colonists[0].building_type
    );
    return 1;
  }

  fprintf(stderr, "unit_colonies: NOTEACHER chrome ok\n");
  return 0;
}

/* GAME.TXT @MORETHANTHREE: at most 3 colonists per building; no-op
 * reassignment (already working there) is exempt from the cap. */
static int unit_more_than_three_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Town Hall");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Plymouth");
  col->has_building[0] = true;
  for (int i = 0; i < 4; ++i) {
    col->colonists[i].active = true;
    col->colonists[i].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[i].field_job = -1;
    col->colonists[i].building_type = (i < 3) ? 0 : -1;
  }
  col->colonist_count = 4;
  col->population = 4;
  pool.colony_count = 1;

  if (colonies_building_worker_count(col, 0) != 3) {
    fprintf(
      stderr, "morethanthree: expected 3 workers, got %d\n", colonies_building_worker_count(col, 0)
    );
    return 1;
  }
  if (colonies_assign_workplace(&pool, 1, 3, 0)) {
    fprintf(stderr, "morethanthree: 4th colonist should not fit in a full building\n");
    return 1;
  }
  /* Already-there colonist reassigning to the same building is not capped. */
  if (!colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "morethanthree: no-op reassignment should still succeed\n");
    return 1;
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "morethanthree: GAME.TXT load failed\n");
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_more_than_three_chrome(col, &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "three") == NULL &&
       strstr(pops.queue[0].body, "Three") == NULL)) {
    fprintf(
      stderr,
      "morethanthree: MORETHANTHREE popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);

  fprintf(stderr, "unit_colonies: MORETHANTHREE chrome ok\n");
  return 0;
}

/* Helper: school tier gates @NEEDCOLLEGE / @NEEDUNIVERSITY. */
static int unit_needschool_chrome(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Schoolhouse");
  snprintf(pool.building_types[1].name, sizeof(pool.building_types[1].name), "College");
  snprintf(pool.building_types[2].name, sizeof(pool.building_types[2].name), "University");
  pool.building_type_count = 3;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Yale");
  col->has_building[0] = true; /* Schoolhouse only */
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_BLACKSMITH;
  col->colonists[0].building_type = -1;
  col->colonists[0].field_job = -1;
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  if (colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "needschool: Blacksmith should not assign to Schoolhouse\n");
    return 1;
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "needschool: GAME.TXT load failed\n");
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_need_school_chrome(COLONIZE_PROF_BLACKSMITH, 1, &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "college") == NULL &&
       strstr(pops.queue[0].body, "College") == NULL &&
       strstr(pops.queue[0].body, "Blacksmith") == NULL)) {
    fprintf(
      stderr,
      "needschool: NEEDCOLLEGE popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }

  /* College only — Preacher needs University. */
  col->has_building[0] = false;
  col->has_building[1] = true;
  col->colonists[0].profession = COLONIZE_PROF_PREACHER;
  col->colonists[0].building_type = -1;
  if (colonies_assign_workplace(&pool, 1, 0, 1)) {
    fprintf(stderr, "needschool: Preacher should not assign to College-only\n");
    assets_msg_free(&game_txt);
    return 1;
  }
  ai_popup_init(&pops);
  colonies_emit_need_school_chrome(COLONIZE_PROF_PREACHER, 2, &pops, &game_txt);
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "university") == NULL &&
       strstr(pops.queue[0].body, "University") == NULL &&
       strstr(pops.queue[0].body, "Preacher") == NULL)) {
    fprintf(
      stderr,
      "needschool: NEEDUNIVERSITY popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);

  /* University + Preacher OK. */
  col->has_building[1] = false;
  col->has_building[2] = true;
  col->colonists[0].building_type = -1;
  if (!colonies_assign_workplace(&pool, 1, 0, 2)) {
    fprintf(stderr, "needschool: Preacher should assign to University\n");
    return 1;
  }

  /* Schoolhouse + Farmer OK. */
  col->has_building[2] = false;
  col->has_building[0] = true;
  col->colonists[0].profession = COLONIZE_JOB_FARMER;
  col->colonists[0].building_type = -1;
  if (!colonies_assign_workplace(&pool, 1, 0, 0)) {
    fprintf(stderr, "needschool: Farmer should assign to Schoolhouse\n");
    return 1;
  }

  fprintf(stderr, "unit_colonies: NEEDCOLLEGE/NEEDUNIVERSITY chrome ok\n");
  return 0;
}

/*
 * colonies_capture_ex with a Col1 context — FUN_5fef_1b0e capture tail:
 * peacetime treasury share gold*pop/(pop + loser's remaining pop) moves to
 * the captor, rebel dividend ×2/3, colony/pop tallies move, WAR set.
 */
static int unit_capture_col1_effects(void) {
  const int failures_before = failures;
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* a = &pool.colonies[0];
  ColonizeColony* b = &pool.colonies[1];
  a->active = true;
  a->id = 0;
  a->x = 10;
  a->y = 10;
  a->nation_id = 0;
  a->colonist_count = 4;
  snprintf(a->name, sizeof(a->name), "Alpha");
  b->active = true;
  b->id = 1;
  b->x = 20;
  b->y = 20;
  b->nation_id = 0;
  b->colonist_count = 6;
  pool.colony_count = 2;

  ColonizeCol1Save col1;
  ColonizeCol1Colony recs[2];
  memset(&col1, 0, sizeof(col1));
  memset(recs, 0, sizeof(recs));
  col1.colony = recs;
  col1.head.colony_count = 2;
  recs[0].x = 10;
  recs[0].y = 10;
  recs[0].nation_id = 0;
  recs[0].population = 4;
  recs[0].rebel_dividend = 90;
  recs[0].rebel_divisor = 100;
  recs[1].x = 20;
  recs[1].y = 20;
  recs[1].nation_id = 0;
  recs[1].population = 6;
  col1.nation[0].gold = 1000;
  col1.nation[1].gold = 5;
  col1.stuff.colony_counts[0] = 2;
  col1.stuff.colony_pop_totals[0] = 10;
  col1.head.crown_nation_id = 1;

  colonies_set_col1_context(&col1);
  int plunder = -1;
  CHECK(colonies_capture_ex(&pool, 0, 1, &plunder), "capture_ex ok");
  CHECK(a->nation_id == 1, "runtime owner swapped");
  CHECK(recs[0].nation_id == 1, "col1 record owner swapped");
  CHECK(plunder == 400, "plunder = 1000*4/(4+6)");
  CHECK(col1.nation[0].gold == 600 && col1.nation[1].gold == 405, "treasury share moved");
  CHECK(recs[0].rebel_dividend == 60, "rebel dividend 90 -> 60 (x2/3)");
  CHECK(col1.stuff.colony_counts[0] == 1 && col1.stuff.colony_counts[1] == 1, "colony tallies");
  CHECK(col1.stuff.colony_pop_totals[0] == 6 && col1.stuff.colony_pop_totals[1] == 4, "pop tallies");
  CHECK((ai_diplo_read(&col1, 0, 1) & AI_DIPLO_WAR) != 0, "capture sets WAR");
  CHECK(col1.head.game_options.ref_unit_threshold == 0, "peacetime: no REF threshold bit");

  /* WoI + crown captor: no plunder, REF threshold bit set. */
  col1.head.game_options.woi = 1;
  plunder = -1;
  CHECK(colonies_capture_ex(&pool, 1, 1, &plunder), "woi capture_ex ok");
  CHECK(plunder == 0, "woi: no treasury share");
  CHECK(col1.nation[0].gold == 600, "woi: loser gold untouched");
  CHECK(col1.head.game_options.ref_unit_threshold == 1, "woi crown capture sets 0x5382|0x40");
  colonies_set_col1_context(NULL);
  return failures != failures_before ? 1 : 0;
}

/*
 * The rest of the original hand-written main(): one long narrative that
 * threads a single map/pool/cid/names/units fixture through founding,
 * assignment, ejection, buildable-list, abandon and trade-stop coverage.
 * It is not split into independent cases: state built in one section
 * (colony id, tile coordinates, loaded building/name tables) is consumed
 * many sections later, so pulling any piece out on its own would just
 * duplicate the setup. Kept as a single TEST_MAIN case.
 */
static int case_colonies_core(void) {
  const int failures_before = failures;
  diag_init(0, NULL);

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char map_err[256];
  const bool map_ok = map_load_mp("COLONIZE/AMER2.MP", &map, map_err, sizeof(map_err));
  CHECK(map_ok, "load AMER2.MP");
  if (!map_ok) {
    fprintf(stderr, "map error: %s\n", map_err);
    return 1;
  }

  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  CHECK(pool.colony_count == 0, "pool starts empty");

  CHECK(colonies_load_names(&pool, "COLONIZE/COLONY.TXT"), "load COLONY.TXT names");
  CHECK(pool.name_count[0] > 0, "at least one English colony name loaded");

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  CHECK(assets_msg_load_file(&names, "COLONIZE/NAMES.TXT"), "load NAMES.TXT");
  CHECK(colonies_load_buildings(&pool, &names), "load @BUILDING");
  CHECK(pool.building_type_count > 8, "enough building types");
  const int town_hall = colonies_find_building(&pool, "Town Hall");
  const int carpenter = colonies_find_building(&pool, "Carpenter's Shop");
  const int warehouse = colonies_find_building(&pool, "Warehouse");
  const int stockade = colonies_find_building(&pool, "Stockade");
  const int docks = colonies_find_building(&pool, "Docks");
  CHECK(town_hall >= 0 && carpenter >= 0, "starter building names resolve");
  CHECK(warehouse >= 0 && stockade >= 0 && docks >= 0, "fence/docks/warehouse names resolve");

  int land_x = -1, land_y = -1;
  for (int y = 0; y < (int)map.height && land_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && land_x < 0; ++x) {
      if (map_tile_is_land(&map, x, y) && !map_tile_is_coastal(&map, x, y)) {
        land_x = x;
        land_y = y;
      }
    }
  }
  if (land_x < 0) {
    for (int y = 0; y < (int)map.height && land_x < 0; ++y) {
      for (int x = 0; x < (int)map.width && land_x < 0; ++x) {
        if (map_tile_is_land(&map, x, y)) {
          land_x = x;
          land_y = y;
        }
      }
    }
  }
  CHECK(land_x >= 0, "found a land tile");

  int water_x = -1, water_y = -1;
  for (int y = 0; y < (int)map.height && water_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && water_x < 0; ++x) {
      if (map_tile_is_water(&map, x, y)) {
        water_x = x;
        water_y = y;
      }
    }
  }
  if (water_x >= 0) {
    CHECK(!colonies_can_found(&pool, &map, water_x, water_y), "cannot found on water");
  }

  CHECK(colonies_can_found(&pool, &map, land_x, land_y), "can found on land tile");

  /* Found without a colonist (type -1) — still gets starter buildings. */
  const int empty_id = colonies_found(&pool, &map, land_x, land_y, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
  CHECK(empty_id >= 0, "colonies_found without founder");
  const ColonizeColony* empty = colonies_get(&pool, empty_id);
  CHECK(empty && empty->population == 0 && empty->colonist_count == 0, "no founder => pop 0");
  CHECK(empty && empty->has_building[town_hall], "starter includes Town Hall");
  CHECK(empty && empty->has_building[carpenter], "starter includes Carpenter's Shop");
  /*
   * bugs.md #430 — pin the WHOLE free starter set, not just two of it. DOS
   * hands every new colony the tier-1 manufacturing houses plus the Town Hall
   * and the Carpenter's Shop (docs/building_production.md "Starter colonies",
   * docs/assets.md); a silent name-lookup miss in colonies_grant_starters
   * would otherwise leave a colony that can never build anything and reports
   * "Build it first" on a shop it should have owned from turn one.
   */
  {
    static const char* const k_starters[] = {
      "Town Hall",
      "Carpenter's Shop",
      "Blacksmith's House",
      "Weaver's House",
      "Tobacconist's House",
      "Rum Distiller's House",
      "Fur Trader's House"
    };
    for (size_t si = 0; si < sizeof(k_starters) / sizeof(k_starters[0]); ++si) {
      const int bi = colonies_find_building(&pool, k_starters[si]);
      CHECK(bi >= 0, k_starters[si]);
      CHECK(empty && bi >= 0 && empty->has_building[bi], k_starters[si]);
    }
  }
  CHECK(empty && !empty->has_building[stockade], "starter excludes Stockade");
  CHECK(empty && !empty->has_building[warehouse], "starter excludes Warehouse");
  CHECK(empty && !empty->has_building[docks], "starter excludes Docks");
  CHECK(empty && empty->stock[COLONIZE_CARGO_FOOD] == 0, "starter food stock is 0");

  /* Coastal colony still does not get free Docks (upgrade only). */
  int coast_x = -1, coast_y = -1;
  for (int y = 0; y < (int)map.height && coast_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && coast_x < 0; ++x) {
      if (map_tile_is_coastal(&map, x, y) && colonies_can_found(&pool, &map, x, y)) {
        coast_x = x;
        coast_y = y;
      }
    }
  }
  if (coast_x >= 0) {
    const int coast_id = colonies_found(&pool, &map, coast_x, coast_y, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
    CHECK(coast_id >= 0, "found coastal colony");
    const ColonizeColony* coastal = colonies_get(&pool, coast_id);
    CHECK(coastal && !coastal->has_building[docks], "coastal starter excludes Docks");
    CHECK(coastal && !coastal->has_building[warehouse], "coastal starter excludes Warehouse");
  } else {
    printf("OK: skip coastal founding check (no free coastal tile)\n");
  }

  /* Another colony with a founder on a different land tile. */
  int land2_x = -1, land2_y = -1;
  for (int y = 0; y < (int)map.height && land2_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && land2_x < 0; ++x) {
      if (map_tile_is_land(&map, x, y) && colonies_can_found(&pool, &map, x, y)) {
        land2_x = x;
        land2_y = y;
      }
    }
  }
  CHECK(land2_x >= 0, "second land tile");
  const int pioneer_type = 2; /* Pioneers are early in @UNIT; index used only for storage. */
  int cid =
    colonies_found(&pool, &map, land2_x, land2_y, 0, pioneer_type, UNITS_JOB_PIONEER, 100, 0, 0);
  CHECK(cid >= 0, "colonies_found with founder");
  const ColonizeColony* col = colonies_get(&pool, cid);
  CHECK(col != NULL, "colonies_get returns colony");
  CHECK(col->active, "colony is active");
  CHECK(col->x == land2_x && col->y == land2_y, "colony at expected coordinates");
  CHECK(col->name[0] != '\0', "colony has a name");
  CHECK(col->population == 1 && col->colonist_count == 1, "founder becomes colonist");
  CHECK(col->colonists[0].unit_type_index == pioneer_type, "colonist type preserved");
  CHECK(col->colonists[0].profession == UNITS_JOB_PIONEER, "founder profession preserved");
  CHECK(col->colonists[0].building_type == town_hall, "founder works in Town Hall");
  CHECK(col->stock[COLONIZE_CARGO_TOOLS] == 100, "founder tools enter stockpile");
  /* Stockade needs 3 colonists (@BUILDING min_colony); a size-1 coastal
   * town starts on Docks (bugs.md; seed-100 goldens), a landlocked one on
   * Warehouse (player-confirmed DOS behaviour). "Coastal" here is DOS's own
   * Docks predicate — the colony +0x1c 0x40 bit (raw 13688), stamped at
   * founding from map_tile_is_open_sea_adjacent — not the looser live
   * map_tile_is_coastal harbour probe. */
  if (map_tile_is_open_sea_adjacent(&map, land2_x, land2_y)) {
    const int docks = colonies_find_building(&pool, "Docks");
    CHECK(col->building_in_production == docks, "coastal size-1 town starts on Docks");
  } else {
    const int warehouse = colonies_find_building(&pool, "Warehouse");
    CHECK(col->building_in_production == warehouse, "landlocked size-1 town starts on Warehouse");
  }
  (void)stockade;
  CHECK(col->hammers == 0, "new colony starts with zero accumulated hammers");
  CHECK(
    units_working_colonist_sprite(NULL, pioneer_type, UNITS_JOB_PIONEER) ==
      UNITS_ICON_HARDY_PIONEER_WORK,
    "hardy pioneer work sprite"
  );
  CHECK(
    units_working_colonist_sprite(NULL, pioneer_type, UNITS_JOB_SOLDIER) ==
      UNITS_ICON_VETERAN_SOLDIER_WORK,
    "veteran soldier work sprite"
  );
  printf("  colony name: %s at (%d,%d) pop=%d tools=%d\n",
         col->name, col->x, col->y, col->population, col->stock[COLONIZE_CARGO_TOOLS]);

  CHECK(
    colonies_assign_workplace(&pool, cid, 0, carpenter),
    "assign founder to Carpenter's Shop"
  );
  CHECK(
    colonies_get(&pool, cid)->colonists[0].building_type == carpenter,
    "workplace is Carpenter's Shop"
  );
  CHECK(!colonies_assign_workplace(&pool, cid, 0, stockade), "cannot assign to unbuilt Stockade");

  CHECK(
    colonies_assign_field(&pool, cid, 0, 0, COLONIZE_JOB_FARMER),
    "assign founder to North field as Farmer"
  );
  CHECK(colonies_get(&pool, cid)->tiles[0] == 0, "tiles[0] is founder");
  CHECK(colonies_get(&pool, cid)->colonists[0].field_job == COLONIZE_JOB_FARMER, "field_job Farmer");
  CHECK(colonies_get(&pool, cid)->colonists[0].building_type < 0, "field clears workplace");
  CHECK(colonies_colonist_tile(colonies_get(&pool, cid), 0) == 0, "colonist_tile is 0");
  CHECK(
    colonies_assign_workplace(&pool, cid, 0, carpenter),
    "reassign to Carpenter clears field"
  );
  CHECK(colonies_get(&pool, cid)->tiles[0] < 0, "field tile cleared by workplace");
  CHECK(colonies_get(&pool, cid)->colonists[0].field_job < 0, "field_job cleared");

  /* Admit outside unit → colonist; eject colonist → outside unit. */
  {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    memset(&units, 0, sizeof(units));
    CHECK(units_load_types(&units, &names), "load unit types for admit/eject");
    const int free_col = units_find_type(&units, "Colonists");
    CHECK(free_col >= 0, "Colonists type");
    const ColonizeColony* before = colonies_get(&pool, cid);
    const int pop0 = before ? before->colonist_count : 0;
    const int uid = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
    CHECK(uid >= 0, "spawn outside Colonist");
    ColonizeUnit* ou = units_get(&units, uid);
    if (ou && before) {
      ou->nation_id = before->nation_id;
    }
    ColonizeWorld w_admit = world_make(&units, &pool, NULL, NULL, false, NULL, NULL);
    const int admitted = colonies_admit_unit_w(&w_admit, cid, uid);
    CHECK(admitted == pop0, "admit returns new colonist index");
    CHECK(colonies_get(&pool, cid)->colonist_count == pop0 + 1, "pop +1 after admit");
    {
      const ColonizeUnit* gone = units_get_const(&units, uid);
      CHECK(!gone || !gone->active, "map unit despawned on admit");
    }
    const int ejected = colonies_eject_colonist(&pool, cid, admitted, &units, COLONIZE_EJECT_COLONIST);
    CHECK(ejected >= 0, "eject returns unit id");
    CHECK(colonies_get(&pool, cid)->colonist_count == pop0, "pop restored after eject");
    const ColonizeUnit* back = units_get_const(&units, ejected);
    CHECK(back && back->active && back->x == land2_x && back->y == land2_y, "ejected on colony tile");
    CHECK(
      colonies_has_fortification(&pool, colonies_get(&pool, cid)) == false,
      "no fortification yet"
    );

    /* #744: Treasure has no default-profession slot (DS:0x30e) and cannot Join;
     * a plain Colonist still can. */
    {
      const int treasure_ti = units_find_type(&units, "Treasure");
      CHECK(treasure_ti >= 0, "Treasure type");
      const ColonizeColony* before_t = colonies_get(&pool, cid);
      const int pop_t0 = before_t ? before_t->colonist_count : 0;
      const int uidt = units_spawn_allow_stack(&units, treasure_ti, land2_x, land2_y);
      CHECK(uidt >= 0, "spawn outside Treasure");
      ColonizeUnit* out = units_get(&units, uidt);
      if (out && before_t) {
        out->nation_id = before_t->nation_id;
      }
      const int adt = colonies_admit_unit_w(&w_admit, cid, uidt);
      CHECK(adt < 0, "Treasure refused Join (no profession slot)");
      CHECK(colonies_get(&pool, cid)->colonist_count == pop_t0, "pop unchanged after refused Treasure Join");
      const ColonizeUnit* still = units_get_const(&units, uidt);
      CHECK(still && still->active, "Treasure unit still on map after refused Join");

      /* (Colonist admission is covered above; not repeated here so the
       * population stays below the Stockade threshold checked later.) */
    }

    /* Pioneer eject spends tools from warehouse. */
    {
      ColonizeColony* col = colonies_get_mut(&pool, cid);
      CHECK(col != NULL, "colony mut for pioneer eject");
      if (col) {
        col->stock[COLONIZE_CARGO_TOOLS] = 100;
      }
      const int uid2 = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      CHECK(uid2 >= 0, "spawn for pioneer admit");
      ColonizeUnit* ou2 = units_get(&units, uid2);
      if (ou2 && col) {
        ou2->nation_id = col->nation_id;
      }
      const int ad2 = colonies_admit_unit_w(&w_admit, cid, uid2);
      CHECK(ad2 >= 0, "admit before pioneer eject");
      CHECK(colonies_get(&pool, cid)->stock[COLONIZE_CARGO_TOOLS] >= 100, "tools in stock after admit");
      const int pej =
        colonies_eject_colonist(&pool, cid, ad2, &units, COLONIZE_EJECT_PIONEER);
      CHECK(pej >= 0, "eject as pioneer");
      const ColonizeUnit* pion = units_get_const(&units, pej);
      CHECK(pion && pion->tools >= 20, "pioneer carries tools");
      CHECK(units_map_sprite(&units, pej) == UNITS_ICON_PIONEER ||
              units_map_sprite(&units, pej) == UNITS_ICON_HARDY_PIONEER,
            "pioneer map icon");
    }
    /*
     * bugs.md: the two equip paths (colonist leaving the colony vs. a unit
     * already standing outside it) had different tool rules — the outside one
     * insisted on a full 100 and refused a Pioneer its own menu offered. Both
     * now go through colonies_equip_tools_take: whole 20s, capped at 100.
     */
    CHECK(colonies_equip_tools_take(0) == 0, "0 tools equips nothing");
    CHECK(colonies_equip_tools_take(19) == 0, "under one step equips nothing");
    CHECK(colonies_equip_tools_take(20) == 20, "one step");
    CHECK(colonies_equip_tools_take(59) == 40, "rounds down to whole steps");
    CHECK(colonies_equip_tools_take(100) == 100, "full load");
    CHECK(colonies_equip_tools_take(340) == 100, "capped at 100");
    {
      ColonizeColony* col = colonies_get_mut(&pool, cid);
      CHECK(col != NULL, "colony mut for partial-tools eject");
      if (col) {
        col->stock[COLONIZE_CARGO_TOOLS] = 40;
      }
      const int uidp = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      ColonizeUnit* oup = units_get(&units, uidp);
      if (oup && col) {
        oup->nation_id = col->nation_id;
      }
      const int adp = colonies_admit_unit_w(&w_admit, cid, uidp);
      CHECK(adp >= 0, "admit before partial-tools eject");
      int roles[COLONIZE_EJECT_ROLE_COUNT];
      const int nrp = colonies_list_eject_roles(&pool, cid, adp, roles, COLONIZE_EJECT_ROLE_COUNT);
      int offers_pioneer = 0;
      for (int i = 0; i < nrp; ++i) {
        if (roles[i] == COLONIZE_EJECT_PIONEER) {
          offers_pioneer = 1;
        }
      }
      CHECK(offers_pioneer, "40 tools must offer Pioneer");
      const int pejp = colonies_eject_colonist(&pool, cid, adp, &units, COLONIZE_EJECT_PIONEER);
      CHECK(pejp >= 0, "40 tools must equip a Pioneer, not refuse");
      const ColonizeUnit* pionp = units_get_const(&units, pejp);
      CHECK(pionp && pionp->tools == 40, "partial pioneer carries the 40 tools");
    }

    /* Skill sticks: hardy pioneer armed as soldier looks non-veteran; re-admit keeps skill. */
    {
      ColonizeColony* col = colonies_get_mut(&pool, cid);
      CHECK(col != NULL, "colony mut for skill stick");
      if (col) {
        col->stock[COLONIZE_CARGO_TOOLS] = 100;
        col->stock[COLONIZE_CARGO_MUSKETS] = 50;
      }
      const int uid3 = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      CHECK(uid3 >= 0, "spawn for hardy skill test");
      ColonizeUnit* ou3 = units_get(&units, uid3);
      if (ou3 && col) {
        ou3->nation_id = col->nation_id;
        ou3->profession = UNITS_JOB_PIONEER;
      }
      const int ad3 = colonies_admit_unit_w(&w_admit, cid, uid3);
      CHECK(ad3 >= 0, "admit hardy pioneer");
      CHECK(
        colonies_get(&pool, cid)->colonists[ad3].profession == UNITS_JOB_PIONEER,
        "admit keeps hardy profession"
      );
      CHECK(
        units_working_colonist_sprite(
          &units,
          colonies_get(&pool, cid)->colonists[ad3].unit_type_index,
          UNITS_JOB_PIONEER
        ) == UNITS_ICON_HARDY_PIONEER_WORK,
        "working hardy pioneer sprite #58"
      );
      const int sej =
        colonies_eject_colonist(&pool, cid, ad3, &units, COLONIZE_EJECT_SOLDIER);
      CHECK(sej >= 0, "eject hardy as soldier");
      const ColonizeUnit* sold = units_get_const(&units, sej);
      CHECK(sold && sold->profession == UNITS_JOB_PIONEER, "soldier keeps hardy skill");
      CHECK(sold && sold->muskets > 0, "soldier carries muskets");
      CHECK(
        units_map_sprite(&units, sej) == UNITS_ICON_SOLDIER,
        "hardy+muskets uses non-veteran soldier icon"
      );
      const int ad4 = colonies_admit_unit_w(&w_admit, cid, sej);
      CHECK(ad4 >= 0, "re-admit armed hardy");
      CHECK(
        colonies_get(&pool, cid)->colonists[ad4].profession == UNITS_JOB_PIONEER,
        "re-admit still hardy pioneer"
      );
      const int pej2 =
        colonies_eject_colonist(&pool, cid, ad4, &units, COLONIZE_EJECT_PIONEER);
      CHECK(pej2 >= 0, "re-eject as pioneer");
      const ColonizeUnit* pion2 = units_get_const(&units, pej2);
      CHECK(pion2 && pion2->profession == UNITS_JOB_PIONEER, "tools eject keeps skill");
      CHECK(
        units_map_sprite(&units, pej2) == UNITS_ICON_HARDY_PIONEER,
        "hardy+tools map icon #101"
      );
    }
    /* Church bless: Leave as Missionary when Church present; absent without. */
    {
      ColonizeColony* col = colonies_get_mut(&pool, cid);
      CHECK(col != NULL, "colony mut for missionary eject");
      const int church = colonies_find_building(&pool, "Church");
      CHECK(church >= 0, "Church building type");
      int roles[COLONIZE_EJECT_ROLE_COUNT];
      const int n0 = colonies_list_eject_roles(&pool, cid, 0, roles, COLONIZE_EJECT_ROLE_COUNT);
      int has_miss = 0;
      for (int i = 0; i < n0; ++i) {
        if (roles[i] == COLONIZE_EJECT_MISSIONARY) {
          has_miss = 1;
        }
      }
      CHECK(!has_miss, "no Missionary leave-as without Church");
      if (col && church >= 0) {
        col->has_building[church] = true;
      }
      /* Need a colonist on site. */
      const int uidm = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      CHECK(uidm >= 0, "spawn for missionary admit");
      ColonizeUnit* oum = units_get(&units, uidm);
      if (oum && col) {
        oum->nation_id = col->nation_id;
      }
      const int adm = colonies_admit_unit_w(&w_admit, cid, uidm);
      CHECK(adm >= 0, "admit before missionary eject");
      const int n1 = colonies_list_eject_roles(&pool, cid, adm, roles, COLONIZE_EJECT_ROLE_COUNT);
      has_miss = 0;
      for (int i = 0; i < n1; ++i) {
        if (roles[i] == COLONIZE_EJECT_MISSIONARY) {
          has_miss = 1;
        }
      }
      CHECK(has_miss, "Missionary leave-as with Church");
      const int mej =
        colonies_eject_colonist(&pool, cid, adm, &units, COLONIZE_EJECT_MISSIONARY);
      CHECK(mej >= 0, "eject as Missionary");
      const ColonizeUnit* miss = units_get_const(&units, mej);
      CHECK(miss && miss->active, "missionary unit active");
      const ColonizeUnitType* mt = units_type(&units, miss->type_index);
      CHECK(mt && strstr(mt->name, "Missionar") != NULL, "Missionaries unit type");
      CHECK(miss->profession == UNITS_JOB_NONE, "plain Church bless is non-Jesuit");
    }
    /*
     * bugs.md #556 / #559: the bless copies the profession byte verbatim
     * (overlays.c:10225-10245), and FUN_15eb_3454's two Jesuit row arms
     * (raw 13561-13569).
     */
    {
      ColonizeColony* col = colonies_get_mut(&pool, cid);
      CHECK(col != NULL, "colony mut for jesuit rows");
      const int church = colonies_find_building(&pool, "Church");
      CHECK(church >= 0, "Church building type for jesuit rows");
      /* (1) #556: an expert keeps his specialty through the bless. */
      const int uidx = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      CHECK(uidx >= 0, "spawn expert for bless");
      ColonizeUnit* oux = units_get(&units, uidx);
      if (oux && col) {
        oux->nation_id = col->nation_id;
        oux->profession = UNITS_JOB_PIONEER;
      }
      const int adx = colonies_admit_unit_w(&w_admit, cid, uidx);
      CHECK(adx >= 0, "admit expert before bless");
      const int bej = colonies_eject_colonist(&pool, cid, adx, &units, COLONIZE_EJECT_MISSIONARY);
      CHECK(bej >= 0, "bless the expert");
      const ColonizeUnit* bu = units_get_const(&units, bej);
      CHECK(bu && bu->profession == UNITS_JOB_PIONEER, "#556 bless keeps the specialty byte");

      /* (2) #559a: a Jesuit is offered the Missionary row without a Church. */
      col = colonies_get_mut(&pool, cid);
      const bool had_church = col ? col->has_building[church] : false;
      if (col) {
        col->has_building[church] = false;
      }
      const int uidj = units_spawn_allow_stack(&units, free_col, land2_x, land2_y);
      CHECK(uidj >= 0, "spawn jesuit");
      ColonizeUnit* ouj = units_get(&units, uidj);
      if (ouj && col) {
        ouj->nation_id = col->nation_id;
        ouj->profession = UNITS_JOB_MISSIONARY;
      }
      const int adj = colonies_admit_unit_w(&w_admit, cid, uidj);
      CHECK(adj >= 0, "admit jesuit");
      int jroles[COLONIZE_EJECT_ROLE_COUNT];
      int njr = colonies_list_eject_roles(&pool, cid, adj, jroles, COLONIZE_EJECT_ROLE_COUNT);
      int jesuit_miss = 0;
      for (int i = 0; i < njr; ++i) {
        if (jroles[i] == COLONIZE_EJECT_MISSIONARY) {
          jesuit_miss = 1;
        }
      }
      CHECK(jesuit_miss, "#559 Jesuit sees Missionary row in a churchless colony");

      /* (3) #559b: the Colonist row under the DS:0x8dc6 nation-table test. */
      int slot = -1;
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        if (pool.colonies[i].active && pool.colonies[i].id == cid) {
          slot = i;
        }
      }
      CHECK(slot >= 0, "colony pool slot found");
      ColonizeCol1Save* jc = (ColonizeCol1Save*)calloc(1, sizeof(ColonizeCol1Save));
      CHECK(jc != NULL, "col1 scratch for jesuit rows");
      if (jc && slot >= 0 && slot < 4) {
        colonies_set_col1_context(jc);
        jc->player[slot].control = 0; /* human */
        njr = colonies_list_eject_roles(&pool, cid, adj, jroles, COLONIZE_EJECT_ROLE_COUNT);
        int has_colonist = 0;
        for (int i = 0; i < njr; ++i) {
          if (jroles[i] == COLONIZE_EJECT_COLONIST) {
            has_colonist = 1;
          }
        }
        CHECK(!has_colonist, "#559 Jesuit loses Colonist row in a human-indexed slot");
        CHECK(
          colonies_eject_colonist(&pool, cid, adj, &units, COLONIZE_EJECT_COLONIST) < 0,
          "#559 applier refuses the row the list never drew"
        );
        jc->player[slot].control = 1; /* AI */
        njr = colonies_list_eject_roles(&pool, cid, adj, jroles, COLONIZE_EJECT_ROLE_COUNT);
        has_colonist = 0;
        for (int i = 0; i < njr; ++i) {
          if (jroles[i] == COLONIZE_EJECT_COLONIST) {
            has_colonist = 1;
          }
        }
        CHECK(has_colonist, "#559 AI-controlled slot keeps the Colonist row");
        colonies_set_col1_context(NULL);
      }
      free(jc);
      col = colonies_get_mut(&pool, cid);
      if (col) {
        col->has_building[church] = had_church;
      }
    }
  }

  int buildable[32];
  ColoniesBuildableOpts bopts;
  memset(&bopts, 0, sizeof(bopts));
  bopts.map = &map;
  const int n_buildable = colonies_list_buildable(&pool, cid, buildable, 32, &bopts);
  CHECK(n_buildable > 0, "list_buildable returns projects");
  bool listed_stockade = false;
  bool listed_warehouse = false;
  bool listed_weavers_shop = false;
  bool listed_iron_works = false;
  for (int i = 0; i < n_buildable; ++i) {
    if (buildable[i] == stockade) {
      listed_stockade = true;
    }
    if (strcmp(pool.building_types[buildable[i]].name, "Warehouse") == 0) {
      listed_warehouse = true;
    }
    if (strcmp(pool.building_types[buildable[i]].name, "Weaver's Shop") == 0) {
      listed_weavers_shop = true;
    }
    if (strcmp(pool.building_types[buildable[i]].name, "Iron Works") == 0) {
      listed_iron_works = true;
    }
  }
  /* New colony pop=1: Stockade needs min_colony 3. */
  CHECK(!listed_stockade, "Stockade hidden until population 3");
  CHECK(listed_warehouse, "Warehouse is buildable at pop 1");
  CHECK(listed_weavers_shop, "Weaver's Shop upgrade is buildable");
  CHECK(!listed_iron_works, "Iron Works requires Adam Smith");

  /* Grow to Stockade min pop and confirm it appears. */
  {
    ColonizeColony* grow = colonies_get_mut(&pool, cid);
    CHECK(grow != NULL, "mut colony for pop bump");
    grow->population = 3;
    grow->colonist_count = 3;
    for (int i = 1; i < 3; ++i) {
      grow->colonists[i] = grow->colonists[0];
      grow->colonists[i].active = true;
    }
  }
  const int n2 = colonies_list_buildable(&pool, cid, buildable, 32, &bopts);
  listed_stockade = false;
  for (int i = 0; i < n2; ++i) {
    if (buildable[i] == stockade) {
      listed_stockade = true;
    }
  }
  CHECK(listed_stockade, "Stockade is buildable at pop 3");

  bopts.has_adam_smith = true;
  {
    ColonizeColony* grow = colonies_get_mut(&pool, cid);
    grow->population = 8;
    grow->colonist_count = 8;
    for (int i = 3; i < 8; ++i) {
      grow->colonists[i] = grow->colonists[0];
      grow->colonists[i].active = true;
    }
    const int shop = colonies_find_building(&pool, "Blacksmith's Shop");
    CHECK(shop >= 0, "Blacksmith's Shop type exists");
    grow->has_building[shop] = true;
  }
  const int n3 = colonies_list_buildable(&pool, cid, buildable, 32, &bopts);
  listed_iron_works = false;
  for (int i = 0; i < n3; ++i) {
    if (strcmp(pool.building_types[buildable[i]].name, "Iron Works") == 0) {
      listed_iron_works = true;
    }
  }
  CHECK(listed_iron_works, "Iron Works buildable with Adam Smith + shop");

  /* Custom House requires Peter Stuyvesant (not available by default). */
  {
    bool listed_ch = false;
    for (int i = 0; i < n3; ++i) {
      if (strcmp(pool.building_types[buildable[i]].name, "Custom House") == 0) {
        listed_ch = true;
      }
    }
    CHECK(!listed_ch, "Custom House hidden without Stuyvesant");
    bopts.has_peter_stuyvesant = true;
    const int n4 = colonies_list_buildable(&pool, cid, buildable, 32, &bopts);
    listed_ch = false;
    for (int i = 0; i < n4; ++i) {
      if (strcmp(pool.building_types[buildable[i]].name, "Custom House") == 0) {
        listed_ch = true;
      }
    }
    CHECK(listed_ch, "Custom House buildable with Stuyvesant");
  }

  /* tools(*10): Warehouse Expansion NAMES value 2 → 20 tools. */
  {
    const int whe = colonies_find_building(&pool, "Warehouse Expansion");
    CHECK(whe >= 0, "Warehouse Expansion type exists");
    CHECK(pool.building_types[whe].tools_cost == 20, "Warehouse Expansion tools are 20");
  }

  CHECK(colonies_clear_construction(&pool, cid), "clear construction");
  CHECK(colonies_get(&pool, cid)->building_in_production < 0, "construction cleared");
  CHECK(colonies_set_construction(&pool, cid, stockade), "set construction to Stockade");
  CHECK(
    colonies_get(&pool, cid)->building_in_production == stockade,
    "building_in_production is Stockade"
  );
  CHECK(!colonies_set_construction(&pool, cid, town_hall), "cannot set already-built Town Hall");

  CHECK(!colonies_can_found(&pool, &map, land2_x, land2_y), "cannot found on occupied tile");
  CHECK(colonies_id_at(&pool, land2_x, land2_y) == cid, "colonies_id_at returns correct id");
  CHECK(colonies_id_at(&pool, land2_x + 1, land2_y) < 0, "colonies_id_at returns -1 for empty tile");

  /*
   * Distance gate: an active colony blocks founding on any Chebyshev-
   * adjacent (dist<=1) tile too, not just its own occupied tile — a
   * distinct rule from the "occupied tile" check above. Cite: GAME.TXT
   * @TOONEAR; DOS Build Colony order gate (FUN_2b5a_1662 fragment) ->
   * FUN_1000_8804/FUN_15eb_0142 (nearest colony, any nation/type) ->
   * FUN_0000_2500 distance==1 -> @TOONEAR bounce (docs/manual_gap.md
   * "Found colony" row). `colonies_id_at(pool, land2_x+1, land2_y) < 0`
   * above already proves that neighbor tile is unoccupied, so this isolates
   * the adjacency rule from the occupied-tile rule.
   */
  {
    static const int k_dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int k_dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    bool found_adjacent_case = false;
    for (int i = 0; i < 8 && !found_adjacent_case; ++i) {
      const int ax = land2_x + k_dx[i];
      const int ay = land2_y + k_dy[i];
      if (ax < 0 || ay < 0 || ax >= (int)map.width || ay >= (int)map.height) {
        continue;
      }
      if (!map_tile_is_land(&map, ax, ay)) {
        continue;
      }
      const int pedia = map_pedia_terrain_index_at(&map, ax, ay);
      if (pedia == 24 || pedia == 27) {
        continue; /* arctic / mountain would fail for an unrelated reason */
      }
      if (colonies_id_at(&pool, ax, ay) >= 0) {
        continue; /* occupied by another colony; wouldn't isolate the adjacency gate */
      }
      CHECK(
        !colonies_can_found(&pool, &map, ax, ay),
        "cannot found on a tile Chebyshev-adjacent to an existing active colony"
      );
      found_adjacent_case = true;
    }
    CHECK(found_adjacent_case, "found a plain land neighbor tile to isolate the adjacency gate");
  }

  /* SoL craft deepen: +sol_bonus per manufacturing worker on output. */
  {
    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "colony for SoL craft");
    const int shop = colonies_find_building(&pool, "Blacksmith's Shop");
    CHECK(shop >= 0, "Blacksmith's Shop for SoL craft");
    if (c && shop >= 0) {
      c->has_building[shop] = true;
      if (c->colonist_count < 1) {
        c->colonist_count = 1;
        c->colonists[0].active = true;
      }
      c->colonists[0].active = true;
      c->colonists[0].building_type = shop;
      c->colonists[0].profession = 19; /* free colonist */
      c->stock[COLONIZE_CARGO_ORE] = 20;
      c->stock[COLONIZE_CARGO_TOOLS] = 0;
      colony_craft_one_colony(&pool, c, NULL, 0);
      const int tools0 = c->stock[COLONIZE_CARGO_TOOLS];
      CHECK(tools0 > 0, "craft without SoL produces tools");
      c->stock[COLONIZE_CARGO_ORE] = 20;
      c->stock[COLONIZE_CARGO_TOOLS] = 0;
      colony_craft_one_colony(&pool, c, NULL, 2);
      const int tools2 = c->stock[COLONIZE_CARGO_TOOLS];
      CHECK(tools2 > tools0, "SoL +2 craft yields more tools than baseline");
      c->has_building[shop] = false;
      c->colonists[0].building_type = -1;
    }
  }

  /* Abandon removes colony (cargo discarded). */
  {
    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "colony before abandon");
    c->stock[COLONIZE_CARGO_FOOD] = 50;
    /* Warehouse spoilage clamp (FUN_15eb_0a50): tools above base 100 → 100. */
    c->stock[COLONIZE_CARGO_TOOLS] = 150;
    CHECK(colonies_warehouse_capacity(&pool, c, COLONIZE_CARGO_TOOLS) == 100, "base tools cap 100");
    CHECK(
      colonies_apply_warehouse_spoilage(&pool, c, NULL, NULL, NULL) == 50,
      "spoil 50 tools over cap"
    );
    CHECK(c->stock[COLONIZE_CARGO_TOOLS] == 100, "tools clamped to 100");

    /*
     * Overflow produced this very turn is clamped silently — no reported loss
     * (FUN_364b_0688 only reports the part that predates production).
     */
    {
      int before_prod[COLONIZE_CARGO_COUNT];
      for (int ci = 0; ci < COLONIZE_CARGO_COUNT; ++ci) {
        before_prod[ci] = c->stock[ci];
      }
      c->stock[COLONIZE_CARGO_TOOLS] = 150;
      CHECK(
        colonies_apply_warehouse_spoilage(&pool, c, before_prod, NULL, NULL) == 0,
        "production overflow does not spoil"
      );
      CHECK(c->stock[COLONIZE_CARGO_TOOLS] == 100, "production overflow still clamped");
    }

    /* Multi-type spoil → type count. */
    c->stock[COLONIZE_CARGO_TOOLS] = 150;
    c->stock[COLONIZE_CARGO_LUMBER] = 150;
    int first = -1, types = 0;
    CHECK(
      colonies_apply_warehouse_spoilage(&pool, c, NULL, &first, &types) == 100,
      "spoil 100 across two cargos"
    );
    CHECK(types == 2, "spoil type count 2");
    CHECK(first == COLONIZE_CARGO_LUMBER || first == COLONIZE_CARGO_TOOLS, "first spoil set");
    CHECK(c->stock[COLONIZE_CARGO_TOOLS] == 100, "tools reclamped");
    CHECK(c->stock[COLONIZE_CARGO_LUMBER] == 100, "lumber clamped");
    const int stockade_b = colonies_find_building(&pool, "Stockade");
    CHECK(stockade_b >= 0, "stockade type for fortification check");
    c->has_building[stockade_b] = true;
    CHECK(colonies_has_fortification(&pool, c), "fortification detected");
    /*
     * bugs.md: the layer2 settlement bit is what the map renderer reads to
     * hide a garrison, so founding must set it and abandoning must clear it —
     * otherwise units left on an abandoned colony's square stay invisible
     * until the next end-of-turn rebuild.
     */
    colonies_set_occupancy_map(&map);
    CHECK(!map_tile_has_city(&map, land2_x, land2_y), "no settlement bit before rebind");
    CHECK(colonies_abandon(&pool, cid), "abandon colony");
    CHECK(colonies_get(&pool, cid) == NULL, "colony gone after abandon");
    CHECK(colonies_id_at(&pool, land2_x, land2_y) < 0, "tile free after abandon");
    {
      const int recid =
        colonies_found(&pool, &map, land2_x, land2_y, 0, pioneer_type, UNITS_JOB_PIONEER, 0, 0, 0);
      CHECK(recid >= 0, "re-found for settlement bit check");
      CHECK(map_tile_has_city(&map, land2_x, land2_y), "found sets the settlement bit");
      CHECK(colonies_abandon(&pool, recid), "abandon again");
      CHECK(!map_tile_has_city(&map, land2_x, land2_y), "abandon clears the settlement bit");
    }
    colonies_set_occupancy_map(NULL);
  }

  /* Re-found so map-icon test has a colony. */
  cid = colonies_found(&pool, &map, land2_x, land2_y, 0, pioneer_type, UNITS_JOB_PIONEER, 0, 0, 0);
  CHECK(cid >= 0, "re-found for map icon");

  /* Map marker: ICONS.SS #0–3 colony settlement (not cargo greys #38+). */
  {
    ColonizeSpriteSheet icons;
    memset(&icons, 0, sizeof(icons));
    char icons_err[256];
    const bool icons_ok = ss_load("COLONIZE/ICONS.SS", &icons, icons_err, sizeof(icons_err));
    CHECK(icons_ok && icons.sprite_count > 3, "load ICONS.SS for colony map icon");
    if (icons_ok) {
      CHECK(icons.sprites[3].width == 21 && icons.sprites[3].height == 16,
            "unfortified colony icon is 21x16");
      uint8_t pixels[16 * 16];
      memset(pixels, 0, sizeof(pixels));
      ColonizeFramebuffer8 fb = {.width = 16, .height = 16, .pixels = pixels};
      colonies_render_on_map(&pool, &icons, &fb, NULL, NULL, land2_x, land2_y, 1, 1, 16, 16, 0, 0, NULL, 0, NULL);

      int cyan = 0;
      int opaque = 0;
      for (int i = 0; i < 16 * 16; ++i) {
        if (pixels[i] == 11) {
          ++cyan;
        }
        if (pixels[i] != 0 && pixels[i] != COLONIZE_SS_TRANSPARENT) {
          ++opaque;
        }
      }
      CHECK(opaque > 0, "colony map icon painted opaque pixels");
      CHECK(cyan < 16 * 16, "colony map icon is not a solid cyan tile");
      ss_free(&icons);
    }
  }

  assets_msg_free(&names);
  map_free(&map);

  /* colonies_destroy_building: clear building + workplace; refuse Town Hall. */
  {
    const int stockade = colonies_find_building(&pool, "Stockade");
    const int town = colonies_find_building(&pool, "Town Hall");
    CHECK(stockade >= 0 && town >= 0, "Stockade and Town Hall types for destroy");
    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "destroy colony lookup");
    if (c && stockade >= 0 && town >= 0) {
      c->has_building[stockade] = true;
      if (c->colonist_count > 0) {
        c->colonists[0].building_type = stockade;
      }
      CHECK(colonies_destroy_building(&pool, cid, stockade), "destroy Stockade");
      CHECK(!c->has_building[stockade], "Stockade cleared");
      if (c->colonist_count > 0) {
        CHECK(c->colonists[0].building_type == -1, "workplace cleared on destroy");
      }
      CHECK(!colonies_destroy_building(&pool, cid, town), "refuse destroy Town Hall");
      CHECK(c->has_building[town], "Town Hall remains");
    }
  }

  /*
   * SoL %: purely the Col1 rebel dividend/divisor pair (FUN_15eb_0274). No
   * pair (or a zero divisor) reads 0 — the old nation-liberty-bells/4
   * stand-in was a nation figure and made a colony founded this turn show
   * 100% (bugs.md). Cite: colony_prod_sol_percent; manual_gap SoL display.
   */
  {
    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "sol colony");
    if (c) {
      ColonizeCol1Save col1;
      ColonizeCol1Colony col1c;
      memset(&col1, 0, sizeof(col1));
      memset(&col1c, 0, sizeof(col1c));
      col1.colony = &col1c;
      col1.head.colony_count = 1;
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        col1.head.founding_father[i] = -1;
      }
      col1c.x = (uint8_t)c->x;
      col1c.y = (uint8_t)c->y;
      col1c.nation_id = (uint8_t)c->nation_id;
      col1c.rebel_dividend = 50;
      col1c.rebel_divisor = 100;
      CHECK(colony_prod_sol_percent(&col1, c) == 50, "SoL from rebel 50/100");
      c->colony_flags = 0;
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0, "sol_50 latch at 50%");
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) == 0, "sol_100 clear at 50%");
      /* Bonus reads the LATCH bits only (18ec 11881-11886), never live SoL. */
      CHECK(colony_prod_sol_bonus(&col1, c) == 1, "SoL bonus +1 at 50%");
      col1c.rebel_dividend = 0;
      col1c.rebel_divisor = 0;
      col1.nation[c->nation_id].liberty_bells_total = 400;
      CHECK(colony_prod_sol_percent(&col1, c) == 0, "no rebel pair reads 0, not nation bells");
      col1c.rebel_dividend = 50;
      col1c.rebel_divisor = 100;
      CHECK(colony_prod_sol_percent(&col1, c) == 50, "SoL back from the pair");
      CHECK(colony_prod_sol_bonus(&col1, c) == 1, "SoL bonus +1 at 50%");
      col1c.rebel_dividend = 100;
      col1c.rebel_divisor = 100;
      CHECK(colony_prod_sol_percent(&col1, c) == 100, "SoL 100 from the pair");
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0, "sol_100 latch at 100%");
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0, "sol_50 stays at 100%");
      CHECK(colony_prod_sol_bonus(&col1, c) == 2, "SoL bonus +2 at 100%");
      /* DOS hysteresis: sol_100 stays while SoL in 95..99. */
      col1c.rebel_dividend = 97;
      col1c.rebel_divisor = 100;
      CHECK(colony_prod_sol_percent(&col1, c) == 97, "SoL 97 for hysteresis");
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0, "sol_100 holds at 97%");
      col1c.rebel_dividend = 90;
      col1c.rebel_divisor = 100;
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) == 0, "sol_100 clear below 95%");
      CHECK((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0, "sol_50 holds at 90%");
      col1c.rebel_dividend = 0;
      col1c.rebel_divisor = 100;
      colony_prod_refresh_sol_flags(c, &col1);
      CHECK((c->colony_flags & (COLONIZE_COLONY_FLAG_SOL_50 | COLONIZE_COLONY_FLAG_SOL_100)) == 0,
            "SoL flags clear when SoL drops");

      /*
       * FUN_15eb_0274 Bolivar: +20 display for human; AI control gets none; cap 100.
       * Cite: sons_of_liberty.md; founding_fathers_bolivar_sol_bonus.
       */
      {
        for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
          col1.head.founding_father[i] = -1;
        }
        col1.player[c->nation_id].control = 0;
        col1c.rebel_dividend = 40;
        col1c.rebel_divisor = 100;
        CHECK(colony_prod_sol_percent(&col1, c) == 40, "SoL 40 without Bolivar");
        /* Ownership is the per-nation bitmask (smell audit #83); the head
         * first-claimer byte is set alongside only because DOS's elect
         * commit writes it once, not because anything gates on it. */
        col1.head.founding_father[FF_SIMON_BOLIVAR] = (int8_t)c->nation_id;
        col1.nation[c->nation_id].founding_fathers[FF_SIMON_BOLIVAR / 8] |=
          (uint8_t)(1u << (FF_SIMON_BOLIVAR % 8));
        CHECK(colony_prod_sol_percent(&col1, c) == 60, "SoL Bolivar human 40+20");
        col1c.rebel_dividend = 90;
        CHECK(colony_prod_sol_percent(&col1, c) == 100, "SoL Bolivar caps at 100");
        col1c.rebel_dividend = 40;
        col1.player[c->nation_id].control = 1; /* AI */
        CHECK(colony_prod_sol_percent(&col1, c) == 40, "SoL Bolivar skipped for AI");
        col1.head.founding_father[FF_SIMON_BOLIVAR] = -1;
        col1.nation[c->nation_id].founding_fathers[FF_SIMON_BOLIVAR / 8] &=
          (uint8_t)~(1u << (FF_SIMON_BOLIVAR % 8));
        col1.player[c->nation_id].control = 0;
        col1c.rebel_dividend = 50;
        col1c.rebel_divisor = 100;
      }

      /*
       * Tory floor: −⌊tories/thresh⌋ + sol latches.
       * Cite: sons_of_liberty.md; difficulty.md; decomp ~11880.
       */
      {
        const int pop_saved = c->population;
        const int count_saved = c->colonist_count;
        const uint8_t flags_saved = c->colony_flags;
        c->population = 12;
        c->colonist_count = 12;
        c->colony_flags = 0;
        col1.head.difficulty = 0; /* Discoverer → human thresh 10 */
        col1.player[c->nation_id].control = 0; /* human */
        col1c.rebel_dividend = 0;
        col1c.rebel_divisor = 100; /* sol% = 0 */
        col1.nation[c->nation_id].liberty_bells_total = 0;
        /* tories=(12*100+50)/100=12; floor(12/10)=1 → mod=-1 */
        CHECK(colony_prod_sol_bonus(&col1, c) == -1, "Tory floor −1 pop12 sol0 Discoverer");

        col1.head.difficulty = 4; /* Viceroy → thresh 6 */
        /* floor(12/6)=2 → mod=-2 */
        CHECK(colony_prod_sol_bonus(&col1, c) == -2, "Tory floor −2 pop12 sol0 Viceroy");

        col1.player[c->nation_id].control = 1; /* AI → thresh 10 always */
        CHECK(colony_prod_sol_bonus(&col1, c) == -1, "Tory floor AI thresh 10 at Viceroy");

        /*
         * bugs.md: the floor is unbounded — DOS clamps the finished yield at 0,
         * never the modifier. A -2 cap used to live in colony_prod_sol_bonus and
         * silently protected large low-SoL colonies.
         */
        c->population = 24;
        c->colonist_count = 24;
        col1.player[c->nation_id].control = 0; /* human, Viceroy → thresh 6 */
        /* tories=24; floor(24/6)=4 → mod=-4, not the old -2 */
        CHECK(colony_prod_sol_bonus(&col1, c) == -4, "Tory floor is not capped at -2");
        c->population = 12;
        c->colonist_count = 12;

        /* sol 50 + latch: tories=(12*50+50)/100=6; floor(6/10)=0; +1 → 1 */
        col1.player[c->nation_id].control = 0;
        col1.head.difficulty = 0;
        col1c.rebel_dividend = 50;
        col1c.rebel_divisor = 100;
        c->colony_flags = COLONIZE_COLONY_FLAG_SOL_50;
        CHECK(colony_prod_sol_bonus(&col1, c) == 1, "Tory floor 0 + sol_50 latch");

        c->population = pop_saved;
        c->colonist_count = count_saved;
        c->colony_flags = flags_saved;
        col1.head.difficulty = 0;
        col1.player[c->nation_id].control = 0;
        col1c.rebel_dividend = 0;
        col1c.rebel_divisor = 0;
      }
    }
  }

  /*
   * TRADE Edit autofill: unload from unit holds; load from colony surplus.
   * Cite: colonies_trade_stop_autofill.
   */
  {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    units_set_occupancy_map(NULL);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
    units.types[0].movement = 2;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[0].cargo = 2;

    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "autofill colony");
    if (c) {
      c->stock[COLONIZE_CARGO_TOOLS] = 80;
      c->stock[COLONIZE_CARGO_LUMBER] = 80;
      c->stock[COLONIZE_CARGO_ORE] = 5;
      const int wid = units_spawn(&units, 0, c->x, c->y);
      ColonizeUnit* w = units_get(&units, wid);
      CHECK(w != NULL, "autofill wagon");
      if (w) {
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_FOOD, 20) == 20, "autofill FOOD hold");
        ColonizeCol1TradeStop st;
        memset(&st, 0, sizeof(st));
        st.colony_index = (uint16_t)cid;
        colonies_trade_stop_autofill(&st, c, &units, wid);
        CHECK(st.unload_count == 1, "autofill unload FOOD");
        CHECK(
          col1_trade_nibble_cargo(st.unload_cargo_nibbles, 0) == COLONIZE_CARGO_FOOD,
          "autofill unload nibble FOOD"
        );
        CHECK(st.load_count >= 2, "autofill load tools+lumber surplus");
        CHECK(
          col1_trade_nibble_cargo(st.load_cargo_nibbles, 0) == COLONIZE_CARGO_TOOLS,
          "autofill load TOOLS first"
        );
        CHECK(
          col1_trade_nibble_cargo(st.load_cargo_nibbles, 1) == COLONIZE_CARGO_LUMBER,
          "autofill load LUMBER second"
        );
        /* Europe: unload only */
        ColonizeCol1TradeStop eu;
        memset(&eu, 0, sizeof(eu));
        eu.colony_index = 999;
        colonies_trade_stop_autofill(&eu, NULL, &units, wid);
        CHECK(eu.unload_count == 1 && eu.load_count == 0, "Europe autofill unload-only");
      }
    }
  }

  /*
   * TRADE cargo picker setter: explicit unload TOOLS + load SILVER (not autofill).
   * Cite: colonies_trade_stop_set_cargos; ColonizeCol1TradeStop.
   */
  {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    units_set_occupancy_map(NULL);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
    units.types[0].movement = 2;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[0].cargo = 2;

    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "picker colony");
    if (c) {
      c->stock[COLONIZE_CARGO_TOOLS] = 40;
      c->stock[COLONIZE_CARGO_SILVER] = 80;
      c->stock[COLONIZE_CARGO_FOOD] = 5;
      const int tools0 = c->stock[COLONIZE_CARGO_TOOLS];
      const int silver0 = c->stock[COLONIZE_CARGO_SILVER];

      const int wid = units_spawn(&units, 0, c->x, c->y);
      ColonizeUnit* w = units_get(&units, wid);
      CHECK(w != NULL, "picker wagon");
      if (w) {
        w->nation_id = c->nation_id;
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_TOOLS, 20) == 20, "picker TOOLS");
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_FOOD, 20) == 20, "picker FOOD");

        ColonizeCol1TradeStop st;
        memset(&st, 0, sizeof(st));
        st.colony_index = (uint16_t)cid;
        const int unload_t[] = {COLONIZE_CARGO_TOOLS};
        const int load_t[] = {COLONIZE_CARGO_SILVER};
        colonies_trade_stop_set_cargos(&st, unload_t, 1, load_t, 1);
        CHECK(st.unload_count == 1 && st.load_count == 1, "picker counts");
        CHECK(
          col1_trade_nibble_cargo(st.unload_cargo_nibbles, 0) == COLONIZE_CARGO_TOOLS,
          "picker unload TOOLS"
        );
        CHECK(
          col1_trade_nibble_cargo(st.load_cargo_nibbles, 0) == COLONIZE_CARGO_SILVER,
          "picker load SILVER"
        );
        CHECK(
          colonies_trade_route_service_stop(&pool, cid, &units, wid, &st) == 1,
          "picker service"
        );
        CHECK(c->stock[COLONIZE_CARGO_TOOLS] == tools0 + 20, "picker unload TOOLS");
        /* DOS load phase drains warehouse stock into free holds (100/hold). */
        CHECK(c->stock[COLONIZE_CARGO_SILVER] == silver0 - 80, "picker load SILVER");
        int food_left = 0;
        int silver_on = 0;
        const int nh = units_goods_hold_count(&units, wid);
        for (int h = 0; h < nh; ++h) {
          if (w->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
            food_left += w->hold_goods_amount[h];
          }
          if (w->hold_goods_type[h] == COLONIZE_CARGO_SILVER) {
            silver_on += w->hold_goods_amount[h];
          }
        }
        CHECK(food_left == 20, "picker FOOD stays aboard");
        CHECK(silver_on == 80, "picker SILVER aboard");

        /* Europe: unload list only */
        ColonizeCol1TradeStop eu;
        memset(&eu, 0, sizeof(eu));
        eu.colony_index = 999;
        colonies_trade_stop_set_cargos(&eu, unload_t, 1, load_t, 1);
        /* Force load_count 0 for Europe sell path semantics in setter use. */
        colonies_trade_stop_set_cargos(&eu, unload_t, 1, NULL, 0);
        CHECK(eu.unload_count == 1 && eu.load_count == 0, "Europe picker unload-only");
      }
    }
  }

  /*
   * TRADE stop Col1 nibbles: unload only listed cargo; load listed cargo.
   * Cite: ColonizeCol1TradeStop; colonies_trade_route_service_stop.
   */
  {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    units_set_occupancy_map(NULL);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
    units.types[0].movement = 2;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[0].cargo = 2;

    ColonizeColony* c = colonies_get_mut(&pool, cid);
    CHECK(c != NULL, "trade-nibble colony");
    if (c) {
      c->stock[COLONIZE_CARGO_TOOLS] = 80;
      c->stock[COLONIZE_CARGO_LUMBER] = 80;
      c->stock[COLONIZE_CARGO_FOOD] = 5;
      const int tools0 = c->stock[COLONIZE_CARGO_TOOLS];
      const int lumber0 = c->stock[COLONIZE_CARGO_LUMBER];

      const int wid = units_spawn(&units, 0, c->x, c->y);
      ColonizeUnit* w = units_get(&units, wid);
      CHECK(w != NULL, "trade-nibble wagon spawn");
      if (w) {
        w->nation_id = c->nation_id;
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_FOOD, 20) == 20, "load FOOD onto wagon");
        CHECK(units_load_goods(&units, wid, COLONIZE_CARGO_TOOLS, 20) == 20, "load TOOLS onto wagon");

        ColonizeCol1TradeStop st;
        memset(&st, 0, sizeof(st));
        st.colony_index = (uint16_t)cid;
        st.unload_count = 1;
        col1_trade_nibble_set(st.unload_cargo_nibbles, 0, COLONIZE_CARGO_TOOLS);
        st.load_count = 1;
        col1_trade_nibble_set(st.load_cargo_nibbles, 0, COLONIZE_CARGO_LUMBER);

        CHECK(
          colonies_trade_route_service_stop(&pool, cid, &units, wid, &st) == 1,
          "trade nibble service moves cargo"
        );
        CHECK(c->stock[COLONIZE_CARGO_TOOLS] == tools0 + 20, "unload TOOLS only into warehouse");
        CHECK(c->stock[COLONIZE_CARGO_FOOD] == 5, "FOOD stay on wagon (not in unload list)");
        int food_left = 0;
        int lumber_on = 0;
        int tools_on = 0;
        const int nh = units_goods_hold_count(&units, wid);
        for (int h = 0; h < nh; ++h) {
          if (w->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
            food_left += w->hold_goods_amount[h];
          }
          if (w->hold_goods_type[h] == COLONIZE_CARGO_LUMBER) {
            lumber_on += w->hold_goods_amount[h];
          }
          if (w->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
            tools_on += w->hold_goods_amount[h];
          }
        }
        CHECK(food_left == 20, "FOOD remains on wagon after selective unload");
        CHECK(tools_on == 0, "TOOLS hold cleared");
        /* DOS load phase fills the freed hold from warehouse stock (≤100). */
        CHECK(lumber_on == 80, "load LUMBER per Col1 load nibble");
        CHECK(c->stock[COLONIZE_CARGO_LUMBER] == lumber0 - 80, "warehouse LUMBER decreased");
        CHECK(
          col1_trade_nibble_cargo(st.unload_cargo_nibbles, 0) == COLONIZE_CARGO_TOOLS,
          "nibble pack TOOLS low"
        );
        CHECK(
          col1_trade_nibble_cargo(st.load_cargo_nibbles, 0) == COLONIZE_CARGO_LUMBER,
          "nibble pack LUMBER low"
        );
      }
    }
  }

  if (failures == failures_before) {
    printf("unit_colonies: all checks passed\n");
    return 0;
  }
  fprintf(stderr, "unit_colonies: %d failure(s)\n", failures - failures_before);
  return 1;
}

/*
 * FUN_5f7a_020e foreign-colony trade (docs/foreign_colony_trade.md): the gates,
 * the price, the counter-offer search and both accept arms.
 */
static int unit_foreign_colony_trade(void) {
  const int failures_before = failures;

  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* c = &pool.colonies[0];
  memset(c, 0, sizeof(*c));
  c->active = true;
  c->id = 0;
  c->nation_id = 1; /* French */
  c->x = 4;
  c->y = 4;
  c->building_in_production = -1;
  c->stock[COLONIZE_CARGO_CIGARS] = 60;
  c->stock[COLONIZE_CARGO_RUM] = 5;
  pool.colony_count = 1;

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;
  units.types[0].movement = 2;
  const int uid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* u = units_get(&units, uid);
  if (!u) {
    fprintf(stderr, "FAIL: foreign trade wagon spawn\n");
    failures++;
    return 1;
  }
  u->nation_id = 0; /* English, human */

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.head.difficulty = 2;
  col1.head.rival_nation_slot_1 = 3;
  col1.head.crown_nation_id = -1;
  col1.player[0].control = 0;
  col1.player[1].control = 1;
  /* DS:0x84bc byte = euro_price - 1: Sugar 6, Cigars 4. */
  col1.nation[1].trade.euro_price[COLONIZE_CARGO_SUGAR] = 7;
  col1.nation[1].trade.euro_price[COLONIZE_CARGO_CIGARS] = 5;
  col1.nation[1].tax_rate = 10;

  ColonizeWorld w = world_make(&units, &pool, NULL, &col1, true, NULL, NULL);

  /* No peace treaty yet → @TRADEATWAR (raw 98924-98927). */
  CHECK(
    colonies_foreign_trade_gate(&w, 0, uid) == COLONIZE_FTRADE_ATWAR,
    "no peace treaty refuses with TRADEATWAR"
  );
  ai_diplo_write(&col1, 0, 1, (uint8_t)(ai_diplo_read(&col1, 0, 1) | AI_DIPLO_PEACE));
  CHECK(
    colonies_foreign_trade_gate(&w, 0, uid) == COLONIZE_FTRADE_MERCANTILISM,
    "no Jan de Witt refuses with TRADEMERCANTILISM"
  );
  col1.nation[0].founding_fathers[FF_JAN_DE_WITT / 8] |=
    (uint8_t)(1u << (FF_JAN_DE_WITT % 8));
  CHECK(
    colonies_foreign_trade_gate(&w, 0, uid) == COLONIZE_FTRADE_NOCARGO,
    "empty transport refuses with TRADENOCARGO"
  );
  /* An AI-controlled actor never gets a dialog at all (raw 98921-98923). */
  u->nation_id = 1;
  c->nation_id = 0;
  CHECK(
    colonies_foreign_trade_gate(&w, 0, uid) == COLONIZE_FTRADE_NONE,
    "AI-controlled nation never trades with a foreign colony"
  );
  u->nation_id = 0;
  c->nation_id = 1;

  CHECK(units_load_goods(&units, uid, COLONIZE_CARGO_SUGAR, 20) == 20, "sugar loaded");
  CHECK(
    colonies_foreign_trade_gate(&w, 0, uid) == COLONIZE_FTRADE_OK, "loaded transport may trade"
  );

  /* rng NULL → the 10% floor of the haggle band, so the price is exact. */
  ColonizeForeignTradeDeal deal;
  CHECK(colonies_foreign_trade_prepare(&w, NULL, 0, uid, 0, &deal) == 1, "deal prepared");
  /* gross = 6*20 = 120; −10% tax = 108; −10% haggle = 108 + (10*108)/-100 = 98. */
  CHECK(deal.sold_cargo == COLONIZE_CARGO_SUGAR && deal.sold_qty == 20, "deal sells the hold");
  CHECK(deal.gold == 98, "gold = gross − tax − haggle");
  /* Cigars at 4 each: 60 in stock is 240 > 120, walked down to 30 (= 120). */
  CHECK(
    deal.offer_cargo == COLONIZE_CARGO_CIGARS && deal.offer_qty == 30,
    "counter-offer is the most valuable affordable stock"
  );

  /* Take the goods: the hold is overwritten, the colony banks the sugar. */
  ColonizeForeignTradeDeal goods_deal = deal;
  CHECK(colonies_foreign_trade_apply(&w, 0, uid, 0, &goods_deal, 1) == 1, "barter applied");
  CHECK(
    u->hold_goods_type[0] == COLONIZE_CARGO_CIGARS && u->hold_goods_amount[0] == 30,
    "hold becomes the offered cargo"
  );
  CHECK(c->stock[COLONIZE_CARGO_SUGAR] == 20, "colony banks the sold cargo");
  CHECK(c->stock[COLONIZE_CARGO_CIGARS] == 60, "DOS never debits the offered cargo");

  /* Take the gold: hold emptied, purse credited. */
  ColonizeUnit* u2 = units_get(&units, uid);
  u2->hold_goods_type[0] = COLONIZE_CARGO_SUGAR;
  u2->hold_goods_amount[0] = 20;
  c->stock[COLONIZE_CARGO_SUGAR] = 0;
  const uint32_t gold0 = europe_nation_gold(NULL, &col1, 0);
  CHECK(colonies_foreign_trade_apply(&w, 0, uid, 0, &deal, 0) == 1, "gold sale applied");
  CHECK(units_hold_amount(&units, uid, 0) == 0, "hold emptied on a gold sale");
  CHECK(
    europe_nation_gold(NULL, &col1, 0) == gold0 + (uint32_t)deal.gold, "gold credited"
  );
  CHECK(c->stock[COLONIZE_CARGO_SUGAR] == 20, "colony banks the sold cargo on a gold sale");

  if (failures == failures_before) {
    printf("unit_colonies: foreign-colony trade ok\n");
    return 0;
  }
  return 1;
}

/*
 * bugs.md #481 — colony ship construction. Pins the FUN_15eb_32f8 raw-code
 * decode (42..48 = @UNIT 11..17, Man-O-War excluded), the FUN_15eb_33aa cost
 * arithmetic (@UNIT cost x 0x20 with the 0x28 floor, tools x 10) and the
 * FUN_15eb_3650 Shipyard gate, then spawns a Frigate through the EOT path.
 */
static int unit_ship_construction(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "ship construction: load NAMES.TXT failed\n");
    return 1;
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "ship construction: load @UNIT failed\n");
    return 1;
  }
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  if (!colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "ship construction: load @BUILDING failed\n");
    return 1;
  }
  int failures = 0;
  const int failures_before = failures;

  /* Decode: codes 42..48 only; @BUILDING has exactly 42 rows. */
  CHECK(pool.building_type_count == 42, "@BUILDING has 0x2a rows (the code base)");
  CHECK(colonies_find_building(&pool, "Shipyard") == 8, "Shipyard is @BUILDING index 8");
  CHECK(units_build_code_to_index(41) < 0, "code 41 is still a building");
  CHECK(units_build_code_to_index(42) == 11, "code 42 -> @UNIT 11 (Artillery)");
  CHECK(units_build_code_to_index(48) == 17, "code 48 -> @UNIT 17 (Frigate)");
  CHECK(units_build_code_to_index(49) < 0, "code 49 (Man-O-War) is not buildable");

  /* Costs straight out of the @UNIT cost/tools columns. */
  struct {
    int code;
    const char* name;
    int hammers;
    int tools;
  } expect[] = {
    {COLONIZE_UNIT_BUILD_ARTILLERY, "Artillery", 192, 40},
    {COLONIZE_UNIT_BUILD_WAGON_TRAIN, "Wagon Train", 40, 0},
    {COLONIZE_UNIT_BUILD_CARAVEL, "Caravel", 128, 40},
    {COLONIZE_UNIT_BUILD_MERCHANTMAN, "Merchantman", 192, 80},
    {COLONIZE_UNIT_BUILD_GALLEON, "Galleon", 320, 100},
    {COLONIZE_UNIT_BUILD_PRIVATEER, "Privateer", 256, 120},
    {COLONIZE_UNIT_BUILD_FRIGATE, "Frigate", 512, 200}
  };
  for (int i = 0; i < (int)(sizeof(expect) / sizeof(expect[0])); ++i) {
    const char* nm = NULL;
    int h = -1;
    int t = -1;
    CHECK(colonies_unit_build_info(expect[i].code, &nm, &h, &t), "unit build info resolves");
    CHECK(nm && strcmp(nm, expect[i].name) == 0, "unit build name matches @UNIT");
    CHECK(h == expect[i].hammers, "unit build hammers = cost * 0x20 (0x28 floor)");
    CHECK(t == expect[i].tools, "unit build tools = tools column * 10");
  }

  ColonizeColony* c = &pool.colonies[0];
  memset(c, 0, sizeof(*c));
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->population = 3;
  c->colonist_count = 3;
  c->x = 5;
  c->y = 5;
  c->building_in_production = -1;
  pool.colony_count = 1;

  int buildable[64];
  ColoniesBuildableOpts bopts;
  memset(&bopts, 0, sizeof(bopts));
  int n = colonies_list_buildable(&pool, 0, buildable, 64, &bopts);
  bool saw_ship = false;
  bool saw_wagon = false;
  bool saw_manowar = false;
  for (int i = 0; i < n; ++i) {
    if (buildable[i] >= COLONIZE_UNIT_BUILD_CARAVEL && buildable[i] <= COLONIZE_UNIT_BUILD_FRIGATE) {
      saw_ship = true;
    }
    if (buildable[i] == COLONIZE_UNIT_BUILD_WAGON_TRAIN) {
      saw_wagon = true;
    }
    if (buildable[i] == 49) {
      saw_manowar = true;
    }
  }
  CHECK(!saw_ship, "ships hidden without a Shipyard");
  CHECK(saw_wagon, "Wagon Train has no building gate");
  CHECK(!saw_manowar, "Man-O-War never listed");
  CHECK(
    !colonies_set_construction(&pool, 0, COLONIZE_UNIT_BUILD_FRIGATE),
    "Frigate refused without a Shipyard"
  );

  c->has_building[colonies_find_building(&pool, "Shipyard")] = true;
  n = colonies_list_buildable(&pool, 0, buildable, 64, &bopts);
  int ship_codes = 0;
  for (int i = 0; i < n; ++i) {
    if (buildable[i] >= COLONIZE_UNIT_BUILD_CARAVEL && buildable[i] <= COLONIZE_UNIT_BUILD_FRIGATE) {
      ship_codes++;
    }
  }
  CHECK(ship_codes == 5, "all five buildable ships listed with a Shipyard");
  CHECK(
    colonies_set_construction(&pool, 0, COLONIZE_UNIT_BUILD_FRIGATE),
    "Frigate accepted with a Shipyard"
  );
  CHECK(c->building_in_production == 48, "raw code 48 stored verbatim");

  /* Completion: hammers + tools gate, then the ship spawns at the colony. */
  c->hammers = 511;
  c->stock[COLONIZE_CARGO_TOOLS] = 200;
  CHECK(
    colonies_try_complete_unit_construction(&pool, 0, &units) < 0,
    "one hammer short does not complete"
  );
  c->hammers = 512;
  c->stock[COLONIZE_CARGO_TOOLS] = 199;
  CHECK(
    colonies_try_complete_unit_construction(&pool, 0, &units) < 0,
    "one tool short does not complete"
  );
  c->stock[COLONIZE_CARGO_TOOLS] = 210;
  const int uid = colonies_try_complete_unit_construction(&pool, 0, &units);
  CHECK(uid > 0, "Frigate completes");
  const ColonizeUnit* ship = units_get_const(&units, uid);
  CHECK(ship && ship->active, "Frigate unit active");
  CHECK(ship && ship->x == 5 && ship->y == 5, "Frigate spawns on the colony tile");
  CHECK(ship && ship->nation_id == 0, "Frigate takes the colony's nation");
  CHECK(ship && units_is_sea(&units, ship->id), "Frigate is a sea unit");
  CHECK(c->stock[COLONIZE_CARGO_TOOLS] == 10, "200 tools debited");
  CHECK(c->hammers == 0, "hammers reset (FUN_364b_0114 raw 56964: +0x92 = 0)");
  CHECK(c->building_in_production == 48, "DOS never clears the project on completion");

  if (failures == failures_before) {
    printf("unit_colonies: ship construction ok\n");
    return 0;
  }
  return 1;
}

/*
 * DOS-LITERAL FUN_15eb_3650 raw 13736-13740 / FUN_364b_0114 raw 56926-56933:
 * wagons < colonies per nation, read off the census window; plus the literal
 * Artillery gate has_building(3) = the Armory bit alone (FUN_15eb_035e tests
 * one bit; FUN_15eb_1030 never clears the tier below on upgrade).
 */
static int unit_wagon_cap_and_armory_gate(void) {
  const int failures_before = failures;
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "wagon cap: load NAMES.TXT failed\n");
    return 1;
  }
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  if (!colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "wagon cap: load @BUILDING failed\n");
    return 1;
  }
  ColonizeColony* c = &pool.colonies[0];
  memset(c, 0, sizeof(*c));
  c->id = 0;
  c->active = true;
  c->nation_id = 1;
  c->population = 3;
  c->colonist_count = 3;
  c->building_in_production = -1;
  pool.colony_count = 1;

  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.stuff.colony_counts[1] = 3;
  col1.stuff.unit_type_counts[1][12] = 2;

  ColoniesBuildableOpts bopts;
  memset(&bopts, 0, sizeof(bopts));
  bopts.col1 = &col1;

  CHECK(!colonies_wagon_cap_reached(&col1, 1), "2 wagons < 3 colonies: cap open");
  CHECK(colonies_nation_colony_count_census(&col1, 1) == 3, "%NUMBER0 = colony count");
  CHECK(
    colonies_set_construction_ex(&pool, 0, COLONIZE_UNIT_BUILD_WAGON_TRAIN, &bopts),
    "Wagon Train accepted under the cap"
  );
  col1.stuff.unit_type_counts[1][12] = 3;
  CHECK(colonies_wagon_cap_reached(&col1, 1), "3 wagons vs 3 colonies: cap reached (<=)");
  CHECK(
    !colonies_set_construction_ex(&pool, 0, COLONIZE_UNIT_BUILD_WAGON_TRAIN, &bopts),
    "Wagon Train refused at the cap"
  );
  int buildable[64];
  int n = colonies_list_buildable(&pool, 0, buildable, 64, &bopts);
  bool saw_wagon = false;
  for (int i = 0; i < n; ++i) {
    if (buildable[i] == COLONIZE_UNIT_BUILD_WAGON_TRAIN) {
      saw_wagon = true;
    }
  }
  CHECK(!saw_wagon, "Wagon Train dropped from the list at the cap");
  CHECK(!colonies_wagon_cap_reached(NULL, 1), "no census = no cap");

  /* Artillery gate: the Armory bit only, never a Magazine/Arsenal fold. */
  const int armory = colonies_find_building(&pool, "Armory");
  const int magazine = colonies_find_building(&pool, "Magazine");
  CHECK(armory >= 0 && magazine >= 0, "Armory / Magazine rows exist");
  CHECK(
    !colonies_set_construction_ex(&pool, 0, COLONIZE_UNIT_BUILD_ARTILLERY, &bopts),
    "Artillery refused with no Armory"
  );
  c->has_building[magazine] = true;
  CHECK(
    !colonies_set_construction_ex(&pool, 0, COLONIZE_UNIT_BUILD_ARTILLERY, &bopts),
    "a pillaged Magazine without the Armory bit still refuses Artillery"
  );
  c->has_building[armory] = true;
  CHECK(
    colonies_set_construction_ex(&pool, 0, COLONIZE_UNIT_BUILD_ARTILLERY, &bopts),
    "Artillery accepted with the Armory bit"
  );

  if (failures == failures_before) {
    printf("unit_colonies: wagon cap + Armory gate ok\n");
    return 0;
  }
  return 1;
}

/*
 * bugs.md #580 / #589 / #590 — the DOS work-assign validator
 * (thunk_FUN_1000_9808, overlays.c:60412-60498): faculty cap by the school
 * the colony OWNS, @NEEDCOLLEGE/@NEEDUNIVERSITY by owned building, and
 * @MORETHANTHREE only for occupation > 9.
 */
static int unit_school_faculty_and_occupation_cap(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "faculty: load NAMES.TXT failed\n");
    return 1;
  }
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  if (!colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "faculty: load @BUILDING failed\n");
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  int failures = 0;
  const int failures_before = failures;

  const int school = colonies_find_building(&pool, "Schoolhouse");
  const int college = colonies_find_building(&pool, "College");
  const int univ = colonies_find_building(&pool, "University");
  const int church = colonies_find_building(&pool, "Church");
  const int distillery = colonies_find_building(&pool, "Rum Distillery");
  CHECK(school >= 0 && college >= 0 && univ >= 0, "school rows exist");
  CHECK(church >= 0 && distillery >= 0, "Church / Rum Distillery rows exist");

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Cambridge");
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    col->tiles[i] = -1;
  }
  for (int i = 0; i < 6; ++i) {
    col->colonists[i].active = true;
    col->colonists[i].profession = COLONIZE_JOB_FARMER; /* @JOB school level 1 */
    col->colonists[i].field_job = -1;
    col->colonists[i].building_type = -1;
  }
  col->colonist_count = 6;
  col->population = 6;
  pool.colony_count = 1;

  /* #580 — Schoolhouse alone supports one teacher (@SCHOOL1). */
  col->has_building[school] = true;
  CHECK(colonies_school_owned_tier(&pool, col) == 1, "Schoolhouse only = owned tier 1");
  CHECK(colonies_assign_workplace(&pool, 1, 0, school), "first teacher fits a Schoolhouse");
  CHECK(!colonies_assign_workplace(&pool, 1, 1, school), "@SCHOOL1: second teacher refused");
  /* College (@COLLEGE2) raises the cap to two — DOS keeps the lower rows. */
  col->has_building[college] = true;
  CHECK(colonies_school_owned_tier(&pool, col) == 2, "College owned = tier 2");
  CHECK(colonies_assign_workplace(&pool, 1, 1, school), "second teacher fits with a College");
  CHECK(!colonies_assign_workplace(&pool, 1, 2, college), "@COLLEGE2: third teacher refused");
  col->has_building[univ] = true;
  CHECK(colonies_school_owned_tier(&pool, col) == 3, "University owned = tier 3");
  CHECK(colonies_assign_workplace(&pool, 1, 2, school), "third teacher fits with a University");
  CHECK(!colonies_assign_workplace(&pool, 1, 3, univ), "@UNIV3: fourth teacher refused");

  /* #589 — the requirement is the OWNED building, not the clicked row: an
   * Elder Statesman (level 3) teaches from the Schoolhouse row when the
   * colony owns a University. */
  CHECK(colonies_job_school_tier(COLONIZE_PROF_STATESMAN) == 3, "Statesman is @JOB level 3");
  /* #596: the tier comes from the loaded NAMES.TXT @JOB column 2, not a
   * compiled copy of it — @JOB row 8 (Fisherman) reads 1. */
  CHECK(colonies_job_school_tier(8) == 1, "#596: @JOB row 8 Fisherman is level 1");
  CHECK(colonies_job_school_tier(17) == 3, "#596: @JOB row 17 Statesman is level 3");
  CHECK(colonies_job_school_tier(19) == 4, "#596: @JOB row 19 Colonist is level 4");
  col->colonists[0].building_type = -1;
  col->colonists[1].building_type = -1;
  col->colonists[2].building_type = -1;
  col->colonists[3].profession = COLONIZE_PROF_STATESMAN;
  CHECK(
    colonies_assign_workplace(&pool, 1, 3, school),
    "#589: University owner accepts a level-3 teacher on the Schoolhouse row"
  );
  col->has_building[univ] = false;
  col->colonists[4].profession = COLONIZE_PROF_STATESMAN;
  CHECK(
    !colonies_assign_workplace(&pool, 1, 4, univ),
    "#589: without the University row owned, a level-3 teacher is refused"
  );
  col->colonists[3].building_type = -1;
  col->has_building[school] = false;
  col->has_building[college] = false;

  /* #590 — @MORETHANTHREE applies to occupation > 9 only. */
  col->has_building[church] = true;
  col->has_building[distillery] = true;
  CHECK(colonies_building_occupation(&pool, church) == 16, "Church occupation = Preacher 16");
  CHECK(colonies_building_occupation(&pool, distillery) == 9, "Rum chain occupation = 9");
  for (int i = 0; i < 6; ++i) {
    col->colonists[i].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[i].building_type = -1;
  }
  for (int i = 0; i < 3; ++i) {
    CHECK(colonies_assign_workplace(&pool, 1, i, church), "three preachers fit");
  }
  CHECK(!colonies_assign_workplace(&pool, 1, 3, church), "@MORETHANTHREE: fourth preacher refused");
  for (int i = 0; i < 6; ++i) {
    col->colonists[i].building_type = -1;
  }
  for (int i = 0; i < 4; ++i) {
    CHECK(
      colonies_assign_workplace(&pool, 1, i, distillery),
      "#590 DOS-LITERAL: @JOB 9 has no @MORETHANTHREE cap"
    );
  }

  if (failures == failures_before) {
    printf("unit_colonies: school faculty + occupation cap ok\n");
    return 0;
  }
  return 1;
}

/*
 * bugs.md #579 — FUN_15eb_23f2's blocked mask: a plot already worked by
 * ANOTHER colony carries bit 0x40 and cannot be seated.
 */
static int unit_plot_blocked_mask(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  int failures = 0;
  const int failures_before = failures;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 24, 24, err, sizeof(err))) {
    fprintf(stderr, "blockedmask: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int y = 0; y < 24; ++y) {
    for (int x = 0; x < 24; ++x) {
      const size_t i = (size_t)y * 24u + (size_t)x;
      map.terrain[i] = 2;    /* plains — not water */
      map.layer3[i] = 0xf1u; /* owner none, continent 1 */
      map.layer2[i] = 0;
    }
  }
  map.prime_resource_seed = 0; /* no procedural rumours */
  map_reveal_all(&map, 0);

  ColonizeColony* a = &pool.colonies[0];
  ColonizeColony* b = &pool.colonies[1];
  memset(a, 0, sizeof(*a));
  memset(b, 0, sizeof(*b));
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    a->tiles[i] = -1;
    b->tiles[i] = -1;
  }
  a->active = b->active = true;
  a->id = 1;
  b->id = 2;
  a->x = 10;
  a->y = 10;
  b->x = 12;
  b->y = 10;
  a->colonists[0].active = true;
  a->colonists[0].field_job = COLONIZE_JOB_FARMER;
  a->colonists[0].building_type = -1;
  a->colonist_count = a->population = 1;
  b->colonists[0].active = true;
  b->colonists[0].field_job = COLONIZE_JOB_FARMER;
  b->colonists[0].building_type = -1;
  b->colonist_count = b->population = 1;
  pool.colony_count = 2;

  const int a_east = colonies_field_tile_index(1, 0);  /* (11,10) */
  const int b_west = colonies_field_tile_index(-1, 0); /* (11,10) */
  const int b_east = colonies_field_tile_index(1, 0);  /* (13,10) */
  CHECK(a_east >= 0 && b_west >= 0, "field slots resolve");
  a->tiles[a_east] = 0; /* colony A works the shared plot */

  const ColonizeWorld w = world_make(NULL, &pool, &map, NULL, false, NULL, NULL);
  CHECK(
    (colonies_plot_blocked_mask(&w, b, b_west) & 0x40u) != 0,
    "#579: plot worked by another colony is blocked (0x40)"
  );
  CHECK(colonies_plot_blocked_mask(&w, b, b_east) == 0, "a free plot is unblocked");
  CHECK(colonies_plot_blocked_mask(&w, a, a_east) == 0, "the owner's own plot is unblocked");
  /* Another colony's CENTRE is 0x20 even when unworked. */
  /* #593: (2,0) IS a runtime slot now (9), but a tier-2 colony cannot work
   * it — FUN_137f_003c rejects it, so the mask is the early 0x10. */
  const int a_east2 = colonies_field_tile_index(2, 0);
  CHECK(a_east2 == 9, "#593: the 5x5 outer ring occupies runtime slots 8..19");
  CHECK(
    colonies_plot_blocked_mask(&w, a, a_east2) == 0x10u,
    "#593: an outer plot is outside a tier-2 colony's radius"
  );
  b->x = 11;
  CHECK(
    (colonies_plot_blocked_mask(&w, a, a_east) & 0x20u) != 0,
    "#579: another colony's centre is blocked (0x20)"
  );
  b->x = 12;

  /* DOS scan order (bugs.md #584): step 0..7 = N,E,S,W,NW,NE,SE,SW. */
  int sdx = 0;
  int sdy = 0;
  colonies_field_tile_delta(colonies_field_scan_order(1), &sdx, &sdy);
  CHECK(sdx == 1 && sdy == 0, "#584: DOS scan step 1 is East");
  colonies_field_tile_delta(colonies_field_scan_order(4), &sdx, &sdy);
  CHECK(sdx == -1 && sdy == -1, "#584: DOS scan step 4 is NW");

  map_free(&map);
  if (failures == failures_before) {
    printf("unit_colonies: plot blocked mask ok\n");
    return 0;
  }
  return 1;
}

/*
 * bugs.md #593 — the work-plot ring is per colony:
 * `DS:0x329[FUN_15eb_0470()]` over {0,4,8,12,20} with
 * `FUN_15eb_0470 = min(FUN_15eb_039e(10),2)+2` (raw 9636-9645 / 9561-9578),
 * i.e. +4 plots per owned @BUILDING row 0x0a / 0x0b. Stock DOS can build
 * neither row (FUN_15eb_3650 zeroes both), so a stock colony stays at 8.
 */
static int unit_work_plot_ring_593(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "ring593: load NAMES.TXT failed\n");
    return 1;
  }
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  if (!colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "ring593: load @BUILDING failed\n");
    assets_msg_free(&names);
    return 1;
  }
  int failures = 0;
  const int failures_before = failures;

  ColonizeColony* c = &pool.colonies[0];
  memset(c, 0, sizeof(*c));
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    c->tiles[i] = -1;
  }
  c->active = true;
  c->id = 1;
  c->x = 10;
  c->y = 10;
  c->nation_id = 0;

  CHECK(colonies_work_plot_count(&pool, c) == 8, "#593: no Town Hall upgrade -> ring 8");
  /* The ring's first 8 slots are unchanged: the port's clockwise MAP_DIR8. */
  int dx = 0;
  int dy = 0;
  colonies_field_tile_delta(0, &dx, &dy);
  CHECK(dx == 0 && dy == -1, "#593: slot 0 is still North");
  colonies_field_tile_delta(7, &dx, &dy);
  CHECK(dx == -1 && dy == -1, "#593: slot 7 is still NW");
  /* Slots 8..19 are the DOS outer ring in DS:0xc8/0xde order. */
  colonies_field_tile_delta(8, &dx, &dy);
  CHECK(dx == 0 && dy == -2, "#593: slot 8 is (0,-2)");
  colonies_field_tile_delta(19, &dx, &dy);
  CHECK(dx == 2 && dy == 1, "#593: slot 19 is (2,1)");
  CHECK(colonies_field_scan_order(8) == 8, "#593: DOS scan step 8 is slot 8");

  const int row10 = colonies_building_row(&pool, COLONY_BUILDING_TOWN_HALL_2);
  const int row11 = colonies_building_row(&pool, COLONY_BUILDING_TOWN_HALL_3);
  CHECK(row10 >= 0 && row11 >= 0, "#593: @BUILDING rows 10/11 are loaded");
  /* Neither row is offerable: DOS FUN_15eb_3650 (raw ~13674) hard-zeroes
   * both, and colonies_list_buildable carries the same block — so the ring
   * only ever grows for a save that already carries the bits. */
  {
    int ids[64];
    const int n = colonies_list_buildable(&pool, c->id, ids, 64, NULL);
    bool offered = false;
    for (int i = 0; i < n; ++i) {
      if (ids[i] == row10 || ids[i] == row11) {
        offered = true;
      }
    }
    CHECK(!offered, "#593: rows 10/11 are never offered by the build list");
  }
  c->has_building[row10] = true;
  CHECK(colonies_work_plot_count(&pool, c) == 12, "#593: row 10 -> ring 12");
  c->has_building[row11] = true;
  CHECK(colonies_work_plot_count(&pool, c) == 20, "#593: rows 10+11 -> ring 20");
  c->has_building[row10] = false;
  CHECK(colonies_work_plot_count(&pool, c) == 12, "#593: one upgrade -> ring 12");

  /* FUN_137f_003c radius: tier 3 takes |dx|+|dy| < 3, tier 4 everything in
   * the 5x5 but the corners. */
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 24, 24, err, sizeof(err))) {
    fprintf(stderr, "ring593: map_alloc: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int y = 0; y < 24; ++y) {
    for (int x = 0; x < 24; ++x) {
      const size_t i = (size_t)y * 24u + (size_t)x;
      map.terrain[i] = 2;
      map.layer3[i] = 0xf1u;
      map.layer2[i] = 0;
    }
  }
  map.prime_resource_seed = 0;
  map_reveal_all(&map, 0);
  const ColonizeWorld w = world_make(NULL, &pool, &map, NULL, false, NULL, NULL);
  const int slot_0_m2 = colonies_field_tile_index(0, -2);
  const int slot_corner = colonies_field_tile_index(2, 2);
  const int slot_2_1 = colonies_field_tile_index(2, 1);
  CHECK(slot_corner < 0, "#593: (2,2) is not a DOS plot at any tier");
  CHECK(
    colonies_plot_blocked_mask(&w, c, slot_0_m2) == 0,
    "#593: tier 3 works (0,-2)"
  );
  CHECK(
    colonies_plot_blocked_mask(&w, c, slot_2_1) == 0x10u,
    "#593: tier 3 does not reach (2,1)"
  );
  c->has_building[row10] = true; /* both upgrades -> tier 4 */
  CHECK(
    colonies_plot_blocked_mask(&w, c, slot_2_1) == 0,
    "#593: tier 4 works (2,1)"
  );

  /* An outer plot really produces: Expert Fisherman on slot 8 over ocean,
   * with Docks, shows up in the shared worked-tile walk the turn production
   * sum uses. */
  const size_t oi = (size_t)(c->y - 2) * 24u + (size_t)c->x;
  map.terrain[oi] = (uint8_t)T_OCEAN;
  const int docks = colonies_building_row(&pool, COLONY_BUILDING_DOCKS);
  c->has_building[docks] = true;
  c->colonist_count = 1;
  c->population = 1;
  c->colonists[0].active = true;
  c->colonists[0].building_type = -1;
  /* @JOB 8 = Expert Fisherman (the field job and the expert share the row). */
  c->colonists[0].profession = COLONIZE_JOB_FISHERMAN;
  CHECK(
    colonies_assign_field(&pool, c->id, 0, slot_0_m2, COLONIZE_JOB_FISHERMAN),
    "#593: a tier-4 colony can seat a colonist on slot 8"
  );
  int food = 0;
  ColonizeWorkedTileIter wit;
  ColonizeWorkedTile wt;
  colony_yield_worked_tiles_begin(&wit, c);
  while (colony_yield_worked_tiles_next(&wit, &wt)) {
    if (wt.colonist->field_job != COLONIZE_JOB_FISHERMAN) {
      continue;
    }
    food += colony_yield_for_worker(
      &map, wt.x, wt.y, wt.colonist->field_job, wt.colonist->profession, true, 0,
      c->colony_flags, false
    );
  }
  CHECK(food > 0, "#593: the outer-ring Fisherman produces food");

  /* Seating outside the ring is refused. */
  c->has_building[row10] = false;
  c->has_building[row11] = false;
  CHECK(
    !colonies_assign_field(&pool, c->id, 0, slot_0_m2, COLONIZE_JOB_FISHERMAN),
    "#593: a tier-2 colony cannot seat outside its ring"
  );

  map_free(&map);
  assets_msg_free(&names);
  if (failures == failures_before) {
    printf("unit_colonies: work plot ring ok\n");
    return 0;
  }
  return 1;
}

/*
 * bugs.md #681 / #682: the per-nation settlement count DOS keeps in the byte
 * `nation + 0x9298` (asm 0x22595, cap 0x26), and the non-consuming default
 * name peek that lets the @COLONY prompt run before the colony exists.
 */
static int unit_settlement_count_and_name_peek(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    return 1;
  }
  for (size_t i = 0; i < map.tile_count; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);

  int rc = 0;
  if (colonies_nation_settlement_count(&pool, 0) != 0) {
    fprintf(stderr, "settlement count: empty pool not 0\n");
    rc = 1;
  }
  const char* peek1 = colonies_peek_next_name(&pool, 0);
  const char* peek2 = colonies_peek_next_name(&pool, 0);
  if (!peek1 || !peek2 || strcmp(peek1, peek2) != 0) {
    fprintf(stderr, "name peek advanced the counter\n");
    rc = 1;
  }
  char first[64];
  snprintf(first, sizeof(first), "%s", peek1 ? peek1 : "");

  const int a = colonies_found(&pool, &map, 2, 2, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
  const int b = colonies_found(&pool, &map, 6, 6, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
  const int c = colonies_found(&pool, &map, 10, 10, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
  if (a < 0 || b < 0 || c < 0) {
    fprintf(stderr, "settlement count: found failed\n");
    rc = 1;
  }
  const ColonizeColony* ca = colonies_get(&pool, a);
  if (ca && first[0] && strcmp(ca->name, first) != 0) {
    fprintf(stderr, "first colony took '%s', peek promised '%s'\n", ca->name, first);
    rc = 1;
  }
  if (colonies_nation_settlement_count(&pool, 0) != 2 ||
      colonies_nation_settlement_count(&pool, 1) != 1 ||
      colonies_nation_settlement_count(&pool, 2) != 0) {
    fprintf(stderr, "settlement count: per-nation tally wrong\n");
    rc = 1;
  }
  map_free(&map);
  return rc;
}

static const TestCase k_cases[] = {
    {"unit_colonies_core", case_colonies_core},
    {"unit_found_chrome", unit_found_chrome},
    {"unit_full_chrome", unit_full_chrome},
    {"unit_alreadyhave_chrome", unit_alreadyhave_chrome},
    {"unit_noteacher_chrome", unit_noteacher_chrome},
    {"unit_more_than_three_chrome", unit_more_than_three_chrome},
    {"unit_needschool_chrome", unit_needschool_chrome},
    {"unit_capture_col1_effects", unit_capture_col1_effects},
    {"unit_hammers_purchased_buy", unit_hammers_purchased_buy},
    {"unit_warehouse_capitol_levels", unit_warehouse_capitol_levels},
    {"unit_build_complete_latch", unit_build_complete_latch},
    {"unit_craft_preview_clamps", unit_craft_preview_clamps},
    {"unit_foreign_colony_trade", unit_foreign_colony_trade},
    {"unit_ship_construction", unit_ship_construction},
    {"unit_wagon_cap_and_armory_gate", unit_wagon_cap_and_armory_gate},
    {"unit_school_faculty_and_occupation_cap", unit_school_faculty_and_occupation_cap},
    {"unit_plot_blocked_mask", unit_plot_blocked_mask},
    {"unit_work_plot_ring_593", unit_work_plot_ring_593},
    {"unit_settlement_count_and_name_peek", unit_settlement_count_and_name_peek},
};
TEST_MAIN(k_cases)
