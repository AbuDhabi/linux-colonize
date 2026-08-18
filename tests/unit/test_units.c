#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/col1_save.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/map.h"
#include "core/sound.h"
#include "core/ss.h"
#include "core/unit_chrome.h"
#include "core/units.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/* Helper: clear-forest grants lumber + @CLEARCUT (avoids main stack pressure). */
static int unit_clearcut_lumber(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "clearcut: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "clearcut: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  if (pioneer < 0) {
    fprintf(stderr, "clearcut: no Pioneers type\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeWorldMap map;
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "clearcut: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  const int fx = 3;
  const int fy = 3;
  map.terrain[fy * map.width + fx] = 10; /* mixed forest */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->x = 2;
  col->y = 3;
  col->warehouse_level = 0;
  col->stock[COLONIZE_CARGO_LUMBER] = 5;
  snprintf(col->name, sizeof(col->name), "Timber");
  colonies.colony_count = 1;

  const int pid = units_spawn(&pool, pioneer, fx, fy);
  ColonizeUnit* u = units_get(&pool, pid);
  if (!u || !units_is_pioneer(&pool, pid)) {
    fprintf(stderr, "clearcut: pioneer spawn failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  u->nation_id = 0;
  u->tools = 100;
  u->moves_left = 1;
  u->profession = UNITS_JOB_NONE;
  u->orders = UNITS_ORDER_NONE;
  u->turns_worked = 0;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "clearcut: GAME.TXT load failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);

  char msg[96];
  msg[0] = '\0';
  if (!units_pioneer_plow(
        &pool, pid, &map, msg, sizeof(msg), &colonies, &pops, &game_txt
      )) {
    fprintf(stderr, "clearcut: plow start failed (%s)\n", msg);
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  int guard = 0;
  while (u->orders == UNITS_ORDER_CLEAR_PLOW && guard++ < 16) {
    u->moves_left = 1;
    if (!units_pioneer_work_tick(
          &pool, pid, &map, msg, sizeof(msg), &colonies, &pops, &game_txt
        )) {
      fprintf(stderr, "clearcut: work tick failed (%s)\n", msg);
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }
  if (u->orders == UNITS_ORDER_CLEAR_PLOW) {
    fprintf(stderr, "clearcut: never completed clear\n");
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int pedia = map_pedia_terrain_index_at(&map, fx, fy);
  if (pedia >= 8 && pedia <= 23) {
    fprintf(stderr, "clearcut: forest still present pedia=%d\n", pedia);
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (col->stock[COLONIZE_CARGO_LUMBER] != 25) {
    fprintf(
      stderr,
      "clearcut: lumber want 25 got %d\n",
      col->stock[COLONIZE_CARGO_LUMBER]
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "lumber") == NULL &&
       strstr(pops.queue[0].body, "Lumber") == NULL &&
       strstr(pops.queue[0].body, "Timber") == NULL)) {
    fprintf(
      stderr,
      "clearcut: CLEARCUT popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  int found_deforest = 0;
  for (int i = 0; i < pops.queue_count; ++i) {
    if (strstr(pops.queue[i].body, "Deforestation") != NULL ||
        strstr(pops.queue[i].body, "deforestation") != NULL ||
        (strstr(pops.queue[i].body, "Timber") != NULL && i > 0)) {
      found_deforest = 1;
      break;
    }
  }
  if (!found_deforest) {
    fprintf(
      stderr,
      "clearcut: DEFOREST popup missing q=%d last='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[pops.queue_count - 1].body : ""
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  assets_msg_free(&game_txt);
  map_free(&map);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: CLEARCUT+DEFOREST chrome ok\n");
  return 0;
}

/* Helper: tools=20 road complete → Colonists + @USEDUPTOOLS. */
static int unit_useduptools(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "usedup: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "usedup: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  const int colonist = units_find_type(&pool, "Colonists");
  if (pioneer < 0 || colonist < 0) {
    fprintf(stderr, "usedup: missing types\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeWorldMap map;
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "usedup: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains — road terr_cost 1 → one tick */
  }
  const int x = 3;
  const int y = 3;
  map_tile_set_road(&map, x, y, false);

  const int pid = units_spawn(&pool, pioneer, x, y);
  ColonizeUnit* u = units_get(&pool, pid);
  if (!u || !units_is_pioneer(&pool, pid)) {
    fprintf(stderr, "usedup: pioneer spawn failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  u->nation_id = 0;
  u->tools = 20;
  u->moves_left = 1;
  u->profession = UNITS_JOB_NONE;
  u->orders = UNITS_ORDER_BUILD_ROAD;
  u->turns_worked = 0;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "usedup: GAME.TXT load failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);

  char msg[96];
  msg[0] = '\0';
  if (!units_pioneer_work_tick(
        &pool, pid, &map, msg, sizeof(msg), NULL, &pops, &game_txt
      )) {
    fprintf(stderr, "usedup: work tick failed (%s)\n", msg);
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (!map_tile_has_road(&map, x, y) || u->type_index != colonist || u->tools != 0) {
    fprintf(
      stderr,
      "usedup: want road+Colonists tools=0 got road=%d type=%d tools=%d (%s)\n",
      (int)map_tile_has_road(&map, x, y),
      u->type_index,
      u->tools,
      msg
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "tools") == NULL &&
       strstr(pops.queue[0].body, "colonist") == NULL &&
       strstr(pops.queue[0].body, "Colonist") == NULL)) {
    fprintf(
      stderr,
      "usedup: USEDUPTOOLS popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  assets_msg_free(&game_txt);
  map_free(&map);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: USEDUPTOOLS demotion + popup ok\n");
  return 0;
}

/* Helper: Drydock repair emits @REFIT. */
static int unit_refit_drydock(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "refit: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "refit: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int caravel = units_find_type(&pool, "Caravel");
  if (caravel < 0) {
    fprintf(stderr, "refit: no Caravel\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Drydock");
  colonies.building_type_count = 1;
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->x = 2;
  col->y = 2;
  col->has_building[0] = true;
  snprintf(col->name, sizeof(col->name), "Harbor");
  colonies.colony_count = 1;

  const int sid = units_spawn_allow_stack(&pool, caravel, 2, 2);
  ColonizeUnit* ship = units_get(&pool, sid);
  if (!ship) {
    fprintf(stderr, "refit: ship spawn failed\n");
    assets_msg_free(&names);
    return 1;
  }
  ship->nation_id = 0;
  ship->col1_unknown15 = 0x80u;
  ship->turns_worked = 99; /* past construction thresh */
  pool.types[caravel].defense = 4;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "refit: GAME.TXT load failed\n");
    assets_msg_free(&names);
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  char st[96];
  st[0] = '\0';
  const int repaired = units_tick_drydock_repair(
    &pool, &colonies, 0, 0, st, sizeof(st), &pops, &game_txt
  );
  ship = units_get(&pool, sid);
  if (repaired != 1 || !ship || (ship->col1_unknown15 & 0x80u) != 0) {
    fprintf(stderr, "refit: repair failed repaired=%d bit7=%02x\n", repaired,
            ship ? (unsigned)ship->col1_unknown15 : 0xffu);
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "repair") == NULL &&
       strstr(pops.queue[0].body, "Harbor") == NULL &&
       strstr(pops.queue[0].body, "Caravel") == NULL)) {
    fprintf(
      stderr,
      "refit: REFIT popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }

  assets_msg_free(&game_txt);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: REFIT Drydock popup ok\n");
  return 0;
}

/* Helper: full warehouse unload → @WAREHOUSEFULL. */
static int unit_warehouse_full(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "whfull: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "whfull: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int caravel = units_find_type(&pool, "Caravel");
  if (caravel < 0) {
    fprintf(stderr, "whfull: no Caravel\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->x = 1;
  col->y = 1;
  col->warehouse_level = 0; /* cap 100 */
  col->stock[COLONIZE_CARGO_LUMBER] = 100;
  snprintf(col->name, sizeof(col->name), "Packed");
  colonies.colony_count = 1;

  const int sid = units_spawn_allow_stack(&pool, caravel, 1, 1);
  ColonizeUnit* ship = units_get(&pool, sid);
  if (!ship) {
    fprintf(stderr, "whfull: ship spawn failed\n");
    assets_msg_free(&names);
    return 1;
  }
  ship->nation_id = 0;
  if (units_load_goods(&pool, sid, COLONIZE_CARGO_LUMBER, 20) <= 0) {
    fprintf(stderr, "whfull: load lumber failed\n");
    assets_msg_free(&names);
    return 1;
  }

  bool full = false;
  const int moved = colonies_transfer_from_unit(&colonies, 1, &pool, sid, 0, &full);
  if (moved != 0 || !full) {
    fprintf(stderr, "whfull: want moved=0 full=1 got moved=%d full=%d\n", moved, (int)full);
    assets_msg_free(&names);
    return 1;
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "whfull: GAME.TXT load failed\n");
    assets_msg_free(&names);
    return 1;
  }
  AiPopupState pops;
  ai_popup_init(&pops);
  colonies_emit_warehouse_full_chrome(
    &colonies, col, COLONIZE_CARGO_LUMBER, "Lumber", &pops, &game_txt
  );
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "warehouse") == NULL &&
       strstr(pops.queue[0].body, "Warehouse") == NULL &&
       strstr(pops.queue[0].body, "Packed") == NULL)) {
    fprintf(
      stderr,
      "whfull: WAREHOUSEFULL popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }
  if (strstr(pops.queue[0].body, "Lumber") == NULL &&
      strstr(pops.queue[0].body, "lumber") == NULL &&
      strstr(pops.queue[0].body, "100") == NULL) {
    fprintf(stderr, "whfull: popup missing cargo/cap '%s'\n", pops.queue[0].body);
    assets_msg_free(&game_txt);
    assets_msg_free(&names);
    return 1;
  }

  assets_msg_free(&game_txt);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: WAREHOUSEFULL popup ok\n");
  return 0;
}

/* Pioneer order-gate @ONLYPIO / @NOPLOW / @NOROAD. */
static int unit_pioneer_order_gates(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "ordgate: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "ordgate: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  const int colonist = units_find_type(&pool, "Colonists");
  if (pioneer < 0 || colonist < 0) {
    fprintf(stderr, "ordgate: missing types\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeWorldMap map;
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "ordgate: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "ordgate: GAME.TXT load failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  char msg[96];
  AiPopupState pops;

  /* 1) Non-pioneer → ONLYPIO */
  {
    const int cid = units_spawn(&pool, colonist, 2, 2);
    ColonizeUnit* cu = units_get(&pool, cid);
    if (!cu) {
      fprintf(stderr, "ordgate: colonist spawn failed\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    cu->nation_id = 0;
    cu->moves_left = 1;
    ai_popup_init(&pops);
    msg[0] = '\0';
    if (units_pioneer_plow(
          &pool, cid, &map, msg, sizeof(msg), NULL, &pops, &game_txt
        )) {
      fprintf(stderr, "ordgate: non-pioneer plow should fail\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (pops.queue_count < 1 || strstr(pops.queue[0].body, "pioneer") == NULL) {
      fprintf(
        stderr,
        "ordgate: ONLYPIO weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, cid);
  }

  /* 2) Pioneer on plowed → NOPLOW */
  {
    map_tile_set_plowed(&map, 3, 3, true);
    map_tile_set_road(&map, 3, 3, false);
    const int pid = units_spawn(&pool, pioneer, 3, 3);
    ColonizeUnit* pu = units_get(&pool, pid);
    if (!pu) {
      fprintf(stderr, "ordgate: pioneer spawn failed\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu->nation_id = 0;
    pu->tools = 100;
    pu->moves_left = 1;
    ai_popup_init(&pops);
    msg[0] = '\0';
    if (units_pioneer_plow(
          &pool, pid, &map, msg, sizeof(msg), NULL, &pops, &game_txt
        )) {
      fprintf(stderr, "ordgate: plowed plow should fail\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "plow") == NULL &&
         strstr(pops.queue[0].body, "plowed") == NULL)) {
      fprintf(
        stderr,
        "ordgate: NOPLOW weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, pid);
  }

  /* 3) Pioneer on road → NOROAD */
  {
    map_tile_set_plowed(&map, 4, 4, false);
    map_tile_set_road(&map, 4, 4, true);
    const int pid = units_spawn(&pool, pioneer, 4, 4);
    ColonizeUnit* pu = units_get(&pool, pid);
    if (!pu) {
      fprintf(stderr, "ordgate: pioneer2 spawn failed\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu->nation_id = 0;
    pu->tools = 100;
    pu->moves_left = 1;
    ai_popup_init(&pops);
    msg[0] = '\0';
    if (units_pioneer_road(&pool, pid, &map, msg, sizeof(msg), NULL, &pops, &game_txt)) {
      fprintf(stderr, "ordgate: existing road should fail\n");
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (pops.queue_count < 1 || strstr(pops.queue[0].body, "road") == NULL) {
      fprintf(
        stderr,
        "ordgate: NOROAD weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, pid);
  }

  assets_msg_free(&game_txt);
  map_free(&map);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: pioneer order-gate popups ok\n");
  return 0;
}

/*
 * units_display_name: real NAMES.TXT @UNIT base type names are plural
 * catalog labels ("Colonists", "Pioneers", "Soldiers", …), not display
 * strings. "Colonists" + no equipment/profession must resolve to the
 * canonical "Free Colonist" name — mirrors the existing "Pioneers"→
 * "Pioneer" / "Soldiers"→"Soldier" branches. Without this, every strstr(
 * units_display_name(...), "Free Colonist") gate across the codebase
 * (ai_contact teach-skill, ai_euro LABOR/founding-site checks, …) silently
 * never matches an ordinary base colonist in real gameplay.
 */
static int unit_display_name_free_colonist(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "display_name: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "display_name: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int colonist_ty = units_find_type(&pool, "Colonists");
  if (colonist_ty < 0) {
    fprintf(stderr, "display_name: no Colonists type\n");
    assets_msg_free(&names);
    return 1;
  }
  const int uid = units_spawn_allow_stack(&pool, colonist_ty, 1, 1);
  ColonizeUnit* u = units_get(&pool, uid);
  if (!u) {
    fprintf(stderr, "display_name: spawn failed\n");
    assets_msg_free(&names);
    return 1;
  }
  u->profession = UNITS_JOB_NONE;
  const char* name = units_display_name(&pool, u);
  if (!name || strcmp(name, "Free Colonist") != 0) {
    fprintf(stderr, "display_name: base Colonists got '%s' want 'Free Colonist'\n",
            name ? name : "(null)");
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: display_name Colonists->Free Colonist ok\n");
  return 0;
}

static int g_music_sting_play_calls = 0;
static int g_music_sting_last_id = -1;
static int g_music_sting_active_id = -1;

static void unit_music_sting_play_mock(int id) {
  g_music_sting_play_calls++;
  g_music_sting_last_id = id;
  g_music_sting_active_id = id; /* mirrors sound.c: playing sets the active id */
}

static int unit_music_sting_active_id_mock(void) {
  return g_music_sting_active_id;
}

/*
 * Combat engagement (units_resolve_land_combat_ff / _naval_combat_ff) should
 * push SOUND_MILITARY_BGM_ID through the units_set_combat_music_hooks play
 * hook once per "new" engagement, and skip the call when that id is already
 * active — mirrors DOS FUN_129f_0318's "cmp [0x9c],id; jz done" restart
 * skip (docs/assets.md; units.c units_combat_music_sting).
 */
static int unit_combat_music_sting(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Soldiers");
  pool.types[0].attack = 99; /* attacker always wins: loser stays put, not despawned */
  pool.types[0].defense = 99;
  pool.types[0].movement = 1;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Soldiers");
  pool.types[1].attack = 0;
  pool.types[1].defense = 0;
  pool.types[1].movement = 1;

  /* Each engagement spawns a fresh pair — combat may demote/despawn the
   * loser (units_clear_slot resets type_index), so reusing ids across
   * calls is not safe; only the hook wiring is under test here. */
  int aid = units_spawn_allow_stack(&pool, 0, 5, 5);
  int did = units_spawn_allow_stack(&pool, 1, 6, 5);
  ColonizeUnit* atk = units_get(&pool, aid);
  ColonizeUnit* def = units_get(&pool, did);
  if (!atk || !def) {
    fprintf(stderr, "combat_music_sting: spawn failed\n");
    return 1;
  }
  atk->nation_id = 0;
  def->nation_id = 1;

  /* No hooks set (default): must not crash. */
  ColonizeDosRng rng0;
  dos_rng_seed(&rng0, 1);
  units_resolve_land_combat_ff(&pool, aid, did, &rng0, NULL);

  g_music_sting_play_calls = 0;
  g_music_sting_last_id = -1;
  g_music_sting_active_id = -1;
  units_set_combat_music_hooks(unit_music_sting_play_mock, unit_music_sting_active_id_mock);

  aid = units_spawn_allow_stack(&pool, 0, 5, 6);
  did = units_spawn_allow_stack(&pool, 1, 6, 6);
  units_get(&pool, aid)->nation_id = 0;
  units_get(&pool, did)->nation_id = 1;
  ColonizeDosRng rng1;
  dos_rng_seed(&rng1, 2);
  units_resolve_land_combat_ff(&pool, aid, did, &rng1, NULL);
  if (g_music_sting_play_calls != 1 || g_music_sting_last_id != SOUND_MILITARY_BGM_ID) {
    fprintf(stderr, "combat_music_sting: first engage calls=%d id=%d want 1/0x%02x\n",
            g_music_sting_play_calls, g_music_sting_last_id, SOUND_MILITARY_BGM_ID);
    units_set_combat_music_hooks(NULL, NULL);
    return 1;
  }

  /* Military cue already active: a second engagement must not restart it. */
  aid = units_spawn_allow_stack(&pool, 0, 5, 7);
  did = units_spawn_allow_stack(&pool, 1, 6, 7);
  units_get(&pool, aid)->nation_id = 0;
  units_get(&pool, did)->nation_id = 1;
  ColonizeDosRng rng2;
  dos_rng_seed(&rng2, 3);
  units_resolve_land_combat_ff(&pool, aid, did, &rng2, NULL);
  if (g_music_sting_play_calls != 1) {
    fprintf(stderr, "combat_music_sting: repeat engage should skip restart, calls=%d\n",
            g_music_sting_play_calls);
    units_set_combat_music_hooks(NULL, NULL);
    return 1;
  }

  units_set_combat_music_hooks(NULL, NULL);
  fprintf(stderr, "unit_units: combat music sting ok\n");
  return 0;
}

int main(void) {
  diag_init(0, NULL);

  if (unit_clearcut_lumber() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_display_name_free_colonist() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_combat_music_sting() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_useduptools() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_refit_drydock() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_warehouse_full() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_pioneer_order_gates() != 0) {
    diag_shutdown();
    return 1;
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "failed to load NAMES.TXT\n");
    return 1;
  }

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }

  const int pioneer = units_find_type(&pool, "Pioneers");
  const int colonist = units_find_type(&pool, "Colonists");
  const int caravel = units_find_type(&pool, "Caravel");
  if (pioneer < 0 || colonist < 0 || caravel < 0) {
    fprintf(stderr, "missing expected unit types\n");
    assets_msg_free(&names);
    return 1;
  }
  if (pool.types[pioneer].domain != COLONIZE_UNIT_DOMAIN_LAND ||
      pool.types[caravel].domain != COLONIZE_UNIT_DOMAIN_SEA) {
    fprintf(stderr, "unexpected unit domain\n");
    assets_msg_free(&names);
    return 1;
  }

  ColonizeWorldMap map;
  char err[256];
  char mp_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "AMER2.MP", mp_path, sizeof(mp_path)) ||
      !map_load_mp(mp_path, &map, err, sizeof(err))) {
    fprintf(stderr, "map load failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }

  units_new_world_start(&pool, &map, 39, 10, 0, 0);
  if (pool.unit_count < 3 || pool.selected_id < 0) {
    fprintf(stderr, "expected starter ship+pioneer+soldier (count=%d)\n", pool.unit_count);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  ColonizeUnit* ship = units_get(&pool, pool.selected_id);
  if (!ship || !units_is_sea(&pool, ship->id)) {
    fprintf(stderr, "selected starter should be the ship\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (!map_tile_is_high_seas(&map, ship->x, ship->y)) {
    fprintf(stderr, "starter ship not on eastern high seas (%d,%d)\n", ship->x, ship->y);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  /* Western rim: tile to the west should not be high seas. */
  if (ship->x > 0 && map_tile_is_high_seas(&map, ship->x - 1, ship->y)) {
    fprintf(
      stderr,
      "starter ship not on western rim of eastern high seas (%d,%d)\n",
      ship->x,
      ship->y
    );
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (ship->cargo_count < 2) {
    fprintf(stderr, "starter ship expected Pioneer+Soldier cargo (got %d)\n", ship->cargo_count);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  /* Discoverer (diff 0) England: plain Pioneer + Veteran Soldier (COLONY00).
   * Hardy Pioneer is French-only, not all Discoverer nations. */
  {
    const ColonizeUnit* p0 = units_get_const(&pool, ship->cargo_ids[0]);
    const ColonizeUnit* p1 = units_get_const(&pool, ship->cargo_ids[1]);
    if (!p0 || !p1 || p0->profession != UNITS_JOB_NONE || p1->profession != UNITS_JOB_SOLDIER) {
      fprintf(
        stderr,
        "Discoverer England expected plain+Veteran professions (got %d,%d)\n",
        p0 ? p0->profession : -1,
        p1 ? p1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }
  /* Discoverer French: Hardy Pioneer + Veteran Soldier. */
  {
    ColonizeUnitPool fr;
    memset(&fr, 0, sizeof(fr));
    fr.type_count = pool.type_count;
    memcpy(fr.types, pool.types, sizeof(pool.types));
    const int fid = units_spawn_euro_starter_fleet(&fr, 1, 0, ship->x + 1, ship->y, 40, 10);
    ColonizeUnit* fs = units_get(&fr, fid);
    if (!fs || fs->cargo_count < 2) {
      fprintf(stderr, "French Discoverer fleet missing cargo\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const ColonizeUnit* fp0 = units_get_const(&fr, fs->cargo_ids[0]);
    const ColonizeUnit* fp1 = units_get_const(&fr, fs->cargo_ids[1]);
    if (!fp0 || !fp1 || fp0->profession != UNITS_JOB_PIONEER ||
        fp1->profession != UNITS_JOB_SOLDIER) {
      fprintf(
        stderr,
        "Discoverer French expected Hardy+Veteran (got %d,%d)\n",
        fp0 ? fp0->profession : -1,
        fp1 ? fp1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (fs->profession != 0) {
      fprintf(stderr, "starter ship profession want 0 got %d\n", fs->profession);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }
  /* Conquistador (diff 2) Dutch: plain Pioneer + plain Soldier. */
  {
    ColonizeUnitPool hard;
    memset(&hard, 0, sizeof(hard));
    hard.type_count = pool.type_count;
    memcpy(hard.types, pool.types, sizeof(pool.types));
    const int sid = units_spawn_euro_starter_fleet(&hard, 3, 2, ship->x, ship->y, 39, 10);
    ColonizeUnit* hs = units_get(&hard, sid);
    if (!hs || hs->cargo_count < 2) {
      fprintf(stderr, "Dutch Conquistador fleet missing cargo\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const ColonizeUnit* hp0 = units_get_const(&hard, hs->cargo_ids[0]);
    const ColonizeUnit* hp1 = units_get_const(&hard, hs->cargo_ids[1]);
    if (!hp0 || !hp1 || hp0->profession != UNITS_JOB_NONE || hp1->profession != UNITS_JOB_NONE) {
      fprintf(
        stderr,
        "Dutch Conquistador expected plain skills (got %d,%d)\n",
        hp0 ? hp0->profession : -1,
        hp1 ? hp1->profession : -1
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }
  int ship_id = ship->id;

  /* Separate land unit for domain / stacking tests (ship is offshore). */
  int land_x = 39;
  int land_y = 10;
  if (!map_tile_is_land(&map, land_x, land_y) || units_id_at(&pool, land_x, land_y) >= 0) {
    land_x = -1;
    for (int y = 0; y < map.height && land_x < 0; ++y) {
      for (int x = 0; x < map.width; ++x) {
        if (map_tile_is_land(&map, x, y) && units_id_at(&pool, x, y) < 0) {
          land_x = x;
          land_y = y;
          break;
        }
      }
    }
  }
  if (land_x < 0) {
    fprintf(stderr, "no free land tile for move tests\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int land_id = units_spawn(&pool, pioneer >= 0 ? pioneer : colonist, land_x, land_y);
  if (land_id < 0) {
    fprintf(stderr, "failed to spawn land test unit\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  ColonizeUnit* starter = units_get(&pool, land_id);
  if (!starter || !map_tile_is_land(&map, starter->x, starter->y)) {
    fprintf(stderr, "land test unit not on land\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  int ocean_x = -1;
  int ocean_y = -1;
  for (int y = 0; y < map.height && ocean_x < 0; ++y) {
    for (int x = 0; x < map.width; ++x) {
      if (map_tile_is_water(&map, x, y) && units_id_at(&pool, x, y) < 0) {
        ocean_x = x;
        ocean_y = y;
        break;
      }
    }
  }
  if (ocean_x < 0) {
    fprintf(stderr, "no free ocean tile found\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (units_try_move(&pool, starter->id, &map, ocean_x, ocean_y, NULL, NULL)) {
    fprintf(stderr, "land unit should not enter ocean\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (units_try_move(&pool, ship_id, &map, land_x, land_y, NULL, NULL)) {
    fprintf(stderr, "sea unit should not enter land\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  if (land_x + 1 < map.width &&
      units_try_move(&pool, starter->id, &map, land_x + 1, land_y, NULL, NULL)) {
    if (starter->x != land_x + 1) {
      fprintf(stderr, "move did not update position\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }

  /* Spawn still rejects occupied tiles; friendly stacks via try_move are OK. */
  {
    const int stack_id = units_spawn_allow_stack(&pool, colonist, starter->x, starter->y);
    if (stack_id < 0) {
      fprintf(stderr, "friendly stack spawn_allow_stack failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_can_enter(&pool, colonist, &map, starter->x, starter->y, stack_id, NULL)) {
      fprintf(stderr, "friendly stack should be enterable\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, stack_id);
  }

  if (units_spawn(&pool, colonist, starter->x, starter->y) >= 0) {
    fprintf(stderr, "units_spawn should still reject occupied tiles\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  int edge_x = 0;
  int edge_y = 0;
  if (!units_find_high_seas_tile(&pool, &map, 39, 10, &edge_x, &edge_y)) {
    fprintf(stderr, "no high-seas tile on map\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (!units_on_high_seas(&map, edge_x, edge_y) || !map_tile_is_high_seas(&map, edge_x, edge_y)) {
    fprintf(stderr, "high-seas helper returned non-high-seas tile (%d,%d)\n", edge_x, edge_y);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  const int before = pool.unit_count;
  const int cargo_n = ship->cargo_count;
  if (!units_despawn(&pool, ship_id) || pool.unit_count != before - 1 - cargo_n) {
    fprintf(stderr, "despawn failed (before=%d cargo=%d after=%d)\n", before, cargo_n, pool.unit_count);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (units_get(&pool, ship_id) != NULL) {
    fprintf(stderr, "despawned unit still resolvable\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  /* Respawn a caravel next to the pioneer and exercise boarding. */
  int unload_x = land_x;
  int unload_y = land_y;
  {
    int bx = -1;
    int by = -1;
    if (!units_find_water_tile(&pool, &map, starter->x, starter->y, -1, &bx, &by)) {
      fprintf(stderr, "no water for boarding test\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Prefer an adjacent water tile so board adjacency succeeds. */
    bool adjacent = false;
    for (int dy = -1; dy <= 1 && !adjacent; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int tx = starter->x + dx;
        const int ty = starter->y + dy;
        if (map_tile_is_water(&map, tx, ty) && units_id_at(&pool, tx, ty) < 0) {
          bx = tx;
          by = ty;
          adjacent = true;
          break;
        }
      }
    }
    if (!adjacent) {
      fprintf(stderr, "no adjacent water tile for boarding\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ship_id = units_spawn(&pool, caravel, bx, by);
    if (ship_id < 0) {
      fprintf(stderr, "respawn caravel failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int land_id = starter->id;
    const int land_tile_x = starter->x;
    const int land_tile_y = starter->y;
    if (!units_board(&pool, land_id, ship_id)) {
      fprintf(stderr, "board failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* boarded = units_get(&pool, land_id);
    ColonizeUnit* carrier = units_get(&pool, ship_id);
    if (!boarded || !carrier || boarded->aboard_ship_id != ship_id ||
        carrier->cargo_count != 1 || units_id_at(&pool, land_tile_x, land_tile_y) >= 0) {
      fprintf(stderr, "board state incorrect\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (boarded->orders != 1 || boarded->moves_left != 0) {
      fprintf(stderr, "board should set sentry and zero moves\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (units_try_move(&pool, land_id, &map, land_tile_x, land_tile_y, NULL, NULL)) {
      fprintf(stderr, "boarded unit should not move on map\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    int cargo_types[COLONIZE_UNIT_CARGO_MAX];
    int cargo_count = 0;
    int exported_type = -1;
    char exported_name[32];
    const int count_before_sail = pool.unit_count;
    int hold_types[COLONIZE_UNIT_CARGO_MAX];
    int hold_amts[COLONIZE_UNIT_CARGO_MAX];
    memset(hold_types, 0, sizeof(hold_types));
    memset(hold_amts, 0, sizeof(hold_amts));
    if (!units_despawn_ship_with_cargo(
          &pool,
          ship_id,
          &exported_type,
          exported_name,
          sizeof(exported_name),
          cargo_types,
          &cargo_count,
          COLONIZE_UNIT_CARGO_MAX,
          hold_types,
          hold_amts,
          COLONIZE_UNIT_CARGO_MAX
        )) {
      fprintf(stderr, "despawn_ship_with_cargo failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (exported_type != caravel || cargo_count != 1 || cargo_types[0] != pioneer ||
        pool.unit_count != count_before_sail - 2 || units_get(&pool, land_id) != NULL) {
      fprintf(
        stderr,
        "sail cargo export wrong (type=%d cargo=%d units=%d)\n",
        exported_type,
        cargo_count,
        pool.unit_count
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    int hx = 0;
    int hy = 0;
    if (!units_find_high_seas_tile(&pool, &map, 39, 10, &hx, &hy)) {
      fprintf(stderr, "no high seas for cargo respawn\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int returned = units_spawn_ship_with_cargo(
      &pool, caravel, hx, hy, cargo_types, cargo_count, hold_types, hold_amts
    );
    if (returned < 0) {
      fprintf(stderr, "spawn_ship_with_cargo failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* returned_ship = units_get(&pool, returned);
    if (!returned_ship || returned_ship->cargo_count != 1) {
      fprintf(stderr, "returned ship missing cargo\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int pax_id = returned_ship->cargo_ids[0];
    ColonizeUnit* pax = units_get(&pool, pax_id);
    if (!pax || pax->aboard_ship_id != returned || pax->type_index != pioneer ||
        units_is_on_map(pax)) {
      fprintf(stderr, "returned passenger state wrong\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /* Find adjacent land for unload. */
    int ux = -1;
    int uy = -1;
    for (int dy = -1; dy <= 1 && ux < 0; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int tx = returned_ship->x + dx;
        const int ty = returned_ship->y + dy;
        if (units_can_enter(&pool, pioneer, &map, tx, ty, -1, NULL)) {
          ux = tx;
          uy = ty;
          break;
        }
      }
    }
    if (ux < 0) {
      /* High-seas tile may not touch land — move ship toward land first. */
      int near_x = land_tile_x;
      int near_y = land_tile_y;
      if (!units_find_water_tile(&pool, &map, land_tile_x, land_tile_y, returned, &near_x, &near_y)) {
        fprintf(stderr, "cannot berth for unload\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      returned_ship->x = near_x;
      returned_ship->y = near_y;
      pax->x = near_x;
      pax->y = near_y;
      for (int dy = -1; dy <= 1 && ux < 0; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          const int tx = near_x + dx;
          const int ty = near_y + dy;
          if (units_can_enter(&pool, pioneer, &map, tx, ty, -1, NULL)) {
            ux = tx;
            uy = ty;
            break;
          }
        }
      }
    }
    if (ux < 0 || !units_unload(&pool, returned, &map, ux, uy, NULL)) {
      fprintf(stderr, "unload failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pax = units_get(&pool, pax_id);
    if (!pax || pax->aboard_ship_id >= 0 || pax->x != ux || pax->y != uy ||
        units_id_at(&pool, ux, uy) != pax_id) {
      fprintf(stderr, "unload state incorrect\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    unload_x = ux;
    unload_y = uy;
    ship_id = returned;
  }

  /* Landfall unload: cargo with moves onto adjacent land; ship stays put. */
  {
    int bx = -1;
    int by = -1;
    int lx = -1;
    int ly = -1;
    for (int y = 1; y < map.height - 1 && lx < 0; ++y) {
      for (int x = 1; x < map.width - 1 && lx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || units_id_at(&pool, x, y) >= 0) {
          continue;
        }
        for (int dy = -1; dy <= 1 && lx < 0; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
              continue;
            }
            const int wx = x + dx;
            const int wy = y + dy;
            if (map_tile_is_water(&map, wx, wy) && units_id_at(&pool, wx, wy) < 0) {
              lx = x;
              ly = y;
              bx = wx;
              by = wy;
              break;
            }
          }
        }
      }
    }
    if (lx < 0) {
      fprintf(stderr, "no land/water pair for landfall test\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int lf_ship = units_spawn(&pool, caravel, bx, by);
    const int lf_pax = units_spawn_allow_stack(&pool, pioneer, lx, ly);
    if (lf_ship < 0 || lf_pax < 0 || !units_board(&pool, lf_pax, lf_ship)) {
      fprintf(stderr, "landfall setup failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* lf_cargo = units_get(&pool, lf_pax);
    ColonizeUnit* lf_boat = units_get(&pool, lf_ship);
    if (!lf_cargo || !lf_boat) {
      fprintf(stderr, "landfall units missing\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    lf_cargo->moves_left = 3;
    lf_boat->moves_left = 4;
    if (units_try_move(&pool, lf_ship, &map, lx, ly, NULL, NULL)) {
      fprintf(stderr, "ship must not enter plain land via try_move\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (units_first_cargo_with_moves(&pool, lf_ship) != lf_pax) {
      fprintf(stderr, "first cargo with moves mismatch\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_unload_passenger(&pool, lf_ship, lf_pax, &map, lx, ly, NULL)) {
      fprintf(stderr, "landfall unload_passenger failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    lf_cargo = units_get(&pool, lf_pax);
    lf_boat = units_get(&pool, lf_ship);
    if (!lf_cargo || !lf_boat || lf_cargo->aboard_ship_id >= 0 || lf_cargo->x != lx ||
        lf_cargo->y != ly || lf_boat->x != bx || lf_boat->y != by) {
      fprintf(stderr, "landfall state wrong\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Shore step charges passenger MP (sentry 0 → allotment − cost). */
    if (lf_cargo->moves_left >= 3) {
      fprintf(
        stderr,
        "landfall must consume passenger MP got %d\n",
        lf_cargo->moves_left
      );
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    fprintf(stderr, "landfall unload MP ok (pax moves=%d)\n", lf_cargo->moves_left);
    units_despawn(&pool, lf_pax);
    units_despawn(&pool, lf_ship);

    /* Sentry cargo (moves_left 0) must still landfall — DOS spent==0. */
    {
      const int lf_ship2 = units_spawn(&pool, caravel, bx, by);
      const int lf_pax2 = units_spawn_allow_stack(&pool, pioneer, lx, ly);
      if (lf_ship2 < 0 || lf_pax2 < 0 || !units_board(&pool, lf_pax2, lf_ship2)) {
        fprintf(stderr, "landfall sentry setup failed\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      ColonizeUnit* sentry_pax = units_get(&pool, lf_pax2);
      if (!sentry_pax || sentry_pax->moves_left != 0) {
        fprintf(stderr, "landfall sentry should board with 0 MP\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (units_first_landfall_cargo(&pool, lf_ship2) != lf_pax2) {
        fprintf(stderr, "landfall sentry cargo not eligible\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (!units_unload_passenger(&pool, lf_ship2, lf_pax2, &map, lx, ly, NULL)) {
        fprintf(stderr, "landfall sentry unload failed\n");
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      sentry_pax = units_get(&pool, lf_pax2);
      const ColonizeUnitType* pt = units_type(&pool, pioneer);
      const int max_mp = pt && pt->movement > 0 ? pt->movement : 1;
      if (!sentry_pax || sentry_pax->aboard_ship_id >= 0 || sentry_pax->moves_left >= max_mp) {
        fprintf(
          stderr,
          "landfall sentry MP want < %d got %d\n",
          max_mp,
          sentry_pax ? sentry_pax->moves_left : -1
        );
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      fprintf(stderr, "landfall sentry cargo MP ok (moves=%d)\n", sentry_pax->moves_left);
      units_despawn(&pool, lf_pax2);
      units_despawn(&pool, lf_ship2);
    }
  }

  /* Colony dock: ship may enter own colony; disembark clears sentry. */
  {
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    if (!colonies_load_names(&colonies, "COLONIZE/COLONY.TXT") ||
        !colonies_load_buildings(&colonies, &names)) {
      fprintf(stderr, "colony catalogs failed for dock test\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int cx = -1;
    int cy = -1;
    int wx = -1;
    int wy = -1;
    for (int y = 1; y < map.height - 1 && cx < 0; ++y) {
      for (int x = 1; x < map.width - 1 && cx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || !map_tile_is_coastal(&map, x, y)) {
          continue;
        }
        if (units_id_at(&pool, x, y) >= 0) {
          continue;
        }
        for (int dy = -1; dy <= 1 && cx < 0; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
              continue;
            }
            const int tx = x + dx;
            const int ty = y + dy;
            if (map_tile_is_water(&map, tx, ty) && units_id_at(&pool, tx, ty) < 0) {
              cx = x;
              cy = y;
              wx = tx;
              wy = ty;
              break;
            }
          }
        }
      }
    }
    if (cx < 0) {
      fprintf(stderr, "no coastal tile for dock test\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int cid = colonies_found(&colonies, &map, cx, cy, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
    if (cid < 0) {
      fprintf(stderr, "colonies_found failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeColony* col = colonies_get_mut(&colonies, cid);
    if (col) {
      col->nation_id = 0;
    }
    const int dock_ship = units_spawn(&pool, caravel, wx, wy);
    if (dock_ship < 0) {
      fprintf(stderr, "dock ship spawn failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* dship = units_get(&pool, dock_ship);
    dship->nation_id = 0;
    const int dock_pax = units_spawn_allow_stack(&pool, pioneer, cx, cy);
    if (dock_pax < 0) {
      fprintf(stderr, "dock pax spawn failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* dpax = units_get(&pool, dock_pax);
    dpax->nation_id = 0;
    dpax->x = wx;
    dpax->y = wy;
    if (!units_board_stacked(&pool, dock_pax, dock_ship)) {
      fprintf(stderr, "dock board_stacked failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    dpax = units_get(&pool, dock_pax);
    if (!dpax || dpax->orders != 1) {
      fprintf(stderr, "board_stacked should set sentry\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_can_enter(&pool, caravel, &map, cx, cy, dock_ship, &colonies)) {
      fprintf(stderr, "ship should enter own colony tile\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_try_move(&pool, dock_ship, &map, cx, cy, &colonies, NULL)) {
      fprintf(stderr, "ship try_move onto colony failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int nd = units_disembark_all(&pool, dock_ship, cx, cy);
    dpax = units_get(&pool, dock_pax);
    if (nd != 1 || !dpax || dpax->aboard_ship_id >= 0 || dpax->orders != 0 ||
        dpax->x != cx || dpax->y != cy) {
      fprintf(stderr, "disembark_all state wrong (n=%d)\n", nd);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    int stack_ids[UNITS_TILE_STACK_MAX];
    /* Re-board for stack collect: ship + passenger. */
    dpax->x = cx;
    dpax->y = cy;
    if (!units_board_stacked(&pool, dock_pax, dock_ship)) {
      /* ship is on colony land; board_stacked does not need water adjacency */
      fprintf(stderr, "reboard for stack collect failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int sn = units_collect_tile_stack(&pool, cx, cy, 0, stack_ids, UNITS_TILE_STACK_MAX);
    if (sn < 2) {
      fprintf(stderr, "tile stack should list ship+cargo (got %d)\n", sn);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, dock_pax);
    units_despawn(&pool, dock_ship);
  }

  /* Phase 7: terrain MP costs, pioneer plow/road, yield bonuses. */
  {
    ColonizeWorldMap tmap;
    memset(&tmap, 0, sizeof(tmap));
    if (!map_alloc(&tmap, 8, 8, err, sizeof(err))) {
      fprintf(stderr, "phase7 map_alloc failed: %s\n", err);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int px = -1;
    int py = -1;
    for (int y = 0; y < 6 && px < 0; ++y) {
      for (int x = 0; x < 6; ++x) {
        if (units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
          px = x;
          py = y;
          break;
        }
      }
    }
    if (px < 0) {
      fprintf(stderr, "phase7: no free adjacent tiles\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int fx = px + 1;
    const int fy = py;
    tmap.terrain[py * tmap.width + px] = 2; /* plains */
    tmap.terrain[fy * tmap.width + fx] = 10; /* mixed forest */
    /* DOS terr_cost table (NAMES scale; Brave uses *3): class10=2, class9=1, class27=3. */
    if (map_move_cost_at(&tmap, fx, fy) != 2) {
      fprintf(stderr, "forest move cost expected 2 got %d\n", map_move_cost_at(&tmap, fx, fy));
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    {
      const uint8_t save = tmap.terrain[fy * tmap.width + fx];
      tmap.terrain[fy * tmap.width + fx] = 9; /* terr_cost[9]=1 */
      if (map_move_cost_at(&tmap, fx, fy) != 1) {
        fprintf(stderr, "class9 move cost expected 1 got %d\n", map_move_cost_at(&tmap, fx, fy));
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      tmap.terrain[fy * tmap.width + fx] = (uint8_t)(2 | 0xa0); /* mountain → class 27, cost 3 */
      if (map_dos_terr_class_at(&tmap, fx, fy) != 27 || map_move_cost_at(&tmap, fx, fy) != 3) {
        fprintf(
          stderr,
          "mountain class/cost expected 27/3 got %d/%d\n",
          map_dos_terr_class_at(&tmap, fx, fy),
          map_move_cost_at(&tmap, fx, fy)
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      tmap.terrain[fy * tmap.width + fx] = save;
    }
    const int pid = units_spawn(&pool, pioneer, px, py);
    ColonizeUnit* pu = units_get(&pool, pid);
    if (!pu || pu->tools != 100 || !units_is_pioneer(&pool, pid)) {
      fprintf(stderr, "phase7 pioneer spawn/tools failed\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu->moves_left = 1; /* pioneer max is 1 — full allotment */
    if (!units_try_move(&pool, pid, &tmap, fx, fy, NULL, NULL) || pu->moves_left != 0) {
      fprintf(
        stderr,
        "phase7 full-MP forest enter should succeed and exhaust (moves_left=%d)\n",
        pu->moves_left
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, pid);

    /* Partial MP without RNG: deny and do not charge. */
    const int pid_partial = units_spawn(&pool, pioneer, px, py);
    ColonizeUnit* pu_partial = units_get(&pool, pid_partial);
    if (!pu_partial) {
      fprintf(stderr, "phase7 partial pioneer spawn failed\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pool.types[pioneer].movement = 2;
    pu_partial->moves_left = 1;
    if (units_try_move(&pool, pid_partial, &tmap, fx, fy, NULL, NULL) ||
        pu_partial->moves_left != 1) {
      fprintf(
        stderr,
        "phase7 partial-MP without RNG should fail uncharged (moves=%d)\n",
        pu_partial->moves_left
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /* DOS FUN_465b: range(1,cost) <= remaining; cost always charged. */
    {
      ColonizeDosRng rng;
      dos_rng_seed(&rng, 1u); /* first range(1,2) → 1 → success */
      const int ox = pu_partial->x;
      const int oy = pu_partial->y;
      if (!units_try_move(&pool, pid_partial, &tmap, fx, fy, NULL, &rng) ||
          pu_partial->x != fx || pu_partial->y != fy || pu_partial->moves_left != 0) {
        fprintf(
          stderr,
          "phase7 RNG success should enter forest (pos=%d,%d moves=%d)\n",
          pu_partial->x,
          pu_partial->y,
          pu_partial->moves_left
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      /* Reset for fail case. */
      pu_partial->x = ox;
      pu_partial->y = oy;
      pu_partial->moves_left = 1;
      dos_rng_seed(&rng, 5006u); /* first range(1,2) → 2 → fail */
      if (units_try_move(&pool, pid_partial, &tmap, fx, fy, NULL, &rng) ||
          pu_partial->x != ox || pu_partial->y != oy || pu_partial->moves_left != 0) {
        fprintf(
          stderr,
          "phase7 RNG fail should stay and exhaust (pos=%d,%d moves=%d)\n",
          pu_partial->x,
          pu_partial->y,
          pu_partial->moves_left
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
    pool.types[pioneer].movement = 1;
    units_despawn(&pool, pid_partial);

    const int pid2 = units_spawn(&pool, pioneer, px, py);
    ColonizeUnit* pu2 = units_get(&pool, pid2);
    if (!pu2) {
      fprintf(stderr, "phase7 second pioneer spawn failed\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    map_tile_set_road(&tmap, fx, fy, true);
    map_tile_set_road(&tmap, px, py, true); /* DOS FA road-pair both tiles */
    if (map_move_cost_step(&tmap, px, py, fx, fy) != 1) {
      fprintf(
        stderr,
        "roaded forest step cost expected 1 got %d\n",
        map_move_cost_step(&tmap, px, py, fx, fy)
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (map_move_cost_at(&tmap, fx, fy) != 1) {
      fprintf(stderr, "roaded forest cost expected 1 got %d\n", map_move_cost_at(&tmap, fx, fy));
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu2->moves_left = 2;
    if (!units_try_move(&pool, pid2, &tmap, fx, fy, NULL, NULL) || pu2->moves_left != 1) {
      fprintf(stderr, "phase7 roaded forest move failed (moves_left=%d)\n", pu2->moves_left);
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, pid2);

    const int pid3 = units_spawn(&pool, pioneer, px, py);
    ColonizeUnit* pu3 = units_get(&pool, pid3);
    if (!pu3) {
      fprintf(stderr, "phase7 third pioneer spawn failed\n");
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    map_tile_set_road(&tmap, px, py, false);
    char pmsg[64];
    pu3->moves_left = 1;
    /* Plains road: terr_cost 1 → completes on first work-tick. */
    if (!units_pioneer_road(&pool, pid3, &tmap, pmsg, sizeof(pmsg), NULL, NULL, NULL) ||
        !map_tile_has_road(&tmap, px, py) || pu3->tools != 80 || pu3->moves_left != 0) {
      fprintf(
        stderr,
        "phase7 road failed tools=%d road=%d moves=%d (%s)\n",
        pu3->tools,
        (int)map_tile_has_road(&tmap, px, py),
        pu3->moves_left,
        pmsg
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /*
     * FUN_479b_0526 road completion: nearest same-nation colony gets a flat
     * +10 hammers_purchased. Separate pioneer/tile so it doesn't disturb
     * pu3's own state ahead of the plow test below.
     */
    {
      ColonizeColonyPool colonies_road;
      colonies_init(&colonies_road);
      ColonizeColony* c = &colonies_road.colonies[0];
      c->id = 0;
      c->active = true;
      c->nation_id = 0;
      c->x = px;
      c->y = py;
      c->hammers_purchased = 5;
      colonies_road.colony_count = 1;
      colonies_road.next_id = 1;
      const int rx = 7;
      const int ry = 7;
      tmap.terrain[ry * tmap.width + rx] = 2; /* plains, own tile away from px/py */
      map_tile_set_road(&tmap, rx, ry, false);
      const int pid4 = units_spawn(&pool, pioneer, rx, ry);
      ColonizeUnit* pu4 = units_get(&pool, pid4);
      if (!pu4) {
        fprintf(stderr, "phase7 fourth pioneer spawn failed\n");
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      pu4->nation_id = 0;
      pu4->moves_left = 1;
      pu4->tools = 100;
      if (!units_pioneer_road(&pool, pid4, &tmap, pmsg, sizeof(pmsg), &colonies_road, NULL, NULL) ||
          !map_tile_has_road(&tmap, rx, ry)) {
        fprintf(stderr, "phase7 road-reward setup failed (%s)\n", pmsg);
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (colonies_road.colonies[0].hammers_purchased != 15) {
        fprintf(
          stderr,
          "phase7 road completion hammers_purchased=%u want 15\n",
          (unsigned)colonies_road.colonies[0].hammers_purchased
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      units_despawn(&pool, pid4);
    }

    pu3->moves_left = 1;
    pu3->tools = 100;
    pu3->orders = UNITS_ORDER_NONE;
    pu3->turns_worked = 0;
    const int farm_base = colony_yield_for_tile(&tmap, px, py, COLONIZE_JOB_FARMER);
    /* Plains plow: terr_cost+2 = 3 turns for non-Hardy; drive ticks to completion. */
    if (!units_pioneer_plow(
          &pool, pid3, &tmap, pmsg, sizeof(pmsg), NULL, NULL, NULL
        )) {
      fprintf(stderr, "phase7 plow start failed (%s)\n", pmsg);
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    while (pu3->orders == UNITS_ORDER_CLEAR_PLOW) {
      if (!units_pioneer_work_tick(
            &pool, pid3, &tmap, pmsg, sizeof(pmsg), NULL, NULL, NULL
          )) {
        fprintf(stderr, "phase7 plow tick failed (%s)\n", pmsg);
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
    if (!map_tile_is_plowed(&tmap, px, py) || pu3->tools != 80) {
      fprintf(
        stderr,
        "phase7 plow failed plowed=%d tools=%d (%s)\n",
        (int)map_tile_is_plowed(&tmap, px, py),
        pu3->tools,
        pmsg
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int farm_plowed = colony_yield_for_tile(&tmap, px, py, COLONIZE_JOB_FARMER);
    if (farm_plowed != farm_base + 1) {
      fprintf(stderr, "phase7 plow yield expected %d got %d\n", farm_base + 1, farm_plowed);
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int lumber_base = colony_yield_for_tile(&tmap, fx, fy, COLONIZE_JOB_LUMBERJACK);
    map_tile_set_road(&tmap, fx, fy, false);
    const int lumber_clear = colony_yield_for_tile(&tmap, fx, fy, COLONIZE_JOB_LUMBERJACK);
    map_tile_set_road(&tmap, fx, fy, true);
    /* Lumberjack's road bonus is base 2 (fur/lumber bucket, same as
     * river's — player-confirmed 2026-08-15, Viceroy, see
     * colony_yield_road_bonus). This call goes through colony_yield_for_tile
     * (profession=-1, no worker context), so the matching-expert-or-
     * Lumberjack "big_unit" x2 doesn't apply here — that only fires for a
     * specific worker via colony_yield_for_worker (see
     * test_colony_yield.c's own expert/road checks for that path); this one
     * is the plain non-expert +2. 2026-08-18: this bucket had regressed
     * back to the old flat "ore/silver +1" grouping for Lumberjack (see
     * colony_yield_road_bonus's fix comment) — re-fixed, and this
     * assertion (which had drifted to a stale "+1" while its own comment
     * already described "+2 base", a leftover from even earlier road-bonus
     * history) now matches it. See docs/terrain_yields.md "Plow / road /
     * river stacking". */
    if (lumber_base != lumber_clear + 2) {
      fprintf(
        stderr,
        "phase7 road lumber yield expected %d got base=%d clear=%d\n",
        lumber_clear + 2,
        lumber_base,
        lumber_clear
      );
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Town commons: plains → food + cotton; forest → food + furs (not lumber). */
    {
      ColonizeTownCommonsYield tc;
      colony_yield_town_commons(&tmap, px, py, 0, 0, &tc);
      if (tc.food <= 0 || tc.secondary_cargo != COLONIZE_CARGO_COTTON) {
        fprintf(
          stderr,
          "town commons plains expected food+cotton got food=%d cargo=%d amt=%d\n",
          tc.food,
          tc.secondary_cargo,
          tc.secondary_amount
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      colony_yield_town_commons(&tmap, fx, fy, 0, 0, &tc);
      if (tc.food <= 0 || tc.secondary_cargo != COLONIZE_CARGO_FURS) {
        fprintf(
          stderr,
          "town commons forest expected food+furs got food=%d cargo=%d amt=%d\n",
          tc.food,
          tc.secondary_cargo,
          tc.secondary_amount
        );
        map_free(&tmap);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
    units_despawn(&pool, pid3);
    map_free(&tmap);
  }

  ColonizeSpriteSheet icons;
  char ss_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "ICONS.SS", ss_path, sizeof(ss_path)) ||
      !ss_load(ss_path, &icons, err, sizeof(err))) {
    fprintf(stderr, "ICONS load failed: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int icon = pool.types[pioneer].icon_sprite;
  const int ship_icon = pool.types[caravel].icon_sprite;
  /* NAMES Pioneers=102, Caravel=6 are 1-based; blit indices are 101 and 5. */
  if (icon != 101) {
    fprintf(stderr, "pioneer icon expected 101 got %d\n", icon);
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (ship_icon != 5) {
    fprintf(stderr, "caravel icon expected 5 got %d\n", ship_icon);
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (icon < 0 || icon >= icons.sprite_count || icons.sprites[icon].width <= 0) {
    fprintf(stderr, "pioneer icon %d invalid (sprites=%d)\n", icon, icons.sprite_count);
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  if (ship_icon < 0 || ship_icon >= icons.sprite_count || icons.sprites[ship_icon].width <= 0) {
    fprintf(stderr, "caravel icon %d invalid\n", ship_icon);
    ss_free(&icons);
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }

  /* Go-to pathfinding: next step, spend MP, keep order, resume after end_turn. */
  {
    int lx = -1;
    int ly = -1;
    for (int y = 20; y < (int)map.height - 20 && lx < 0; ++y) {
      for (int x = 20; x < (int)map.width - 20; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 3, y + 2) &&
            map_tile_is_land(&map, x + 1, y) &&
            units_can_enter(&pool, pioneer, &map, x, y, -1, NULL) &&
            units_can_enter(&pool, pioneer, &map, x + 1, y, -1, NULL) &&
            units_can_enter(&pool, pioneer, &map, x + 3, y + 2, -1, NULL) &&
            map_move_cost_at(&map, x + 1, y) <= 1) {
          lx = x;
          ly = y;
          break;
        }
      }
    }
    if (lx < 0) {
      fprintf(stderr, "goto test: no land path found\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int uid = units_spawn_allow_stack(&pool, pioneer, lx, ly);
    ColonizeUnit* walker = units_get(&pool, uid);
    if (!walker) {
      fprintf(stderr, "goto spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int gx = lx + 3;
    const int gy = ly + 2;
    if (!units_set_goto(&pool, uid, &map, gx, gy, NULL)) {
      fprintf(stderr, "units_set_goto failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (walker->orders != UNITS_ORDER_GOTO) {
      fprintf(stderr, "expected ORDER_GOTO got %d\n", walker->orders);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int nx = -1;
    int ny = -1;
    if (!units_next_goto_step(&pool, uid, &map, NULL, &nx, &ny)) {
      fprintf(stderr, "units_next_goto_step failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int before_mp = walker->moves_left;
    units_advance_goto(&pool, uid, &map, NULL, NULL);
    walker = units_get(&pool, uid);
    if (!walker || (walker->x == lx && walker->y == ly && walker->moves_left >= before_mp)) {
      fprintf(stderr, "advance_goto made no progress\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (walker->x == gx && walker->y == gy) {
      if (walker->orders != UNITS_ORDER_NONE) {
        fprintf(stderr, "arrival should clear orders\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    } else if (walker->orders != UNITS_ORDER_GOTO) {
      fprintf(stderr, "partial advance should keep GOTO\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    } else {
      units_end_turn(&pool);
      walker = units_get(&pool, uid);
      const int refreshed = walker ? walker->moves_left : 0;
      const int px = walker ? walker->x : -1;
      const int py = walker ? walker->y : -1;
      units_advance_all_goto(&pool, &map, NULL);
      walker = units_get(&pool, uid);
      if (!walker) {
        fprintf(stderr, "walker missing after resume\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (walker->moves_left >= refreshed && walker->x == px && walker->y == py &&
          !(walker->x == gx && walker->y == gy)) {
        fprintf(stderr, "resume after end_turn made no progress\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
  }

  /* Orders / allegiance chrome: corner table + @ORDERS letters + nation ink. */
  {
    unit_chrome_load_orders(&names);
    const int caravel_t = units_find_type(&pool, "Caravel");
    const int frigate_t = units_find_type(&pool, "Frigate");
    const int treasure_t = units_find_type(&pool, "Treasure");
    const int dragoon_t = units_find_type(&pool, "Dragoons");
    if (unit_chrome_corner_for_type(colonist, false) != UNIT_CHROME_CORNER_BOTTOM_RIGHT ||
        unit_chrome_corner_for_type(caravel_t, false) != UNIT_CHROME_CORNER_TOP_LEFT ||
        unit_chrome_corner_for_type(frigate_t, false) != UNIT_CHROME_CORNER_TOP_RIGHT ||
        unit_chrome_corner_for_type(treasure_t, false) != UNIT_CHROME_CORNER_TOP_CENTER ||
        unit_chrome_corner_for_type(dragoon_t, false) != UNIT_CHROME_CORNER_TOP_LEFT) {
      fprintf(stderr, "unit_chrome corner table mismatch\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (unit_chrome_order_letter(0, 0) != '-' || unit_chrome_order_letter(1, 0) != 'S' ||
        unit_chrome_order_letter(3, 0) != 'G') {
      fprintf(stderr, "unit_chrome order letters mismatch\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Natives always '-'; Sentry letter uses NAMES color-8; England fill is saturated 112. */
    if (unit_chrome_order_letter(1, 5) != '-' ||
        unit_chrome_letter_color(0, 1) != (uint8_t)(12 - 8) ||
        unit_chrome_letter_color(0, 0) != 0 ||
        unit_chrome_nation_color(0) != 112 || unit_chrome_nation_color(1) != 9) {
      fprintf(stderr, "unit_chrome letter/nation color mismatch\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Equipment remap: horses → Scout (top-left). */
    {
      const int id = units_spawn_allow_stack(&pool, colonist, 10, 10);
      ColonizeUnit* u = units_get(&pool, id);
      if (!u) {
        fprintf(stderr, "chrome remap spawn failed\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      u->horses = 50;
      const int dtype = units_display_type_index(&pool, id);
      if (unit_chrome_corner_for_type(dtype, false) != UNIT_CHROME_CORNER_TOP_LEFT) {
        fprintf(stderr, "horses should display as Scout top-left (dtype=%d)\n", dtype);
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      units_despawn(&pool, id);
    }
  }

  /* Fortify / sentry / disband orders. */
  {
    const int soldier = units_find_type(&pool, "Soldier");
    const int sid = units_spawn(&pool, soldier >= 0 ? soldier : pioneer, 12, 12);
    if (sid < 0) {
      fprintf(stderr, "orders spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* su = units_get(&pool, sid);
    su->nation_id = 0;
    su->moves_left = 3;
    if (!units_order_fortify(&pool, sid) || su->orders != UNITS_ORDER_FORTIFY ||
        su->moves_left != 0) {
      fprintf(stderr, "fortify order failed orders=%d mp=%d\n", su->orders, su->moves_left);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Overnight promotion (same as turn_refresh_moves_for_nation). */
    su->orders = UNITS_ORDER_FORTIFIED;
    su->moves_left = 0;
    if (su->orders != UNITS_ORDER_FORTIFIED || su->moves_left != 0) {
      fprintf(stderr, "fortify overnight failed orders=%d mp=%d\n", su->orders, su->moves_left);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_wake(&pool, sid) || su->orders != UNITS_ORDER_NONE || su->moves_left <= 0) {
      fprintf(stderr, "wake fortified failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_order_sentry(&pool, sid) || su->orders != UNITS_ORDER_SENTRY) {
      fprintf(stderr, "sentry order failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_disband(&pool, sid) || units_get_const(&pool, sid) != NULL) {
      fprintf(stderr, "disband failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }

  /* Dump overboard / anchor / trade route / pillage. */
  {
    const int caravel_t = units_find_type(&pool, "Caravel");
    int sx = -1, sy = -1;
    for (int y = 1; y < (int)map.height - 1 && sx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && sx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) && !map_tile_is_high_seas(&map, x, y)) {
          sx = x;
          sy = y;
        }
      }
    }
    if (caravel_t < 0 || sx < 0) {
      fprintf(stderr, "dump/anchor: no caravel or sea tile\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int ship = units_spawn(&pool, caravel_t, sx, sy);
    ColonizeUnit* sh = units_get(&pool, ship);
    if (!sh) {
      fprintf(stderr, "dump ship spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    sh->nation_id = 0;
    if (units_load_goods(&pool, ship, COLONIZE_CARGO_SUGAR, 40) != 40) {
      fprintf(stderr, "load sugar for dump failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int ctype = -1, amt = -1;
    if (units_dump_cargo_overboard(&pool, ship, &ctype, &amt) != 40 || ctype != COLONIZE_CARGO_SUGAR ||
        amt != 40 || units_first_goods_hold(&pool, ship) >= 0) {
      fprintf(stderr, "dump overboard failed ctype=%d amt=%d\n", ctype, amt);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_order_trade_route(&pool, ship) || sh->orders != UNITS_ORDER_TRADE_ROUTE ||
        sh->moves_left != 0) {
      fprintf(stderr, "trade route order failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!units_orders_follow_goto(sh->orders)) {
      fprintf(stderr, "trade route should follow goto for advance\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_clear_orders(&pool, ship);

    /* Anchor: need own colony adjacent/on tile — found a tiny colony next to ship. */
    ColonizeColonyPool cpool;
    colonies_init(&cpool);
    int cx = -1, cy = -1;
    for (int dy = -1; dy <= 1 && cx < 0; ++dy) {
      for (int dx = -1; dx <= 1 && cx < 0; ++dx) {
        const int tx = sx + dx;
        const int ty = sy + dy;
        if (map_coords_inset(&map, tx, ty) && map_tile_is_land(&map, tx, ty)) {
          cx = tx;
          cy = ty;
        }
      }
    }
    if (cx < 0) {
      fprintf(stderr, "anchor: no land near ship\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (colonies_found(&cpool, &map, cx, cy, 0, -1, UNITS_JOB_NONE, 0, 0, 0) < 0) {
      fprintf(stderr, "anchor colony found failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    sh = units_get(&pool, ship);
    if (sh) {
      sh->moves_left = 4;
    }
    if (!units_order_anchor(&pool, ship, &cpool) || !sh || sh->orders != UNITS_ORDER_FORTIFY) {
      fprintf(stderr, "anchor order failed orders=%d\n", sh ? sh->orders : -1);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, ship);

    /* Pillage improvements on land. */
    int px = -1, py = -1;
    for (int y = 1; y < (int)map.height - 1 && px < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && px < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && !map_tile_is_high_seas(&map, x, y)) {
          px = x;
          py = y;
        }
      }
    }
    const int soldier = units_find_type(&pool, "Soldiers");
    const int mil = units_spawn(&pool, soldier >= 0 ? soldier : pioneer, px, py);
    ColonizeUnit* mu = units_get(&pool, mil);
    if (!mu || px < 0) {
      fprintf(stderr, "pillage spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    mu->nation_id = 0;
    mu->moves_left = 1;
    map_tile_set_road(&map, px, py, true);
    char pmsg[64];
    if (!units_pillage(&pool, mil, &map, NULL, pmsg, sizeof(pmsg)) ||
        map_tile_has_road(&map, px, py)) {
      fprintf(stderr, "pillage road failed: %s\n", pmsg);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, mil);
  }

  /* Land combat T0: Soldier (atk2) vs Brave (def1) — attacker wins without RNG. */
  {
    const int soldier = units_find_type(&pool, "Soldiers");
    const int brave = units_find_type(&pool, "Braves");
    if (soldier < 0 || brave < 0) {
      fprintf(stderr, "missing Soldiers/Braves for combat test\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int ax = -1, ay = -1, dx = -1, dy = -1;
    for (int y = 1; y < (int)map.height - 1 && ax < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && ax < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y)) {
          ax = x;
          ay = y;
          dx = x + 1;
          dy = y;
        }
      }
    }
    if (ax < 0) {
      fprintf(stderr, "no adjacent land for combat\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int aid = units_spawn(&pool, soldier, ax, ay);
    const int did = units_spawn_allow_stack(&pool, brave, dx, dy);
    ColonizeUnit* a = units_get(&pool, aid);
    ColonizeUnit* d = units_get(&pool, did);
    if (!a || !d) {
      fprintf(stderr, "combat spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    a->nation_id = 0;
    a->moves_left = 3;
    d->nation_id = 4;
    d->moves_left = 1;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 1);
    if (!units_try_move(&pool, aid, &map, dx, dy, NULL, &rng)) {
      fprintf(stderr, "combat move failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (units_last_combat_outcome() != 1 || units_get_const(&pool, did) != NULL) {
      fprintf(stderr, "expected attacker win, outcome=%d\n", units_last_combat_outcome());
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    a = units_get(&pool, aid);
    if (!a || a->x != dx || a->y != dy) {
      fprintf(stderr, "attacker did not enter tile\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_despawn(&pool, aid);
  }

  /* units_follow_unit + advance one step (Brave escort API). */
  {
    int fx = -1;
    int fy = -1;
    for (int y = 1; y + 2 < map.height && fx < 0; ++y) {
      for (int x = 1; x + 2 < map.width && fx < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 2, y) &&
            units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 2, y) < 0) {
          fx = x;
          fy = y;
        }
      }
    }
    if (fx < 0) {
      fprintf(stderr, "follow setup: no free land pair\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int a = units_spawn_allow_stack(&pool, pioneer, fx, fy);
    const int b = units_spawn_allow_stack(&pool, pioneer, fx + 2, fy);
    if (a < 0 || b < 0) {
      fprintf(stderr, "follow setup spawn failed\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnit* ua = units_get(&pool, a);
    ColonizeUnit* ub = units_get(&pool, b);
    if (!ua || !ub) {
      fprintf(stderr, "follow unit lookup failed\n");
      return 1;
    }
    ua->moves_left = 4;
    if (!units_follow_unit(&pool, a, b)) {
      fprintf(stderr, "units_follow_unit failed\n");
      return 1;
    }
    if (ua->orders != UNITS_ORDER_FOLLOW || ua->follow_unit_id != b) {
      fprintf(stderr, "follow order not set\n");
      return 1;
    }
    const int ax0 = ua->x;
    (void)units_advance_follow_one_step(&pool, a, &map, NULL, NULL);
    ua = units_get(&pool, a);
    if (!ua || ua->orders != UNITS_ORDER_FOLLOW || ua->follow_unit_id != b) {
      fprintf(stderr, "follow not retained after step\n");
      return 1;
    }
    if (ua->x == ax0 && abs(ua->x - ub->x) > 1) {
      fprintf(stderr, "follow did not step toward target\n");
      return 1;
    }
    units_despawn(&pool, a);
    units_despawn(&pool, b);
  }

  /* Naval hold plunder on combat resolve (FUN_5fef_016c-shaped). */
  {
    int wx = -1;
    int wy = -1;
    int lx = -1;
    int ly = -1;
    for (int y = 0; y < map.height && wx < 0; ++y) {
      for (int x = 0; x < map.width; ++x) {
        if (map_tile_is_water(&map, x, y)) {
          if (wx < 0) {
            wx = x;
            wy = y;
          } else if (abs(x - wx) + abs(y - wy) == 1) {
            lx = x;
            ly = y;
            break;
          }
        }
      }
    }
    const int privateer = units_find_type(&pool, "Privateer");
    const int merchant = units_find_type(&pool, "Merchantman");
    const int sty = privateer >= 0 ? privateer : caravel;
    const int lty = merchant >= 0 ? merchant : caravel;
    if (wx >= 0 && lx >= 0 && sty >= 0 && lty >= 0) {
      const int wid = units_spawn_allow_stack(&pool, sty, wx, wy);
      const int lid = units_spawn_allow_stack(&pool, lty, lx, ly);
      if (wid >= 0 && lid >= 0) {
        ColonizeUnit* w = units_get(&pool, wid);
        ColonizeUnit* l = units_get(&pool, lid);
        if (w && l) {
          w->nation_id = 0;
          l->nation_id = 1;
            (void)units_load_goods(&pool, lid, COLONIZE_CARGO_SUGAR, 40);
            const bool won = units_resolve_naval_combat(&pool, wid, lid, NULL);
          w = units_get(&pool, wid);
          l = units_get(&pool, lid);
          if (won) {
            if (!w || !w->active || (l && l->active)) {
              fprintf(stderr, "naval winner/loser active state wrong\n");
              return 1;
            }
            int sugar = 0;
            for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
              if (w->hold_goods_amount[h] > 0 && w->hold_goods_type[h] == COLONIZE_CARGO_SUGAR) {
                sugar += w->hold_goods_amount[h];
              }
            }
            if (sugar < 40) {
              fprintf(stderr, "naval plunder expected sugar>=40 got %d\n", sugar);
              return 1;
            }
          }
          if (w && w->active) {
            units_despawn(&pool, wid);
          }
          if (l && l->active) {
            units_despawn(&pool, lid);
          }
        }
      }
    }
  }

  /* Treasure train spawn: NAMES "Treasure" + COL1 LE16 gold in hold[0..1]. */
  {
    const int tid = units_spawn_treasure_train(&pool, 3, 3, 2, 0x1234);
    if (tid < 0) {
      fprintf(stderr, "spawn_treasure_train failed\n");
      return 1;
    }
    const ColonizeUnit* tr = units_get_const(&pool, tid);
    const ColonizeUnitType* tt = tr ? units_type(&pool, tr->type_index) : NULL;
    if (!tr || !tr->active || !tt || strcmp(tt->name, "Treasure") != 0) {
      fprintf(stderr, "spawn_treasure_train type/active mismatch\n");
      return 1;
    }
    if (tr->nation_id != 2 || tr->hold_goods_amount[0] != 0x34 ||
        tr->hold_goods_amount[1] != 0x12) {
      fprintf(
        stderr,
        "spawn_treasure_train nation/gold LE16 got nation=%d lo=%d hi=%d\n",
        tr->nation_id,
        tr->hold_goods_amount[0],
        tr->hold_goods_amount[1]
      );
      return 1;
    }
    if (units_spawn_treasure_train(&pool, 4, 4, 0, -1) >= 0) {
      fprintf(stderr, "spawn_treasure_train must reject negative gold\n");
      return 1;
    }
    units_despawn(&pool, tid);
  }

  /* Native settlement conquer: tribe remove + Cortes peels FUN_5fef_31ea gold. */
  {
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.tribe_count = 1;
    col1.head.difficulty = 0;
    col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
    if (!col1.tribe) {
      fprintf(stderr, "tribe alloc failed\n");
      return 1;
    }
    col1.tribe[0].x = 10;
    col1.tribe[0].y = 10;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE; /* no convert-join RNG before Cortes */
    col1.head.founding_father[FF_HERNAN_CORTES] = 0;
    col1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] |=
      (uint8_t)(1u << (FF_HERNAN_CORTES % 8));

    ColonizeWorldMap tmap;
    memset(&tmap, 0, sizeof(tmap));
    tmap.width = 20;
    tmap.height = 20;
    tmap.layer3 = calloc(400, 1);
    if (!tmap.layer3) {
      free(col1.tribe);
      fprintf(stderr, "tmap layer3 alloc failed\n");
      return 1;
    }
    tmap.layer3[10 * 20 + 10] = (uint8_t)((4u << 4) | 1u);

    const int brave_ti = units_find_type(&pool, "Braves");
    const int soldier_ti = units_find_type(&pool, "Soldiers");
    if (brave_ti < 0 || soldier_ti < 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "Brave/Soldier type missing for conquer smoke\n");
      return 1;
    }

    const int bid = units_spawn_allow_stack(&pool, brave_ti, 10, 10);
    const int sid = units_spawn_allow_stack(&pool, soldier_ti, 10, 10);
    ColonizeUnit* brave = units_get(&pool, bid);
    ColonizeUnit* soldier = units_get(&pool, sid);
    if (!brave || !soldier) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer spawn failed\n");
      return 1;
    }
    brave->nation_id = 4;
    soldier->nation_id = 0;
    pool.types[soldier_ti].attack = 99;
    pool.types[brave_ti].defense = 1;

    ColonizeDosRng crng;
    dos_rng_seed(&crng, 42);
    units_set_native_fallout_context(&col1, &tmap, -1);
    if (col1.nation[0].villages_burned != 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: villages_burned should start 0\n");
      return 1;
    }
    if (!units_resolve_land_combat_ff(&pool, sid, bid, &crng, &col1)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer combat: attacker should win\n");
      return 1;
    }
    /* Killing a map Brave does not destroy the dwelling (FUN_5fef_1b0e). */
    if (col1.head.tribe_count != 1) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: tribe should remain after map Brave death\n");
      return 1;
    }
    if (!units_try_native_settlement_fallout(
          &col1, &pool, &tmap, 0, 4, 10, 10, -1, &crng
        )) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: fallout should destroy empty dwelling\n");
      return 1;
    }
    if (col1.head.tribe_count != 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: tribe should be removed (count=%u)\n", col1.head.tribe_count);
      return 1;
    }
    /* col1_save.h nation.villages_burned; reports.c villages_penalty. */
    if (col1.nation[0].villages_burned != 1) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(
        stderr,
        "conquer: villages_burned should be 1 got %u\n",
        col1.nation[0].villages_burned
      );
      return 1;
    }
    if ((tmap.layer3[10 * 20 + 10] >> 4) != 0x0f) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer: village tile should be unowned 0xf\n");
      return 1;
    }
    int peel_gold = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->aboard_ship_id >= 0) {
        continue;
      }
      if (u->x == 10 && u->y == 10) {
        const ColonizeUnitType* tt = units_type(&pool, u->type_index);
        if (tt && strcmp(tt->name, "Treasure") == 0) {
          peel_gold = u->hold_goods_amount[0] | (u->hold_goods_amount[1] << 8);
          break;
        }
      }
    }
    /* Diff 0 + Cortes + non-Spanish: amount 2..4 then ×1.5 → gold 300/400/600. */
    if (peel_gold != 300 && peel_gold != 400 && peel_gold != 600) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "Cortes peel treasure gold got %d want 300|400|600\n", peel_gold);
      return 1;
    }

    /* rich_capital (-0xcc) ← tribe.state.capital doubles amount before Cortes boost. */
    {
      ColonizeDosRng r0;
      ColonizeDosRng r1;
      dos_rng_seed(&r0, 99);
      dos_rng_seed(&r1, 99);
      const int plain = units_cortes_conquest_treasure_gold(&col1, 0, &r0, 0);
      const int rich = units_cortes_conquest_treasure_gold(&col1, 0, &r1, 1);
      if (plain <= 0 || rich <= plain) {
        free(tmap.layer3);
        free(col1.tribe);
        fprintf(stderr, "rich_capital peel plain=%d rich=%d (want rich>plain>0)\n", plain, rich);
        return 1;
      }
      fprintf(stderr, "unit_units: Cortes rich_capital plain=%d rich=%d ok\n", plain, rich);
    }

    /* Known gold path: Cortes spawns treasure when caller supplies amount. */
    col1.head.tribe_count = 1;
    col1.tribe[0].x = 11;
    col1.tribe[0].y = 10;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    tmap.layer3[10 * 20 + 11] = (uint8_t)((4u << 4) | 1u);
    const int bid2 = units_spawn_allow_stack(&pool, brave_ti, 11, 10);
    const int sid2 = units_spawn_allow_stack(&pool, soldier_ti, 11, 10);
    brave = units_get(&pool, bid2);
    soldier = units_get(&pool, sid2);
    brave->nation_id = 4;
    soldier->nation_id = 0;
    units_set_native_fallout_context(&col1, &tmap, 500);
    if (!units_resolve_land_combat_ff(&pool, sid2, bid2, NULL, &col1)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer combat2 failed\n");
      return 1;
    }
    if (!units_try_native_settlement_fallout(
          &col1, &pool, &tmap, 0, 4, 11, 10, 500, NULL
        )) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "conquer fallout2 failed\n");
      return 1;
    }
    int treasure_id = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->x != 11 || u->y != 10) {
        continue;
      }
      const ColonizeUnitType* tt = units_type(&pool, u->type_index);
      if (tt && strcmp(tt->name, "Treasure") == 0) {
        treasure_id = u->id;
        if (u->hold_goods_amount[0] != (500 & 0xff) || u->hold_goods_amount[1] != ((500 >> 8) & 0xff)) {
          free(tmap.layer3);
          free(col1.tribe);
          fprintf(stderr, "Cortes treasure LE16 mismatch\n");
          return 1;
        }
        break;
      }
    }
    if (treasure_id < 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "Cortes conquest expected treasure when gold supplied\n");
      return 1;
    }
    if (col1.nation[0].villages_burned != 2) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(
        stderr,
        "conquer2: villages_burned should be 2 got %u\n",
        col1.nation[0].villages_burned
      );
      return 1;
    }
    units_despawn(&pool, treasure_id);
    units_set_native_fallout_context(NULL, NULL, -1);
    free(tmap.layer3);
    free(col1.tribe);
  }

  /* FUN_5fef_31ea convert-join: mission-owned tribe + Sepulveda/Spanish/Jesuit. */
  {
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.tribe_count = 1;
    col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
    if (!col1.tribe) {
      fprintf(stderr, "convert-join tribe alloc failed\n");
      return 1;
    }
    col1.tribe[0].x = 12;
    col1.tribe[0].y = 12;
    col1.tribe[0].nation_id = 5;
    /* Spanish (2) + Jesuit bit + Sepulveda → threshold 8+4+4=16 → always join. */
    col1.tribe[0].mission =
      (uint8_t)(2u | COL1_TRIBE_MISSION_JESUIT_BIT);
    col1.head.founding_father[FF_JUAN_DE_SEPULVEDA] = 2;
    col1.nation[2].founding_fathers[FF_JUAN_DE_SEPULVEDA / 8] |=
      (uint8_t)(1u << (FF_JUAN_DE_SEPULVEDA % 8));

    ColonizeWorldMap tmap;
    memset(&tmap, 0, sizeof(tmap));
    tmap.width = 20;
    tmap.height = 20;
    tmap.layer3 = calloc(400, 1);
    if (!tmap.layer3) {
      free(col1.tribe);
      fprintf(stderr, "convert-join tmap alloc failed\n");
      return 1;
    }
    tmap.layer3[12 * 20 + 12] = (uint8_t)((5u << 4) | 1u);

    const int brave_ti = units_find_type(&pool, "Braves");
    const int soldier_ti = units_find_type(&pool, "Soldiers");
    const int colonist_ti = units_find_type(&pool, "Colonists");
    if (brave_ti < 0 || soldier_ti < 0 || colonist_ti < 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join types missing\n");
      return 1;
    }

    const int bid = units_spawn_allow_stack(&pool, brave_ti, 12, 12);
    const int sid = units_spawn_allow_stack(&pool, soldier_ti, 12, 12);
    ColonizeUnit* brave = units_get(&pool, bid);
    ColonizeUnit* soldier = units_get(&pool, sid);
    if (!brave || !soldier) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join spawn failed\n");
      return 1;
    }
    brave->nation_id = 5;
    soldier->nation_id = 2;
    pool.types[soldier_ti].attack = 99;
    pool.types[brave_ti].defense = 1;

    ColonizeDosRng crng;
    dos_rng_seed(&crng, 1);
    units_set_native_fallout_context(&col1, &tmap, -1);
    if (!units_resolve_land_combat_ff(&pool, sid, bid, &crng, &col1)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join combat: attacker should win\n");
      return 1;
    }
    if (!units_try_native_settlement_fallout(
          &col1, &pool, &tmap, 2, 5, 12, 12, -1, &crng
        )) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join fallout failed\n");
      return 1;
    }
    int convert_id = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->aboard_ship_id >= 0) {
        continue;
      }
      if (u->x == 12 && u->y == 12 && u->nation_id == 2 && u->profession == 27) {
        convert_id = u->id;
        break;
      }
    }
    if (convert_id < 0) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "Sepulveda convert-join: expected Convert profession 27 on tile\n");
      return 1;
    }
    units_despawn(&pool, convert_id);

    /* No mission → no convert even with Sepulveda. */
    col1.head.tribe_count = 1;
    col1.tribe[0].x = 13;
    col1.tribe[0].y = 12;
    col1.tribe[0].nation_id = 5;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    tmap.layer3[12 * 20 + 13] = (uint8_t)((5u << 4) | 1u);
    const int bid2 = units_spawn_allow_stack(&pool, brave_ti, 13, 12);
    const int sid2 = units_spawn_allow_stack(&pool, soldier_ti, 13, 12);
    brave = units_get(&pool, bid2);
    soldier = units_get(&pool, sid2);
    brave->nation_id = 5;
    soldier->nation_id = 2;
    dos_rng_seed(&crng, 1);
    if (!units_resolve_land_combat_ff(&pool, sid2, bid2, &crng, &col1)) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "convert-join no-mission combat failed\n");
      return 1;
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->aboard_ship_id >= 0) {
        continue;
      }
      if (u->x == 13 && u->y == 12 && u->nation_id == 2 && u->profession == 27) {
        free(tmap.layer3);
        free(col1.tribe);
        fprintf(stderr, "convert-join must not spawn without mission\n");
        return 1;
      }
    }

    units_set_native_fallout_context(NULL, NULL, -1);
    free(tmap.layer3);
    free(col1.tribe);
    fprintf(stderr, "unit_units: Sepulveda convert-join ok\n");
  }

  /* FUN_3844_0004: Treasure outside colony despawns after >8 ticks. */
  {
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    ColonizeColony* home = &colonies.colonies[0];
    home->id = 0;
    home->active = true;
    home->nation_id = 0;
    home->x = 2;
    home->y = 2;
    colonies.colony_count = 1;
    int tid = units_spawn_treasure_train(&pool, 7, 7, 0, 100);
    if (tid < 0) {
      fprintf(stderr, "treasure tick spawn failed\n");
      return 1;
    }
    ColonizeUnit* tr = units_get(&pool, tid);
    tr->turns_worked = 0;
    for (int t = 0; t < 8; ++t) {
      if (units_tick_treasure_outside_colony(&pool, &colonies, 0, NULL, 0) != 0) {
        fprintf(stderr, "treasure should survive tick %d\n", t + 1);
        return 1;
      }
      tr = units_get(&pool, tid);
      if (!tr || !tr->active || tr->turns_worked != t + 1) {
        fprintf(stderr, "treasure counter want %d\n", t + 1);
        return 1;
      }
    }
    if (units_tick_treasure_outside_colony(&pool, &colonies, 0, NULL, 0) < 1) {
      fprintf(stderr, "treasure should despawn on tick 9\n");
      return 1;
    }
    tr = units_get(&pool, tid);
    if (tr && tr->active) {
      fprintf(stderr, "treasure still active after tick 9\n");
      return 1;
    }
    /* On own colony: counter resets; never despawns. */
    tid = units_spawn_treasure_train(&pool, 2, 2, 0, 50);
    tr = units_get(&pool, tid);
    tr->turns_worked = 7;
    if (units_tick_treasure_outside_colony(&pool, &colonies, 0, NULL, 0) != 0) {
      fprintf(stderr, "treasure on colony should not despawn\n");
      return 1;
    }
    tr = units_get(&pool, tid);
    if (!tr || tr->turns_worked != 0) {
      fprintf(stderr, "treasure on colony should reset counter\n");
      return 1;
    }
    units_despawn(&pool, tid);
    fprintf(stderr, "unit_units: treasure outside-colony 8-turn tick ok\n");
  }

  /* Stockade/Fort/Fortress defense bonus in land combat + Treasure capture loot. */
  {
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
    snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
    snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Fortress");
    colonies.building_type_count = 3;
    ColonizeColony* col = &colonies.colonies[0];
    col->id = 0;
    col->active = true;
    col->nation_id = 0;
    col->x = 5;
    col->y = 5;
    col->population = 3;
    colonies.colony_count = 1;
    if (colonies_fortification_defense_bonus_percent(&colonies, col) != 0) {
      fprintf(stderr, "fort bonus should be 0 without buildings\n");
      return 1;
    }
    col->has_building[0] = true;
    if (colonies_fortification_defense_bonus_percent(&colonies, col) != 100) {
      fprintf(stderr, "Stockade bonus want 100\n");
      return 1;
    }
    col->has_building[1] = true;
    if (colonies_fortification_defense_bonus_percent(&colonies, col) != 150) {
      fprintf(stderr, "Fort bonus want 150\n");
      return 1;
    }
    col->has_building[2] = true;
    if (colonies_fortification_defense_bonus_percent(&colonies, col) != 200) {
      fprintf(stderr, "Fortress bonus want 200\n");
      return 1;
    }

    const int soldier_ti = units_find_type(&pool, "Soldiers");
    if (soldier_ti < 0) {
      fprintf(stderr, "Soldiers missing for fort-defense smoke\n");
      return 1;
    }
    /* Deterministic: attack 2→×8=16 →×3/2=24 vs Stockade def base2→16 ×2=32 → loses. */
    pool.types[soldier_ti].attack = 2;
    pool.types[soldier_ti].defense = 2;
    const int atk_id = units_spawn_allow_stack(&pool, soldier_ti, 5, 5);
    const int def_id = units_spawn_allow_stack(&pool, soldier_ti, 5, 5);
    ColonizeUnit* atk = units_get(&pool, atk_id);
    ColonizeUnit* defu = units_get(&pool, def_id);
    if (!atk || !defu) {
      fprintf(stderr, "fort-defense spawn failed\n");
      return 1;
    }
    atk->nation_id = 1;
    defu->nation_id = 0;
    defu->orders = UNITS_ORDER_NONE;
    col->has_building[0] = true;
    col->has_building[1] = false;
    col->has_building[2] = false;
    units_set_combat_colonies(&colonies);
    /* FUN_157e_015e Stockade local_1a=4 → ((4+4)*16)>>2=32 > atk ((0+4)*16>>2)*3>>1=24. */
    if (units_resolve_land_combat_ff(&pool, atk_id, def_id, NULL, NULL)) {
      fprintf(stderr, "Stockade defense should beat attack 2 vs base 2\n");
      return 1;
    }
    if (!units_get(&pool, def_id) || !units_get(&pool, def_id)->active) {
      fprintf(stderr, "Stockade defender should survive\n");
      return 1;
    }
    units_set_combat_colonies(NULL);

    /* Veteran + fortify stack peels (FUN_157e_004a / 015e). */
    {
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &pool;
      sctx.colonies = &colonies;
      const int vti = units_find_type(&pool, "Soldiers");
      const int vid = units_spawn_allow_stack(&pool, vti, 6, 6);
      ColonizeUnit* vu = units_get(&pool, vid);
      if (!vu) {
        fprintf(stderr, "vet spawn failed\n");
        return 1;
      }
      vu->nation_id = 0;
      vu->profession = UNITS_JOB_SOLDIER;
      pool.types[vti].defense = 2;
      ColonizeCombatSideFlags fl;
      const int base = combat_unit_base_x8(&sctx, vid, 0, &fl);
      /* 2*8=16 +50% vet = 24 */
      if (base != 24 || (fl.flags & COMBAT_FLAG_VETERAN) == 0) {
        fprintf(stderr, "vet base×8 want 24+flag got %d flags=%x\n", base, fl.flags);
        return 1;
      }
      vu->orders = UNITS_ORDER_FORTIFIED;
      col->x = 6;
      col->y = 6;
      col->has_building[0] = true;
      col->has_building[1] = false;
      col->has_building[2] = false;
      const int eng = combat_engagement_strength(&sctx, vid, -1, &fl);
      /* Stockade local_1a=4 + fortify +2 → 6; ((6+4)*24)>>2 = 60 */
      if (eng != 60) {
        fprintf(stderr, "Stockade+fortify engagement want 60 got %d\n", eng);
        return 1;
      }
      units_despawn(&pool, vid);
    }

    /* Combat Analysis gate. */
    {
      ColonizeCol1Save ag;
      memset(&ag, 0, sizeof(ag));
      ag.player[0].control = 0;
      ag.player[1].control = 1;
      if (combat_analysis_should_show(&ag, 0, 1, 0)) {
        fprintf(stderr, "analysis should be off when option clear\n");
        return 1;
      }
      ag.head.game_options.combat_analysis = 1;
      if (!combat_analysis_should_show(&ag, 0, 1, 0)) {
        fprintf(stderr, "analysis should show for human attacker\n");
        return 1;
      }
      if (combat_analysis_should_show(&ag, 1, 1, 0)) {
        fprintf(stderr, "analysis should skip AI-only fight\n");
        return 1;
      }
      /* Header = baseline; Attack Bonus listed for land attacker. */
      {
        ColonizeCombatEngagement eng;
        memset(&eng, 0, sizeof(eng));
        eng.attacker_id = 0;
        eng.defender_id = 0;
        eng.atk_strength = 24;
        eng.def_strength = 16;
        eng.is_naval = false;
        eng.atk_flags.base_combat = 2;
        eng.atk_flags.flags = COMBAT_FLAG_MODE_ATK | COMBAT_FLAG_VETERAN;
        eng.def_flags.base_combat = 1;
        CombatAnalysisDialog dlg;
        memset(&dlg, 0, sizeof(dlg));
        if (!combat_analysis_open(&dlg, &pool, &eng)) {
          fprintf(stderr, "combat_analysis_open failed\n");
          return 1;
        }
        if (dlg.eng.atk_flags.base_combat != 2 || dlg.eng.def_flags.base_combat != 1) {
          fprintf(stderr, "analysis should keep baseline combat bytes\n");
          return 1;
        }
        int found_atk_bonus = 0;
        int found_vet = 0;
        for (int i = 0; i < dlg.atk_line_count; ++i) {
          if (strstr(dlg.atk_lines[i], "Attack Bonus")) {
            found_atk_bonus = 1;
          }
          if (strstr(dlg.atk_lines[i], "Veteran")) {
            found_vet = 1;
          }
        }
        if (!found_atk_bonus || !found_vet) {
          fprintf(stderr, "land analysis missing Attack Bonus/Veteran lines\n");
          return 1;
        }
        /* Village Attack same-click: unarmed until mouse up / idle frame. */
        {
          ColonizeInputState in;
          memset(&in, 0, sizeof(in));
          in.mouse_left_down = true;
          in.mouse_left_clicked = true;
          (void)combat_analysis_handle_input(&dlg, &in);
          if (!dlg.open || dlg.arm_input) {
            fprintf(stderr, "analysis should stay open unarmed while click held\n");
            return 1;
          }
          memset(&in, 0, sizeof(in));
          (void)combat_analysis_handle_input(&dlg, &in);
          if (!dlg.arm_input || !dlg.open) {
            fprintf(stderr, "analysis should arm after idle frame\n");
            return 1;
          }
          in.mouse_left_clicked = true;
          (void)combat_analysis_handle_input(&dlg, &in);
          if (dlg.open) {
            fprintf(stderr, "analysis should dismiss on armed click\n");
            return 1;
          }
        }
        if (!combat_analysis_open(&dlg, &pool, &eng)) {
          fprintf(stderr, "combat_analysis_open re-open failed\n");
          return 1;
        }
        for (int i = 0; i < dlg.def_line_count; ++i) {
          if (strstr(dlg.def_lines[i], "Attack Bonus")) {
            fprintf(stderr, "defender must not list Attack Bonus\n");
            return 1;
          }
        }
        combat_analysis_close(&dlg);
        eng.is_naval = true;
        if (!combat_analysis_open(&dlg, &pool, &eng)) {
          fprintf(stderr, "naval combat_analysis_open failed\n");
          return 1;
        }
        for (int i = 0; i < dlg.atk_line_count; ++i) {
          if (strstr(dlg.atk_lines[i], "Attack Bonus")) {
            fprintf(stderr, "naval analysis must not list land Attack Bonus\n");
            return 1;
          }
        }
        combat_analysis_close(&dlg);
      }
      fprintf(stderr, "unit_units: combat analysis gate ok\n");
    }

    /* Treasure capture: winner gets LE16 gold into nation treasury. */
    ColonizeCol1Save tcol1;
    memset(&tcol1, 0, sizeof(tcol1));
    tcol1.nation[1].gold = 50;
    int use_ti = units_find_type(&pool, "Treasure");
    if (use_ti < 0) {
      if (pool.type_count >= (int)(sizeof(pool.types) / sizeof(pool.types[0]))) {
        fprintf(stderr, "no room for Treasure type\n");
        return 1;
      }
      use_ti = pool.type_count;
      snprintf(pool.types[use_ti].name, sizeof(pool.types[use_ti].name), "Treasure");
      pool.types[use_ti].domain = COLONIZE_UNIT_DOMAIN_LAND;
      pool.types[use_ti].defense = 0;
      pool.types[use_ti].attack = 0;
      pool.type_count++;
    }
    const int capturer = units_spawn_allow_stack(&pool, soldier_ti, 6, 6);
    const int loot_id = units_spawn_allow_stack(&pool, use_ti, 6, 6);
    ColonizeUnit* cap = units_get(&pool, capturer);
    ColonizeUnit* loot = units_get(&pool, loot_id);
    if (!cap || !loot) {
      fprintf(stderr, "treasure capture spawn failed\n");
      return 1;
    }
    cap->nation_id = 1;
    loot->nation_id = 0;
    loot->hold_goods_amount[0] = 200 & 0xff;
    loot->hold_goods_amount[1] = (200 >> 8) & 0xff;
    pool.types[soldier_ti].attack = 5;
    pool.types[use_ti].defense = 1;
    if (!units_resolve_land_combat_ff(&pool, capturer, loot_id, NULL, &tcol1)) {
      fprintf(stderr, "treasure capture combat should win\n");
      return 1;
    }
    if (tcol1.nation[1].gold != 250) {
      fprintf(stderr, "treasure capture gold want 250 got %u\n", tcol1.nation[1].gold);
      return 1;
    }
    if (units_get(&pool, loot_id) && units_get(&pool, loot_id)->active) {
      fprintf(stderr, "captured Treasure should despawn\n");
      return 1;
    }
    fprintf(stderr, "unit_units: fortification defense + treasure capture ok\n");
  }

  /* Coastal Fort/Fortress naval fire (FUN_364b_03f6). */
  {
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
    snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
    snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name), "Fortress");
    colonies.building_type_count = 3;

    int cx = -1, cy = -1, wx = -1, wy = -1;
    for (int y = 1; y < map.height - 1 && cx < 0; ++y) {
      for (int x = 1; x < map.width - 1; ++x) {
        if (!map_tile_is_land(&map, x, y)) {
          continue;
        }
        static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        for (int d = 0; d < 8; ++d) {
          const int nx = x + dx[d];
          const int ny = y + dy[d];
          if (map_tile_is_water(&map, nx, ny)) {
            cx = x;
            cy = y;
            wx = nx;
            wy = ny;
            break;
          }
        }
        if (cx >= 0) {
          break;
        }
      }
    }
    if (cx < 0) {
      fprintf(stderr, "no coastal tile for fort-fire smoke\n");
      return 1;
    }

    ColonizeColony* col = &colonies.colonies[0];
    col->id = 0;
    col->active = true;
    col->nation_id = 0;
    col->x = cx;
    col->y = cy;
    col->population = 3;
    colonies.colony_count = 1;
    col->has_building[1] = true; /* Fort */

    if (units_coastal_fort_attack_strength(&colonies, col, &pool) != 4) {
      fprintf(stderr, "Fort strength want 4 got %d\n",
              units_coastal_fort_attack_strength(&colonies, col, &pool));
      return 1;
    }
    col->has_building[2] = true; /* Fortress overrides */
    if (units_coastal_fort_attack_strength(&colonies, col, &pool) != 8) {
      fprintf(stderr, "Fortress strength want 8 got %d\n",
              units_coastal_fort_attack_strength(&colonies, col, &pool));
      return 1;
    }
    const int art_ti = units_find_type(&pool, "Artillery");
    if (art_ti < 0) {
      fprintf(stderr, "Artillery type missing for fort-fire smoke\n");
      return 1;
    }
    const int art_id = units_spawn_allow_stack(&pool, art_ti, cx, cy);
    ColonizeUnit* art = units_get(&pool, art_id);
    if (!art) {
      fprintf(stderr, "Artillery spawn failed\n");
      return 1;
    }
    art->nation_id = 0;
    if (units_coastal_fort_attack_strength(&colonies, col, &pool) != 16) {
      fprintf(stderr, "Fortress+1 arty want 16 got %d\n",
              units_coastal_fort_attack_strength(&colonies, col, &pool));
      return 1;
    }
    units_despawn(&pool, art_id);
    col->has_building[2] = false; /* Fort only, strength 4 */

    ColonizeCol1Save fcol1;
    memset(&fcol1, 0, sizeof(fcol1));
    /* Unclaimed FF slots must be -1 (memset 0 → nation 0 falsely owns Franklin). */
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      fcol1.head.founding_father[i] = -1;
    }
    ai_diplo_declare_war(&fcol1, 0, 1);
    if (!ai_diplo_at_war(&fcol1, 0, 1)) {
      fprintf(stderr, "fort-fire smoke: declare_war 0 vs 1 failed\n");
      return 1;
    }

    const int caravel_ti = units_find_type(&pool, "Caravel");
    if (caravel_ti < 0) {
      fprintf(stderr, "Caravel missing for fort-fire smoke\n");
      return 1;
    }
    const int old_def = pool.types[caravel_ti].defense;
    pool.types[caravel_ti].defense = 2; /* Fort atk 4 >= 2 → sink */

    const int foe_id = units_spawn_allow_stack(&pool, caravel_ti, wx, wy);
    ColonizeUnit* foe = units_get(&pool, foe_id);
    if (!foe) {
      fprintf(stderr, "enemy ship spawn failed\n");
      return 1;
    }
    foe->nation_id = 1;

    char fort_status[80];
    fort_status[0] = '\0';
    const int sunk =
      units_coastal_fort_fire_pulse(&pool, &colonies, &map, &fcol1, NULL, 0, fort_status, sizeof(fort_status));
    if (sunk < 1 || (units_get(&pool, foe_id) && units_get(&pool, foe_id)->active)) {
      fprintf(stderr, "Fort at war should sink adjacent enemy ship (sunk=%d)\n", sunk);
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    if (strstr(fort_status, "sank") == NULL) {
      fprintf(stderr, "fort fire want sank status got '%s'\n", fort_status);
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }

    /* Ship-slow: fort miss leaves ship with moves_left=0. */
    ai_diplo_declare_war(&fcol1, 0, 1);
    pool.types[caravel_ti].defense = 100; /* fort atk 4 loses */
    const int slow_id = units_spawn_allow_stack(&pool, caravel_ti, wx, wy);
    foe = units_get(&pool, slow_id);
    if (!foe) {
      fprintf(stderr, "ship-slow spawn failed\n");
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    foe->nation_id = 1;
    foe->moves_left = 4;
    if (units_coastal_fort_fire_pulse(&pool, &colonies, &map, &fcol1, NULL, -1, NULL, 0) != 0) {
      fprintf(stderr, "ship-slow: fort should miss high-def ship\n");
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    foe = units_get(&pool, slow_id);
    if (!foe || !foe->active || foe->moves_left != 0) {
      fprintf(
        stderr,
        "ship-slow: want active moves=0 got active=%d moves=%d\n",
        foe ? foe->active : 0,
        foe ? foe->moves_left : -1
      );
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    units_despawn(&pool, slow_id);
    pool.types[caravel_ti].defense = 2;

    /* Peace: no fire (unless Privateer). */
    ai_diplo_make_peace(&fcol1, 0, 1);
    const int peace_id = units_spawn_allow_stack(&pool, caravel_ti, wx, wy);
    foe = units_get(&pool, peace_id);
    foe->nation_id = 1;
    if (units_coastal_fort_fire_pulse(&pool, &colonies, &map, &fcol1, NULL, -1, NULL, 0) != 0) {
      fprintf(stderr, "Fort at peace should not sink Caravel\n");
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    units_despawn(&pool, peace_id);

    const int priv_ti = units_find_type(&pool, "Privateer");
    if (priv_ti >= 0) {
      const int old_pdef = pool.types[priv_ti].defense;
      pool.types[priv_ti].defense = 2;
      const int pid = units_spawn_allow_stack(&pool, priv_ti, wx, wy);
      ColonizeUnit* pr = units_get(&pool, pid);
      pr->nation_id = 1;
      const int psunk =
        units_coastal_fort_fire_pulse(&pool, &colonies, &map, &fcol1, NULL, -1, NULL, 0);
      if (psunk < 1 || (units_get(&pool, pid) && units_get(&pool, pid)->active)) {
        fprintf(stderr, "Fort should sink Privateer at peace\n");
        pool.types[priv_ti].defense = old_pdef;
        pool.types[caravel_ti].defense = old_def;
        return 1;
      }
      pool.types[priv_ti].defense = old_pdef;
    }

    pool.types[caravel_ti].defense = old_def;
    fprintf(stderr, "unit_units: coastal fort naval fire ok\n");
  }

  /* LCR rumour: clear + de Soto reveal path. */
  {
    if (!map_tile_has_rumour(&map, 8, 14)) {
      fprintf(stderr, "AMER2 (8,14) expected procedural rumour\n");
      return 1;
    }
    ColonizeCol1Save lcol1;
    memset(&lcol1, 0, sizeof(lcol1));
    const int scout_ti = units_find_type(&pool, "Scouts");
    if (scout_ti < 0) {
      fprintf(stderr, "Scout type missing for LCR smoke\n");
      return 1;
    }
    const int scid = units_spawn_allow_stack(&pool, scout_ti, 8, 14);
    ColonizeUnit* scout = units_get(&pool, scid);
    if (!scout) {
      fprintf(stderr, "LCR scout spawn failed\n");
      return 1;
    }
    scout->nation_id = 0;
    if (!units_resolve_lcr_rumour(&pool, scid, &map, &lcol1, NULL, NULL, -1)) {
      fprintf(stderr, "LCR resolve without de Soto failed\n");
      return 1;
    }
    if (map_tile_has_rumour(&map, 8, 14)) {
      fprintf(stderr, "LCR rumour should be cleared\n");
      return 1;
    }
    lcol1.head.founding_father[FF_HERNANDO_DE_SOTO] = 0;
    lcol1.nation[0].founding_fathers[FF_HERNANDO_DE_SOTO / 8] |=
      (uint8_t)(1u << (FF_HERNANDO_DE_SOTO % 8));
    ColonizeWorldMap lmap;
    memset(&lmap, 0, sizeof(lmap));
    lmap.width = map.width;
    lmap.height = map.height;
    const size_t n = (size_t)map.width * (size_t)map.height;
    lmap.terrain = malloc(n);
    lmap.layer2 = calloc(n, 1);
    lmap.layer3 = calloc(n, 1);
    lmap.seen = calloc(n, 1);
    if (!lmap.terrain || !lmap.layer2 || !lmap.layer3 || !lmap.seen) {
      free(lmap.terrain);
      free(lmap.layer2);
      free(lmap.layer3);
      free(lmap.seen);
      fprintf(stderr, "LCR mini-map alloc failed\n");
      return 1;
    }
    memcpy(lmap.terrain, map.terrain, n);
    if (!map_tile_has_rumour(&lmap, 8, 14)) {
      fprintf(stderr, "LCR fresh map (8,14) expected rumour\n");
      map_free(&lmap);
      return 1;
    }
    const int scid2 = units_spawn_allow_stack(&pool, scout_ti, 8, 14);
    scout = units_get(&pool, scid2);
    scout->nation_id = 0;
    if (!units_resolve_lcr_rumour(&pool, scid2, &lmap, &lcol1, NULL, NULL, -1)) {
      map_free(&lmap);
      fprintf(stderr, "LCR resolve with de Soto failed\n");
      return 1;
    }
    if (map_tile_has_rumour(&lmap, 8, 14)) {
      map_free(&lmap);
      fprintf(stderr, "de Soto LCR rumour not cleared\n");
      return 1;
    }
    if (!map_tile_seen_by(&lmap, 8, 14, 0)) {
      map_free(&lmap);
      fprintf(stderr, "de Soto LCR should reveal scout tile\n");
      return 1;
    }
    units_despawn(&pool, scid);
    units_despawn(&pool, scid2);
    map_free(&lmap);
  }

  /*
   * LCR outcome dispatch: real seeded RNG (not the rng==NULL fallback above)
   * over every rumour tile on the map, one fresh Scout per tile. Confirms
   * each real-effect branch actually fires at least once: gold credited,
   * Fountain of Youth dock immigrants, a Treasure train spawned (Cibola /
   * Burial3), a colonist joining (survivors), and a vanished scout.
   */
  {
    EuropeScreen eu;
    char eerr[256];
    if (!europe_load(&eu, "COLONIZE", eerr, sizeof(eerr))) {
      fprintf(stderr, "LCR dispatch: europe_load failed: %s\n", eerr);
      return 1;
    }
    const int scout_ti = units_find_type(&pool, "Scouts");
    if (scout_ti < 0) {
      fprintf(stderr, "LCR dispatch: Scout type missing\n");
      europe_free(&eu);
      return 1;
    }
    ColonizeCol1Save dcol1;
    memset(&dcol1, 0, sizeof(dcol1));
    /* founding_father[]==0 would misread as "nation 0 elected FF 0" (the
     * real sentinel is -1 = unrecruited) and force every trial down the de
     * Soto positive-only branch. */
    for (int i = 0; i < COLONIZE_COL1_FF_COUNT; ++i) {
      dcol1.head.founding_father[i] = -1;
    }
    bool saw_gold = false;
    bool saw_fountain = false;
    bool saw_treasure = false;
    bool saw_survivor = false;
    bool saw_vanish = false;
    const int treasure_ti = units_find_type(&pool, "Treasure");
    /* One persistent RNG advanced across every trial — reseeding per trial
     * with sequential seeds would just resample the LCG's early outputs and
     * skew the distribution, not exercise it. */
    ColonizeDosRng drng;
    dos_rng_seed(&drng, 12345);
    int trials = 0;
    for (int pass = 0; pass < 15 && trials < 400; ++pass) {
      for (int y = 0; y < (int)map.height && trials < 400; ++y) {
        for (int x = 0; x < (int)map.width && trials < 400; ++x) {
          if (!map_tile_has_rumour(&map, x, y)) {
            continue;
          }
          trials++;
          const uint32_t gold_before = dcol1.nation[0].gold;
          const int dock_before = eu.dock_count;
          int treasure_before = 0;
          int colonist_before = 0;
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            if (!pool.units[i].active) continue;
            if (treasure_ti >= 0 && pool.units[i].type_index == treasure_ti) treasure_before++;
            if (pool.units[i].type_index == colonist) colonist_before++;
          }
          const int sid = units_spawn_allow_stack(&pool, scout_ti, x, y);
          ColonizeUnit* su = units_get(&pool, sid);
          if (!su) {
            continue;
          }
          su->nation_id = 0;
          if (!units_resolve_lcr_rumour(&pool, sid, &map, &dcol1, &drng, &eu, 0)) {
            fprintf(stderr, "LCR dispatch: resolve failed at (%d,%d)\n", x, y);
            europe_free(&eu);
            return 1;
          }
        if (dcol1.nation[0].gold > gold_before) {
          saw_gold = true;
        }
        if (eu.dock_count >= dock_before + 8) {
          saw_fountain = true;
        }
        int treasure_after = 0;
        int colonist_after = 0;
        bool scout_active = false;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          if (!pool.units[i].active) continue;
          if (treasure_ti >= 0 && pool.units[i].type_index == treasure_ti) treasure_after++;
          if (pool.units[i].type_index == colonist) colonist_after++;
          if (pool.units[i].id == sid) scout_active = true;
        }
        if (treasure_after > treasure_before) {
          saw_treasure = true;
        }
        if (colonist_after > colonist_before) {
          saw_survivor = true;
        }
        if (!scout_active) {
          saw_vanish = true;
        }
        if (scout_active) {
          units_despawn(&pool, sid);
        }
        /* Re-arm for the next pass over the same tile set (test-only). */
        map.layer2[y * map.width + x] &= (uint8_t)~MAP_LAYER2_RUMOUR_CLEARED;
        }
      }
    }
    europe_free(&eu);
    if (trials < 10) {
      fprintf(stderr, "LCR dispatch: too few rumour tiles on AMER2 (%d)\n", trials);
      return 1;
    }
    if (!saw_gold || !saw_fountain || !saw_treasure || !saw_survivor || !saw_vanish) {
      fprintf(
        stderr,
        "LCR dispatch: missing outcome over %d trials (gold=%d fountain=%d treasure=%d survivor=%d vanish=%d)\n",
        trials,
        saw_gold,
        saw_fountain,
        saw_treasure,
        saw_survivor,
        saw_vanish
      );
      return 1;
    }
    fprintf(stderr, "unit_units: LCR outcome dispatch ok (%d trials)\n", trials);
  }

  /* Enter-probe matrix: bounce / domain / land combat / naval / capture. */
  {
    const int pioneer_t = pioneer;
    const int soldier = units_find_type(&pool, "Soldiers");
    const int brave = units_find_type(&pool, "Braves");
    const int caravel_t = caravel;
    const int colonist_t = colonist;
    if (pioneer_t < 0 || soldier < 0 || brave < 0 || caravel_t < 0) {
      fprintf(stderr, "enter-probe: missing unit types\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int ax = -1, ay = -1, dx = -1, dy = -1;
    for (int y = 1; y < (int)map.height - 1 && ax < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && ax < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y) &&
            units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
          ax = x;
          ay = y;
          dx = x + 1;
          dy = y;
        }
      }
    }
    if (ax < 0) {
      fprintf(stderr, "enter-probe: no free land pair\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /* Friendly stack → OK; can_enter true. */
    {
      const int a = units_spawn(&pool, colonist_t, ax, ay);
      const int b = units_spawn_allow_stack(&pool, pioneer_t, ax, ay);
      ColonizeUnit* ua = units_get(&pool, a);
      ColonizeUnit* ub = units_get(&pool, b);
      if (!ua || !ub) {
        fprintf(stderr, "enter-probe friendly spawn failed\n");
        return 1;
      }
      ua->nation_id = 0;
      ub->nation_id = 0;
      const ColonizeEnterReason r =
        units_enter_probe(&pool, ub->type_index, &map, ax, ay, b, NULL);
      if (r != COLONIZE_ENTER_OK || !units_can_enter(&pool, ub->type_index, &map, ax, ay, b, NULL)) {
        fprintf(stderr, "enter-probe friendly expected OK got %d\n", (int)r);
        return 1;
      }
      units_despawn(&pool, a);
      units_despawn(&pool, b);
    }

    /* Pioneer into Brave → bounce (non-combat). */
    {
      const int pid = units_spawn(&pool, pioneer_t, ax, ay);
      const int bid = units_spawn_allow_stack(&pool, brave, dx, dy);
      ColonizeUnit* p = units_get(&pool, pid);
      ColonizeUnit* br = units_get(&pool, bid);
      if (!p || !br) {
        fprintf(stderr, "enter-probe bounce spawn failed\n");
        return 1;
      }
      p->nation_id = 0;
      br->nation_id = 4;
      p->moves_left = 3;
      const ColonizeEnterReason r =
        units_enter_probe(&pool, p->type_index, &map, dx, dy, pid, NULL);
      if (r != COLONIZE_ENTER_BOUNCE_FOREIGN) {
        fprintf(stderr, "enter-probe pioneer bounce expected got %d\n", (int)r);
        return 1;
      }
      if (units_try_move(&pool, pid, &map, dx, dy, NULL, NULL)) {
        fprintf(stderr, "enter-probe pioneer should not enter Brave tile\n");
        return 1;
      }
      if (units_last_enter_reason() != COLONIZE_ENTER_BOUNCE_FOREIGN) {
        fprintf(stderr, "enter-probe last reason bounce got %d\n", (int)units_last_enter_reason());
        return 1;
      }
      units_despawn(&pool, pid);
      units_despawn(&pool, bid);
    }

    /* Soldier into Brave → combat land; can_enter false. */
    {
      const int sid = units_spawn(&pool, soldier, ax, ay);
      const int bid = units_spawn_allow_stack(&pool, brave, dx, dy);
      ColonizeUnit* s = units_get(&pool, sid);
      ColonizeUnit* br = units_get(&pool, bid);
      if (!s || !br) {
        fprintf(stderr, "enter-probe combat spawn failed\n");
        return 1;
      }
      s->nation_id = 0;
      br->nation_id = 4;
      s->moves_left = 3;
      const ColonizeEnterReason r =
        units_enter_probe(&pool, s->type_index, &map, dx, dy, sid, NULL);
      if (r != COLONIZE_ENTER_COMBAT_LAND) {
        fprintf(stderr, "enter-probe combat land expected got %d\n", (int)r);
        return 1;
      }
      if (units_can_enter(&pool, s->type_index, &map, dx, dy, sid, NULL)) {
        fprintf(stderr, "enter-probe can_enter should be false for combat dest\n");
        return 1;
      }
      units_despawn(&pool, sid);
      units_despawn(&pool, bid);
    }

    /* Domain deny: land into ocean when adjacent water exists. */
    {
      int ox = -1, oy = -1;
      for (int y = 0; y < (int)map.height && ox < 0; ++y) {
        for (int x = 0; x < (int)map.width && ox < 0; ++x) {
          if (map_tile_is_water(&map, x, y) && abs(x - ax) <= 1 && abs(y - ay) <= 1 &&
              !(x == ax && y == ay)) {
            ox = x;
            oy = y;
          }
        }
      }
      if (ox >= 0) {
        const int pid = units_spawn(&pool, pioneer_t, ax, ay);
        ColonizeUnit* p = units_get(&pool, pid);
        if (p) {
          p->nation_id = 0;
          const ColonizeEnterReason r =
            units_enter_probe(&pool, p->type_index, &map, ox, oy, pid, NULL);
          if (r != COLONIZE_ENTER_BLOCKED_DOMAIN) {
            fprintf(stderr, "enter-probe domain deny expected got %d\n", (int)r);
            return 1;
          }
        }
        units_despawn(&pool, pid);
      }
    }

    /* Capture-on-enter: soldier onto empty foreign colony. */
    {
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      if (!colonies_load_names(&colonies, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&colonies, &names)) {
        fprintf(stderr, "enter-probe colonies init failed\n");
        return 1;
      }
      const int cid =
        colonies_found(&colonies, &map, dx, dy, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
      if (cid < 0) {
        fprintf(stderr, "enter-probe found rival colony failed\n");
        return 1;
      }
      ColonizeColony* col = colonies_get_mut(&colonies, cid);
      if (col) {
        col->nation_id = 1;
        col->population = 1;
      }
      const int sid = units_spawn(&pool, soldier, ax, ay);
      ColonizeUnit* s = units_get(&pool, sid);
      if (!s) {
        fprintf(stderr, "enter-probe capture soldier spawn failed\n");
        return 1;
      }
      s->nation_id = 0;
      s->moves_left = 5;
      if (!units_try_move(&pool, sid, &map, dx, dy, &colonies, NULL)) {
        fprintf(stderr, "enter-probe capture move failed reason=%d\n", (int)units_last_enter_reason());
        return 1;
      }
      col = colonies_get_mut(&colonies, cid);
      if (!col || col->nation_id != 0) {
        fprintf(
          stderr,
          "enter-probe capture expected nation 0 got %d\n",
          col ? col->nation_id : -1
        );
        return 1;
      }
      units_despawn(&pool, sid);
      (void)colonist_t;
    }

    /* Naval move-into combat. */
    {
      int wx = -1, wy = -1, wx2 = -1, wy2 = -1;
      for (int y = 1; y < (int)map.height - 1 && wx < 0; ++y) {
        for (int x = 1; x < (int)map.width - 1 && wx < 0; ++x) {
          if (map_tile_is_water(&map, x, y) && map_tile_is_water(&map, x + 1, y) &&
              units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
            wx = x;
            wy = y;
            wx2 = x + 1;
            wy2 = y;
          }
        }
      }
      if (wx < 0) {
        fprintf(stderr, "enter-probe: no water pair for naval\n");
        return 1;
      }
      const int a = units_spawn(&pool, caravel_t, wx, wy);
      const int b = units_spawn_allow_stack(&pool, caravel_t, wx2, wy2);
      ColonizeUnit* ua = units_get(&pool, a);
      ColonizeUnit* ub = units_get(&pool, b);
      if (!ua || !ub) {
        fprintf(stderr, "enter-probe naval spawn failed\n");
        return 1;
      }
      ua->nation_id = 0;
      ub->nation_id = 1;
      ua->moves_left = 4;
      const ColonizeEnterReason r =
        units_enter_probe(&pool, ua->type_index, &map, wx2, wy2, a, NULL);
      if (r != COLONIZE_ENTER_COMBAT_NAVAL) {
        fprintf(stderr, "enter-probe naval combat expected got %d\n", (int)r);
        return 1;
      }
      pool.types[caravel_t].attack = 99;
      pool.types[caravel_t].defense = 1;
      if (!units_try_move(&pool, a, &map, wx2, wy2, NULL, NULL)) {
        fprintf(stderr, "enter-probe naval move combat failed\n");
        return 1;
      }
      if (units_get_const(&pool, b) != NULL) {
        fprintf(stderr, "enter-probe naval defender should be gone\n");
        return 1;
      }
      ua = units_get(&pool, a);
      if (!ua || ua->x != wx2 || ua->y != wy2) {
        fprintf(stderr, "enter-probe naval winner should occupy tile\n");
        return 1;
      }
      units_despawn(&pool, a);
    }
  }

  /* Land → ocean with own ship → BOARD; sentry auto-load when ship leaves. */
  {
    int lx = -1, ly = -1, wx = -1, wy = -1, wx2 = -1, wy2 = -1;
    for (int y = 1; y < (int)map.height - 1 && lx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && lx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y)) {
          continue;
        }
        static const int kdx[4] = {1, -1, 0, 0};
        static const int kdy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
          const int nx = x + kdx[d];
          const int ny = y + kdy[d];
          if (!map_tile_is_water(&map, nx, ny) || units_id_at(&pool, x, y) >= 0 ||
              units_id_at(&pool, nx, ny) >= 0) {
            continue;
          }
          /* Need a second adjacent water tile for the ship to leave to. */
          int ox = -1, oy = -1;
          for (int e = 0; e < 8; ++e) {
            static const int edx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
            static const int edy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
            const int tx = nx + edx[e];
            const int ty = ny + edy[e];
            if (map_tile_is_water(&map, tx, ty) && (tx != x || ty != y) &&
                units_id_at(&pool, tx, ty) < 0) {
              ox = tx;
              oy = ty;
              break;
            }
          }
          if (ox < 0) {
            continue;
          }
          lx = x;
          ly = y;
          wx = nx;
          wy = ny;
          wx2 = ox;
          wy2 = oy;
          break;
        }
      }
    }
    if (lx < 0) {
      fprintf(stderr, "board-enter: no land/water/water triple\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    const int land_id = units_spawn(&pool, pioneer, lx, ly);
    const int ship_id = units_spawn(&pool, units_find_type(&pool, "Caravel"), wx, wy);
    ColonizeUnit* land = units_get(&pool, land_id);
    ColonizeUnit* ship = units_get(&pool, ship_id);
    if (!land || !ship) {
      fprintf(stderr, "board-enter spawn failed\n");
      return 1;
    }
    land->nation_id = 0;
    ship->nation_id = 0;
    land->moves_left = 3;
    ship->moves_left = 4;

    const ColonizeEnterReason br =
      units_enter_probe(&pool, land->type_index, &map, wx, wy, land_id, NULL);
    if (br != COLONIZE_ENTER_BOARD) {
      fprintf(stderr, "board-enter probe expected BOARD got %d\n", (int)br);
      return 1;
    }
    if (units_can_enter(&pool, land->type_index, &map, wx, wy, land_id, NULL)) {
      fprintf(stderr, "board-enter can_enter should stay false (goto)\n");
      return 1;
    }
    if (!units_try_move(&pool, land_id, &map, wx, wy, NULL, NULL)) {
      fprintf(stderr, "board-enter try_move failed\n");
      return 1;
    }
    land = units_get(&pool, land_id);
    ship = units_get(&pool, ship_id);
    if (!land || !ship || land->aboard_ship_id != ship_id || ship->cargo_count != 1) {
      fprintf(stderr, "board-enter state wrong cargo=%d aboard=%d\n",
              ship ? ship->cargo_count : -1, land ? land->aboard_ship_id : -1);
      return 1;
    }
    /* Unload onto land for sentry auto-board test. */
    if (!units_unload_passenger(&pool, ship_id, land_id, &map, lx, ly, NULL)) {
      fprintf(stderr, "board-enter unload failed\n");
      return 1;
    }
    land = units_get(&pool, land_id);
    ship = units_get(&pool, ship_id);
    if (!land || !ship) {
      fprintf(stderr, "board-enter after unload missing\n");
      return 1;
    }
    /* Ocean sentry orphan on ship tile; ship departs → auto-board. */
    land->x = wx;
    land->y = wy;
    land->orders = UNITS_ORDER_SENTRY;
    land->moves_left = 0;
    ship->x = wx;
    ship->y = wy;
    ship->moves_left = 4;
    if (!units_try_move(&pool, ship_id, &map, wx2, wy2, NULL, NULL)) {
      fprintf(stderr, "sentry-board ship move failed reason=%d\n", (int)units_last_enter_reason());
      return 1;
    }
    land = units_get(&pool, land_id);
    ship = units_get(&pool, ship_id);
    if (!land || !ship || land->aboard_ship_id != ship_id || ship->cargo_count < 1) {
      fprintf(stderr, "sentry-board expected auto-load aboard=%d cargo=%d\n",
              land ? land->aboard_ship_id : -1, ship ? ship->cargo_count : -1);
      return 1;
    }
    units_despawn(&pool, ship_id); /* also clears cargo */

    /* Ship → village tile (HAS_CITY, no colony) → VILLAGE_SHIP not LANDFALL. */
    {
      int vx = -1, vy = -1, sx = -1, sy = -1;
      for (int y = 1; y < (int)map.height - 1 && vx < 0; ++y) {
        for (int x = 1; x < (int)map.width - 1 && vx < 0; ++x) {
          if (!map_tile_is_land(&map, x, y) || !map.layer2) {
            continue;
          }
          static const int dx4[4] = {1, -1, 0, 0};
          static const int dy4[4] = {0, 0, 1, -1};
          for (int d = 0; d < 4; ++d) {
            const int nx = x + dx4[d];
            const int ny = y + dy4[d];
            if (map_tile_is_water(&map, nx, ny) && units_id_at(&pool, nx, ny) < 0) {
              vx = x;
              vy = y;
              sx = nx;
              sy = ny;
              break;
            }
          }
        }
      }
      if (vx < 0) {
        fprintf(stderr, "village-ship: no coastal land\n");
        return 1;
      }
      const size_t idx = (size_t)vy * (size_t)map.width + (size_t)vx;
      map.layer2[idx] = (uint8_t)(map.layer2[idx] | MAP_OCCUPANCY_HAS_CITY);
      const int sid = units_spawn(&pool, units_find_type(&pool, "Caravel"), sx, sy);
      ColonizeUnit* boat = units_get(&pool, sid);
      if (!boat) {
        fprintf(stderr, "village-ship spawn failed\n");
        return 1;
      }
      boat->nation_id = 0;
      boat->moves_left = 4;
      /* Loaded or empty: village must not become landfall. */
      const ColonizeEnterReason vr =
        units_enter_probe(&pool, boat->type_index, &map, vx, vy, sid, NULL);
      if (vr != COLONIZE_ENTER_VILLAGE_SHIP) {
        fprintf(stderr, "village-ship expected VILLAGE_SHIP got %d\n", (int)vr);
        return 1;
      }
      if (units_try_move(&pool, sid, &map, vx, vy, NULL, NULL)) {
        fprintf(stderr, "village-ship try_move should deny\n");
        return 1;
      }
      map.layer2[idx] = (uint8_t)(map.layer2[idx] & (uint8_t)~MAP_OCCUPANCY_HAS_CITY);
      units_despawn(&pool, sid);
    }
  }

  /* Phase-2 combat: 1b0e peels, best-defender, capture, naval damage, popups. */
  {
    AiPopupState pops;
    ai_popup_init(&pops);
    units_set_combat_popups(&pops, NULL);
    units_set_combat_human_nation(0);

    /* Spanish ambush +50% on colony vs Indian. */
    {
      ColonizeColonyPool cols;
      colonies_init(&cols);
      ColonizeColony* col = &cols.colonies[0];
      col->id = 0;
      col->active = true;
      col->nation_id = 2;
      col->x = 20;
      col->y = 20;
      cols.colony_count = 1;
      units_set_combat_colonies(&cols);
      const int spa_ti = units_find_type(&pool, "Soldiers");
      const int ind_ti = units_find_type(&pool, "Braves");
      if (spa_ti < 0 || ind_ti < 0) {
        fprintf(stderr, "phase2 ambush types missing\n");
        return 1;
      }
      const int aid = units_spawn_allow_stack(&pool, spa_ti, 20, 20);
      const int did = units_spawn_allow_stack(&pool, ind_ti, 20, 20);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      if (!a || !d) {
        fprintf(stderr, "phase2 ambush spawn failed\n");
        return 1;
      }
      a->nation_id = 2;
      d->nation_id = 4;
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &pool;
      sctx.colonies = &cols;
      ColonizeCombatEngageResult er;
      combat_land_engage(&sctx, aid, did, &er);
      if ((er.atk_flags.flags & COMBAT_FLAG_AMBUSH) == 0) {
        fprintf(stderr, "phase2 Spanish ambush flag missing\n");
        return 1;
      }
      units_despawn(&pool, aid);
      units_despawn(&pool, did);
      units_set_combat_colonies(NULL);
      fprintf(stderr, "unit_units: Spanish ambush peel ok\n");
    }

    /* Terrain stash: Indian→Euro and human→AI-Euro under WoI (REF). */
    {
      ColonizeWorldMap tmap;
      memset(&tmap, 0, sizeof(tmap));
      tmap.width = 16;
      tmap.height = 16;
      tmap.terrain = calloc(256, 1);
      tmap.layer2 = calloc(256, 1);
      tmap.layer3 = calloc(256, 1);
      if (!tmap.terrain || !tmap.layer2 || !tmap.layer3) {
        fprintf(stderr, "terrain-stash map alloc failed\n");
        return 1;
      }
      for (int i = 0; i < 256; ++i) {
        tmap.terrain[i] = 8; /* forest class → found_score 2 */
      }
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.difficulty = 4; /* Viceroy: difficulty peel adj=0 */
      c1.player[0].control = 0; /* human */
      c1.player[1].control = 1; /* AI Euro / REF-shaped */
      const int sol = units_find_type(&pool, "Soldiers");
      if (sol < 0) {
        fprintf(stderr, "terrain-stash Soldiers missing\n");
        free(tmap.terrain);
        free(tmap.layer2);
        free(tmap.layer3);
        return 1;
      }
      pool.types[sol].attack = 2;
      pool.types[sol].defense = 2;
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &pool;
      sctx.map = &tmap;
      sctx.col1 = &c1;

      /* Indian attacks Euro: defender loses terrain; attacker absorbs stash. */
      {
        const int aid = units_spawn_allow_stack(&pool, sol, 5, 5);
        const int did = units_spawn_allow_stack(&pool, sol, 5, 5);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "terrain-stash Indian spawn failed\n");
          return 1;
        }
        a->nation_id = 4;
        d->nation_id = 0;
        d->orders = UNITS_ORDER_NONE;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        /* def base 16, local_1a=0 → 16; atk base 16 → ((2+4)*16>>2)*3>>1 = 36 */
        if (er.def_strength != 16 || (er.def_flags.flags & COMBAT_FLAG_TERRAIN) != 0) {
          fprintf(
            stderr,
            "Indian ambush: def want 16/no-terrain got %d flags=%x stash=%d\n",
            er.def_strength,
            er.def_flags.flags,
            er.def_flags.terrain_stash
          );
          return 1;
        }
        if (er.def_flags.terrain_stash != 2) {
          fprintf(stderr, "Indian ambush: stash want 2 got %d\n", er.def_flags.terrain_stash);
          return 1;
        }
        if (er.atk_strength != 36 || (er.atk_flags.flags & COMBAT_FLAG_TERRAIN) == 0) {
          fprintf(
            stderr,
            "Indian ambush: atk want 36+terrain got %d flags=%x\n",
            er.atk_strength,
            er.atk_flags.flags
          );
          return 1;
        }
        units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      /* WoI: human attacks AI Euro (REF) — same stash path. */
      {
        c1.head.game_options.woi = 1;
        const int aid = units_spawn_allow_stack(&pool, sol, 6, 6);
        const int did = units_spawn_allow_stack(&pool, sol, 6, 6);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "terrain-stash WoI spawn failed\n");
          return 1;
        }
        a->nation_id = 0;
        d->nation_id = 1;
        d->orders = UNITS_ORDER_NONE;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        if (er.def_flags.terrain_stash != 2 || (er.def_flags.flags & COMBAT_FLAG_TERRAIN) != 0) {
          fprintf(
            stderr,
            "WoI REF ambush: def stash want 2/no-flag got stash=%d flags=%x\n",
            er.def_flags.terrain_stash,
            er.def_flags.flags
          );
          return 1;
        }
        if ((er.atk_flags.flags & COMBAT_FLAG_TERRAIN) == 0 || er.atk_strength != 36) {
          fprintf(
            stderr,
            "WoI REF ambush: atk want 36+terrain got %d flags=%x\n",
            er.atk_strength,
            er.atk_flags.flags
          );
          return 1;
        }
        units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      /* Control: Euro vs Euro pre-WoI — defender keeps terrain, stash 0. */
      {
        c1.head.game_options.woi = 0;
        const int aid = units_spawn_allow_stack(&pool, sol, 7, 7);
        const int did = units_spawn_allow_stack(&pool, sol, 7, 7);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "terrain-stash euro spawn failed\n");
          return 1;
        }
        a->nation_id = 1;
        d->nation_id = 0;
        d->orders = UNITS_ORDER_NONE;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        /* def ((2+4)*16)>>2=24; atk ((0+4)*16>>2)*3>>1=24 */
        if (er.def_strength != 24 || (er.def_flags.flags & COMBAT_FLAG_TERRAIN) == 0) {
          fprintf(
            stderr,
            "euro terrain: def want 24+flag got %d flags=%x\n",
            er.def_strength,
            er.def_flags.flags
          );
          return 1;
        }
        if (er.def_flags.terrain_stash != 0 || er.atk_strength != 24) {
          fprintf(
            stderr,
            "euro terrain: stash/atk want 0/24 got stash=%d atk=%d\n",
            er.def_flags.terrain_stash,
            er.atk_strength
          );
          return 1;
        }
        units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      free(tmap.terrain);
      free(tmap.layer2);
      free(tmap.layer3);
      fprintf(stderr, "unit_units: terrain stash ambush ok\n");
    }

    /* WoI REF +50% / Tory-Rebel support — colony only (FUN_5fef_1b0e). */
    {
      ColonizeColonyPool cols;
      colonies_init(&cols);
      ColonizeColony* col = &cols.colonies[0];
      col->id = 0;
      col->active = true;
      col->nation_id = 0;
      col->x = 10;
      col->y = 10;
      cols.colony_count = 1;
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.difficulty = 4;
      c1.head.game_options.woi = 1;
      c1.head.game_options.ref_present = 1;
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        c1.head.founding_father[i] = -1; /* unclaimed — memset 0 would look like nation 0 owns FF */
      }
      /* Colony SoL 60% via col1 colony record. */
      ColonizeCol1Colony cc;
      memset(&cc, 0, sizeof(cc));
      cc.x = 10;
      cc.y = 10;
      cc.rebel_dividend = 60;
      cc.rebel_divisor = 100;
      c1.colony = &cc;
      c1.head.colony_count = 1;

      const int sol = units_find_type(&pool, "Soldiers");
      if (sol < 0) {
        fprintf(stderr, "woi-ref Soldiers missing\n");
        return 1;
      }
      pool.types[sol].attack = 2;
      pool.types[sol].defense = 2;
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &pool;
      sctx.colonies = &cols;
      sctx.col1 = &c1;

      /* Human rebel attacks on colony with ref_present → +50% REF. */
      {
        const int aid = units_spawn_allow_stack(&pool, sol, 10, 10);
        const int did = units_spawn_allow_stack(&pool, sol, 10, 10);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "woi-ref rebel spawn failed\n");
          return 1;
        }
        a->nation_id = 0;
        d->nation_id = 1;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        /* atk base 16 → ×3/2=24; +50% REF → 36; +60% Rebels → 57 */
        if ((er.atk_flags.flags & COMBAT_FLAG_REF) == 0) {
          fprintf(stderr, "woi-ref rebel: REF flag missing\n");
          return 1;
        }
        if ((er.atk_flags.flags2 & COMBAT_FLAG_REBELS) == 0 || er.atk_flags.sol_percent != 60) {
          fprintf(
            stderr,
            "woi-ref rebel: Rebels want 60 got flags2=%x sol=%d\n",
            er.atk_flags.flags2,
            er.atk_flags.sol_percent
          );
          return 1;
        }
        if (er.atk_strength != 57) {
          fprintf(stderr, "woi-ref rebel: atk want 57 got %d\n", er.atk_strength);
          return 1;
        }
        units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      /* Crown attacks same colony → +50% REF + Tory share 40%. */
      {
        const int aid = units_spawn_allow_stack(&pool, sol, 10, 10);
        const int did = units_spawn_allow_stack(&pool, sol, 10, 10);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "woi-ref crown spawn failed\n");
          return 1;
        }
        a->nation_id = 1;
        d->nation_id = 0;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        /* 24 +50%=36; +40% Tories → 50 */
        if ((er.atk_flags.flags & COMBAT_FLAG_REF) == 0) {
          fprintf(stderr, "woi-ref crown: REF flag missing\n");
          return 1;
        }
        if ((er.atk_flags.flags2 & COMBAT_FLAG_TORIES) == 0 || er.atk_flags.sol_percent != 40) {
          fprintf(
            stderr,
            "woi-ref crown: Tories want 40 got flags2=%x sol=%d\n",
            er.atk_flags.flags2,
            er.atk_flags.sol_percent
          );
          return 1;
        }
        if (er.atk_strength != 50) {
          fprintf(stderr, "woi-ref crown: atk want 50 got %d\n", er.atk_strength);
          return 1;
        }
        units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      /* Open field: no REF flag even with ref_present. */
      {
        c1.head.game_options.ref_present = 1;
        const int aid = units_spawn_allow_stack(&pool, sol, 11, 11);
        const int did = units_spawn_allow_stack(&pool, sol, 11, 11);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "woi-ref field spawn failed\n");
          return 1;
        }
        a->nation_id = 0;
        d->nation_id = 1;
        ColonizeCombatEngageResult er;
        combat_land_engage(&sctx, aid, did, &er);
        if ((er.atk_flags.flags & COMBAT_FLAG_REF) != 0) {
          fprintf(stderr, "woi-ref field: REF must not apply off colony\n");
          return 1;
        }
        units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }

      fprintf(stderr, "unit_units: WoI REF colony peels ok\n");
    }

    /* Best defender: Artillery preferred over Colonist on same tile. */
    {
      const int sol = units_find_type(&pool, "Soldiers");
      const int col = units_find_type(&pool, "Colonists");
      const int arty = units_find_type(&pool, "Artillery");
      if (sol < 0 || col < 0 || arty < 0) {
        fprintf(stderr, "phase2 best-def types missing\n");
        return 1;
      }
      const int atk = units_spawn_allow_stack(&pool, sol, 30, 30);
      const int weak = units_spawn_allow_stack(&pool, col, 31, 30);
      const int strong = units_spawn_allow_stack(&pool, arty, 31, 30);
      ColonizeUnit* au = units_get(&pool, atk);
      ColonizeUnit* wu = units_get(&pool, weak);
      ColonizeUnit* su = units_get(&pool, strong);
      if (!au || !wu || !su) {
        fprintf(stderr, "phase2 best-def spawn failed\n");
        return 1;
      }
      au->nation_id = 0;
      wu->nation_id = 1;
      su->nation_id = 1;
      pool.types[arty].attack = 4;
      pool.types[arty].defense = 8;
      pool.types[col].attack = 0;
      pool.types[col].defense = 1;
      const int best = units_best_defender_at(&pool, NULL, 31, 30, atk, atk);
      if (best != strong) {
        fprintf(stderr, "phase2 best-def want arty id=%d got %d\n", strong, best);
        return 1;
      }
      units_despawn(&pool, atk);
      units_despawn(&pool, weak);
      units_despawn(&pool, strong);
      fprintf(stderr, "unit_units: best-defender pick ok\n");
    }

    /* Capture-alive Colonists. */
    {
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.difficulty = 2;
      c1.player[0].control = 0;
      const int sol = units_find_type(&pool, "Soldiers");
      const int fc = units_find_type(&pool, "Colonists");
      if (sol < 0 || fc < 0) {
        fprintf(stderr, "phase2 capture types missing\n");
        return 1;
      }
      const int aid = units_spawn_allow_stack(&pool, sol, 40, 40);
      const int did = units_spawn_allow_stack(&pool, fc, 40, 40);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      if (!a || !d) {
        fprintf(stderr, "phase2 capture spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      d->nation_id = 1;
      pool.types[sol].attack = 8;
      pool.types[fc].attack = 0;
      pool.types[fc].defense = 1;
      if (!units_resolve_land_combat_ff(&pool, aid, did, NULL, &c1)) {
        fprintf(stderr, "phase2 capture combat should win\n");
        return 1;
      }
      d = units_get(&pool, did);
      if (!d || !d->active || d->nation_id != 0) {
        fprintf(stderr, "phase2 Colonists should be captured\n");
        return 1;
      }
      {
        int found = 0;
        for (int i = 0; i < pops.queue_count; ++i) {
          if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_CAPTURE) {
            found = 1;
            break;
          }
        }
        if (!found) {
          fprintf(stderr, "phase2 COLONISTCAPTURE popup missing\n");
          return 1;
        }
      }
      units_despawn(&pool, aid);
      units_despawn(&pool, did);
      fprintf(stderr, "unit_units: capture-alive ok\n");
    }

    /* Native win: Pioneer destroyed (not captured); Soldier demoted to Colonist. */
    {
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.difficulty = 2;
      c1.player[0].control = 0;
      const int pio = units_find_type(&pool, "Pioneers");
      const int sol = units_find_type(&pool, "Soldiers");
      const int col_ti = units_find_type(&pool, "Colonists");
      const int brave = units_find_type(&pool, "Braves");
      if (pio < 0 || sol < 0 || col_ti < 0 || brave < 0) {
        fprintf(stderr, "phase2 native-outcome types missing\n");
        return 1;
      }
      pool.types[brave].attack = 8;
      pool.types[brave].defense = 8;
      pool.types[pio].attack = 0;
      pool.types[pio].defense = 1;
      pool.types[sol].attack = 2;
      pool.types[sol].defense = 1;
      {
        const int aid = units_spawn_allow_stack(&pool, brave, 42, 42);
        const int did = units_spawn_allow_stack(&pool, pio, 42, 42);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "phase2 native-pioneer spawn failed\n");
          return 1;
        }
        a->nation_id = 4;
        d->nation_id = 0;
        if (!units_resolve_land_combat_ff(&pool, aid, did, NULL, &c1)) {
          fprintf(stderr, "phase2 native should beat pioneer\n");
          return 1;
        }
        d = units_get(&pool, did);
        if (d && d->active) {
          fprintf(stderr, "phase2 native win should destroy pioneer (not capture)\n");
          return 1;
        }
        units_despawn(&pool, aid);
      }
      {
        const int aid = units_spawn_allow_stack(&pool, brave, 43, 43);
        const int did = units_spawn_allow_stack(&pool, sol, 43, 43);
        ColonizeUnit* a = units_get(&pool, aid);
        ColonizeUnit* d = units_get(&pool, did);
        if (!a || !d) {
          fprintf(stderr, "phase2 native-soldier spawn failed\n");
          return 1;
        }
        a->nation_id = 4;
        d->nation_id = 0;
        if (!units_resolve_land_combat_ff(&pool, aid, did, NULL, &c1)) {
          fprintf(stderr, "phase2 native should beat soldier\n");
          return 1;
        }
        d = units_get(&pool, did);
        if (!d || !d->active || d->nation_id != 0 || d->type_index != col_ti) {
          fprintf(
            stderr,
            "phase2 soldier should demote to Colonist same nation (active=%d nat=%d ti=%d want %d)\n",
            d && d->active,
            d ? d->nation_id : -1,
            d ? d->type_index : -1,
            col_ti
          );
          return 1;
        }
        units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }
      fprintf(stderr, "unit_units: native destroy-pioneer / demote-soldier ok\n");
    }

    /* Naval damage-not-always-sink: weaker ship escapes damaged when close. */
    {
      const int frig = units_find_type(&pool, "Frigate");
      const int car = units_find_type(&pool, "Caravel");
      if (frig < 0 || car < 0) {
        fprintf(stderr, "phase2 naval types missing\n");
        return 1;
      }
      pool.types[frig].attack = 6;
      pool.types[frig].defense = 6;
      pool.types[car].attack = 5;
      pool.types[car].defense = 5;
      const int aid = units_spawn_allow_stack(&pool, frig, 2, 2);
      const int did = units_spawn_allow_stack(&pool, car, 3, 2);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      if (!a || !d) {
        fprintf(stderr, "phase2 naval spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      d->nation_id = 1;
      if (!units_resolve_naval_combat_ff(&pool, aid, did, NULL, NULL)) {
        fprintf(stderr, "phase2 naval should win\n");
        return 1;
      }
      d = units_get(&pool, did);
      if (!d || !d->active || (d->col1_unknown15 & 0x80u) == 0) {
        fprintf(stderr, "phase2 weaker ship should survive damaged\n");
        return 1;
      }
      units_despawn(&pool, aid);
      units_despawn(&pool, did);
      fprintf(stderr, "unit_units: naval damage-escape ok\n");
    }

    /* Outcome popups enqueued for human side. */
    {
      ai_popup_clear(&pops);
      ColonizeMsgCatalog game_txt;
      assets_msg_init(&game_txt);
      if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
        fprintf(stderr, "phase2 popup GAME.TXT load failed\n");
        return 1;
      }
      units_set_combat_popups(&pops, &game_txt);
      const int sol = units_find_type(&pool, "Soldiers");
      if (sol < 0) {
        fprintf(stderr, "phase2 popup types missing\n");
        assets_msg_free(&game_txt);
        return 1;
      }
      const int aid = units_spawn_allow_stack(&pool, sol, 50, 50);
      const int did = units_spawn_allow_stack(&pool, sol, 50, 50);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      a->nation_id = 0;
      d->nation_id = 1;
      pool.types[sol].attack = 8;
      pool.types[sol].defense = 1;
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      if (!units_resolve_land_combat_ff(&pool, aid, did, NULL, &c1)) {
        fprintf(stderr, "phase2 popup combat should win\n");
        assets_msg_free(&game_txt);
        return 1;
      }
      int found = 0;
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_EUROPE) {
          found = 1;
          /* @EUROPEWIN: "{atk} defeat {def unit} near {place}!" */
          if (!strstr(pops.queue[i].body, "defeat") || !strstr(pops.queue[i].body, "English") ||
              !strstr(pops.queue[i].body, "French") || !strstr(pops.queue[i].body, "Soldiers") ||
              !strstr(pops.queue[i].body, "Wilderness")) {
            fprintf(
              stderr,
              "phase2 EUROPEWIN body tokens wrong: [%s]\n",
              pops.queue[i].body
            );
            assets_msg_free(&game_txt);
            return 1;
          }
          break;
        }
      }
      if (!found) {
        fprintf(stderr, "phase2 expected EUROPEWIN popup enqueue (queue=%d)\n", pops.queue_count);
        assets_msg_free(&game_txt);
        return 1;
      }
      units_despawn(&pool, aid);
      if (units_get(&pool, did) && units_get(&pool, did)->active) {
        units_despawn(&pool, did);
      }
      units_set_combat_popups(&pops, NULL);
      assets_msg_free(&game_txt);
      fprintf(stderr, "unit_units: combat outcome popup enqueue ok\n");
    }

    /*
     * Village Attack empty tile: FUN_5fef_1b0e temp Brave + population drain.
     * pop>=2 survives (pop--); pop<2 destroys. Not nearby-Brave pull.
     */
    {
      const int soldier = units_find_type(&pool, "Soldiers");
      const int brave = units_find_type(&pool, "Braves");
      if (soldier < 0 || brave < 0) {
        fprintf(stderr, "village-temp types missing\n");
        return 1;
      }
      int vx = -1, vy = -1;
      for (int y = 2; y < (int)map.height - 3 && vx < 0; ++y) {
        for (int x = 2; x < (int)map.width - 3 && vx < 0; ++x) {
          if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y)) {
            vx = x + 1;
            vy = y;
          }
        }
      }
      if (vx < 0) {
        fprintf(stderr, "village-temp no land pair\n");
        return 1;
      }
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.head.tribe_count = 1;
      c1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
      if (!c1.tribe) {
        fprintf(stderr, "village-temp tribe alloc\n");
        return 1;
      }
      c1.tribe[0].x = (uint8_t)vx;
      c1.tribe[0].y = (uint8_t)vy;
      c1.tribe[0].nation_id = 4;
      c1.tribe[0].population = 3;
      c1.player[0].control = 0;
      c1.head.game_options.combat_analysis = 1;

      const int aid = units_spawn(&pool, soldier, vx - 1, vy);
      ColonizeUnit* a = units_get(&pool, aid);
      if (!a) {
        free(c1.tribe);
        fprintf(stderr, "village-temp soldier spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      a->moves_left = 3;
      pool.types[soldier].attack = 8;
      pool.types[brave].attack = 1;
      pool.types[brave].defense = 1;

      units_set_ff_col1(&c1);
      units_set_combat_human_nation(0);
      units_set_native_fallout_context(&c1, &map, -1);
      units_set_occupancy_map(&map);

      /* First empty-village attack: temp Brave, pop 3→2, dwelling remains. */
      if (!units_try_move(&pool, aid, &map, vx, vy, NULL, NULL)) {
        fprintf(
          stderr,
          "village-temp try_move should engage (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        free(c1.tribe);
        return 1;
      }
      if (units_last_combat_outcome() <= 0) {
        fprintf(stderr, "village-temp expected combat win\n");
        free(c1.tribe);
        return 1;
      }
      if (c1.head.tribe_count != 1 || c1.tribe[0].population != 2) {
        fprintf(
          stderr,
          "village-temp after win want pop=2 count=1 got pop=%u count=%u\n",
          c1.tribe[0].population,
          c1.head.tribe_count
        );
        free(c1.tribe);
        return 1;
      }

      /* Drain to pop=1 then destroy on next temp fight. */
      a = units_get(&pool, aid);
      if (!a || !a->active) {
        free(c1.tribe);
        fprintf(stderr, "village-temp soldier should survive\n");
        return 1;
      }
      /* DOS: fight from adjacent and stay — do not enter the village tile. */
      if (a->x != vx - 1 || a->y != vy) {
        fprintf(
          stderr,
          "village-temp after win should stay at (%d,%d) got (%d,%d)\n",
          vx - 1,
          vy,
          a->x,
          a->y
        );
        free(c1.tribe);
        return 1;
      }
      a->moves_left = 3;
      if (!units_try_move(&pool, aid, &map, vx, vy, NULL, NULL) ||
          units_last_combat_outcome() <= 0) {
        fprintf(stderr, "village-temp second attack failed\n");
        free(c1.tribe);
        return 1;
      }
      if (c1.tribe[0].population != 1) {
        fprintf(stderr, "village-temp want pop=1 got %u\n", c1.tribe[0].population);
        free(c1.tribe);
        return 1;
      }
      a = units_get(&pool, aid);
      if (!a || a->x != vx - 1 || a->y != vy) {
        fprintf(stderr, "village-temp second attack should leave soldier adjacent\n");
        free(c1.tribe);
        return 1;
      }
      a->moves_left = 3;
      if (!units_try_move(&pool, aid, &map, vx, vy, NULL, NULL) ||
          units_last_combat_outcome() <= 0) {
        fprintf(stderr, "village-temp destroy attack failed\n");
        free(c1.tribe);
        return 1;
      }
      if (c1.head.tribe_count != 0) {
        fprintf(stderr, "village-temp pop<2 should destroy dwelling\n");
        free(c1.tribe);
        return 1;
      }

      units_despawn(&pool, aid);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      units_set_native_fallout_context(NULL, NULL, -1);
      free(c1.tribe);
      fprintf(stderr, "unit_units: village temp Brave + pop drain ok\n");
    }

    /* Treasure ransom Accept credits gold; Refuse does not. */
    {
      ai_popup_clear(&pops);
      units_set_combat_popups(&pops, NULL);
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      c1.nation[0].gold = 10;
      int use_ti = units_find_type(&pool, "Treasure");
      const int sol = units_find_type(&pool, "Soldiers");
      if (use_ti < 0 || sol < 0) {
        fprintf(stderr, "ransom types missing\n");
        return 1;
      }
      const int aid = units_spawn_allow_stack(&pool, sol, 55, 55);
      const int did = units_spawn_allow_stack(&pool, use_ti, 55, 55);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      a->nation_id = 0;
      d->nation_id = 1;
      d->hold_goods_amount[0] = 100 & 0xff;
      d->hold_goods_amount[1] = 0;
      pool.types[sol].attack = 8;
      pool.types[use_ti].defense = 1;
      if (!units_resolve_land_combat_ff(&pool, aid, did, NULL, &c1)) {
        fprintf(stderr, "ransom combat should win\n");
        return 1;
      }
      if (c1.nation[0].gold != 10) {
        fprintf(stderr, "ransom should defer gold until Accept (got %u)\n", c1.nation[0].gold);
        return 1;
      }
      int ransom_q = -1;
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_RANSOM) {
          ransom_q = i;
          break;
        }
      }
      if (ransom_q < 0) {
        fprintf(stderr, "ransom CHOICE not enqueued\n");
        return 1;
      }
      /* Simulate Refuse. */
      pops.has_result = true;
      pops.result_tag = AI_POPUP_TAG_COMBAT_RANSOM;
      pops.result_nation_a = 0;
      pops.result_payload = 100;
      pops.result_choice_id = 0;
      pops.result_cancelled = false;
      (void)units_combat_apply_ransom_popup(&c1, &pops);
      if (c1.nation[0].gold != 10) {
        fprintf(stderr, "ransom Refuse should not credit\n");
        return 1;
      }
      pops.result_choice_id = 1;
      (void)units_combat_apply_ransom_popup(&c1, &pops);
      if (c1.nation[0].gold != 110) {
        fprintf(stderr, "ransom Accept want gold 110 got %u\n", c1.nation[0].gold);
        return 1;
      }
      units_despawn(&pool, aid);
      fprintf(stderr, "unit_units: treasure ransom Accept/Refuse ok\n");
    }

    /* Colony capture notify @CAPTURED*. */
    {
      ai_popup_clear(&pops);
      units_set_combat_popups(&pops, NULL);
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      ColonizeColony col;
      memset(&col, 0, sizeof(col));
      col.active = true;
      col.nation_id = 1;
      snprintf(col.name, sizeof(col.name), "Jamestown");
      col.stock[COLONIZE_CARGO_FOOD] = 40;
      units_combat_notify_colony_captured(&c1, &col, 0, 40);
      int found = 0;
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_COLONY) {
          found = 1;
          break;
        }
      }
      if (!found) {
        fprintf(stderr, "colony CAPTURED popup missing\n");
        return 1;
      }
      fprintf(stderr, "unit_units: colony CAPTURED popup ok\n");
    }

    /* Privateer seizure tag. */
    {
      ai_popup_clear(&pops);
      units_set_combat_popups(&pops, NULL);
      const int priv = units_find_type(&pool, "Privateer");
      const int car = units_find_type(&pool, "Caravel");
      if (priv < 0 || car < 0) {
        fprintf(stderr, "seizure types missing\n");
        return 1;
      }
      pool.types[priv].attack = 16;
      pool.types[priv].defense = 8;
      pool.types[car].attack = 2;
      pool.types[car].defense = 2;
      const int aid = units_spawn_allow_stack(&pool, priv, 4, 4);
      const int did = units_spawn_allow_stack(&pool, car, 5, 4);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      a->nation_id = 0;
      d->nation_id = 1;
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      if (!units_resolve_naval_combat_ff(&pool, aid, did, NULL, &c1)) {
        fprintf(stderr, "seizure naval should win\n");
        return 1;
      }
      int found = 0;
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_SEIZURE) {
          found = 1;
          break;
        }
      }
      if (!found) {
        fprintf(stderr, "SEIZURE popup missing (queue=%d)\n", pops.queue_count);
        return 1;
      }
      units_despawn(&pool, aid);
      if (units_get(&pool, did) && units_get(&pool, did)->active) {
        units_despawn(&pool, did);
      }
      fprintf(stderr, "unit_units: privateer SEIZURE popup ok\n");
    }

    /* Fort fire: miss → MP slow only; hit close → bit7 damage; Drydock repairs. */
    {
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Fort");
      snprintf(colonies.building_types[3].name, sizeof(colonies.building_types[3].name), "Drydock");
      colonies.building_type_count = 4;
      int cx = 1, cy = 1, wx = 2, wy = 1;
      for (int y = 1; y < map.height - 1; ++y) {
        for (int x = 1; x < map.width - 1; ++x) {
          if (!map_tile_is_land(&map, x, y)) {
            continue;
          }
          static const int dx8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
          static const int dy8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
          for (int d = 0; d < 8; ++d) {
            if (map_tile_is_water(&map, x + dx8[d], y + dy8[d])) {
              cx = x;
              cy = y;
              wx = x + dx8[d];
              wy = y + dy8[d];
              goto found_coast_bit7;
            }
          }
        }
      }
    found_coast_bit7:
      ColonizeColony* col = &colonies.colonies[0];
      col->active = true;
      col->nation_id = 0;
      col->x = cx;
      col->y = cy;
      col->has_building[1] = true;
      colonies.colony_count = 1;
      const int car = units_find_type(&pool, "Caravel");
      const int sid_miss = units_spawn_allow_stack(&pool, car, wx, wy);
      ColonizeUnit* ship = units_get(&pool, sid_miss);
      ship->nation_id = 1;
      ship->moves_left = 4;
      ship->col1_unknown15 = 0;
      ship->turns_worked = 0;
      /* Fort atk=4 (tier1, 0 arty); ship defense 99 → fort miss, MP drain only. */
      pool.types[car].attack = 2;
      pool.types[car].defense = 99;
      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        c1.head.founding_father[i] = -1;
      }
      ai_diplo_declare_war(&c1, 0, 1);
      if (!ai_diplo_at_war(&c1, 0, 1)) {
        fprintf(stderr, "fort bit7 smoke: declare_war failed\n");
        return 1;
      }
      char st[64];
      (void)units_coastal_fort_fire_pulse(
        &pool, &colonies, &map, &c1, NULL, -1, st, sizeof(st)
      );
      ship = units_get(&pool, sid_miss);
      if (!ship || !ship->active) {
        fprintf(stderr, "fort miss should leave ship alive\n");
        return 1;
      }
      if ((ship->col1_unknown15 & 0x80u) != 0) {
        fprintf(stderr, "fort miss must not set damaged bit7\n");
        return 1;
      }
      if (ship->moves_left != 0) {
        fprintf(stderr, "fort miss should drain moves_left (got %d)\n", ship->moves_left);
        return 1;
      }
      units_despawn(&pool, sid_miss);

      /* Close fort hit: defense*2 > atk and atk >= defense (null rng) → bit7. */
      const int sid_hit = units_spawn_allow_stack(&pool, car, wx, wy);
      ship = units_get(&pool, sid_hit);
      ship->nation_id = 1;
      ship->moves_left = 4;
      ship->col1_unknown15 = 0;
      ship->turns_worked = 0;
      pool.types[car].defense = 3; /* fort atk 4 wins; 3*2 > 4 → damage-not-sink */
      (void)units_coastal_fort_fire_pulse(
        &pool, &colonies, &map, &c1, NULL, -1, st, sizeof(st)
      );
      ship = units_get(&pool, sid_hit);
      if (!ship || !ship->active) {
        fprintf(stderr, "close fort hit should damage not sink\n");
        return 1;
      }
      if ((ship->col1_unknown15 & 0x80u) == 0) {
        fprintf(stderr, "close fort hit must set damaged bit7\n");
        return 1;
      }
      if (ship->turns_worked < 3) {
        fprintf(stderr, "combat damage should mark past construction thresh\n");
        return 1;
      }
      /* Ship-build tick must not clear combat damage. */
      (void)units_tick_ship_build_ready(
        &pool, &colonies, 1, -1, st, sizeof(st), NULL
      );
      ship = units_get(&pool, sid_hit);
      if (!ship || (ship->col1_unknown15 & 0x80u) == 0) {
        fprintf(stderr, "ship-build must leave combat bit7 set\n");
        return 1;
      }
      /* Drydock at colony repairs finished damaged ship on dock. */
      ship->x = cx;
      ship->y = cy;
      ship->nation_id = 0;
      col->has_building[3] = true;
      const int repaired = units_tick_drydock_repair(
        &pool, &colonies, 0, 0, st, sizeof(st), NULL, NULL
      );
      ship = units_get(&pool, sid_hit);
      if (repaired != 1 || !ship || (ship->col1_unknown15 & 0x80u) != 0) {
        fprintf(stderr, "Drydock should clear combat bit7 (repaired=%d)\n", repaired);
        return 1;
      }
      units_despawn(&pool, sid_hit);
      fprintf(stderr, "unit_units: fort bit7 + Drydock repair ok\n");
    }

    units_set_combat_popups(NULL, NULL);
    units_set_combat_human_nation(-1);
  }

  fprintf(
    stderr,
    "units tests ok (types=%d pioneer@%d,%d caravel_icon=%d edge=%d,%d)\n",
    pool.type_count,
    unload_x,
    unload_y,
    ship_icon,
    edge_x,
    edge_y
  );

  ss_free(&icons);
  map_free(&map);
  assets_msg_free(&names);
  diag_shutdown();
  return 0;
}
