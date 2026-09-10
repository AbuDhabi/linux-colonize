#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_production.h"
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
#include "core/unit_stack.h"
#include "core/units.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * bugs.md: equipping an existing unit keeps its tier. A Continental Army given
 * horses becomes Continental Cavalry (DOS DS:0x30e files type 9 under @JOB 21
 * Soldier and type 7 under @JOB 23 Dragoon — the same pair the WoI promotion
 * and the combat demote table use), never the plain Veteran Dragoons the flat
 * @JOB->@UNIT table name would give. Also covers the spawn kit: "Cont. Cav."
 * matched neither "Dragoon" nor "Cavalry", so it used to spawn with no
 * muskets and no horses at all.
 */
static int unit_continental_equip_tier(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "cont_equip: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "cont_equip: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int cont_army = units_find_type(&pool, "Cont. Army");
  const int cont_cav = units_find_type(&pool, "Cont. Cav.");
  const int colonists = units_find_type(&pool, "Colonists");
  const int regulars = units_find_type(&pool, "Regulars");
  if (cont_army < 0 || cont_cav < 0 || colonists < 0 || regulars < 0) {
    fprintf(stderr, "cont_equip: missing WoI @UNIT types\n");
    assets_msg_free(&names);
    return 1;
  }
  struct {
    int type;
    int role;
    const char* want;
  } cases[] = {
    {cont_army, COLONIZE_EJECT_DRAGOON, "Cont. Cav."},
    {cont_army, COLONIZE_EJECT_SOLDIER, "Cont. Army"},
    {cont_army, COLONIZE_EJECT_COLONIST, "Colonists"},
    {cont_cav, COLONIZE_EJECT_SOLDIER, "Cont. Army"},
    {cont_cav, COLONIZE_EJECT_DRAGOON, "Cont. Cav."},
    {colonists, COLONIZE_EJECT_DRAGOON, "Dragoons"},
    {colonists, COLONIZE_EJECT_SOLDIER, "Soldiers"},
    {regulars, COLONIZE_EJECT_DRAGOON, "Cavalry"},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    const char* got = units_equip_role_type_name(&pool, cases[i].type, cases[i].role);
    if (!got || strcmp(got, cases[i].want) != 0) {
      fprintf(stderr, "cont_equip: %s + role %d -> %s (want %s)\n",
              pool.types[cases[i].type].name, cases[i].role, got ? got : "(null)",
              cases[i].want);
      assets_msg_free(&names);
      return 1;
    }
  }
  const int cav_id = units_spawn_allow_stack(&pool, cont_cav, 3, 3);
  const ColonizeUnit* cav = units_get_const(&pool, cav_id);
  if (!cav || cav->muskets != UNITS_EQUIP_MUSKETS || cav->horses != UNITS_EQUIP_HORSES) {
    fprintf(stderr, "cont_equip: Cont. Cav. spawn kit muskets=%d horses=%d\n",
            cav ? cav->muskets : -1, cav ? cav->horses : -1);
    assets_msg_free(&names);
    return 1;
  }
  const int army_id = units_spawn_allow_stack(&pool, cont_army, 4, 3);
  const ColonizeUnit* army = units_get_const(&pool, army_id);
  if (!army || army->muskets != UNITS_EQUIP_MUSKETS || army->horses != 0) {
    fprintf(stderr, "cont_equip: Cont. Army spawn kit muskets=%d horses=%d\n",
            army ? army->muskets : -1, army ? army->horses : -1);
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  return 0;
}

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
  memset(&map, 0, sizeof(map)); /* map_alloc frees the old buffers first */
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
  colonies_set_occupancy_map(NULL);
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
  u->moves_left = 1 * UNITS_MP_PER_TILE;
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
    u->moves_left = 1 * UNITS_MP_PER_TILE;
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
  /* bugs.md 284: @DEFOREST is a dead GAME.TXT section (tag absent from
   * VICEROY.EXE) — a chop shows @CLEARCUT alone, never a second popup. */
  for (int i = 0; i < pops.queue_count; ++i) {
    if (strstr(pops.queue[i].body, "Deforestation") != NULL ||
        strstr(pops.queue[i].body, "deforestation") != NULL) {
      fprintf(stderr, "clearcut: unexpected DEFOREST popup body='%s'\n", pops.queue[i].body);
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
  }

  assets_msg_free(&game_txt);
  map_free(&map);
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: CLEARCUT chrome ok (no DEFOREST double)\n");
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
  memset(&map, 0, sizeof(map)); /* map_alloc frees the old buffers first */
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
  u->moves_left = 1 * UNITS_MP_PER_TILE;
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
  /*
   * Non-Hardy road-turns threshold is the real DS:0x2f78 terrain byte + 2
   * (5 turns for this test's terrain class, since the live 2026-08-20
   * capture landed pioneer_threshold[2]=3) — not a single-tick "terr_cost
   * 1" approximation any more, so drive the tick loop to completion rather
   * than assuming one call finishes the road.
   */
  for (int tick = 0; tick < 10 && u->orders == UNITS_ORDER_BUILD_ROAD; ++tick) {
    u->moves_left = 1 * UNITS_MP_PER_TILE;
    if (!units_pioneer_work_tick(
          &pool, pid, &map, msg, sizeof(msg), NULL, &pops, &game_txt
        )) {
      fprintf(stderr, "usedup: work tick failed (%s)\n", msg);
      assets_msg_free(&game_txt);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
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
  colonies_set_occupancy_map(NULL);
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
  colonies_set_occupancy_map(NULL);
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
  /* bugs.md: full warehouse no longer blocks — the hold lands anyway and
   * the flag informs (excess spoils next turn). */
  if (moved != 20 || !full) {
    fprintf(stderr, "whfull: want moved=20 full=1 got moved=%d full=%d\n", moved, (int)full);
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

/*
 * FUN_521d_5b66 case 8 (= FUN_479b_01a6), the three arms the port was thin
 * on before 2026-09-06e:
 *   1. the lumber grant needs a same-nation colony within DOS distance 4
 *      (FUN_281f_0614 + DS:0x8db8 < 4) — a colony 5 away gets nothing;
 *   2. with a Lumber Mill the scale is the terrain +8 byte PLUS the colony
 *      tile's layer2 road|city bump, i.e. (byte + 1) * 20;
 *   3. LAB_479b_043b: finishing a clear on tribal land angers the tribe
 *      (base 5 + difficulty for a human, x2 within distance 3, +base again
 *      within 2), while an AI nation that can cover price + price/2 buys the
 *      tile through FUN_479b_00ca instead and takes no anger.
 */
static int unit_pioneer_case8_tail(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "case8: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "case8: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int pioneer = units_find_type(&pool, "Pioneers");
  if (pioneer < 0) {
    fprintf(stderr, "case8: no Pioneers type\n");
    assets_msg_free(&names);
    return 1;
  }

  int rc = 1;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    fprintf(stderr, "case8: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 2; /* plains */
  }

  col1.head.tribe_count = 1;
  col1.head.difficulty = 0;
  /* col1_save_init: unclaimed FFs are -1. Ownership is the per-nation bitmask
   * (smell audit #83), so a zeroed head no longer grants anything; keep the
   * -1 fill so the first-claimer record matches a real save. */
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    col1.head.founding_father[i] = -1;
  }
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    fprintf(stderr, "case8: tribe alloc failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  col1.tribe[0].nation_id = 4; /* indian[0] */
  col1.tribe[0].x = 6;
  col1.tribe[0].y = 6;
  col1.indian[0].tech = 0; /* tech tier 1 -> homeland radius 1 */
  units_set_native_fallout_context(&col1, &map, -1);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "case8: colonies_load_buildings failed\n");
    goto done;
  }
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->warehouse_level = 0;
  snprintf(col->name, sizeof(col->name), "Timber");
  colonies.colony_count = 1;

  const int fx = 5;
  const int fy = 6; /* DOS distance 1 from the village at (6,6) */
  const int terr_scale = map_dos_terr_lumber_reward_byte(map_dos_terr_class_at(&map, fx, fy));

  /* --- arm 1 + 2: colony out of range, then a Lumber Mill colony in range. */
  for (int pass = 0; pass < 2; ++pass) {
    map.terrain[fy * map.width + fx] = 10; /* mixed forest again */
    col->x = pass == 0 ? (uint8_t)(fx + 5) : (uint8_t)(fx - 1);
    col->y = (uint8_t)fy;
    col->stock[COLONIZE_CARGO_LUMBER] = 0;
    const int mill = colonies_find_building(&colonies, "Lumber Mill");
    if (mill < 0) {
      fprintf(stderr, "case8: no Lumber Mill building type\n");
      goto done;
    }
    col->has_building[mill] = pass == 1;

    const int pid = units_spawn(&pool, pioneer, fx, fy);
    ColonizeUnit* u = units_get(&pool, pid);
    if (!u) {
      fprintf(stderr, "case8: pioneer spawn failed\n");
      goto done;
    }
    u->nation_id = 0;
    u->tools = 100;
    u->profession = UNITS_JOB_NONE;
    u->orders = UNITS_ORDER_CLEAR_PLOW;
    u->turns_worked = 0;
    int guard = 0;
    while (u->orders == UNITS_ORDER_CLEAR_PLOW && guard++ < 24) {
      u->moves_left = 1 * UNITS_MP_PER_TILE;
      (void)units_pioneer_work_tick(&pool, pid, &map, NULL, 0, &colonies, NULL, NULL);
    }
    units_despawn(&pool, pid);
    const int got = (int)col->stock[COLONIZE_CARGO_LUMBER];
    if (pass == 0) {
      if (got != 0) {
        fprintf(stderr, "case8: colony 5 away must get no lumber, got %d\n", got);
        goto done;
      }
    } else {
      int want = (terr_scale + 1) * 20;
      const int cap = colonies_warehouse_capacity(&colonies, col, COLONIZE_CARGO_LUMBER);
      if (want > cap) {
        want = cap;
      }
      if (got != want) {
        fprintf(stderr, "case8: mill lumber want %d got %d\n", want, got);
        goto done;
      }
    }
  }

  /* --- arm 3a: human nation on tribal land -> alarm 5 * 3 (distance 1). */
  col1.player[0].control = 0; /* human */
  col1.indian[0].alarm_by_player[0] = 0;
  map.terrain[fy * map.width + fx] = 10;
  {
    const int pid = units_spawn(&pool, pioneer, fx, fy);
    ColonizeUnit* u = units_get(&pool, pid);
    if (!u) {
      fprintf(stderr, "case8: human pioneer spawn failed\n");
      goto done;
    }
    u->nation_id = 0;
    u->tools = 100;
    u->profession = UNITS_JOB_NONE;
    u->orders = UNITS_ORDER_CLEAR_PLOW;
    u->turns_worked = 0;
    int guard = 0;
    while (u->orders == UNITS_ORDER_CLEAR_PLOW && guard++ < 24) {
      u->moves_left = 1 * UNITS_MP_PER_TILE;
      (void)units_pioneer_work_tick(&pool, pid, &map, NULL, 0, &colonies, NULL, NULL);
    }
    units_despawn(&pool, pid);
    if (col1.indian[0].alarm_by_player[0] != 15) {
      fprintf(
        stderr,
        "case8: human clear alarm want 15 got %d\n",
        (int)col1.indian[0].alarm_by_player[0]
      );
      goto done;
    }
  }

  /* --- arm 3b: AI nation with gold buys the land instead of angering it. */
  col1.player[1].control = 1; /* AI */
  col1.indian[0].alarm_by_player[1] = 0;
  col1.indian[0].lands_bought = 0;
  col1.nation[1].gold = 5000;
  map.terrain[fy * map.width + fx] = 10;
  {
    const int price = colonies_indian_land_purchase_gold(&col1, &map, fx, fy, 1);
    if (price <= 0) {
      fprintf(stderr, "case8: AI land price should be positive, got %d\n", price);
      goto done;
    }
    const int pid = units_spawn(&pool, pioneer, fx, fy);
    ColonizeUnit* u = units_get(&pool, pid);
    if (!u) {
      fprintf(stderr, "case8: AI pioneer spawn failed\n");
      goto done;
    }
    u->nation_id = 1;
    u->tools = 100;
    u->profession = UNITS_JOB_NONE;
    u->orders = UNITS_ORDER_CLEAR_PLOW;
    u->turns_worked = 0;
    int guard = 0;
    while (u->orders == UNITS_ORDER_CLEAR_PLOW && guard++ < 24) {
      u->moves_left = 1 * UNITS_MP_PER_TILE;
      (void)units_pioneer_work_tick(&pool, pid, &map, NULL, 0, &colonies, NULL, NULL);
    }
    units_despawn(&pool, pid);
    if (col1.indian[0].alarm_by_player[1] != 0) {
      fprintf(
        stderr,
        "case8: AI buyer must not anger the tribe, alarm %d\n",
        (int)col1.indian[0].alarm_by_player[1]
      );
      goto done;
    }
    if (col1.nation[1].gold != (uint32_t)(5000 - price)) {
      fprintf(
        stderr,
        "case8: AI gold want %d got %u\n",
        5000 - price,
        col1.nation[1].gold
      );
      goto done;
    }
    if (col1.indian[0].lands_bought != 1) {
      fprintf(stderr, "case8: lands_bought want 1 got %d\n", (int)col1.indian[0].lands_bought);
      goto done;
    }
    if ((map.layer2[fy * map.width + fx] & MAP_LAYER2_PURCHASED) == 0) {
      fprintf(stderr, "case8: purchased bit not stamped\n");
      goto done;
    }
  }

  rc = 0;
  fprintf(stderr, "unit_units: 5b66 case-8 lumber radius + tribal-land tail ok\n");
done:
  units_set_native_fallout_context(NULL, NULL, -1);
  free(col1.tribe);
  map_free(&map);
  assets_msg_free(&names);
  return rc;
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
  memset(&map, 0, sizeof(map)); /* map_alloc frees the old buffers first */
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
    cu->moves_left = 1 * UNITS_MP_PER_TILE;
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
    pu->moves_left = 1 * UNITS_MP_PER_TILE;
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
    pu->moves_left = 1 * UNITS_MP_PER_TILE;
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
  if (id >= SOUND_EVENT_ID_BASE) {
    return; /* event SFX (attack fire, win) bypass the BGM scheduler in sound.c */
  }
  g_music_sting_play_calls++;
  g_music_sting_last_id = id;
  g_music_sting_active_id = id; /* mirrors sound.c: playing sets the active id */
}

static int unit_music_sting_active_id_mock(void) {
  return g_music_sting_active_id;
}

static int g_event_sfx_calls = 0;

static void unit_event_sfx_play_mock(int id) {
  if (id >= SOUND_EVENT_ID_BASE) {
    g_event_sfx_calls++;
  }
}

static int unit_event_sfx_active_id_mock(void) {
  return -1;
}

/*
 * bugs.md: cannon fire landed on top of the prices-fall / immigration popups.
 * DOS FUN_5fef_1b0e only plays the fire/win event sounds when its `param_4`
 * visible flag is set — FUN_465b_0000 passes 1 when either side is human
 * (`0x543f == 0`), the AI move scorer at 521d:52aa passes 0. Combat between
 * two AI nations must therefore be silent; human-involved combat must not.
 */
static int unit_combat_sfx_visibility(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Soldiers");
  pool.types[0].attack = 99;
  pool.types[0].defense = 99;
  pool.types[0].movement = 1;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Soldiers");
  pool.types[1].attack = 0;
  pool.types[1].defense = 0;
  pool.types[1].movement = 1;

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[0].control = 0; /* human */
  col1.player[1].control = 1; /* AI */
  col1.player[2].control = 1; /* AI */

  units_set_combat_music_hooks(unit_event_sfx_play_mock, unit_event_sfx_active_id_mock);
  units_set_ff_col1(&col1);

  int rc = 0;
  /* AI vs AI: silent. */
  g_event_sfx_calls = 0;
  {
    const int aid = units_spawn_allow_stack(&pool, 0, 5, 5);
    const int did = units_spawn_allow_stack(&pool, 1, 6, 5);
    units_get(&pool, aid)->nation_id = 1;
    units_get(&pool, did)->nation_id = 2;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 7);
    units_resolve_land_combat_ff(&pool, aid, did, &rng, &col1);
    if (g_event_sfx_calls != 0) {
      fprintf(stderr, "combat_sfx: AI-vs-AI played %d event sounds (want 0)\n",
              g_event_sfx_calls);
      rc = 1;
    }
  }
  /* Human attacker: audible. */
  if (rc == 0) {
    g_event_sfx_calls = 0;
    const int aid = units_spawn_allow_stack(&pool, 0, 5, 6);
    const int did = units_spawn_allow_stack(&pool, 1, 6, 6);
    units_get(&pool, aid)->nation_id = 0;
    units_get(&pool, did)->nation_id = 1;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 8);
    units_resolve_land_combat_ff(&pool, aid, did, &rng, &col1);
    if (g_event_sfx_calls == 0) {
      fprintf(stderr, "combat_sfx: human attack must stay audible\n");
      rc = 1;
    }
  }
  /* Human defender: audible. */
  if (rc == 0) {
    g_event_sfx_calls = 0;
    const int aid = units_spawn_allow_stack(&pool, 0, 5, 7);
    const int did = units_spawn_allow_stack(&pool, 1, 6, 7);
    units_get(&pool, aid)->nation_id = 2;
    units_get(&pool, did)->nation_id = 0;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 9);
    units_resolve_land_combat_ff(&pool, aid, did, &rng, &col1);
    if (g_event_sfx_calls == 0) {
      fprintf(stderr, "combat_sfx: human defence must stay audible\n");
      rc = 1;
    }
  }

  units_set_ff_col1(NULL);
  units_set_combat_music_hooks(NULL, NULL);
  if (rc == 0) {
    fprintf(stderr, "unit_units: combat SFX visibility gate ok\n");
  }
  return rc;
}

/*
 * bugs.md: "I can't move a ship onto a sea lane either way." DOS
 * FUN_4720_015c (~76048) only raises reason 5 when the ship is ALREADY on a
 * high-seas tile and steps further east without a Go To / Trade Route order;
 * entering the lane from ordinary ocean is always legal, and a Go To may
 * target a lane tile (which then sails the ship to Europe, game_loop.c).
 */
static int unit_sea_lane_entry(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Caravel");
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "sea_lane: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  /* Lanes sit INSIDE the playable board, as on AMER2 (west lane = column 1,
   * east lane = columns 51..56; the outer rim, column 0 / w-1, is plain ocean
   * a unit may never occupy — DOS FUN_137f_000a, bugs.md 435). */
  for (int y = 0; y < 8; ++y) {
    map.terrain[y * 8 + 5] = 26; /* high seas / sea lane */
    map.terrain[y * 8 + 6] = 26;
  }

  const int id = units_spawn_allow_stack(&pool, 0, 4, 3);
  ColonizeUnit* u = units_get(&pool, id);
  if (!u) {
    map_free(&map);
    fprintf(stderr, "sea_lane: spawn failed\n");
    return 1;
  }
  u->nation_id = 0;
  u->moves_left = 4 * UNITS_MP_PER_TILE;

  int rc = 0;
  if (!units_can_enter(&pool, u->type_index, &map, 5, 3, id, NULL)) {
    fprintf(stderr, "sea_lane: ocean->lane must be allowed\n");
    rc = 1;
  }
  if (rc == 0 && !units_set_goto(&pool, id, &map, 5, 3, NULL)) {
    fprintf(stderr, "sea_lane: Go To onto a lane tile must be accepted\n");
    rc = 1;
  }
  units_clear_orders(&pool, id);
  u = units_get(&pool, id);
  u->x = 5;
  u->y = 3;
  u->orders = UNITS_ORDER_NONE;
  if (rc == 0 && units_can_enter(&pool, u->type_index, &map, 6, 3, id, NULL)) {
    fprintf(stderr, "sea_lane: lane->east without a sail order must be denied\n");
    rc = 1;
  }
  if (rc == 0 && units_last_enter_reason() != COLONIZE_ENTER_BLOCKED_HS_SAIL) {
    fprintf(stderr, "sea_lane: lane->east deny should be reason 5\n");
    rc = 1;
  }
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = 6;
  u->goto_y = 3;
  if (rc == 0 && !units_can_enter(&pool, u->type_index, &map, 6, 3, id, NULL)) {
    fprintf(stderr, "sea_lane: lane->east with Go To must be allowed\n");
    rc = 1;
  }
  u->orders = UNITS_ORDER_NONE;
  if (rc == 0 && !units_can_enter(&pool, u->type_index, &map, 4, 3, id, NULL)) {
    fprintf(stderr, "sea_lane: lane->west back to ocean must be allowed\n");
    rc = 1;
  }
  /*
   * bugs.md 435: the outer rim is not a playable tile. A ship on the lane may
   * not step onto column w-1 (nor column 0 / row 0 / row h-1) — DOS
   * FUN_137f_000a. Before the fix the port only tested the raw array bounds,
   * so a ship could slide off the west sea lane onto column 0, a tile the
   * viewport never scrolls to and no click can address.
   */
  u->x = 1;
  u->y = 3;
  if (rc == 0 && units_can_enter(&pool, u->type_index, &map, 0, 3, id, NULL)) {
    fprintf(stderr, "sea_lane: step onto the west rim column must be denied\n");
    rc = 1;
  }
  if (rc == 0 && units_last_enter_reason() != COLONIZE_ENTER_BLOCKED_EDGE) {
    fprintf(stderr, "sea_lane: rim deny should be reason EDGE\n");
    rc = 1;
  }
  u->x = 3;
  u->y = 1;
  if (rc == 0 && units_can_enter(&pool, u->type_index, &map, 3, 0, id, NULL)) {
    fprintf(stderr, "sea_lane: step onto the north rim row must be denied\n");
    rc = 1;
  }
  u->x = 3;
  u->y = 6;
  if (rc == 0 && units_can_enter(&pool, u->type_index, &map, 3, 7, id, NULL)) {
    fprintf(stderr, "sea_lane: step onto the south rim row must be denied\n");
    rc = 1;
  }
  u->x = 4;
  u->y = 3;
  u->orders = UNITS_ORDER_NONE;

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: sea-lane entry rules ok\n");
  }
  return rc;
}

/*
 * bugs.md 427: "Newly bought Merchantman sent to the New World spawned in an
 * unexplored sea lane tile, and did not even insta-reveal the fog."
 *
 * DOS FUN_48d3_048e (viceroy_unpacked.c:77810) is the Europe->map placement:
 * an expanding ring hunt around the nation's landfall goal accepting the first
 * tile that passes FUN_48d3_0434 (terrain 0x1a AND empty-or-own-nation), then
 * unconditionally
 *   FUN_281f_0948 (set x/y) -> FUN_281f_084e -> FUN_281f_07a0
 * where FUN_281f_07a0 == FUN_13f1_02f8, the same sight reveal a normal move
 * runs. This test pins both halves of the sequence game_loop.c's
 * game_europe_deliver_bound_ships now performs.
 */
static int unit_europe_arrival_reveals(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Merchantman");
  pool.types[0].movement = 5;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 12, 12, err, sizeof(err))) {
    fprintf(stderr, "arrival: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 12 * 12; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  for (int y = 0; y < 12; ++y) {
    map.terrain[y * 12 + 10] = 26; /* eastern high-seas lane */
  }
  memset(map.seen, 0, map.tile_count); /* whole map unexplored */

  int rc = 0;
  const int nation = 2;
  const int goal_x = 10;
  const int goal_y = 5;

  /* An own ship already parked on the landfall goal must not push the arrival
   * across the map: DOS 48d3_0434 accepts an own-nation occupant, and the ring
   * walk keeps the pick adjacent. (units_find_high_seas_tile, the old pick,
   * refuses ANY occupied tile.) */
  const int blocker = units_spawn_allow_stack(&pool, 0, goal_x, goal_y);
  if (blocker < 0) {
    fprintf(stderr, "arrival: blocker spawn failed\n");
    map_free(&map);
    return 1;
  }
  units_get(&pool, blocker)->nation_id = nation;

  int fx = -1;
  int fy = -1;
  if (!units_spiral_place_hs_near(&pool, &map, goal_x, goal_y, nation, &fx, &fy)) {
    fprintf(stderr, "arrival: spiral place found no high-seas tile\n");
    map_free(&map);
    return 1;
  }
  if (!map_tile_is_high_seas(&map, fx, fy)) {
    fprintf(stderr, "arrival: placed on non-high-seas tile (%d,%d)\n", fx, fy);
    rc = 1;
  }
  if (rc == 0 && (fx != goal_x || abs(fy - goal_y) > 1)) {
    fprintf(stderr, "arrival: ring pick (%d,%d) strayed from goal (%d,%d)\n", fx, fy, goal_x, goal_y);
    rc = 1;
  }

  if (rc == 0) {
    const int ship = units_spawn_allow_stack(&pool, 0, fx, fy);
    if (ship < 0) {
      fprintf(stderr, "arrival: ship spawn failed\n");
      map_free(&map);
      return 1;
    }
    ColonizeUnit* u = units_get(&pool, ship);
    u->nation_id = nation;

    /* Pre-condition: the arrival tile is still fogged for the arriving nation
     * — this is exactly the state the player was left in. */
    if (map_tile_seen_by(&map, fx, fy, nation)) {
      fprintf(stderr, "arrival: fixture tile was already explored\n");
      rc = 1;
    }
    if (rc == 0) {
      (void)units_reveal_sight(&map, &pool, NULL, u, NULL);
    }
    if (rc == 0 && !map_tile_seen_by(&map, fx, fy, nation)) {
      fprintf(stderr, "arrival: own tile still fogged after reveal\n");
      rc = 1;
    }
    /* Sight radius 1 inner box: every inset neighbour is revealed. */
    for (int dy = -1; rc == 0 && dy <= 1; ++dy) {
      for (int dx = -1; rc == 0 && dx <= 1; ++dx) {
        const int tx = fx + dx;
        const int ty = fy + dy;
        if (!map_coords_inset(&map, tx, ty)) {
          continue;
        }
        if (!map_tile_seen_by(&map, tx, ty, nation)) {
          fprintf(stderr, "arrival: neighbour (%d,%d) still fogged after reveal\n", tx, ty);
          rc = 1;
        }
      }
    }
    /* Other nations gained nothing. */
    if (rc == 0 && map_tile_seen_by(&map, fx, fy, 0)) {
      fprintf(stderr, "arrival: reveal leaked to another nation\n");
      rc = 1;
    }
  }

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: Europe arrival place+reveal ok\n");
  }
  return rc;
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


/* FUN_465b_0000 → FUN_5fef_1908 King's Galleon offer (@KINGGALLEON2/3, @CASHTREASURE). */
static int unit_king_galleon_offer(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "galleon: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "galleon: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int treasure_ti = units_find_type(&pool, "Treasure");
  const int galleon_ti = units_find_type(&pool, "Galleon");
  if (treasure_ti < 0 || galleon_ti < 0) {
    fprintf(stderr, "galleon: types missing\n");
    assets_msg_free(&names);
    return 1;
  }
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map)); /* map_alloc frees the old buffers first */
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "galleon: map_alloc failed: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  for (int y = 0; y < 8; ++y) {
    map.terrain[y * map.width + 0] = 25; /* ocean column */
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  const int cx = 1;
  const int cy = 3;
  if (!map_tile_is_coastal(&map, cx, cy)) {
    fprintf(stderr, "galleon: (1,3) should be coastal\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  const int cid = colonies_found(&colonies, &map, cx, cy, 0, -1, UNITS_JOB_NONE, 0, 0, 0);
  if (cid < 0) {
    fprintf(stderr, "galleon: colonies_found failed\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  ColonizeCol1Save c1;
  memset(&c1, 0, sizeof(c1));
  c1.player[0].control = 0;
  memset(c1.head.founding_father, -1, sizeof(c1.head.founding_father)); /* unclaimed */
  c1.nation[0].gold = 100;
  c1.nation[0].tax_rate = 20;
  c1.head.difficulty = 2; /* Conquistador: (2+10)*5 = 60 > 2*20 */
  AiPopupState pops;
  ai_popup_clear(&pops);

  /* No Cortes: share = max(60, 40) = 60. */
  if (units_king_galleon_share_pct(&c1, 0) != 60) {
    fprintf(stderr, "galleon: share want 60 got %d\n", units_king_galleon_share_pct(&c1, 0));
    goto fail;
  }
  c1.nation[0].tax_rate = 45; /* 2*45 = 90 wins; cap holds at 90 */
  if (units_king_galleon_share_pct(&c1, 0) != 90) {
    fprintf(stderr, "galleon: share cap want 90\n");
    goto fail;
  }
  c1.nation[0].tax_rate = 20;
  c1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] |= (uint8_t)(1u << (FF_HERNAN_CORTES % 8));
  if (units_king_galleon_share_pct(&c1, 0) != 20) {
    fprintf(stderr, "galleon: Cortes share want tax 20\n");
    goto fail;
  }
  c1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] = 0;

  const int tid = units_spawn_allow_stack(&pool, treasure_ti, cx, cy);
  ColonizeUnit* t = units_get(&pool, tid);
  t->nation_id = 0;
  t->hold_goods_amount[0] = 1000 & 0xff;
  t->hold_goods_amount[1] = (1000 >> 8) & 0xff;

  /* Owning a Galleon without Cortes → no offer. */
  const int gid = units_spawn_allow_stack(&pool, galleon_ti, 0, 3);
  units_get(&pool, gid)->nation_id = 0;
  if (units_king_galleon_offer_coastal_treasures(&pool, &colonies, &map, NULL, &c1, 0, &pops, NULL) != 0 ||
      pops.queue_count != 0) {
    fprintf(stderr, "galleon: own Galleon should suppress the offer\n");
    goto fail;
  }
  units_despawn(&pool, gid);

  /* Offer enqueued; Refuse leaves the Treasure. */
  if (units_king_galleon_offer_coastal_treasures(&pool, &colonies, &map, NULL, &c1, 0, &pops, NULL) != 1 ||
      pops.queue_count != 1 || pops.queue[0].tag != AI_POPUP_TAG_KING_GALLEON ||
      pops.queue[0].payload != tid) {
    fprintf(stderr, "galleon: KINGGALLEON2 CHOICE not enqueued\n");
    goto fail;
  }
  /* Re-running while queued must not stack a duplicate. */
  (void)units_king_galleon_offer_coastal_treasures(&pool, &colonies, &map, NULL, &c1, 0, &pops, NULL);
  if (pops.queue_count != 1) {
    fprintf(stderr, "galleon: duplicate offer queued\n");
    goto fail;
  }
  pops.has_result = true;
  pops.result_tag = AI_POPUP_TAG_KING_GALLEON;
  pops.result_nation_a = 0;
  pops.result_payload = tid;
  pops.result_choice_id = 0;
  pops.result_cancelled = false;
  if (!units_king_galleon_apply_popup(&pool, NULL, &c1, &pops, NULL)) {
    fprintf(stderr, "galleon: apply should consume tag\n");
    goto fail;
  }
  if (c1.nation[0].gold != 100 || !units_get(&pool, tid) || !units_get(&pool, tid)->active) {
    fprintf(stderr, "galleon: Refuse must leave gold/treasure untouched\n");
    goto fail;
  }
  /* Accept: 60% share → 600 to royal_money, 400 to gold, Treasure gone. */
  pops.result_choice_id = 1;
  (void)units_king_galleon_apply_popup(&pool, NULL, &c1, &pops, NULL);
  if (c1.nation[0].gold != 500 || c1.nation[0].royal_money != 600) {
    fprintf(stderr, "galleon: Accept want gold 500 royal 600 got %u/%d\n", c1.nation[0].gold,
            c1.nation[0].royal_money);
    goto fail;
  }
  {
    const ColonizeUnit* gone = units_get_const(&pool, tid);
    if (gone && gone->active) {
      fprintf(stderr, "galleon: Treasure should be despawned\n");
      goto fail;
    }
  }
  /* WoI declared: full value, no CHOICE. */
  c1.head.game_options.woi = 1;
  const int tid2 = units_spawn_allow_stack(&pool, treasure_ti, cx, cy);
  units_get(&pool, tid2)->nation_id = 0;
  units_get(&pool, tid2)->hold_goods_amount[0] = 200 & 0xff;
  units_get(&pool, tid2)->hold_goods_amount[1] = 0;
  ai_popup_clear(&pops);
  if (units_king_galleon_offer_coastal_treasures(&pool, &colonies, &map, NULL, &c1, 0, &pops, NULL) != 1 ||
      c1.nation[0].gold != 700 || c1.nation[0].royal_money != 600) {
    fprintf(stderr, "galleon: WoI should cash full value at once (gold %u)\n", c1.nation[0].gold);
    goto fail;
  }
  fprintf(stderr, "unit_units: King's Galleon offer ok\n");
  map_free(&map);
  assets_msg_free(&names);
  return 0;
fail:
  map_free(&map);
  assets_msg_free(&names);
  return 1;
}

/*
 * DOS fog model: FUN_13f1_0158 reveal marks unit vis bits (FUN_1427_09ac) and
 * colony pop/fort snapshots (FUN_364b_1b4c); FUN_1427_0c9a/0968 reset a mover's
 * mask to "tile owner | who watches the tile"; rim tiles never reveal.
 */
static int unit_fog_vis_mask_and_snapshot(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 12, 12, err, sizeof(err))) {
    fprintf(stderr, "fog: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 12 * 12; ++i) {
    map.terrain[i] = 2;    /* plains */
    map.layer3[i] = 0xf1;  /* unowned, continent 1 */
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Colonists");
  units.type_count = 1;
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(&map);

  const int a = units_spawn(&units, 0, 3, 3);
  const int b = units_spawn(&units, 0, 5, 5);
  const int brave = units_spawn(&units, 0, 3, 5); /* outer ring of a's radius-2 sight */
  units_set_nation(units_get(&units, a), 0);
  units_set_nation(units_get(&units, b), 1);
  units_set_nation(units_get(&units, brave), 6);
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 1;
  col->nation_id = 2;
  col->x = 4;
  col->y = 3;
  col->population = 5;
  colonies.colony_count = 1;
  units_occupancy_rebuild(&units);

  /* Reveal radius 1 from a: b (5,5) is outside, colony (4,3) inside. */
  (void)units_reveal_sight(&map, &units, &colonies, units_get(&units, a), NULL);
  if ((units_get(&units, b)->col1_vis_mask & 1u) != 0) {
    fprintf(stderr, "fog: unit outside sight got viewer bit\n");
    return 1;
  }
  if (col->pop_on_map[0] != 5 || col->fort_on_map[0] != 0 || col->pop_on_map[1] != 0) {
    fprintf(stderr, "fog: colony snapshot wrong (%d/%d/%d)\n", col->pop_on_map[0],
            col->fort_on_map[0], col->pop_on_map[1]);
    return 1;
  }
  if (colonies_known_to(col, 1, false) || !colonies_known_to(col, 0, false) ||
      !colonies_known_to(col, 1, true)) {
    fprintf(stderr, "fog: colonies_known_to gate wrong\n");
    return 1;
  }
  /* Owner stamp: revealed unowned tile (2,2) now carries nation 0 (FUN_137f_0228). */
  if (((map_get_layer3(&map, 2, 2) >> 4) & 0x0f) != 0) {
    fprintf(stderr, "fog: reveal did not stamp owner nibble\n");
    return 1;
  }
  /* Rim never reveals (FUN_137f_000a inset gate). */
  if (map_tile_seen_by(&map, 0, 0, 0) && map_tile_seen_by(&map, 11, 11, 0)) {
    fprintf(stderr, "fog: rim tile revealed\n");
    return 1;
  }

  /* Outer ring (radius 2 via a fake Galleon-less path): use map_reveal_sight_each directly. */
  map_reveal_sight(&map, 3, 3, 3, 2, false);
  if (!map_tile_seen_by(&map, 3, 5, 3)) {
    fprintf(stderr, "fog: same-continent land outer ring not revealed\n");
    return 1;
  }

  /* Move b next to a: mask := watchers (nation 0 adjacent) | own bit. */
  units_vis_mask_after_move(&units, &map, b, 4, 4);
  const uint8_t mb = units_get(&units, b)->col1_vis_mask;
  if ((mb & 1u) == 0 || (mb & 2u) == 0) {
    fprintf(stderr, "fog: mask after move next to watcher = 0x%02x\n", mb);
    return 1;
  }
  /* Move b away again: watcher bit drops, own bit stays. */
  units_vis_mask_after_move(&units, &map, b, 8, 8);
  const uint8_t mb2 = units_get(&units, b)->col1_vis_mask;
  if ((mb2 & 1u) != 0 || (mb2 & 2u) == 0) {
    fprintf(stderr, "fog: mask after move away = 0x%02x\n", mb2);
    return 1;
  }
  /*
   * bugs.md: an Indian Convert must carry its own sprite on the map, not the
   * plain Free Colonist one — DOS FUN_112b_0060 routes every @UNIT type 0
   * through the profession table (FUN_112b_0002 case 0x1b -> icon 0x43 =
   * sprite 66 here), the same icon the colony screen already used.
   */
  {
    ColonizeUnitPool cpool;
    memset(&cpool, 0, sizeof(cpool));
    memset(&cpool, 0, sizeof(cpool));
    units_reset(&cpool);
    units_set_occupancy_map(NULL);
    cpool.type_count = 1;
    snprintf(cpool.types[0].name, sizeof(cpool.types[0].name), "Colonists");
    cpool.types[0].movement = 1;
    cpool.types[0].icon_sprite = 100; /* Free Colonist */
    const int plain = units_spawn_allow_stack(&cpool, 0, 3, 3);
    const int conv = units_spawn_allow_stack(&cpool, 0, 4, 3);
    ColonizeUnit* cu = units_get(&cpool, conv);
    if (!cu || plain < 0) {
      fprintf(stderr, "convert sprite: spawn failed\n");
      return 1;
    }
    cu->profession = COLONIZE_PROF_CONVERT;
    if (units_map_sprite(&cpool, plain) != 100) {
      fprintf(stderr, "convert sprite: plain colonist map icon %d want 100\n",
              units_map_sprite(&cpool, plain));
      return 1;
    }
    if (units_map_sprite(&cpool, conv) != 66) {
      fprintf(stderr, "convert sprite: convert map icon %d want 66\n",
              units_map_sprite(&cpool, conv));
      return 1;
    }
    /* Equipment still wins, as it does in DOS's type != 0 overrides. */
    cu->muskets = UNITS_EQUIP_MUSKETS;
    if (units_map_sprite(&cpool, conv) != UNITS_ICON_SOLDIER) {
      fprintf(stderr, "convert sprite: armed convert should draw as a soldier\n");
      return 1;
    }
    cu->muskets = 0;
    fprintf(stderr, "convert map sprite ok\n");

    /* bugs.md 269: a mounted Veteran Soldier (profession 0x15) draws with the
     * Veteran Dragoon art, not the plain grey dragoon. */
    ColonizeUnit* vu = units_get(&cpool, plain);
    vu->profession = UNITS_JOB_SOLDIER;
    vu->muskets = UNITS_EQUIP_MUSKETS;
    if (units_map_sprite(&cpool, plain) != UNITS_ICON_VETERAN_SOLDIER) {
      fprintf(stderr, "veteran sprite: armed vet icon %d want %d\n",
              units_map_sprite(&cpool, plain), UNITS_ICON_VETERAN_SOLDIER);
      return 1;
    }
    vu->horses = UNITS_EQUIP_HORSES;
    if (units_map_sprite(&cpool, plain) != UNITS_ICON_VETERAN_DRAGOON) {
      fprintf(stderr, "veteran sprite: mounted vet icon %d want %d\n",
              units_map_sprite(&cpool, plain), UNITS_ICON_VETERAN_DRAGOON);
      return 1;
    }
    if (strcmp(units_display_name(&cpool, vu), "Veteran Dragoon") != 0) {
      fprintf(stderr, "veteran name: mounted vet reads '%s' want 'Veteran Dragoon'\n",
              units_display_name(&cpool, vu));
      return 1;
    }
    vu->muskets = 0;
    vu->horses = 0;
    vu->profession = 0;
    fprintf(stderr, "veteran dragoon chrome ok\n");

    /*
     * bugs.md 426: FUN_112b_0060's tail downgrades a commissioned missionary
     * whose colonist is not a Jesuit (profession != 0x18) to icon 0x4e =
     * sprite 77; the Jesuit keeps the @UNIT icon 106 → sprite 105. And
     * FUN_112b_0002 case 5 gives a Jesuit *working colonist* icon 0x3e =
     * sprite 61, not either commissioned pose.
     */
    cpool.type_count = 2;
    snprintf(cpool.types[1].name, sizeof(cpool.types[1].name), "Missionaries");
    cpool.types[1].movement = 1;
    cpool.types[1].icon_sprite = UNITS_ICON_JESUIT_MISSIONARY;
    const int miss = units_spawn_allow_stack(&cpool, 1, 5, 3);
    ColonizeUnit* mu = units_get(&cpool, miss);
    if (!mu) {
      fprintf(stderr, "missionary sprite: spawn failed\n");
      return 1;
    }
    mu->profession = UNITS_JOB_NONE;
    if (units_map_sprite(&cpool, miss) != UNITS_ICON_MISSIONARY) {
      fprintf(stderr, "missionary sprite: unskilled icon %d want %d\n",
              units_map_sprite(&cpool, miss), UNITS_ICON_MISSIONARY);
      return 1;
    }
    mu->profession = UNITS_JOB_MISSIONARY;
    if (units_map_sprite(&cpool, miss) != UNITS_ICON_JESUIT_MISSIONARY) {
      fprintf(stderr, "missionary sprite: Jesuit icon %d want %d\n",
              units_map_sprite(&cpool, miss), UNITS_ICON_JESUIT_MISSIONARY);
      return 1;
    }
    if (units_job_icon_sprite(UNITS_JOB_MISSIONARY) != UNITS_ICON_JESUIT_MISSIONARY_WORK) {
      fprintf(stderr, "missionary sprite: working portrait %d want %d\n",
              units_job_icon_sprite(UNITS_JOB_MISSIONARY),
              UNITS_ICON_JESUIT_MISSIONARY_WORK);
      return 1;
    }
    fprintf(stderr, "missionary sprite split ok\n");
  }

  /*
   * Founding reveal is Coronado-only (FUN_364b_1dd6 gates FUN_13f1_00a6 on
   * FF 6 per nation): without him nothing is revealed; with him it is ±5 and
   * seeds pop_on_map=1 on colonies inside that had not been observed.
   */
  ColonizeColony* col2 = &colonies.colonies[1];
  col2->active = true;
  col2->id = 2;
  col2->nation_id = 3;
  col2->x = 8;
  col2->y = 8;
  colonies.colony_count = 2;

  ColonizeCol1Save fog_col1;
  memset(&fog_col1, 0, sizeof(fog_col1));
  colonies_reveal_founded(&map, &colonies, &fog_col1, 2);
  if (col2->pop_on_map[3] != 0) {
    fprintf(stderr, "fog: founding revealed without Coronado\n");
    return 1;
  }

  fog_col1.nation[3].founding_fathers[FF_FRANCISCO_CORONADO / 8] =
    (uint8_t)(1u << (FF_FRANCISCO_CORONADO % 8));
  colonies_reveal_founded(&map, &colonies, &fog_col1, 2);
  if (!map_tile_seen_by(&map, 3, 3, 3) || map_tile_seen_by(&map, 1, 9, 3) ||
      col->pop_on_map[3] != 1 || col2->pop_on_map[3] != 1) {
    fprintf(stderr, "fog: founding reveal/seed wrong\n");
    return 1;
  }

  units_set_occupancy_map(NULL);
  map_free(&map);
  fprintf(stderr, "fog vis mask / snapshot ok\n");
  return 0;
}

/*
 * FUN_OVL20_L0000__0015bc neighbour pick (viceroy_overlays.c:86760-86840):
 * score = flood cost[cand] + edge(unit->cand), where a cardinal step
 * between two river tiles costs 1 (else 3 for a `movement < 4` unit).
 * Pioneer at (4,2), goal (2,4), minor river on (4,2),(3,2),(2,2),(2,3),(2,4):
 * (3,2) costs 4+1 = 5 along the river; (3,3) costs 4+3 = 7 diagonally.
 * DOS steps west onto the river; picking by cost[] alone would take (3,3).
 */
static int unit_flood_river_pair_step(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Pioneers");
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "river_pair: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  static const int k_river[5][2] = {{4, 2}, {3, 2}, {2, 2}, {2, 3}, {2, 4}};
  for (int i = 0; i < 5; ++i) {
    map.terrain[k_river[i][1] * 8 + k_river[i][0]] = 2 | 0x40; /* minor river */
  }

  const int id = units_spawn_allow_stack(&pool, 0, 4, 2);
  ColonizeUnit* u = units_get(&pool, id);
  if (!u) {
    map_free(&map);
    fprintf(stderr, "river_pair: spawn failed\n");
    return 1;
  }
  u->nation_id = 0;
  u->moves_left = 1 * UNITS_MP_PER_TILE;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = 2;
  u->goto_y = 4;

  int rc = 0;
  int nx = -1;
  int ny = -1;
  if (!units_next_goto_step(&pool, id, &map, NULL, NULL, &nx, &ny)) {
    fprintf(stderr, "river_pair: no step found\n");
    rc = 1;
  } else if (nx != 3 || ny != 2) {
    fprintf(stderr, "river_pair: expected (3,2) along the river, got (%d,%d)\n", nx, ny);
    rc = 1;
  }
  /* Without the river the diagonal (3,3) wins (4+3 = 7 vs (3,2) 7+3). */
  for (int i = 0; i < 5; ++i) {
    map.terrain[k_river[i][1] * 8 + k_river[i][0]] = 2;
  }
  if (rc == 0 && (!units_next_goto_step(&pool, id, &map, NULL, NULL, &nx, &ny) || nx != 3 || ny != 3)) {
    fprintf(stderr, "river_pair: plain map should step (3,3), got (%d,%d)\n", nx, ny);
    rc = 1;
  }

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: flood river-pair neighbour pick ok\n");
  }
  return rc;
}


/*
 * bugs.md: waking a loaded unit out of a ship's hold (tile-stack picker /
 * ORDERS Activate Unit) has to leave it able to walk ashore even when the
 * ship itself has no moves left. Boarding parks the passenger at moves_left
 * 0 as a skip-select flag, so the wake has to restore its allotment.
 */
static int unit_wake_passenger_can_land(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Caravel");
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Colonists");
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "wake_pax: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  for (int y = 0; y < 8; ++y) {
    map.terrain[y * 8 + 4] = 1; /* land column */
  }

  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 4, 3);
  ColonizeUnit* su = units_get(&pool, ship);
  ColonizeUnit* pu = units_get(&pool, pax);
  if (!su || !pu) {
    map_free(&map);
    fprintf(stderr, "wake_pax: spawn failed\n");
    return 1;
  }
  su->nation_id = 0;
  pu->nation_id = 0;
  su->moves_left = 4 * UNITS_MP_PER_TILE;
  pu->moves_left = UNITS_MP_PER_TILE;

  int rc = 0;
  if (!units_board(&pool, pax, ship)) {
    fprintf(stderr, "wake_pax: board failed\n");
    rc = 1;
  }
  pu = units_get(&pool, pax);
  su = units_get(&pool, ship);
  if (rc == 0 && (pu->aboard_ship_id != ship || pu->moves_left != 0)) {
    fprintf(stderr, "wake_pax: boarding should park the passenger at 0 MP\n");
    rc = 1;
  }
  /* The ship is out of moves: only the passenger's own allotment can land it. */
  if (rc == 0) {
    su->moves_left = 0;
  }
  if (rc == 0 && !units_wake(&pool, pax)) {
    fprintf(stderr, "wake_pax: units_wake should report the sentry cleared\n");
    rc = 1;
  }
  pu = units_get(&pool, pax);
  if (rc == 0 && (pu->orders != 0 || pu->moves_left <= 0)) {
    fprintf(stderr, "wake_pax: wake must clear orders and restore MP (orders=%d mp=%d)\n",
            pu->orders, pu->moves_left);
    rc = 1;
  }
  if (rc == 0 &&
      !units_unload_passenger(&pool, ship, pax, &map, 4, 3, NULL)) {
    fprintf(stderr, "wake_pax: woken passenger must be able to step ashore\n");
    rc = 1;
  }
  pu = units_get(&pool, pax);
  if (rc == 0 && (pu->aboard_ship_id >= 0 || pu->x != 4 || pu->y != 3)) {
    fprintf(stderr, "wake_pax: passenger should stand on land at (4,3)\n");
    rc = 1;
  }

  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: wake loaded passenger -> land ok\n");
  }
  return rc;
}

/*
 * bugs.md: the tile-stack picker must work like DOS FUN_2b5a_1b5a — ONE pick
 * wakes a sentried passenger AND hands it back for selection, closing the
 * dialog. The old two-step (first click only woke, dialog stayed open with
 * no selection) read as "impossible to wake up unit loaded onto a ship
 * without moves".
 */
static int unit_stack_one_click_wakes_and_selects(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Caravel");
  pool.types[0].movement = 4;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].cargo = 2;
  pool.types[1].space = 1;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Colonists");
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  const int ship = units_spawn_allow_stack(&pool, 0, 3, 3);
  const int pax = units_spawn_allow_stack(&pool, 1, 3, 3);
  ColonizeUnit* su = units_get(&pool, ship);
  ColonizeUnit* pu = units_get(&pool, pax);
  if (!su || !pu) {
    fprintf(stderr, "stack_pick: spawn failed\n");
    return 1;
  }
  su->nation_id = 0;
  pu->nation_id = 0;
  su->moves_left = 0; /* ship exhausted — the reported scenario */
  if (!units_board_stacked(&pool, pax, ship)) {
    fprintf(stderr, "stack_pick: board failed\n");
    return 1;
  }

  UnitStackPopup dlg;
  memset(&dlg, 0, sizeof(dlg));
  if (!unit_stack_try_open(&dlg, &pool, 3, 3, 0) || dlg.count != 2) {
    fprintf(stderr, "stack_pick: popup should open with ship+passenger\n");
    return 1;
  }
  /* bugs.md two-step: the first pick on an ordered row only cancels its
   * orders and keeps the popup open; the second pick activates and closes. */
  dlg.selection = (dlg.ids[0] == pax) ? 0 : 1;
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  int sel = -1;
  unit_stack_handle_input(&dlg, &pool, &in, &sel);
  pu = units_get(&pool, pax);
  if (sel != -1 || !dlg.open) {
    fprintf(stderr, "stack_pick: first pick must only cancel (sel=%d open=%d)\n", sel, dlg.open);
    return 1;
  }
  if (pu->orders != 0 || pu->moves_left <= 0) {
    fprintf(
      stderr,
      "stack_pick: first pick must wake the passenger (orders=%d mp=%d)\n",
      pu->orders,
      pu->moves_left
    );
    return 1;
  }
  unit_stack_handle_input(&dlg, &pool, &in, &sel);
  if (sel != pax || dlg.open) {
    fprintf(stderr, "stack_pick: second pick must select and close (sel=%d open=%d)\n", sel, dlg.open);
    return 1;
  }
  fprintf(stderr, "unit_units: stack picker two-step cancel+activate ok\n");
  return 0;
}

/*
 * FUN_465b_0000 (75600-75626): a Euro attack onto an Indian-held tile slams
 * tribe alarm by (difficulty+5), doubled on a village tile — where the
 * village record's alarm[nation].attacks byte is bumped — sextupled on a
 * capital. Written before the combat resolves, win or lose.
 */
static int unit_native_tile_attack_alarm(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 8;
  map.height = 8;
  map.tile_count = 64;
  map.terrain = calloc(64, 1);
  map.layer2 = calloc(64, 1);
  map.layer3 = calloc(64, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return 1;
  }
  for (int i = 0; i < 64; ++i) {
    map.terrain[i] = 2; /* plains */
  }

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Soldiers");
  pool.types[0].attack = 99;
  pool.types[0].defense = 99;
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Braves");
  pool.types[1].attack = 1;
  pool.types[1].defense = 1;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  /* Zeroed founding_father[] reads as "nation 0 holds every FF" — mark all
   * unclaimed so Pocahontas doesn't halve the 00f2 alarm bump under test. */
  memset(col1.head.founding_father, 0xff, sizeof(col1.head.founding_father));
  col1.head.difficulty = 2; /* base = 7 */
  static ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.x = 5;
  tribe.y = 5;
  tribe.nation_id = 4;
  tribe.population = 5;
  col1.tribe = &tribe;
  col1.head.tribe_count = 1;
  col1.indian[0].alarm_by_player[0] = 10;
  units_set_ff_col1(&col1);

  int rc = 0;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7);

  /* Village tile: base*2 alarm + attacks++. */
  {
    const int aid = units_spawn_allow_stack(&pool, 0, 4, 5);
    const int did = units_spawn_allow_stack(&pool, 1, 5, 5);
    units_get(&pool, aid)->nation_id = 0;
    units_get(&pool, aid)->moves_left = UNITS_MP_PER_TILE;
    units_get(&pool, did)->nation_id = 4;
    units_try_move(&pool, aid, &map, 5, 5, NULL, &rng);
    if (tribe.alarm[0].attacks != 1) {
      fprintf(stderr, "465b: village attacks=%d (want 1)\n", tribe.alarm[0].attacks);
      rc = 1;
    }
    if (col1.indian[0].alarm_by_player[0] != 10 + 14) {
      fprintf(stderr, "465b: village alarm=%d (want 24)\n",
              col1.indian[0].alarm_by_player[0]);
      rc = 1;
    }
  }
  /* Plain tile: base only, attacks untouched. */
  if (rc == 0) {
    const int before = col1.indian[0].alarm_by_player[0];
    const int aid = units_spawn_allow_stack(&pool, 0, 2, 2);
    const int did = units_spawn_allow_stack(&pool, 1, 3, 2);
    units_get(&pool, aid)->nation_id = 0;
    units_get(&pool, aid)->moves_left = UNITS_MP_PER_TILE;
    units_get(&pool, did)->nation_id = 4;
    units_try_move(&pool, aid, &map, 3, 2, NULL, &rng);
    if (tribe.alarm[0].attacks != 1) {
      fprintf(stderr, "465b: plain-tile bumped attacks=%d\n", tribe.alarm[0].attacks);
      rc = 1;
    }
    if (col1.indian[0].alarm_by_player[0] != before + 7) {
      fprintf(stderr, "465b: plain alarm=%d (want %d)\n",
              col1.indian[0].alarm_by_player[0], before + 7);
      rc = 1;
    }
  }
  /* Capital: base*6 replaces the *2. */
  if (rc == 0) {
    tribe.state.capital = 1;
    const int before = col1.indian[0].alarm_by_player[0];
    const int aid = units_spawn_allow_stack(&pool, 0, 6, 5);
    const int did = units_spawn_allow_stack(&pool, 1, 5, 5);
    units_get(&pool, aid)->nation_id = 0;
    units_get(&pool, aid)->moves_left = UNITS_MP_PER_TILE;
    units_get(&pool, did)->nation_id = 4;
    units_try_move(&pool, aid, &map, 5, 5, NULL, &rng);
    if (tribe.alarm[0].attacks != 2 ||
        col1.indian[0].alarm_by_player[0] != before + 42) {
      fprintf(stderr, "465b: capital attacks=%d alarm=%d (want 2 / %d)\n",
              tribe.alarm[0].attacks, col1.indian[0].alarm_by_player[0], before + 42);
      rc = 1;
    }
  }

  units_set_ff_col1(NULL);
  col1.tribe = NULL;
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  if (rc == 0) {
    fprintf(stderr, "unit_units: native tile attack alarm (465b) ok\n");
  }
  return rc;
}

/*
 * FUN_5fef_1b0e residue, 2026-09-08.
 *  a) capture arm raw 100937-100948 — the 8-neighbour owner-nibble claim the
 *     prize brings with it, skipping any tile that already holds a unit or a
 *     settlement (FUN_281f_06d2 gate).
 *  b) the `local_a6` alarm vent, raw 101043-101196 — natives that beat an
 *     undefended colony vent `difficulty − 10`, and the whole table is gated
 *     on NOT already being at war with that European.
 */
static int unit_capture_ring_and_alarm_vent(void) {
  int rc = 0;
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 8;
  map.height = 8;
  map.tile_count = 64;
  map.terrain = calloc(64, 1);
  map.layer2 = calloc(64, 1);
  map.layer3 = calloc(64, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return 1;
  }
  for (int i = 0; i < 64; ++i) {
    map.terrain[i] = 2;    /* plains */
    map.layer3[i] = 0xf0u; /* owner nibble 15 = unowned */
  }

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 2;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Soldiers");
  pool.types[0].attack = 99;
  pool.types[0].defense = 99;
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool.types[1].name, sizeof(pool.types[1].name), "Braves");
  pool.types[1].attack = 99;
  pool.types[1].defense = 1;
  pool.types[1].movement = 1;
  pool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  units_set_occupancy_map(&map);
  colonies_set_occupancy_map(&map);
  const int cid = colonies_found(&colonies, &map, 4, 4, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
  if (cid < 0) {
    fprintf(stderr, "capture_ring: colonies_found failed\n");
    rc = 1;
    goto done;
  }

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 11);
  units_set_combat_popups(NULL, NULL); /* headless: no @CAPTURED, no colony zoom */

  /* (a) Euro nation 0 walks into nation 1's undefended colony. A bystander on
   *     (5,5) must keep its own stamp; every other neighbour flips to 0. */
  {
    units_set_ff_col1(NULL); /* no col1 → no Revere/temp defender, plain walk-in */
    const int bystander = units_spawn_allow_stack(&pool, 0, 5, 5);
    units_set_nation(units_get(&pool, bystander), 2);
    const int aid = units_spawn_allow_stack(&pool, 0, 3, 4);
    units_set_nation(units_get(&pool, aid), 0);
    units_get(&pool, aid)->moves_left = UNITS_MP_PER_TILE;
    if (!units_try_move(&pool, aid, &map, 4, 4, &colonies, &rng)) {
      fprintf(stderr, "capture_ring: walk-in move refused\n");
      rc = 1;
    } else if (colonies_get(&colonies, cid)->nation_id != 0) {
      fprintf(stderr, "capture_ring: colony not captured\n");
      rc = 1;
    } else {
      static const int k_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int k_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      for (int d = 0; d < 8 && rc == 0; ++d) {
        const int tx = 4 + k_dx[d];
        const int ty = 4 + k_dy[d];
        const int owner = (map.layer3[ty * map.width + tx] >> 4) & 0x0f;
        const int want = (tx == 5 && ty == 5) ? 2 : 0;
        if (owner != want) {
          fprintf(stderr, "capture_ring: (%d,%d) owner %d want %d\n", tx, ty, owner, want);
          rc = 1;
        }
      }
    }
    /* Clear the tile again so (b) is a walk-in, not a fight with the captor. */
    (void)units_despawn(&pool, aid);
    units_occupancy_notify_moved(&pool, 4, 4, -1, -1);
  }

  /* (b) Native beats an undefended colony: alarm vents `difficulty - 10`,
   *     and the raider's tension row is discharged. */
  if (rc == 0) {
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    memset(col1.head.founding_father, 0xff, sizeof(col1.head.founding_father));
    col1.head.difficulty = 3;
    col1.head.tribe_count = 0;
    col1.player[0].control = 0; /* human victim → the difficulty term applies */
    col1.indian[0].alarm_by_player[0] = 40;
    col1.head.tribe_count = 1;
    static ColonizeCol1Tribe tribe;
    memset(&tribe, 0, sizeof(tribe));
    tribe.nation_id = 4;
    /* DS:0x54f6 = this record's attitude[euro 0] word (friction | attacks<<8). */
    col1_tribe_attitude_set(&tribe, 0, 200);
    col1.tribe = &tribe;
    units_set_ff_col1(&col1);
    units_set_combat_human_nation(0);

    ColonizeColony* col = colonies_get_mut(&colonies, cid);
    col->nation_id = 0;
    col->population = 3;
    col->colonist_count = 3;

    const int bid = units_spawn_allow_stack(&pool, 1, 3, 4);
    ColonizeUnit* brave = units_get(&pool, bid);
    units_set_nation(brave, 4);
    brave->home_tribe_id = 0;
    brave->moves_left = 0; /* natives: SPENT byte — 0 = fresh full allotment */
    (void)units_try_move(&pool, bid, &map, 4, 4, &colonies, &rng);

    /* difficulty 3 − 10 = −7 → 40 - 7 = 33. */
    if (col1.indian[0].alarm_by_player[0] != 33) {
      fprintf(
        stderr, "alarm_vent: undefended-colony alarm %u want 33\n",
        (unsigned)col1.indian[0].alarm_by_player[0]
      );
      rc = 1;
    }
    if (col1_tribe_attitude(&tribe, 0) != 0) {
      fprintf(
        stderr, "alarm_vent: attitude %d want 0\n", col1_tribe_attitude(&tribe, 0)
      );
      rc = 1;
    }

    /* War bit set (FUN_15b3_0004 & 2): DOS gives no relief at all. */
    if (rc == 0) {
      col1.indian[0].alarm_by_player[0] = 40;
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_WAR_BIT;
      col->population = 3;
      col->colonist_count = 3;
      const int b2 = units_spawn_allow_stack(&pool, 1, 3, 4);
      ColonizeUnit* br2 = units_get(&pool, b2);
      units_set_nation(br2, 4);
      br2->home_tribe_id = 0;
      br2->moves_left = 0; /* spent byte: fresh */
      (void)units_try_move(&pool, b2, &map, 4, 4, &colonies, &rng);
      if (col1.indian[0].alarm_by_player[0] != 40) {
        fprintf(
          stderr, "alarm_vent: at-war alarm %u want 40 (no vent)\n",
          (unsigned)col1.indian[0].alarm_by_player[0]
        );
        rc = 1;
      }
    }
    units_set_ff_col1(NULL);
    units_set_combat_human_nation(-1);
    col1.tribe = NULL;
  }

done:
  units_set_occupancy_map(NULL);
  colonies_set_occupancy_map(NULL);
  units_set_combat_colonies(NULL);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  if (rc == 0) {
    fprintf(stderr, "unit_units: capture ring + 1b0e alarm vent ok\n");
  }
  return rc;
}

/*
 * FUN_5fef_1b0e raw 100573-100577: a plain Brave (@UNIT type 0x13) attacking
 * the Artillery (@UNIT type 0xb) of a HUMAN-controlled European never wins,
 * whatever the roll — DOS forces `bVar8 = false` and latches `local_ca`. An
 * AI-controlled European (control != 0) is NOT covered by the gate.
 */
static int unit_brave_vs_human_artillery_autoloss(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "brave_arty: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "brave_arty: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int brave = units_find_type(&pool, "Braves");
  const int arty = units_find_type(&pool, "Artillery");
  assets_msg_free(&names);
  if (brave != 0x13 || arty != 0x0b) {
    fprintf(stderr, "brave_arty: @UNIT ids Braves=%d Artillery=%d (want 19/11)\n", brave, arty);
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 8;
  map.height = 8;
  map.tile_count = 64;
  map.terrain = calloc(64, 1);
  map.layer2 = calloc(64, 1);
  map.layer3 = calloc(64, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return 1;
  }
  for (int i = 0; i < 64; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  units_set_occupancy_map(&map);

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  memset(col1.head.founding_father, 0xff, sizeof(col1.head.founding_father));
  col1.head.difficulty = 2;
  col1.player[0].control = 0; /* human */
  col1.player[1].control = 1; /* AI */
  units_set_ff_col1(&col1);
  units_set_combat_human_nation(0);

  int rc = 0;
  int ai_def_wins = 0;
  for (int seed = 1; seed <= 40 && rc == 0; ++seed) {
    /* Human-controlled European Artillery: the Brave must ALWAYS lose. */
    {
      const int aid = units_spawn_allow_stack(&pool, brave, 4, 5);
      const int did = units_spawn_allow_stack(&pool, arty, 5, 5);
      if (aid < 0 || did < 0) {
        fprintf(stderr, "brave_arty: spawn failed\n");
        rc = 1;
        break;
      }
      units_get(&pool, aid)->nation_id = 4;
      units_get(&pool, did)->nation_id = 0;
      ColonizeDosRng rng;
      /* Small seeds draw small first values (the "tiny-seed RNG fixture" trap
       * in bugs.md); spread them so the 14-vs-240 roll can actually land. */
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
      if (units_resolve_land_combat_ff(&pool, aid, did, &rng, &col1)) {
        fprintf(stderr, "brave_arty: seed %d — Brave beat HUMAN Artillery\n", seed);
        rc = 1;
      }
      units_despawn(&pool, aid);
      units_despawn(&pool, did);
    }
    /* AI-controlled European Artillery: no auto-loss, the roll decides. */
    if (rc == 0) {
      const int aid = units_spawn_allow_stack(&pool, brave, 4, 6);
      const int did = units_spawn_allow_stack(&pool, arty, 5, 6);
      if (aid < 0 || did < 0) {
        fprintf(stderr, "brave_arty: spawn failed\n");
        rc = 1;
        break;
      }
      units_get(&pool, aid)->nation_id = 4;
      units_get(&pool, did)->nation_id = 1;
      ColonizeDosRng rng;
      /* Small seeds draw small first values (the "tiny-seed RNG fixture" trap
       * in bugs.md); spread them so the 14-vs-240 roll can actually land. */
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
      if (units_resolve_land_combat_ff(&pool, aid, did, &rng, &col1)) {
        ++ai_def_wins;
      }
      units_despawn(&pool, aid);
      units_despawn(&pool, did);
    }
  }
  if (rc == 0 && ai_def_wins == 0) {
    fprintf(stderr, "brave_arty: AI-Euro Artillery never lost in 40 rolls "
                    "(gate must not cover control != 0)\n");
    rc = 1;
  }
  /* Armed Braves (type 0x14) are outside the DOS gate — the roll decides. */
  if (rc == 0) {
    const int armed = units_find_type(&pool, "Armed Braves");
    int armed_wins = 0;
    for (int seed = 1; seed <= 40 && armed >= 0; ++seed) {
      const int aid = units_spawn_allow_stack(&pool, armed, 4, 7);
      const int did = units_spawn_allow_stack(&pool, arty, 5, 7);
      if (aid < 0 || did < 0) {
        break;
      }
      units_get(&pool, aid)->nation_id = 4;
      units_get(&pool, did)->nation_id = 0;
      ColonizeDosRng rng;
      /* Small seeds draw small first values (the "tiny-seed RNG fixture" trap
       * in bugs.md); spread them so the 14-vs-240 roll can actually land. */
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
      if (units_resolve_land_combat_ff(&pool, aid, did, &rng, &col1)) {
        ++armed_wins;
      }
      units_despawn(&pool, aid);
      units_despawn(&pool, did);
    }
    if (armed >= 0 && armed_wins == 0) {
      fprintf(stderr, "brave_arty: Armed Braves never won — gate is too wide\n");
      rc = 1;
    }
  }

  units_set_combat_human_nation(-1);
  units_set_ff_col1(NULL);
  units_set_occupancy_map(NULL);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  if (rc == 0) {
    fprintf(stderr, "unit_units: Brave vs human Artillery auto-loss (1b0e) ok\n");
  }
  return rc;
}

/* One handicap application on a fresh result blob; returns the peeled attacker. */
static int handicap_atk(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  int atk_in
) {
  ColonizeCombatEngageResult er;
  memset(&er, 0, sizeof(er));
  combat_side_flags_clear(&er.atk_flags);
  combat_side_flags_clear(&er.def_flags);
  er.atk_strength = atk_in;
  er.def_strength = 64;
  combat_apply_1b0e_resolve_handicaps(ctx, attacker_id, defender_id, &er);
  return er.atk_strength;
}

/* Same, for the colony-tile tail's defender arm (raw 100561-100563). */
static int handicap_def(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  int def_in
) {
  ColonizeCombatEngageResult er;
  memset(&er, 0, sizeof(er));
  combat_side_flags_clear(&er.atk_flags);
  combat_side_flags_clear(&er.def_flags);
  er.atk_strength = 100;
  er.def_strength = def_in;
  combat_apply_1b0e_resolve_handicaps(ctx, attacker_id, defender_id, &er);
  return er.def_strength;
}

/*
 * FUN_5fef_1b0e difficulty-handicap group (raw 100534-100556), called directly:
 * combat_apply_1b0e_resolve_handicaps(). Table-driven cover of the three DOS
 * blocks and their gates —
 *   gate: 0x53a6 < 2 && (!WoI || no colony under the defender || attacker hull)
 *   A:    human-Euro defender + turn < 0x50 + colony → −25% (diff 0) / −50%
 *         (diff 1), then ZERO when the defender is the 1b0e auto-spawned
 *         phantom (bVar28) on diff 0;
 *   B:    human-Euro defender + (Euro attacker || turn < 0x50) → a further >>1;
 *   C:    unconditional (outside the gate): diff 0 + human-Euro ATTACKER → <<1.
 * The integration side of block A lives in the beginner-shield sub-test below.
 */
static int unit_1b0e_resolve_handicaps(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "1b0e-handicap: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "1b0e-handicap: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  const int soldier = units_find_type(&pool, "Soldiers");
  const int brave = units_find_type(&pool, "Braves");
  const int frigate = units_find_type(&pool, "Frigate");
  if (soldier < 0 || brave < 0 || frigate < 0) {
    fprintf(stderr, "1b0e-handicap: types missing (%d/%d/%d)\n", soldier, brave, frigate);
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 8;
  map.height = 8;
  map.tile_count = 64;
  map.terrain = calloc(64, 1);
  map.layer2 = calloc(64, 1);
  map.layer3 = calloc(64, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return 1;
  }
  for (int i = 0; i < 64; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  units_set_occupancy_map(&map);

  /* One colony at (5,5); the off-colony defenders sit elsewhere. */
  ColonizeColonyPool cols;
  colonies_init(&cols);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &cols.colonies[0];
  col->id = 0;
  col->active = true;
  col->nation_id = 0;
  col->x = 5;
  col->y = 5;
  cols.colony_count = 1;

  /*
   * Control bytes explicitly for every Euro slot: a zeroed ColonizeCol1Save
   * reads as control 0 = HUMAN on all four, which would make every nation
   * "human European" and quietly satisfy blocks A/B/C at once.
   */
  ColonizeCol1Save c1;
  memset(&c1, 0, sizeof(c1));
  memset(c1.head.founding_father, 0xff, sizeof(c1.head.founding_father));
  c1.player[0].control = 0; /* human */
  c1.player[1].control = 1; /* AI */
  c1.player[2].control = 1; /* AI */
  c1.player[3].control = 1; /* AI */
  c1.head.difficulty = 0;
  c1.head.turn = 10;
  c1.head.game_options.woi = 0;

  ColonizeCombatStrengthCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;
  ctx.map = &map;
  ctx.colonies = &cols;
  ctx.col1 = &c1;

  const int a_ai = units_spawn_allow_stack(&pool, soldier, 1, 1); /* AI Euro attacker */
  const int a_human = units_spawn_allow_stack(&pool, soldier, 2, 2); /* human Euro attacker */
  const int a_native = units_spawn_allow_stack(&pool, brave, 3, 3); /* native attacker */
  const int a_ship = units_spawn_allow_stack(&pool, frigate, 4, 4); /* AI Euro hull */
  const int d_human_on = units_spawn_allow_stack(&pool, soldier, 5, 5); /* human, on colony */
  const int d_human_off = units_spawn_allow_stack(&pool, soldier, 6, 6); /* human, open field */
  const int d_ai_off = units_spawn_allow_stack(&pool, soldier, 7, 7); /* AI Euro, open field */
  if (a_ai < 0 || a_human < 0 || a_native < 0 || a_ship < 0 || d_human_on < 0 ||
      d_human_off < 0 || d_ai_off < 0) {
    fprintf(stderr, "1b0e-handicap: spawn failed\n");
    units_set_occupancy_map(NULL);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return 1;
  }
  units_get(&pool, a_ai)->nation_id = 1;
  units_get(&pool, a_human)->nation_id = 0;
  units_get(&pool, a_native)->nation_id = 4;
  units_get(&pool, a_ship)->nation_id = 1;
  units_get(&pool, d_human_on)->nation_id = 0;
  units_get(&pool, d_human_off)->nation_id = 0;
  units_get(&pool, d_ai_off)->nation_id = 1;

  int rc = 0;
  struct {
    const char* label;
    uint8_t difficulty;
    int turn;
    int woi;
    int auto_defender;
    int attacker;
    int defender;
    int want;
  } cases[] = {
    /* 1: diff 0, AI attacker on a human colony inside the 0x50 window: A then B. */
    {"diff0 colony A+B", 0, 10, 0, 0, a_ai, d_human_on, ((100 - (100 >> 2)) >> 1)},
    /* 2: diff 1 takes A's >>1 arm, then B. */
    {"diff1 colony A+B", 1, 10, 0, 0, a_ai, d_human_on, ((100 >> 1) >> 1)},
    /* 3: block C only — human Euro attacker vs an AI defender in the open. */
    {"diff0 human attacker doubles", 0, 10, 0, 0, a_human, d_ai_off, 200},
    /* 4: past turn 0x50 a native attacker satisfies neither A (no colony) nor
     *    B (not Euro, turn too late) nor C (not a human European). */
    {"late native vs human, no colony", 0, 0x50, 0, 0, a_native, d_human_off, 100},
    /* 5: B alone — a Euro attacker keeps the >>1 forever, colony or not. */
    {"late AI Euro vs human, B only", 0, 0x50, 0, 0, a_ai, d_human_off, 50},
    /* 6: bVar28 beginner shield zeroes the attacker on Discoverer. */
    {"diff0 auto-defender shield", 0, 10, 0, 1, a_ai, d_human_on, 0},
    /* 7: WoI + colony + land attacker closes the gate: no A, no B, C still on. */
    {"WoI colony land: gate shut", 0, 10, 1, 0, a_human, d_human_on, 200},
    /* 7b: a hull re-opens the very same gate (uVar19 0xd..0x12). */
    {"WoI colony ship: gate open", 0, 10, 1, 0, a_ship, d_human_on, ((100 - (100 >> 2)) >> 1)},
    /* 8: Conquistador and up: outside `0x53a6 < 2`, and C wants diff 0. */
    {"diff2 no dampers no doubling", 2, 10, 0, 0, a_human, d_human_on, 100},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]) && rc == 0; ++i) {
    c1.head.difficulty = cases[i].difficulty;
    c1.head.turn = (uint16_t)cases[i].turn;
    c1.head.game_options.woi = (uint8_t)(cases[i].woi ? 1 : 0);
    combat_set_auto_defender(cases[i].auto_defender != 0);
    const int got = handicap_atk(&ctx, cases[i].attacker, cases[i].defender, 100);
    combat_set_auto_defender(false);
    if (got != cases[i].want) {
      fprintf(
        stderr,
        "1b0e-handicap [%s]: atk 100 -> %d, want %d\n",
        cases[i].label,
        got,
        cases[i].want
      );
      rc = 1;
    }
  }

  /* The latch must not leak into the next engagement (DOS clears bVar28 per roll). */
  if (rc == 0) {
    c1.head.difficulty = 0;
    c1.head.turn = 10;
    c1.head.game_options.woi = 0;
    combat_set_auto_defender(true);
    (void)handicap_atk(&ctx, a_ai, d_human_on, 100);
    combat_set_auto_defender(false);
    const int after = handicap_atk(&ctx, a_ai, d_human_on, 100);
    if (after != ((100 - (100 >> 2)) >> 1)) {
      fprintf(stderr, "1b0e-handicap: auto-defender latch leaked (got %d)\n", after);
      rc = 1;
    }
    if (combat_auto_defender()) {
      fprintf(stderr, "1b0e-handicap: combat_auto_defender() stuck on\n");
      rc = 1;
    }
  }

  /*
   * Colony-tile tail, raw 100557-100564. It sits OUTSIDE the `0x53a6 < 2`
   * gate, so drive it at Conquistador where blocks A/B/C are all silent and
   * only this pair can move a number.
   *   (a) native attacker (3 < uVar16) vs a nation on its last colony → 0;
   *   (b) human Euro defender whose colony holds ≥ half the nation's
   *       colonists → defence + (4 − difficulty) * 4.
   * Both read the FUN_4962_0018 census bytes DS:0x9298 / DS:0x940c, mirrored
   * as col1->stuff.colony_counts / colony_pop_totals.
   */
  if (rc == 0) {
    c1.head.difficulty = 2;
    c1.head.turn = 10;
    c1.head.game_options.woi = 0;
    col->population = 3;
    col->colonist_count = 3;
    c1.stuff.colony_pop_totals[0] = 0;

    struct {
      const char* label;
      uint8_t counts0;
      int attacker;
      int defender;
      int want;
    } shield[] = {
      /* Last colony + native attacker: zeroed outright. */
      {"tail(a) last colony vs native", 1, a_native, d_human_on, 0},
      /* Two colonies left: no shield. */
      {"tail(a) two colonies", 2, a_native, d_human_on, 100},
      /* `3 < uVar16` — a European attacker is never shielded. */
      {"tail(a) euro attacker", 1, a_ai, d_human_on, 100},
      /* `-1 < iVar18` — no colony under the defender, no tail. */
      {"tail(a) no colony", 1, a_native, d_human_off, 100},
    };
    for (size_t i = 0; i < sizeof(shield) / sizeof(shield[0]) && rc == 0; ++i) {
      c1.stuff.colony_counts[0] = shield[i].counts0;
      const int got = handicap_atk(&ctx, shield[i].attacker, shield[i].defender, 100);
      if (got != shield[i].want) {
        fprintf(
          stderr,
          "1b0e-handicap [%s]: atk 100 -> %d, want %d\n",
          shield[i].label,
          got,
          shield[i].want
        );
        rc = 1;
      }
    }
    c1.stuff.colony_counts[0] = 2; /* keep the shield out of the defence cases */

    if (rc == 0) {
      /* 6 >> 1 = 3 <= pop 3 → +(4−2)*4 = +8 at Conquistador. */
      c1.stuff.colony_pop_totals[0] = 6;
      int got = handicap_def(&ctx, a_native, d_human_on, 64);
      if (got != 72) {
        fprintf(stderr, "1b0e-handicap [tail(b) half]: def 64 -> %d, want 72\n", got);
        rc = 1;
      }
      /* 8 >> 1 = 4 > pop 3 → silent. */
      c1.stuff.colony_pop_totals[0] = 8;
      got = handicap_def(&ctx, a_native, d_human_on, 64);
      if (rc == 0 && got != 64) {
        fprintf(stderr, "1b0e-handicap [tail(b) under half]: def 64 -> %d, want 64\n", got);
        rc = 1;
      }
      /* Difficulty scales it: Discoverer pays +16. */
      c1.stuff.colony_pop_totals[0] = 6;
      c1.head.difficulty = 0;
      got = handicap_def(&ctx, a_native, d_human_on, 64);
      if (rc == 0 && got != 80) {
        fprintf(stderr, "1b0e-handicap [tail(b) diff0]: def 64 -> %d, want 80\n", got);
        rc = 1;
      }
      /* 0x543f control byte: an AI-run nation gets nothing. */
      c1.player[0].control = 1;
      got = handicap_def(&ctx, a_native, d_human_on, 64);
      c1.player[0].control = 0;
      if (rc == 0 && got != 64) {
        fprintf(stderr, "1b0e-handicap [tail(b) ai defender]: def 64 -> %d, want 64\n", got);
        rc = 1;
      }
      /* No colony under the defender: no defence bonus either. */
      got = handicap_def(&ctx, a_native, d_human_off, 64);
      if (rc == 0 && got != 64) {
        fprintf(stderr, "1b0e-handicap [tail(b) no colony]: def 64 -> %d, want 64\n", got);
        rc = 1;
      }
    }
    c1.stuff.colony_counts[0] = 0;
    c1.stuff.colony_pop_totals[0] = 0;
    c1.head.difficulty = 0;
  }

  units_set_occupancy_map(NULL);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  if (rc == 0) {
    fprintf(stderr, "unit_units: 1b0e resolve handicaps ok\n");
  }
  return rc;
}

/*
 * LIVE path for the colony-tile tail's defender arm (raw 100557-100564).
 * DOS rolls `iVar23 = FUN_281f_04d4(1, local_a8 + local_92)` (raw 100571)
 * AFTER the tail bumped local_a8, so the +(4 − difficulty) * 4 must reach the
 * resolvers' own `total`, not just the helper's blob — the port used to copy
 * back `er.atk_strength` alone and the defence bonus died in the callee.
 *
 * Driven at Conquistador (difficulty 2), where blocks A/B/C are all silent
 * and only the tail can move a number, with rng == NULL so the outcome is the
 * plain `atk >= def` comparison: the attacker's type attack byte is tuned
 * until its final strength straddles the tail's +8, then the identical fight
 * is resolved with the bonus armed (defender must win) and disarmed (attacker
 * must win). Land and naval, since DOS's single resolver covers both.
 */
static int unit_1b0e_defender_bonus_live(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "1b0e-def-live: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "1b0e-def-live: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  const int soldier = units_find_type(&pool, "Soldiers");
  const int frigate = units_find_type(&pool, "Frigate");
  if (soldier < 0 || frigate < 0) {
    fprintf(stderr, "1b0e-def-live: types missing (%d/%d)\n", soldier, frigate);
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 8;
  map.height = 8;
  map.tile_count = 64;
  map.terrain = calloc(64, 1);
  map.layer2 = calloc(64, 1);
  map.layer3 = calloc(64, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return 1;
  }
  for (int i = 0; i < 64; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  units_set_occupancy_map(&map);

  /* Defended tile (5,5) carries the colony; DOS reads the record, not the map. */
  ColonizeColonyPool cols;
  colonies_init(&cols);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &cols.colonies[0];
  col->id = 0;
  col->active = true;
  col->nation_id = 0;
  col->x = 5;
  col->y = 5;
  col->population = 3;
  col->colonist_count = 3;
  cols.colony_count = 1;
  units_set_combat_colonies(&cols);

  /* Zeroed control bytes read as human on all four Euro slots — spell them. */
  ColonizeCol1Save c1;
  memset(&c1, 0, sizeof(c1));
  memset(c1.head.founding_father, 0xff, sizeof(c1.head.founding_father));
  c1.player[0].control = 0; /* human — the defender's nation */
  c1.player[1].control = 1; /* AI attacker */
  c1.player[2].control = 1;
  c1.player[3].control = 1;
  c1.head.difficulty = 2; /* outside `0x53a6 < 2`: only the tail is live */
  c1.head.turn = 10;
  c1.head.game_options.woi = 0;
  c1.stuff.colony_counts[0] = 2; /* keep tail(a)'s last-colony shield out */
  units_set_ff_col1(&c1);
  units_set_combat_human_nation(-1);

  ColonizeCombatStrengthCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;
  ctx.map = &map;
  ctx.colonies = &cols;
  ctx.col1 = &c1;

  int rc = 0;
  for (int is_naval = 0; is_naval <= 1 && rc == 0; ++is_naval) {
    const int type = is_naval ? frigate : soldier;
    const char* label = is_naval ? "naval" : "land";

    /*
     * Same fixture, same seeds, twice: colony_pop_totals 6 (>> 1 = 3 <= the
     * colony's 3 colonists, so the tail pays +(4 - 2) * 4 = +8) and 100
     * (50 > 3, silent). The bump widens the roll's `local_a8 + local_92`
     * range and moves the win line, so the two runs MUST disagree on at
     * least one seed and the armed run must not win more often. With the
     * bonus stranded in the callee both runs are bit-identical.
     */
    int wins[2] = {0, 0};
    int differed = 0;
    for (int s = 0; s < 256 && rc == 0; ++s) {
      /* Spread the seeds: tiny seeds leave the DOS LCG degenerate for its
       * first draws and every fight would come out the same way. */
      const int seed = 12345 + s * 7919;
      bool won[2] = {false, false};
      for (int i = 0; i < 2 && rc == 0; ++i) {
        c1.stuff.colony_pop_totals[0] = (uint8_t)(i == 0 ? 6 : 100);
        const int aid = units_spawn_allow_stack(&pool, type, 4, 5);
        const int did = units_spawn_allow_stack(&pool, type, 5, 5);
        if (aid < 0 || did < 0) {
          fprintf(stderr, "1b0e-def-live [%s]: spawn failed\n", label);
          rc = 1;
          break;
        }
        units_get(&pool, aid)->nation_id = 1;
        units_get(&pool, did)->nation_id = 0;
        ColonizeDosRng rng;
        /* Small seeds draw small first values (the "tiny-seed RNG fixture" trap
       * in bugs.md); spread them so the 14-vs-240 roll can actually land. */
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
        won[i] = is_naval ? units_resolve_naval_combat_ff(&pool, aid, did, &rng, &c1)
                          : units_resolve_land_combat_ff(&pool, aid, did, &rng, &c1);
        wins[i] += won[i] ? 1 : 0;
        units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }
      if (rc == 0 && won[0] != won[1]) {
        ++differed;
      }
    }
    if (rc == 0 && differed == 0) {
      fprintf(
        stderr,
        "1b0e-def-live [%s]: +8 defence never changed an outcome over 256 seeds — "
        "the colony-tail bump is not reaching the resolver's roll\n",
        label
      );
      rc = 1;
    }
    if (rc == 0 && wins[0] > wins[1]) {
      fprintf(
        stderr,
        "1b0e-def-live [%s]: armed bonus won MORE attacks (%d) than the disarmed "
        "run (%d) — sign of the bump is wrong\n",
        label,
        wins[0],
        wins[1]
      );
      rc = 1;
    }
  }

  c1.stuff.colony_pop_totals[0] = 0;
  c1.stuff.colony_counts[0] = 0;
  units_set_ff_col1(NULL);
  units_set_combat_colonies(NULL);
  units_set_occupancy_map(NULL);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  if (rc == 0) {
    fprintf(stderr, "unit_units: 1b0e colony-tail defence bonus reaches the roll ok\n");
  }
  return rc;
}

/*
 * Smell audit 2026-09-09, units/combat batch (#4, #6, #7, #8, #12, #13).
 * Everything here runs on synthetic rosters/pools on purpose — the point of
 * several of these fixes is that a Linux pool index is NOT a DOS @UNIT id.
 */
static void audit_type(
  ColonizeUnitType* t, const char* name, int mv, int atk, int def, ColonizeUnitDomain dom
) {
  memset(t, 0, sizeof(*t));
  snprintf(t->name, sizeof(t->name), "%s", name);
  t->movement = mv;
  t->attack = atk;
  t->defense = def;
  t->domain = dom;
}

static int unit_smell_audit_2026_09_09(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 8;
  map.height = 8;
  map.tile_count = 64;
  map.terrain = calloc(64, 1);
  map.layer2 = calloc(64, 1);
  map.layer3 = calloc(64, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return 1;
  }
  for (int i = 0; i < 64; ++i) {
    map.terrain[i] = 2; /* plains — every tile is land */
  }
  units_set_occupancy_map(&map);

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  memset(col1.head.founding_father, 0xff, sizeof(col1.head.founding_father));
  col1.head.difficulty = 2;
  col1.player[0].control = 0; /* human */
  col1.player[1].control = 1; /* AI */
  units_set_ff_col1(&col1);
  units_set_combat_human_nation(0);

  int rc = 0;

  /*
   * #8 — the Brave-vs-human-Artillery auto-loss must key on the @UNIT NAME
   * family, not on a raw pool index. Here Braves sits at 0 and Artillery at 1,
   * so the old `type_index == 0x13 && == 0x0b` test could never fire.
   */
  {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Braves", 1, 1, 1, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[1], "Artillery", 1, 7, 5, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[2], "Armed Braves", 1, 2, 2, COLONIZE_UNIT_DOMAIN_LAND);
    pool.type_count = 3;

    int armed_wins = 0;
    for (int seed = 1; seed <= 40 && rc == 0; ++seed) {
      const int aid = units_spawn_allow_stack(&pool, 0, 4, 5);
      const int did = units_spawn_allow_stack(&pool, 1, 5, 5);
      units_get(&pool, aid)->nation_id = 4;
      units_get(&pool, did)->nation_id = 0;
      ColonizeDosRng rng;
      /* Small seeds draw small first values (the "tiny-seed RNG fixture" trap
       * in bugs.md); spread them so the 14-vs-240 roll can actually land. */
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
      if (units_resolve_land_combat_ff(&pool, aid, did, &rng, &col1)) {
        fprintf(stderr,
                "audit#8: seed %d — plain Brave beat HUMAN Artillery on a shuffled roster\n",
                seed);
        rc = 1;
      }
      units_despawn(&pool, aid);
      units_despawn(&pool, did);

      if (rc == 0) {
        /* Armed Braves (DOS 0x14) is outside the latch: the roll must decide. */
        const int aid2 = units_spawn_allow_stack(&pool, 2, 4, 6);
        const int did2 = units_spawn_allow_stack(&pool, 1, 5, 6);
        units_get(&pool, aid2)->nation_id = 4;
        units_get(&pool, did2)->nation_id = 0;
        ColonizeDosRng rng2;
        dos_rng_seed(&rng2, seed);
        if (units_resolve_land_combat_ff(&pool, aid2, did2, &rng2, &col1)) {
          ++armed_wins;
        }
        units_despawn(&pool, aid2);
        units_despawn(&pool, did2);
      }
    }
    if (rc == 0 && armed_wins == 0) {
      fprintf(stderr, "audit#8: Armed Braves never won in 40 rolls — latch is too wide\n");
      rc = 1;
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: audit#8 brave/artillery auto-loss is name-matched ok\n");
    }
  }

  /*
   * #13 — FUN_5fef_0000's domain gate reads the SCANNED TILE's
   * ocean_or_high_seas bit (its `param_2` is the first unit standing on the
   * target tile, raw 100353-100354), not the attacker's own ship-ness. A land
   * tile is defended by land units even against a warship attacking out of a
   * harbour berth; a water tile is defended by ships only.
   * #12 rides along: the armed tier ranks a musket-carrying colonist BODY.
   */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Privateer", 8, 8, 4, COLONIZE_UNIT_DOMAIN_SEA);
    audit_type(&pool.types[1], "Caravel", 4, 0, 2, COLONIZE_UNIT_DOMAIN_SEA);
    audit_type(&pool.types[2], "Colonists", 1, 0, 1, COLONIZE_UNIT_DOMAIN_LAND);
    pool.type_count = 3;

    const int atk = units_spawn_allow_stack(&pool, 0, 3, 4); /* berthed: land tile */
    const int hull = units_spawn_allow_stack(&pool, 1, 4, 4);
    const int body = units_spawn_allow_stack(&pool, 2, 4, 4);
    units_get(&pool, atk)->nation_id = 0;
    units_get(&pool, hull)->nation_id = 1;
    units_get(&pool, body)->nation_id = 1;
    units_get(&pool, body)->muskets = 50; /* colony-armed colonist body */

    /* Target tile is LAND: the armed body defends, the moored hull is out of
     * domain — even though the attacker is a ship. */
    const int pick = units_best_defender_at(&pool, &col1, 4, 4, atk, atk);
    if (pick != body) {
      fprintf(stderr,
              "audit#13: land target tile picked %d, want the armed body %d "
              "(hull %d must be out of domain)\n",
              pick, body, hull);
      rc = 1;
    }
    if (rc == 0) {
      /* Same stack on a WATER tile: now only the hull is in domain. */
      map.terrain[4 * 8 + 4] = 25; /* ocean class on the target tile */
      const int sea_pick = units_best_defender_at(&pool, &col1, 4, 4, atk, atk);
      map.terrain[4 * 8 + 4] = 2;
      if (sea_pick != hull) {
        fprintf(stderr, "audit#13: water target tile picked %d, want hull %d\n", sea_pick, hull);
        rc = 1;
      }
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: audit#13 defender domain gate reads the target tile ok\n");
    }
    units_despawn(&pool, atk);
    units_despawn(&pool, hull);
    units_despawn(&pool, body);
  }

  /*
   * #4 — entry seizure takes the non-combat LAND bystanders only. A berthed
   * foreign Caravel (attack 0, no capture/demote row) used to be despawned
   * outright while an armed hull was left alone.
   */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Soldiers", 1, 2, 2, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[1], "Colonists", 1, 0, 1, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[2], "Caravel", 4, 0, 2, COLONIZE_UNIT_DOMAIN_SEA);
    pool.type_count = 3;

    const int win = units_spawn_allow_stack(&pool, 0, 4, 4);
    const int civ = units_spawn_allow_stack(&pool, 1, 4, 4);
    const int hull = units_spawn_allow_stack(&pool, 2, 4, 4);
    units_set_nation(units_get(&pool, win), 0);
    units_set_nation(units_get(&pool, civ), 1);
    units_set_nation(units_get(&pool, hull), 1);

    units_seize_noncombat_at(&pool, win, 4, 4, &col1);

    const ColonizeUnit* h = units_get(&pool, hull);
    const ColonizeUnit* c = units_get(&pool, civ);
    if (!h || !h->active || h->nation_id != 1) {
      fprintf(stderr, "audit#4: berthed Caravel must survive a colony capture untouched\n");
      rc = 1;
    } else if (!c || !c->active || c->nation_id != 0) {
      fprintf(stderr, "audit#4: civilian bystander should have been seized (nation=%d)\n",
              c && c->active ? c->nation_id : -1);
      rc = 1;
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: audit#4 entry seizure spares hulls, takes civilians ok\n");
    }
    units_despawn(&pool, win);
    units_despawn(&pool, civ);
    units_despawn(&pool, hull);
  }

  /*
   * Parked lead (2026-09-09): FUN_5fef_0352's hull arm, raw 99518-99649.
   * FUN_5fef_0ec0 (raw 99719-99730) hands EVERY unit on the swept stack to
   * 0352 with no predicate, and 0352 tests the LOSER's type byte first —
   * `if ((0xc < type) && (type < 0x13))` — so a berthed hull caught by a LAND
   * loss takes the damage/repair-port arm, not a despawn and not a skip.
   * Because the roll at raw 99527 is guarded by `winner_type*0xe + 0x523b != 0`
   * (the @UNIT guns column, zero for every land type), a land winner never
   * draws: the hull is ALWAYS damaged. Assert the whole shape — survives,
   * keeps its flag, carries bit7 and the repair timer, and loses its cargo.
   */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Soldiers", 1, 1, 1, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[1], "Regulars", 1, 30, 30, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[2], "Caravel", 4, 0, 2, COLONIZE_UNIT_DOMAIN_SEA);
    pool.types[2].guns = 0;
    pool.types[2].hull = 4;
    pool.type_count = 3;

    /* Attacker sallies out of its own port and loses; the hull is berthed on
     * the attacker's tile, which is the stack 0ec0 sweeps (raw 100758-100760
     * passes the attacker's own x/y). */
    const int hull = units_spawn_allow_stack(&pool, 2, 4, 4);
    units_set_nation(units_get(&pool, hull), 0);
    units_get(&pool, hull)->hold_goods_type[0] = 3;
    units_get(&pool, hull)->hold_goods_amount[0] = 100;

    int lost = 0;
    for (int seed = 1; seed <= 40 && !lost && rc == 0; ++seed) {
      const int atk = units_spawn_allow_stack(&pool, 0, 4, 4);
      const int def = units_spawn_allow_stack(&pool, 1, 5, 4);
      if (atk < 0 || def < 0) {
        rc = 1;
        break;
      }
      units_set_nation(units_get(&pool, atk), 0);
      units_set_nation(units_get(&pool, def), 1);
      ColonizeDosRng rng;
      /* Small seeds draw small first values (the "tiny-seed RNG fixture" trap
       * in bugs.md); spread them so the 14-vs-240 roll can actually land. */
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
      if (!units_resolve_land_combat_ff(&pool, atk, def, &rng, &col1)) {
        lost = 1;
      }
      if (units_get(&pool, atk) && units_get(&pool, atk)->active) {
        units_despawn(&pool, atk);
      }
      if (units_get(&pool, def) && units_get(&pool, def)->active) {
        units_despawn(&pool, def);
      }
    }
    if (rc == 0 && !lost) {
      fprintf(stderr, "0352-hull: attack-1 vs defense-30 never lost in 40 seeds\n");
      rc = 1;
    }
    const ColonizeUnit* h = rc == 0 ? units_get(&pool, hull) : NULL;
    if (rc == 0 && (!h || !h->active)) {
      fprintf(stderr, "0352-hull: berthed Caravel was destroyed by the land sweep\n");
      rc = 1;
    } else if (rc == 0 && (h->col1_unknown15 & 0x80u) == 0) {
      fprintf(stderr, "0352-hull: damaged bit7 (+0x3148|0x80) not set\n");
      rc = 1;
    } else if (rc == 0 && h->repair_pending == 0) {
      fprintf(stderr, "0352-hull: repair timer not armed\n");
      rc = 1;
    } else if (rc == 0 && h->hold_goods_amount[0] != 0) {
      fprintf(stderr, "0352-hull: damage tail must zero the holds (+0x3150)\n");
      rc = 1;
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: 0352 hull arm damages berthed ships in a land sweep ok\n");
    }
    units_despawn(&pool, hull);
  }

  /*
   * #6 / #7 — moves_left holds REMAINING for Euro units but the DOS SPENT
   * byte for natives, so park/restore must go through the spent-aware
   * helpers. A raw `= 0` park hands a Brave a FULL allotment.
   */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Braves", 1, 1, 1, COLONIZE_UNIT_DOMAIN_LAND);
    pool.type_count = 1;

    const int bid = units_spawn_allow_stack(&pool, 0, 4, 4);
    ColonizeUnit* b = units_get(&pool, bid);
    b->nation_id = 4;
    b->moves_left = 0; /* native: nothing spent = full allotment */
    const int full = units_remaining_mp(&pool, bid);
    if (full <= 0) {
      fprintf(stderr, "audit#6: fresh Brave should read a full allotment, got %d\n", full);
      rc = 1;
    }
    if (rc == 0 && !units_set_orders(&pool, bid, UNITS_ORDER_SENTRY)) {
      fprintf(stderr, "audit#6: Sentry refused\n");
      rc = 1;
    }
    if (rc == 0 && units_remaining_mp(&pool, bid) != 0) {
      fprintf(stderr, "audit#6: parked Brave still has %d MP (raw moves_left=%d)\n",
              units_remaining_mp(&pool, bid), units_get(&pool, bid)->moves_left);
      rc = 1;
    }
    if (rc == 0) {
      units_get(&pool, bid)->park_nights = 1; /* stood there overnight */
      (void)units_wake(&pool, bid);
      if (units_remaining_mp(&pool, bid) != full) {
        fprintf(stderr, "audit#6: woken Brave has %d MP, want %d\n",
                units_remaining_mp(&pool, bid), full);
        rc = 1;
      }
    }
    if (rc == 0) {
      /* #7: the village phantom is spawned native and must not be able to
       * act — under spent semantics that is moves_left = max, not 0. */
      ColonizeCol1Tribe tribe;
      memset(&tribe, 0, sizeof(tribe));
      tribe.x = 6;
      tribe.y = 6;
      tribe.nation_id = 4;
      ColonizeCol1Save vcol1;
      memset(&vcol1, 0, sizeof(vcol1));
      memset(vcol1.head.founding_father, 0xff, sizeof(vcol1.head.founding_father));
      vcol1.head.tribe_count = 1;
      vcol1.tribe = &tribe;
      const int atk = units_spawn_allow_stack(&pool, 0, 5, 6);
      units_get(&pool, atk)->nation_id = 0;
      const int ph = units_spawn_village_temp_defender(&pool, &vcol1, 6, 6, 4, atk);
      if (ph < 0) {
        fprintf(stderr, "audit#7: village temp defender not spawned\n");
        rc = 1;
      } else if (units_remaining_mp(&pool, ph) != 0) {
        fprintf(stderr, "audit#7: phantom has %d MP left (raw moves_left=%d)\n",
                units_remaining_mp(&pool, ph), units_get(&pool, ph)->moves_left);
        rc = 1;
      }
      if (ph >= 0) {
        units_despawn(&pool, ph);
      }
      units_despawn(&pool, atk);
      units_set_ff_col1(&col1);
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: audit#6/#7 native park/restore are spent-aware ok\n");
    }
    units_despawn(&pool, bid);
  }

  units_set_occupancy_map(NULL);
  units_set_ff_col1(NULL);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  return rc;
}

int main(void) {
  diag_init(0, NULL);

  if (unit_smell_audit_2026_09_09() != 0) {
    return 1;
  }

  if (unit_flood_river_pair_step() != 0) {
    return 1;
  }

  if (unit_king_galleon_offer() != 0) {
    return 1;
  }
  if (unit_clearcut_lumber() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_continental_equip_tier() != 0) {
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
  if (unit_pioneer_case8_tail() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_combat_sfx_visibility() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_native_tile_attack_alarm() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_capture_ring_and_alarm_vent() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_brave_vs_human_artillery_autoloss() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_1b0e_resolve_handicaps() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_1b0e_defender_bonus_live() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_wake_passenger_can_land() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_stack_one_click_wakes_and_selects() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_sea_lane_entry() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_europe_arrival_reveals() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_fog_vis_mask_and_snapshot() != 0) {
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
    /*
     * Interior tiles only (map_coords_inset / DOS FUN_137f_000a): AMER2 row 0
     * carries land in columns 1..3, but the outer rim is not a playable tile
     * and no unit may stand there — bugs.md 435. The tests below then step
     * the unit one tile east and want a free water neighbour for the boarding
     * case, so require: (x,y) and (x+1,y) both interior land, and (x+1,y)
     * coastal.
     */
    for (int y = 0; y < map.height && land_x < 0; ++y) {
      for (int x = 0; x < map.width; ++x) {
        if (!map_coords_inset(&map, x, y) || !map_coords_inset(&map, x + 1, y)) {
          continue;
        }
        if (!map_tile_is_land(&map, x, y) || units_id_at(&pool, x, y) >= 0) {
          continue;
        }
        if (!map_tile_is_land(&map, x + 1, y) || units_id_at(&pool, x + 1, y) >= 0) {
          continue;
        }
        bool coastal = false;
        for (int dy = -1; dy <= 1 && !coastal; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int tx = x + 1 + dx;
            const int ty = y + dy;
            if ((dx == 0 && dy == 0) || !map_coords_inset(&map, tx, ty)) {
              continue;
            }
            if (map_tile_is_water(&map, tx, ty) && units_id_at(&pool, tx, ty) < 0) {
              coastal = true;
              break;
            }
          }
        }
        if (!coastal) {
          continue;
        }
        land_x = x;
        land_y = y;
        break;
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
    lf_cargo->moves_left = 3 * UNITS_MP_PER_TILE;
    lf_boat->moves_left = 4 * UNITS_MP_PER_TILE;
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
    if (lf_cargo->moves_left >= 3 * UNITS_MP_PER_TILE) {
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
    colonies_set_occupancy_map(NULL);
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
    /* bugs.md: docking already put the passenger ashore, with no orders — a
     * further disembark call has nothing left to do. */
    const ColonizeUnit* dock_ship_u = units_get_const(&pool, dock_ship);
    const int nd = units_disembark_all(&pool, dock_ship, cx, cy);
    dpax = units_get(&pool, dock_pax);
    if (nd != 0 || !dock_ship_u || dock_ship_u->cargo_count != 0 || !dpax ||
        dpax->aboard_ship_id >= 0 || dpax->orders != 0 || dpax->x != cx || dpax->y != cy) {
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
    /* Interior tiles only — the 1-tile rim is not playable (map_coords_inset /
     * DOS FUN_137f_000a; bugs.md 435), so a unit may not stand or step there. */
    for (int y = 1; y < 6 && px < 0; ++y) {
      for (int x = 1; x < 6; ++x) {
        if (!map_coords_inset(&tmap, x, y) || !map_coords_inset(&tmap, x + 1, y)) {
          continue;
        }
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
    pu->moves_left = 1 * UNITS_MP_PER_TILE; /* pioneer max is 1 — full allotment */
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
    pu_partial->moves_left = 1 * UNITS_MP_PER_TILE;
    if (units_try_move(&pool, pid_partial, &tmap, fx, fy, NULL, NULL) ||
        pu_partial->moves_left != 1 * UNITS_MP_PER_TILE) {
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
      pu_partial->moves_left = 1 * UNITS_MP_PER_TILE;
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
    /* Single-tile query has no `from` tile, so the FA road pair cannot apply:
     * DOS 465b's cost head never discounts on the destination alone. */
    if (map_move_cost_at(&tmap, fx, fy) != 2) {
      fprintf(stderr, "roaded forest cost expected 2 got %d\n", map_move_cost_at(&tmap, fx, fy));
      map_free(&tmap);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pu2->moves_left = 2 * UNITS_MP_PER_TILE;
    if (!units_try_move(&pool, pid2, &tmap, fx, fy, NULL, NULL) ||
        pu2->moves_left != 2 * UNITS_MP_PER_TILE - 1) {
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
    pu3->moves_left = 1 * UNITS_MP_PER_TILE;
    /*
     * Plains road: real DS:0x2f78 threshold byte + 2 (5 turns for
     * non-Hardy, since the live 2026-08-20 capture landed
     * pioneer_threshold[2]=3), not the old "terr_cost 1, one tick"
     * approximation — drive ticks to completion.
     */
    bool road3_ok = true;
    while (!map_tile_has_road(&tmap, px, py)) {
      road3_ok = units_pioneer_road(&pool, pid3, &tmap, pmsg, sizeof(pmsg), NULL, NULL, NULL);
      if (!road3_ok) {
        break;
      }
    }
    if (!road3_ok || !map_tile_has_road(&tmap, px, py) || pu3->tools != 80 || pu3->moves_left != 0) {
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
      colonies_set_occupancy_map(NULL);
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
      pu4->moves_left = 1 * UNITS_MP_PER_TILE;
      pu4->tools = 100;
      bool road4_ok = true;
      while (!map_tile_has_road(&tmap, rx, ry)) {
        road4_ok = units_pioneer_road(&pool, pid4, &tmap, pmsg, sizeof(pmsg), &colonies_road, NULL, NULL);
        if (!road4_ok) {
          break;
        }
      }
      if (!road4_ok || !map_tile_has_road(&tmap, rx, ry)) {
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

    pu3->moves_left = 1 * UNITS_MP_PER_TILE;
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
    /*
     * A non-expert Farmer's unconditional +1 (colony_yield_pipeline) DOES
     * stack with plow (+1 more) — the "doesn't stack" conclusion reached
     * 2026-08-18 used New Amsterdam/Fort Orange's aggregates as its two
     * anchors, but both were computed under the then-undiscovered
     * town-commons plow bug (+2 instead of the real +1), which inflated
     * commons by exactly +1 for those same two plowed colonies — so the
     * "no stacking" fit was curve-fit against a wrong baseline. Once
     * commons plow was corrected, both colonies needed their field total
     * back up by +1, which this stacking plow term supplies exactly,
     * restoring this test's original expectation.
     */
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
      colony_yield_town_commons(&tmap, px, py, 0, 2, &tc);
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
      colony_yield_town_commons(&tmap, fx, fy, 0, 2, &tc);
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
    if (!units_next_goto_step(&pool, uid, &map, NULL, NULL, &nx, &ny)) {
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
      /* Stand in for the engine's per-nation MP refresh
       * (turn_refresh_moves_for_nation); this target does not link turn.c. */
      walker = units_get(&pool, uid);
      if (walker) {
        walker->moves_left = units_max_mp(&pool, uid);
      }
      const int refreshed = walker ? walker->moves_left : 0;
      const int px = walker ? walker->x : -1;
      const int py = walker ? walker->y : -1;
      units_advance_goto(&pool, uid, &map, NULL, NULL);
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
      /*
       * bugs.md: the move slide / combat lunge hide the piece from the base
       * frame by clearing `active`, and `units_display_type_index` resolves
       * through `units_get_const`, which skips inactive units. Reading the
       * chrome corner while the piece is hidden therefore yields -1, and
       * unit_chrome_corner_for_type falls through to BOTTOM_RIGHT — every
       * top-corner class (mounted, artillery, wagons, ships) wore the wrong
       * corner for the length of the animation. Both call sites now hoist
       * the lookup; this pins the trap so an in-loop call is caught here.
       */
      u->active = false;
      if (units_display_type_index(&pool, id) != -1 ||
          unit_chrome_corner_for_type(units_display_type_index(&pool, id), false) !=
            UNIT_CHROME_CORNER_BOTTOM_RIGHT) {
        fprintf(stderr, "expected -1 dtype (default corner) for a hidden unit\n");
        ss_free(&icons);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      u->active = true;
      if (unit_chrome_corner_for_type(units_display_type_index(&pool, id), false) !=
          UNIT_CHROME_CORNER_TOP_LEFT) {
        fprintf(stderr, "hidden-unit dtype probe did not restore\n");
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
    su->moves_left = 3 * UNITS_MP_PER_TILE;
    if (!units_order_fortify(&pool, sid) || su->orders != UNITS_ORDER_FORTIFY ||
        su->moves_left != 0) {
      fprintf(stderr, "fortify order failed orders=%d mp=%d\n", su->orders, su->moves_left);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Overnight promotion (same as turn_refresh_moves_for_nation) — the
     * refresh also counts the night in park_nights, which is what lets
     * the wake refund the allotment (bugs.md: fortified on a PREVIOUS turn
     * moves on activation; same-turn dig-ins do not get refunds). Smell
     * #39: turns_worked is the shared DOS +0x16 clock, no longer used. */
    su->orders = UNITS_ORDER_FORTIFIED;
    su->moves_left = 0;
    su->park_nights = 1;
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
        if (!map_tile_is_land(&map, x, y) && !map_tile_is_high_seas(&map, x, y) &&
            units_id_at(&pool, x, y) < 0) {
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
    colonies_set_occupancy_map(NULL);
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
      sh->moves_left = 4 * UNITS_MP_PER_TILE;
    }
    if (!units_order_anchor(&pool, ship, &cpool) || !sh || sh->orders != UNITS_ORDER_FORTIFY) {
      fprintf(stderr, "anchor order failed orders=%d\n", sh ? sh->orders : -1);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /*
     * Open water, no colony anywhere near, and no colony pool at all: DOS's
     * Fortify (FUN_2b5a_1112) has no harbour or water condition, so both the
     * ORDERS row and the F key must take a ship wherever it floats. This used
     * to need an own colony within one tile, which is what made it look like
     * fortifying at sea worked only sometimes (bugs.md).
     */
    units_clear_orders(&pool, ship);
    sh = units_get(&pool, ship);
    if (sh) {
      sh->moves_left = 4 * UNITS_MP_PER_TILE;
    }
    if (!units_order_anchor(&pool, ship, NULL) || !sh || sh->orders != UNITS_ORDER_FORTIFY) {
      fprintf(stderr, "anchor at sea failed orders=%d\n", sh ? sh->orders : -1);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_clear_orders(&pool, ship);
    sh = units_get(&pool, ship);
    if (sh) {
      sh->moves_left = 4 * UNITS_MP_PER_TILE;
    }
    if (!units_order_fortify(&pool, ship) || !sh || sh->orders != UNITS_ORDER_FORTIFY) {
      fprintf(stderr, "F-key fortify on a ship failed orders=%d\n", sh ? sh->orders : -1);
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
    mu->moves_left = 1 * UNITS_MP_PER_TILE;
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
    a->moves_left = 3 * UNITS_MP_PER_TILE;
    d->nation_id = 4;
    d->moves_left = 1 * UNITS_MP_PER_TILE;
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
    /* bugs.md 249: a land attacker does NOT advance into the vacated tile
     * (only ships and colony-capturing attacks enter). */
    a = units_get(&pool, aid);
    if (!a || a->x != ax || a->y != ay) {
      fprintf(stderr, "attacker should stay put after land win\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (a->moves_left >= 3 * UNITS_MP_PER_TILE) {
      fprintf(stderr, "attack should drain MP even when staying put\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    (void)units_despawn(&pool, aid);
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
    ua->moves_left = 4 * UNITS_MP_PER_TILE;
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

  /*
   * P7.3: Treasure Trains may only board a Galleon, not any ship
   * (Colonization.pdf). units_find_boardable_ship's require_galleon param
   * (2026-08-26 fix — was unconditional, any ship with room qualified).
   */
  {
    const int caravel_t = units_find_type(&pool, "Caravel");
    const int galleon_t = units_find_type(&pool, "Galleon");
    if (caravel_t < 0 || galleon_t < 0) {
      fprintf(stderr, "boardable-ship: Caravel/Galleon type missing\n");
      return 1;
    }
    const int caravel_id = units_spawn(&pool, caravel_t, 20, 20);
    ColonizeUnit* caravel = units_get(&pool, caravel_id);
    if (!caravel) {
      fprintf(stderr, "boardable-ship: Caravel spawn failed\n");
      return 1;
    }
    caravel->nation_id = 1;
    /* Only a Caravel present: plain search finds it, Galleon-only search does not. */
    if (units_find_boardable_ship(&pool, 20, 20, 1, false) != caravel_id) {
      fprintf(stderr, "boardable-ship: plain search should find the Caravel\n");
      return 1;
    }
    if (units_find_boardable_ship(&pool, 20, 20, 1, true) >= 0) {
      fprintf(stderr, "boardable-ship: Galleon-only search must reject a Caravel\n");
      return 1;
    }
    const int galleon_id = units_spawn_allow_stack(&pool, galleon_t, 20, 20);
    ColonizeUnit* galleon = units_get(&pool, galleon_id);
    if (!galleon) {
      fprintf(stderr, "boardable-ship: Galleon spawn failed\n");
      return 1;
    }
    galleon->nation_id = 1;
    if (units_find_boardable_ship(&pool, 20, 20, 1, true) != galleon_id) {
      fprintf(stderr, "boardable-ship: Galleon-only search should now find the Galleon\n");
      return 1;
    }
    units_despawn(&pool, caravel_id);
    units_despawn(&pool, galleon_id);
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
      const int plain = units_conquest_treasure_gold(&col1, 0, &r0, 0);
      const int rich = units_conquest_treasure_gold(&col1, 0, &r1, 1);
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

    /*
     * bugs.md 387: conquest treasure is NOT Cortes-gated. Strip Cortes and burn
     * a village on Conquistador (difficulty 2), where FUN_5fef_31ea's amount is
     * unconditional — (roll 2..6 + 0 Cortes + 0 Spanish) * 10 * 100, i.e.
     * 2000..6000 gold — and a Treasure Train must still appear.
     */
    col1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] &=
      (uint8_t)~(1u << (FF_HERNAN_CORTES % 8));
    col1.head.founding_father[FF_HERNAN_CORTES] = -1; /* unclaimed */
    col1.head.difficulty = 2;
    col1.head.tribe_count = 1;
    col1.tribe[0].x = 12;
    col1.tribe[0].y = 10;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    col1.tribe[0].state.capital = 0;
    tmap.layer3[10 * 20 + 12] = (uint8_t)((4u << 4) | 1u);
    ColonizeDosRng nrng;
    dos_rng_seed(&nrng, 7);
    if (!units_try_native_settlement_fallout(
          &col1, &pool, &tmap, 0, 4, 12, 10, -1, &nrng
        )) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(stderr, "no-Cortes conquer: fallout should destroy dwelling\n");
      return 1;
    }
    int nc_gold = -1;
    int nc_id = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &pool.units[i];
      if (!u->active || u->x != 12 || u->y != 10) {
        continue;
      }
      const ColonizeUnitType* tt = units_type(&pool, u->type_index);
      if (tt && strcmp(tt->name, "Treasure") == 0) {
        nc_gold = u->hold_goods_amount[0] | (u->hold_goods_amount[1] << 8);
        nc_id = u->id;
        break;
      }
    }
    if (nc_gold < 2000 || nc_gold > 6000) {
      free(tmap.layer3);
      free(col1.tribe);
      fprintf(
        stderr, "no-Cortes conquest treasure got %d want 2000..6000\n", nc_gold
      );
      return 1;
    }
    fprintf(stderr, "unit_units: non-Cortes conquest treasure %d ok\n", nc_gold);
    if (nc_id >= 0) {
      units_despawn(&pool, nc_id);
    }
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
    colonies_set_occupancy_map(NULL);
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
    colonies_set_occupancy_map(NULL);
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

      /*
       * Veteran DRAGOONS carry profession 0x17, never 0x15 — FUN_157e_004a
       * grants them the same +50%. (Regression: the peel used to test 0x15
       * only, so every veteran dragoon fought as a green one.)
       */
      const int dti = units_find_type(&pool, "Dragoons");
      if (dti < 0) {
        fprintf(stderr, "vet-dragoon: Dragoons type missing\n");
        return 1;
      }
      const int did2 = units_spawn_allow_stack(&pool, dti, 6, 7);
      ColonizeUnit* du = units_get(&pool, did2);
      if (!du) {
        fprintf(stderr, "vet-dragoon spawn failed\n");
        return 1;
      }
      du->nation_id = 0;
      pool.types[dti].defense = 2;
      ColonizeCombatSideFlags dfl;
      du->profession = UNITS_JOB_NONE;
      const int green = combat_unit_base_x8(&sctx, did2, 0, &dfl);
      if (green != 16 || (dfl.flags & COMBAT_FLAG_VETERAN) != 0) {
        fprintf(stderr, "green dragoon want 16 no-flag got %d flags=%x\n", green, dfl.flags);
        return 1;
      }
      du->profession = UNITS_JOB_DRAGOON;
      const int vet = combat_unit_base_x8(&sctx, did2, 0, &dfl);
      if (vet != 24 || (dfl.flags & COMBAT_FLAG_VETERAN) == 0) {
        fprintf(stderr, "vet dragoon want 24+flag got %d flags=%x\n", vet, dfl.flags);
        return 1;
      }
      units_despawn(&pool, did2);
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
          if (strstr(dlg.atk_rows[i].label, "Attack Bonus")) {
            found_atk_bonus = 1;
          }
          if (strstr(dlg.atk_rows[i].label, "Veteran")) {
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
          if (strstr(dlg.def_rows[i].label, "Attack Bonus")) {
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
        /*
         * Naval attackers DO get the ×3/2 attack factor and DOS prints its
         * row: FUN_5fef_1b0e applies the scale unconditionally
         * (viceroy_unpacked.c 100457-100458 — 1b0e is the one resolver for
         * both domains) and FUN_636c_0000 walks DS:0x8d00 bit 0
         * (viceroy_unpacked.c 101874-101891), which FUN_157e_004a sets on
         * every mode-1 evaluation (8926-8928). No domain test anywhere.
         */
        {
          int naval_atk_bonus = 0;
          for (int i = 0; i < dlg.atk_line_count; ++i) {
            if (strstr(dlg.atk_rows[i].label, "Attack Bonus")) {
              naval_atk_bonus = 1;
            }
          }
          if (!naval_atk_bonus) {
            fprintf(stderr, "naval analysis must list Attack Bonus (DOS 636c bit 0)\n");
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
    colonies_set_occupancy_map(NULL);
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
    const int old_hull = pool.types[caravel_ti].hull;
    pool.types[caravel_ti].defense = 2; /* Fort atk 4 >= 2 → fort wins */
    /* bugs.md 255: fort win rolls damage-vs-sink on the hull (no-rng
     * fallback: hull >= atk → damaged). Hull 0 keeps this check a sink. */
    pool.types[caravel_ti].hull = 0;

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

    /* bugs.md 255: fort miss leaves the ship UNTOUCHED — no MP drain (DOS
     * undoes the temp attacker; the old ship-slow was invented). */
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
    foe->moves_left = 4 * UNITS_MP_PER_TILE;
    if (units_coastal_fort_fire_pulse(&pool, &colonies, &map, &fcol1, NULL, -1, NULL, 0) != 0) {
      fprintf(stderr, "ship-slow: fort should miss high-def ship\n");
      pool.types[caravel_ti].defense = old_def;
      return 1;
    }
    foe = units_get(&pool, slow_id);
    if (!foe || !foe->active || foe->moves_left != 4 * UNITS_MP_PER_TILE) {
      fprintf(
        stderr,
        "fort miss: want untouched ship (moves=%d) got active=%d moves=%d\n",
        4 * UNITS_MP_PER_TILE,
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
      const int old_phull = pool.types[priv_ti].hull;
      pool.types[priv_ti].defense = 2;
      /* bugs.md 255: fort win now rolls damage-vs-sink on the ship's hull
       * (no-rng fallback: hull > fort atk → damaged). Zero the hull so this
       * deterministic check still ends in a sink. */
      pool.types[priv_ti].hull = 0;
      const int pid = units_spawn_allow_stack(&pool, priv_ti, wx, wy);
      ColonizeUnit* pr = units_get(&pool, pid);
      pr->nation_id = 1;
      const int psunk =
        units_coastal_fort_fire_pulse(&pool, &colonies, &map, &fcol1, NULL, -1, NULL, 0);
      if (psunk < 1 || (units_get(&pool, pid) && units_get(&pool, pid)->active)) {
        fprintf(stderr, "Fort should sink Privateer at peace\n");
        pool.types[priv_ti].defense = old_pdef;
        pool.types[priv_ti].hull = old_phull;
        pool.types[caravel_ti].defense = old_def;
        return 1;
      }
      pool.types[priv_ti].defense = old_pdef;
      pool.types[priv_ti].hull = old_phull;
    }

    pool.types[caravel_ti].defense = old_def;
    pool.types[caravel_ti].hull = old_hull;
    fprintf(stderr, "unit_units: coastal fort naval fire ok\n");
  }

  /* LCR rumour: clear + de Soto reveal path. AMER2's rumour nearest the
   * old (8,14) fixture moved to (9,15) on 2026-09-09 when
   * map_procedural_rumour_at dropped the unverified +1 coordinate bias its
   * resource-hash sibling had already lost (smell_audit #98). */
  {
    if (!map_tile_has_rumour(&map, 9, 15)) {
      fprintf(stderr, "AMER2 (9,15) expected procedural rumour\n");
      return 1;
    }
    ColonizeCol1Save lcol1;
    memset(&lcol1, 0, sizeof(lcol1));
    const int scout_ti = units_find_type(&pool, "Scouts");
    if (scout_ti < 0) {
      fprintf(stderr, "Scout type missing for LCR smoke\n");
      return 1;
    }
    const int scid = units_spawn_allow_stack(&pool, scout_ti, 9, 15);
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
    if (map_tile_has_rumour(&map, 9, 15)) {
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
    if (!map_tile_has_rumour(&lmap, 9, 15)) {
      fprintf(stderr, "LCR fresh map (9,15) expected rumour\n");
      map_free(&lmap);
      return 1;
    }
    const int scid2 = units_spawn_allow_stack(&pool, scout_ti, 9, 15);
    scout = units_get(&pool, scid2);
    scout->nation_id = 0;
    if (!units_resolve_lcr_rumour(&pool, scid2, &lmap, &lcol1, NULL, NULL, -1)) {
      map_free(&lmap);
      fprintf(stderr, "LCR resolve with de Soto failed\n");
      return 1;
    }
    if (map_tile_has_rumour(&lmap, 9, 15)) {
      map_free(&lmap);
      fprintf(stderr, "de Soto LCR rumour not cleared\n");
      return 1;
    }
    if (!map_tile_seen_by(&lmap, 9, 15, 0)) {
      map_free(&lmap);
      fprintf(stderr, "de Soto LCR should reveal scout tile\n");
      return 1;
    }
    units_despawn(&pool, scid);
    units_despawn(&pool, scid2);
    map_free(&lmap);
  }

  {
    /* lcr_case5_bonus_used: first vanish (raw case 5) roll → burial
     * (FUN_65dd_0004:103608). Non-Scout, no FF: units_lcr_roll_outcome's
     * base roll is a single un-rerolled pass (P7.1's real state machine —
     * old flat RNG(1,100) percentage table is gone), so the very first
     * dos_rng_range(rng,1,9) call landing on 5 is enough to hit raw case 5
     * directly (skill 0 means the case-5 "kicker" at 103476-103483 is a
     * trivial RNG(1,1), always sticks). Brute-force via the real resolve
     * call so this stays correct regardless of the exact RNG call shape. */
    ColonizeCol1Save c5col1;
    memset(&c5col1, 0, sizeof(c5col1));
    for (int i = 0; i < COLONIZE_COL1_FF_COUNT; ++i) {
      c5col1.head.founding_father[i] = -1;
    }
    const int case5_ti = units_find_type(&pool, "Colonists");
    if (case5_ti < 0) {
      fprintf(stderr, "case5 latch colonist type missing\n");
      return 1;
    }
    uint32_t trespass_seed = 0;
    for (uint32_t s = 1; s < 50000u && trespass_seed == 0; ++s) {
      ColonizeDosRng probe;
      dos_rng_seed(&probe, s);
      if (dos_rng_range(&probe, 1, 9) == 5) {
        trespass_seed = s;
      }
    }
    if (trespass_seed == 0) {
      fprintf(stderr, "case5 latch could not find raw-case-5 RNG seed\n");
      return 1;
    }
    int rx1 = -1;
    int ry1 = -1;
    int rx2 = -1;
    int ry2 = -1;
    for (int y = 0; y < (int)map.height; ++y) {
      for (int x = 0; x < (int)map.width; ++x) {
        if (!map_tile_has_rumour(&map, x, y)) {
          continue;
        }
        if (rx1 < 0) {
          rx1 = x;
          ry1 = y;
        } else if (x != rx1 || y != ry1) {
          rx2 = x;
          ry2 = y;
          break;
        }
      }
      if (rx2 >= 0) {
        break;
      }
    }
    if (rx1 < 0 || rx2 < 0) {
      fprintf(stderr, "case5 latch need two rumour tiles on AMER2\n");
      return 1;
    }
    ColonizeDosRng rng1;
    dos_rng_seed(&rng1, trespass_seed);
    const int sc1 = units_spawn_allow_stack(&pool, case5_ti, rx1, ry1);
    ColonizeUnit* u1 = units_get(&pool, sc1);
    if (!u1) {
      fprintf(stderr, "case5 latch scout spawn failed\n");
      return 1;
    }
    u1->nation_id = 0;
    if (!units_resolve_lcr_rumour(&pool, sc1, &map, &c5col1, &rng1, NULL, 0)) {
      fprintf(stderr, "case5 latch first resolve failed\n");
      return 1;
    }
    if (!c5col1.player[0].lcr_case5_bonus_used) {
      fprintf(stderr, "case5 latch must set lcr_case5_bonus_used\n");
      return 1;
    }
    map.layer2[ry2 * map.width + rx2] &= (uint8_t)~MAP_LAYER2_RUMOUR_CLEARED;
    ColonizeDosRng rng2;
    dos_rng_seed(&rng2, trespass_seed);
    const int sc2 = units_spawn_allow_stack(&pool, case5_ti, rx2, ry2);
    ColonizeUnit* u2 = units_get(&pool, sc2);
    if (!u2) {
      fprintf(stderr, "case5 latch scout2 spawn failed\n");
      return 1;
    }
    u2->nation_id = 0;
    if (!units_resolve_lcr_rumour(&pool, sc2, &map, &c5col1, &rng2, NULL, 0)) {
      fprintf(stderr, "case5 latch second resolve failed\n");
      return 1;
    }
    units_despawn(&pool, sc1);
    units_despawn(&pool, sc2);
    fprintf(stderr, "unit_units: lcr_case5_bonus_used latch ok\n");
  }

  {
    /*
     * P7.1: de Soto's LCR bonus is gated on the explorer being Scout-type
     * (FUN_65dd_0004:103454-103458 — the FF7 check only fires when
     * local_36!=0, and local_36 starts 0 for any non-Scout unit type).
     * A Colonist should therefore roll byte-identically (same RNG
     * consumption, same side effects) whether or not the nation owns de
     * Soto, since de_soto_reroll can never become true for it. Regression
     * test for that gate: run the same seed on the same tile with de Soto
     * off, then on, and require identical gold/colonist-join/vanish side
     * effects both times. */
    int rtx = -1;
    int rty = -1;
    for (int y = 0; y < (int)map.height && rtx < 0; ++y) {
      for (int x = 0; x < (int)map.width; ++x) {
        if (map_tile_has_rumour(&map, x, y)) {
          rtx = x;
          rty = y;
          break;
        }
      }
    }
    if (colonist < 0 || rtx < 0) {
      fprintf(stderr, "de Soto gate test: missing colonist type or rumour tile\n");
      return 1;
    }
    const uint32_t probe_seed = 4242;
    uint32_t gold_delta[2] = {0, 0};
    int colonist_delta[2] = {0, 0};
    bool vanished[2] = {false, false};
    for (int pass = 0; pass < 2; ++pass) {
      ColonizeCol1Save pcol1;
      memset(&pcol1, 0, sizeof(pcol1));
      for (int i = 0; i < COLONIZE_COL1_FF_COUNT; ++i) {
        pcol1.head.founding_father[i] = -1;
      }
      if (pass == 1) {
        pcol1.head.founding_father[FF_HERNANDO_DE_SOTO] = 0;
        pcol1.nation[0].founding_fathers[FF_HERNANDO_DE_SOTO / 8] |=
          (uint8_t)(1u << (FF_HERNANDO_DE_SOTO % 8));
      }
      ColonizeDosRng prng;
      dos_rng_seed(&prng, probe_seed);
      const int uid = units_spawn_allow_stack(&pool, colonist, rtx, rty);
      ColonizeUnit* pu = units_get(&pool, uid);
      if (!pu) {
        fprintf(stderr, "de Soto gate test: spawn failed (pass %d)\n", pass);
        return 1;
      }
      pu->nation_id = 0;
      const uint32_t gold_before = pcol1.nation[0].gold;
      int colonist_before = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        if (pool.units[i].active && pool.units[i].type_index == colonist) {
          colonist_before++;
        }
      }
      if (!units_resolve_lcr_rumour(&pool, uid, &map, &pcol1, &prng, NULL, 0)) {
        fprintf(stderr, "de Soto gate test: resolve failed (pass %d)\n", pass);
        return 1;
      }
      ColonizeUnit* after = units_get(&pool, uid);
      vanished[pass] = !after || !after->active;
      gold_delta[pass] = pcol1.nation[0].gold - gold_before;
      int colonist_after = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        if (pool.units[i].active && pool.units[i].type_index == colonist) {
          colonist_after++;
        }
      }
      colonist_delta[pass] = colonist_after - colonist_before;
      if (!vanished[pass]) {
        units_despawn(&pool, uid);
      }
      map.layer2[rty * map.width + rtx] &= (uint8_t)~MAP_LAYER2_RUMOUR_CLEARED;
    }
    if (vanished[0] != vanished[1] || gold_delta[0] != gold_delta[1] ||
        colonist_delta[0] != colonist_delta[1]) {
      fprintf(
        stderr,
        "de Soto gate test: non-Scout outcome differs with/without de Soto "
        "(vanish %d/%d gold %u/%u colonist-delta %d/%d)\n",
        vanished[0], vanished[1], gold_delta[0], gold_delta[1],
        colonist_delta[0], colonist_delta[1]
      );
      return 1;
    }
    fprintf(stderr, "unit_units: LCR de Soto Scout-type gate ok\n");
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
    /* FUN_65dd_0004:103597 downgrades Vanishes to Nothing for a nation
     * with < 5 census pop and < 3 colonies — give nation 0 an established
     * footprint so the vanish branch is reachable at all. */
    dcol1.stuff.census_pop_proxy[0] = 20;
    dcol1.stuff.colony_counts[0] = 3;
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
      p->moves_left = 3 * UNITS_MP_PER_TILE;
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
      s->moves_left = 3 * UNITS_MP_PER_TILE;
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
          /* Interior tiles only: a rim tile denies with BLOCKED_EDGE before the
           * domain check ever runs (map_coords_inset / DOS FUN_137f_000a). */
          if (map_coords_inset(&map, x, y) && map_tile_is_water(&map, x, y) &&
              abs(x - ax) <= 1 && abs(y - ay) <= 1 && !(x == ax && y == ay)) {
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
      colonies_set_occupancy_map(NULL);
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
      s->moves_left = 5 * UNITS_MP_PER_TILE;
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
      ua->moves_left = 4 * UNITS_MP_PER_TILE;
      /*
       * bugs.md: unarmed transports (Caravel attack=0 in @UNIT) must bounce
       * off a foreign ship, not fight — "Only Privateers and Frigates can
       * attack enemy ships" (GAME.TXT). Confirm the bounce before arming the
       * type below to exercise the actual-combat path.
       */
      const ColonizeEnterReason unarmed_r =
        units_enter_probe(&pool, ua->type_index, &map, wx2, wy2, a, NULL);
      if (unarmed_r != COLONIZE_ENTER_BOUNCE_FOREIGN) {
        fprintf(stderr, "enter-probe unarmed naval bounce expected got %d\n", (int)unarmed_r);
        return 1;
      }
      pool.types[caravel_t].attack = 99;
      pool.types[caravel_t].defense = 1;
      /* Real Caravels carry guns 0 (@UNIT col 9) — a gunless victor can only
       * damage, never sink (DOS 0352). Arm the type so the win clears the
       * tile and the move-through completes. */
      pool.types[caravel_t].guns = 99;
      const ColonizeEnterReason r =
        units_enter_probe(&pool, ua->type_index, &map, wx2, wy2, a, NULL);
      if (r != COLONIZE_ENTER_COMBAT_NAVAL) {
        fprintf(stderr, "enter-probe naval combat expected got %d\n", (int)r);
        return 1;
      }
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

      /*
       * Ship-slow: a fast ship survives the combat-entry MP surcharge with
       * moves left (only slowed), unlike land units whose low max MP is
       * fully consumed by it (attack ends the turn). Cite: FUN_5fef_1b0e
       * `*(char*)(unit+0x3149) += 3` (viceroy_unpacked.c ~100340-100343) —
       * see units_try_move's combat_attack_mp_surcharge comment.
       */
      const int saved_movement = pool.types[caravel_t].movement;
      pool.types[caravel_t].movement = 8;
      const int sa = units_spawn(&pool, caravel_t, wx, wy);
      const int sb = units_spawn_allow_stack(&pool, caravel_t, wx2, wy2);
      ColonizeUnit* sua = units_get(&pool, sa);
      ColonizeUnit* sub = units_get(&pool, sb);
      if (!sua || !sub) {
        fprintf(stderr, "ship-slow spawn failed\n");
        return 1;
      }
      sua->nation_id = 0;
      sub->nation_id = 1;
      sua->moves_left = 8 * UNITS_MP_PER_TILE; /* full MP: attack + ocean step (3 thirds) + 3 surcharge = 6 spent. */
      if (!units_try_move(&pool, sa, &map, wx2, wy2, NULL, NULL)) {
        fprintf(stderr, "ship-slow move combat failed\n");
        return 1;
      }
      sua = units_get(&pool, sa);
      if (!sua || sua->moves_left != 8 * UNITS_MP_PER_TILE - (UNITS_MP_PER_TILE + 3)) {
        fprintf(
          stderr,
          "ship-slow expected 4 moves left after win, got %d\n",
          sua ? sua->moves_left : -1
        );
        return 1;
      }
      units_despawn(&pool, sa);
      pool.types[caravel_t].movement = saved_movement;
      fprintf(stderr, "unit_units: naval combat-entry ship-slow MP surcharge ok\n");
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
    land->moves_left = 3 * UNITS_MP_PER_TILE;
    ship->moves_left = 4 * UNITS_MP_PER_TILE;

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
    /*
     * bugs.md 429: walking aboard from open shore is DOS's 465b_05ca
     * force-to-max — that passenger is spent, so FUN_4720_015c offers it no
     * landfall and the unload is refused until the next turn.
     */
    if (!land->mp_spent_turn) {
      fprintf(stderr, "board-enter: shore boarding must mark the pax spent\n");
      return 1;
    }
    if (units_cargo_can_landfall(&pool, land_id) ||
        units_first_landfall_cargo(&pool, ship_id) >= 0) {
      fprintf(stderr, "board-enter: spent pax must not be landfall-eligible\n");
      return 1;
    }
    if (units_unload_passenger(&pool, ship_id, land_id, &map, lx, ly, NULL)) {
      fprintf(stderr, "board-enter: spent pax must not make landfall\n");
      return 1;
    }
    fprintf(stderr, "board-enter spent pax stays aboard ok\n");
    /* Next turn (DOS day top clears the spent byte): unload onto land for
     * the sentry auto-board test. */
    land->mp_spent_turn = 0;
    if (units_first_landfall_cargo(&pool, ship_id) != land_id) {
      fprintf(stderr, "board-enter: fresh parked pax must be landfall-eligible\n");
      return 1;
    }
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
    ship->moves_left = 4 * UNITS_MP_PER_TILE;
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
      boat->moves_left = 4 * UNITS_MP_PER_TILE;
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
      colonies_set_occupancy_map(NULL);
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
      (void)units_despawn(&pool, aid);
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
        (void)units_despawn(&pool, aid);
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
        (void)units_despawn(&pool, aid);
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
        (void)units_despawn(&pool, aid);
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
      colonies_set_occupancy_map(NULL);
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
        (void)units_despawn(&pool, aid);
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
        (void)units_despawn(&pool, aid);
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
        (void)units_despawn(&pool, aid);
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
      (void)units_despawn(&pool, aid);
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
        (void)units_despawn(&pool, aid);
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
        (void)units_despawn(&pool, aid);
        units_despawn(&pool, did);
      }
      fprintf(stderr, "unit_units: native destroy-pioneer / demote-soldier ok\n");
    }

    /* Naval damage-not-always-sink (DOS 0352): loser hull vs winner guns —
     * a tough hull survives damaged deterministically when hull > guns.
     * Privateer (guns 12) beats Frigate (hull 32) → damaged, not sunk. */
    {
      const int frig = units_find_type(&pool, "Frigate");
      const int priv = units_find_type(&pool, "Privateer");
      if (frig < 0 || priv < 0) {
        fprintf(stderr, "phase2 naval types missing\n");
        return 1;
      }
      units_set_combat_colonies(NULL); /* no reroute target in this fixture */
      pool.types[priv].attack = 20; /* force the win; guns stay 12 */
      const int aid = units_spawn_allow_stack(&pool, priv, 2, 2);
      const int did = units_spawn_allow_stack(&pool, frig, 3, 2);
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
      (void)units_despawn(&pool, aid);
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
      (void)units_despawn(&pool, aid);
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
      a->moves_left = 3 * UNITS_MP_PER_TILE;
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
      a->moves_left = 3 * UNITS_MP_PER_TILE;
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
      a->moves_left = 3 * UNITS_MP_PER_TILE;
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

      (void)units_despawn(&pool, aid);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      units_set_native_fallout_context(NULL, NULL, -1);
      free(c1.tribe);
      fprintf(stderr, "unit_units: village temp Brave + pop drain ok\n");
    }

    /*
     * Undefended Euro colony: token militia defender (W1.8 / P5.4 fix,
     * 2026-08-26). A colony with colonists but no standing soldier and no
     * Paul Revere must still fight back with a weak civilian stand-in —
     * not hand over a free capture. Phantom: never touches the colony's
     * real colonist_count, never lingers on the map afterward.
     */
    {
      const int soldier2 = units_find_type(&pool, "Soldiers");
      if (soldier2 < 0) {
        fprintf(stderr, "colony-temp-defender soldier type missing\n");
        return 1;
      }
      int cx = -1, cy = -1;
      for (int y = 2; y < (int)map.height - 3 && cx < 0; ++y) {
        for (int x = 2; x < (int)map.width - 3 && cx < 0; ++x) {
          if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y) &&
              units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
            cx = x + 1;
            cy = y;
          }
        }
      }
      if (cx < 0) {
        fprintf(stderr, "colony-temp-defender no land pair\n");
        return 1;
      }
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&colonies, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&colonies, &names)) {
        fprintf(stderr, "colony-temp-defender colonies init failed\n");
        return 1;
      }
      const int cid = colonies_found(&colonies, &map, cx, cy, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
      if (cid < 0) {
        fprintf(stderr, "colony-temp-defender found rival colony failed\n");
        return 1;
      }
      ColonizeColony* col = colonies_get_mut(&colonies, cid);
      if (!col) {
        fprintf(stderr, "colony-temp-defender colony fetch failed\n");
        return 1;
      }
      col->nation_id = 1;
      col->population = 1;
      col->colonist_count = 1;
      col->colonists[0].active = true;

      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;
      /* No Revere owned, no muskets stock — Revere override must not apply. */

      const int aid = units_spawn(&pool, soldier2, cx - 1, cy);
      ColonizeUnit* a = units_get(&pool, aid);
      if (!a) {
        fprintf(stderr, "colony-temp-defender attacker spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      a->moves_left = 5 * UNITS_MP_PER_TILE;
      pool.types[soldier2].attack = 8;
      pool.types[soldier2].defense = 1;

      units_set_ff_col1(&c1);
      units_set_combat_human_nation(0);
      units_set_occupancy_map(&map);

      if (!units_try_move(&pool, aid, &map, cx, cy, &colonies, NULL)) {
        fprintf(
          stderr,
          "colony-temp-defender attack should win (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      if (units_last_combat_outcome() <= 0) {
        fprintf(stderr, "colony-temp-defender expected a real combat win, not a free capture\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      col = colonies_get_mut(&colonies, cid);
      if (!col || col->nation_id != 0) {
        fprintf(stderr, "colony-temp-defender attacker should capture colony\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      if (col->colonist_count != 1 || !col->colonists[0].active) {
        fprintf(stderr, "colony-temp-defender phantom must not touch real colonist_count\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->id != aid && u->x == cx && u->y == cy) {
          fprintf(stderr, "colony-temp-defender phantom should not remain on the tile\n");
          units_set_ff_col1(NULL);
          return 1;
        }
      }

      (void)units_despawn(&pool, aid);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      fprintf(stderr, "unit_units: undefended colony token militia ok\n");
    }

    /*
     * smell_audit 2026-09-09 #2 — the militia / Paul Revere phantom is DOS's
     * scratch @UNIT row 0x17 (built by FUN_291f_0a20 = FUN_478c_002c, raw
     * 76545-76564) and FUN_5fef_1b0e deletes it with FUN_291f_0a06 at raw
     * 100636-100639, BEFORE the win/lose branch:
     *
     *   if (bVar28) { FUN_291f_0a06(0x281f); }
     *   if (bVar8) { ... if (bVar28) { town consequences } else { 0352 } }
     *
     * So the phantom never reaches FUN_5fef_0352 — no @COLONISTCAPTURE, no
     * @DEMOTE, no nation flip — and never reaches FUN_5fef_172c, whose
     * opening `type byte == 1 || type byte == 4` gate a 0x17 row fails
     * before FUN_281f_04d4, so a phantom that WINS draws no promotion RNG.
     */
    {
      const int mil_sol = units_find_type(&pool, "Soldiers");
      const int mil_atk_ty = units_find_type(&pool, "Dragoons");
      /*
       * Half 2's attacker must lose without any 0352 chrome of its OWN, or
       * the popup assertion cannot tell the attacker's legitimate @DEMOTE
       * from a phantom one: Artillery takes the damaged bit and @ARTILLERY
       * (tag COMBAT_SHIP), never CAPTURE/DEMOTE.
       */
      const int mil_loser_ty = units_find_type(&pool, "Artillery");
      if (mil_sol < 0 || mil_atk_ty < 0 || mil_loser_ty < 0) {
        fprintf(stderr, "militia-phantom types missing\n");
        return 1;
      }
      ColonizeColonyPool mcols;
      colonies_init(&mcols);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&mcols, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&mcols, &names)) {
        fprintf(stderr, "militia-phantom colonies init failed\n");
        return 1;
      }
      int mx = -1, my = -1, mcid = -1;
      for (int y = (int)map.height / 2; y < (int)map.height - 4 && mcid < 0; ++y) {
        for (int x = 3; x < (int)map.width - 4 && mcid < 0; ++x) {
          if (!map_tile_is_land(&map, x, y) || !map_tile_is_land(&map, x + 1, y)) {
            continue;
          }
          const int cand =
            colonies_found(&mcols, &map, x + 1, y, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
          if (cand >= 0) {
            mx = x + 1;
            my = y;
            mcid = cand;
          }
        }
      }
      ColonizeColony* mcol = mcid >= 0 ? colonies_get_mut(&mcols, mcid) : NULL;
      if (!mcol) {
        fprintf(stderr, "militia-phantom found colony failed\n");
        return 1;
      }
      /* Shared pool: purge survivors parked on the two tiles under test. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->y == my && (u->x == mx || u->x == mx - 1)) {
          (void)units_despawn(&pool, u->id);
        }
      }
      mcol->nation_id = 1;
      mcol->population = 2;
      mcol->colonist_count = 2;
      mcol->colonists[0].active = true;
      mcol->colonists[1].active = true;

      ColonizeCol1Save mc1;
      memset(&mc1, 0, sizeof(mc1));
      for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
        mc1.head.founding_father[i] = -1; /* zeroed = "nation 0 owns them all" */
      }
      mc1.player[0].control = 0; /* human attacker */
      mc1.player[1].control = 1;
      mc1.head.difficulty = 3; /* off the Discoverer beginner shield */
      mc1.head.turn = 200;

      units_set_ff_col1(&mc1);
      units_set_combat_human_nation(0);
      units_set_occupancy_map(&map);
      units_set_combat_popups(&pops, NULL);

      /*
       * Half 1 — attacker WINS: the phantom must not be captured or demoted,
       * and neither popup may be enqueued. NULL rng makes the roll
       * deterministic (strength compare) and keeps every promotion path shut,
       * so a COMBAT_CAPTURE / COMBAT_DEMOTE entry could only come from the
       * phantom's own 0352 tail.
       */
      pool.types[mil_atk_ty].attack = 8;
      pool.types[mil_atk_ty].defense = 1;
      pool.types[mil_sol].attack = 2;
      pool.types[mil_sol].defense = 2;
      ai_popup_clear(&pops);
      const int mil_aid = units_spawn(&pool, mil_atk_ty, mx - 1, my);
      ColonizeUnit* mil_a = units_get(&pool, mil_aid);
      if (!mil_a) {
        fprintf(stderr, "militia-phantom attacker spawn failed\n");
        return 1;
      }
      mil_a->nation_id = 0;
      mil_a->moves_left = 5 * UNITS_MP_PER_TILE;
      (void)units_try_move(&pool, mil_aid, &map, mx, my, &mcols, NULL);
      if (units_last_combat_outcome() <= 0) {
        fprintf(
          stderr,
          "militia-phantom attacker should beat the token militia (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_CAPTURE ||
            pops.queue[i].tag == AI_POPUP_TAG_COMBAT_DEMOTE) {
          fprintf(
            stderr,
            "militia-phantom: 0352 popup on the scratch 0x17 row (tag=%d body=[%s])\n",
            (int)pops.queue[i].tag,
            pops.queue[i].body
          );
          units_set_ff_col1(NULL);
          return 1;
        }
      }
      (void)units_despawn(&pool, mil_aid);

      /*
       * Half 2 — the PHANTOM wins. Arm it through Revere (FF 12 + muskets >
       * 0x31) so it spawns as the Soldiers body whose profession/kit would
       * otherwise satisfy units_promote_on_win; DOS's 172c type-byte gate
       * rejects it before FUN_281f_04d4, so the engagement must consume
       * exactly ONE draw — the combat roll itself.
       */
      mcol = colonies_get_mut(&mcols, mcid);
      mcol->nation_id = 1; /* half 1 flipped it — hand the town back */
      mcol->population = 2;
      mcol->colonist_count = 2;
      mcol->colonists[0].active = true;
      mcol->colonists[1].active = true;
      mcol->stock[COLONIZE_CARGO_MUSKETS] = 100;
      mc1.nation[1].founding_fathers[FF_PAUL_REVERE / 8] |=
        (uint8_t)(1u << (FF_PAUL_REVERE % 8));
      pool.types[mil_loser_ty].attack = 1;
      pool.types[mil_loser_ty].defense = 1;
      /* The phantom's own row: 2/2 in DOS, floored up here so the roll lands
       * on the defender for most seeds — the branch under test is the tail,
       * not the odds. */
      pool.types[mil_sol].defense = 60;

      int mil_seed = -1;
      int mil_draws = -1;
      for (uint32_t seed = 1; seed <= 400 && mil_seed < 0; ++seed) {
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &pool.units[i];
          if (u->active && u->y == my && (u->x == mx || u->x == mx - 1)) {
            (void)units_despawn(&pool, u->id);
          }
        }
        /* A previous iteration may have taken the town — hand it back. */
        mcol = colonies_get_mut(&mcols, mcid);
        mcol->nation_id = 1;
        mcol->population = 2;
        mcol->colonist_count = 2;
        mcol->colonists[0].active = true;
        mcol->colonists[1].active = true;
        mcol->stock[COLONIZE_CARGO_MUSKETS] = 100;
        const int lid = units_spawn(&pool, mil_loser_ty, mx - 1, my);
        ColonizeUnit* la = units_get(&pool, lid);
        if (!la) {
          fprintf(stderr, "militia-phantom loser spawn failed\n");
          return 1;
        }
        la->nation_id = 0;
        la->moves_left = 5 * UNITS_MP_PER_TILE;
        ai_popup_clear(&pops);
        ColonizeDosRng mrng;
        dos_rng_seed(&mrng, seed);
        const uint32_t start_state = mrng.state;
        (void)units_try_move(&pool, lid, &map, mx, my, &mcols, &mrng);
        if (units_last_combat_outcome() >= 0) {
          if (units_get(&pool, lid)) {
            (void)units_despawn(&pool, lid);
          }
          continue; /* attacker survived/won — not the case under test */
        }
        /* Count LCG advances: the stream is a pure state machine. */
        ColonizeDosRng replay;
        replay.state = start_state;
        int steps = -1;
        for (int k = 0; k <= 32; ++k) {
          if (replay.state == mrng.state) {
            steps = k;
            break;
          }
          (void)dos_rng_next(&replay);
        }
        mil_seed = (int)seed;
        mil_draws = steps;
        if (units_get(&pool, lid)) {
          (void)units_despawn(&pool, lid);
        }
      }
      if (mil_seed < 0) {
        fprintf(stderr, "militia-phantom: no seed made the phantom win\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      if (mil_draws != 1) {
        fprintf(
          stderr,
          "militia-phantom: seed %d consumed %d RNG draws, expected 1 "
          "(combat roll only — 172c must not roll for a 0x17 row)\n",
          mil_seed,
          mil_draws
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      for (int i = 0; i < pops.queue_count; ++i) {
        if (pops.queue[i].tag == AI_POPUP_TAG_COMBAT_CAPTURE ||
            pops.queue[i].tag == AI_POPUP_TAG_COMBAT_DEMOTE) {
          fprintf(
            stderr,
            "militia-phantom: winning phantom produced a 0352/172c popup (tag=%d body=[%s])\n",
            (int)pops.queue[i].tag,
            pops.queue[i].body
          );
          units_set_ff_col1(NULL);
          return 1;
        }
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->x == mx && u->y == my) {
          fprintf(stderr, "militia-phantom: phantom must not survive the roll\n");
          units_set_ff_col1(NULL);
          return 1;
        }
      }

      ai_popup_clear(&pops);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      fprintf(
        stderr, "unit_units: militia phantom bypasses 0352/172c ok (seed %d)\n", mil_seed
      );
    }

    /*
     * Discoverer beginner shield — FUN_5fef_1b0e raw 100536-100545.
     * `if ((bVar28) && (*(char *)0x53a6 == '\0')) local_92 = 0;` inside
     * `0x53a6 < 2` / defender-is-a-human-European / turn < 0x50 / colony on
     * the tile. local_92 is the ATTACKER (it is what the roll compares
     * against), so on Discoverer an attack on a human player's UNDEFENDED
     * town — the militia phantom, bVar28 — can never be won, however strong
     * the attacker. Above difficulty 0 the same fixture must still fall.
     */
    {
      const int soldier3 = units_find_type(&pool, "Soldiers");
      if (soldier3 < 0) {
        fprintf(stderr, "beginner-shield soldier type missing\n");
        return 1;
      }
      int sx = -1, sy = -1;
      for (int y = (int)map.height - 4; y > 2 && sx < 0; --y) {
        for (int x = (int)map.width - 4; x > 2 && sx < 0; --x) {
          if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x - 1, y) &&
              units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x - 1, y) < 0) {
            sx = x;
            sy = y;
          }
        }
      }
      if (sx < 0) {
        fprintf(stderr, "beginner-shield no land pair\n");
        return 1;
      }
      /* Shared pool: earlier sub-tests park survivors around. Purge the two
       * tiles this block asserts on. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->y == sy && (u->x == sx || u->x == sx - 1)) {
          (void)units_despawn(&pool, u->id);
        }
      }

      ColonizeColonyPool scol;
      colonies_init(&scol);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&scol, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&scol, &names)) {
        fprintf(stderr, "beginner-shield colonies init failed\n");
        return 1;
      }
      const int scid = colonies_found(&scol, &map, sx, sy, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
      ColonizeColony* scolony = scid >= 0 ? colonies_get_mut(&scol, scid) : NULL;
      if (!scolony) {
        fprintf(stderr, "beginner-shield found colony failed\n");
        return 1;
      }
      scolony->nation_id = 1;
      scolony->population = 1;
      scolony->colonist_count = 1;
      scolony->colonists[0].active = true;

      ColonizeCol1Save sc1;
      memset(&sc1, 0, sizeof(sc1));
      sc1.player[0].control = 1; /* AI raider */
      sc1.player[1].control = 0; /* the human's town — DOS 0x543f == 0 */
      sc1.head.difficulty = 0; /* Discoverer */
      sc1.head.turn = 10; /* inside the DS:0x538e < 0x50 window */

      pool.types[soldier3].attack = 8;
      pool.types[soldier3].defense = 1;
      units_set_ff_col1(&sc1);
      units_set_combat_human_nation(1);
      units_set_occupancy_map(&map);

      const int shield_atk = units_spawn(&pool, soldier3, sx - 1, sy);
      ColonizeUnit* sa = units_get(&pool, shield_atk);
      if (!sa) {
        fprintf(stderr, "beginner-shield attacker spawn failed\n");
        return 1;
      }
      sa->nation_id = 0;
      sa->moves_left = 5 * UNITS_MP_PER_TILE;
      if (units_try_move(&pool, shield_atk, &map, sx, sy, &scol, NULL)) {
        fprintf(stderr, "beginner-shield: Discoverer attacker must not take the town\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      scolony = colonies_get_mut(&scol, scid);
      if (!scolony || scolony->nation_id != 1) {
        fprintf(stderr, "beginner-shield: colony must stay with its owner\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      {
        /* The loser may be demoted rather than destroyed — what matters is
         * that it never set foot in the town, and that the phantom (which
         * always evaporates) left nothing behind. */
        const ColonizeUnit* loser = units_get(&pool, shield_atk);
        if (loser && loser->x == sx && loser->y == sy) {
          fprintf(stderr, "beginner-shield: beaten attacker must not enter the colony\n");
          units_set_ff_col1(NULL);
          return 1;
        }
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->x == sx && u->y == sy) {
          fprintf(stderr, "beginner-shield: phantom must not remain on the tile\n");
          units_set_ff_col1(NULL);
          return 1;
        }
      }

      /* Same fixture at difficulty 2: outside DOS's `0x53a6 < 2` gate, so the
       * shield is off and the identical attack carries the town. */
      sc1.head.difficulty = 2;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &pool.units[i];
        if (u->active && u->x == sx - 1 && u->y == sy) {
          (void)units_despawn(&pool, u->id);
        }
      }
      const int hard_atk = units_spawn(&pool, soldier3, sx - 1, sy);
      ColonizeUnit* ha = units_get(&pool, hard_atk);
      if (!ha) {
        fprintf(stderr, "beginner-shield hard attacker spawn failed\n");
        return 1;
      }
      ha->nation_id = 0;
      ha->moves_left = 5 * UNITS_MP_PER_TILE;
      if (!units_try_move(&pool, hard_atk, &map, sx, sy, &scol, NULL)) {
        fprintf(
          stderr,
          "beginner-shield: above Discoverer the same attack should win (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      scolony = colonies_get_mut(&scol, scid);
      if (!scolony || scolony->nation_id != 0) {
        fprintf(stderr, "beginner-shield: difficulty 2 attacker should capture the colony\n");
        units_set_ff_col1(NULL);
        return 1;
      }

      (void)units_despawn(&pool, hard_atk);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      fprintf(stderr, "unit_units: Discoverer undefended-town shield ok\n");
    }

    /*
     * FUN_5fef_1b0e port-ship fate + DS:0x54f6 discharge (2026-09-08).
     *
     * (1) A foreign hull berthed in a colony is invisible to a land assault:
     *     FUN_5fef_0000's domain gate (raw 99190-99196) skips it as a
     *     defender, and the capture arm (raw 100905-101034) never touches the
     *     unit array. So the town falls with the ship still in it, still
     *     flying the loser's flag — not sunk, not seized.
     * (2) FUN_5fef_1b0e site 1 (raw 101039-101041): a native attacker that
     *     resolves against a European clears tribe[t].alarm[euro] (the whole
     *     DS:0x54f6 attitude word),
     *     unless it LOST at a colony (DOS routes that to FUN_5fef_0f14).
     */
    {
      const int soldier3 = units_find_type(&pool, "Soldiers");
      const int frig3 = units_find_type(&pool, "Frigate");
      const int brave3 = units_find_type(&pool, "Braves");
      if (soldier3 < 0 || frig3 < 0 || brave3 < 0) {
        fprintf(stderr, "1b0e-port-ship types missing\n");
        return 1;
      }
      int cx = -1, cy = -1;
      for (int y = 2; y < (int)map.height - 3 && cx < 0; ++y) {
        for (int x = 2; x < (int)map.width - 3 && cx < 0; ++x) {
          if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y) &&
              units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
            cx = x + 1;
            cy = y;
          }
        }
      }
      if (cx < 0) {
        fprintf(stderr, "1b0e-port-ship no land pair\n");
        return 1;
      }
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      colonies_set_occupancy_map(NULL);
      if (!colonies_load_names(&colonies, "COLONIZE/COLONY.TXT") ||
          !colonies_load_buildings(&colonies, &names)) {
        fprintf(stderr, "1b0e-port-ship colonies init failed\n");
        return 1;
      }
      const int cid = colonies_found(&colonies, &map, cx, cy, 1, -1, UNITS_JOB_NONE, 0, 0, 0);
      ColonizeColony* col = colonies_get_mut(&colonies, cid);
      if (cid < 0 || !col) {
        fprintf(stderr, "1b0e-port-ship found rival colony failed\n");
        return 1;
      }
      col->nation_id = 1;
      col->population = 1;
      col->colonist_count = 1;
      col->colonists[0].active = true;

      ColonizeCol1Save c1;
      memset(&c1, 0, sizeof(c1));
      c1.player[0].control = 0;
      c1.player[1].control = 1;

      /* Rival Frigate berthed in the port (armed: attack > 0, so the
       * non-combat seizure sweep leaves it alone). */
      const int ship = units_spawn_allow_stack(&pool, frig3, cx, cy);
      ColonizeUnit* sv = units_get(&pool, ship);
      if (!sv) {
        fprintf(stderr, "1b0e-port-ship frigate spawn failed\n");
        return 1;
      }
      sv->nation_id = 1;

      const int aid = units_spawn(&pool, soldier3, cx - 1, cy);
      ColonizeUnit* a = units_get(&pool, aid);
      if (!a) {
        fprintf(stderr, "1b0e-port-ship attacker spawn failed\n");
        return 1;
      }
      a->nation_id = 0;
      a->moves_left = 5 * UNITS_MP_PER_TILE;
      pool.types[soldier3].attack = 8;
      pool.types[soldier3].defense = 1;

      units_set_ff_col1(&c1);
      units_set_combat_human_nation(0);
      units_set_occupancy_map(&map);

      if (!units_try_move(&pool, aid, &map, cx, cy, &colonies, NULL)) {
        fprintf(
          stderr,
          "1b0e-port-ship attack should win (enter=%d combat=%d)\n",
          (int)units_last_enter_reason(),
          units_last_combat_outcome()
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      col = colonies_get_mut(&colonies, cid);
      if (!col || col->nation_id != 0) {
        fprintf(stderr, "1b0e-port-ship berthed hull must not block the capture\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      sv = units_get(&pool, ship);
      if (!sv || !sv->active) {
        fprintf(stderr, "1b0e-port-ship DOS does not sink the berthed hull\n");
        units_set_ff_col1(NULL);
        return 1;
      }
      if (sv->nation_id != 1 || sv->x != cx || sv->y != cy) {
        fprintf(
          stderr,
          "1b0e-port-ship DOS does not seize/move the berthed hull (nation %d at %d,%d)\n",
          sv->nation_id, sv->x, sv->y
        );
        units_set_ff_col1(NULL);
        return 1;
      }
      units_despawn(&pool, ship);
      (void)units_despawn(&pool, aid);

      /* --- DS:0x54f6 discharge, site 1 --- the slot is the settlement
       * record's own attitude[euro] word (tribe.alarm[euro]). */
      ColonizeCol1Tribe tribes[2];
      memset(tribes, 0, sizeof(tribes));
      tribes[0].nation_id = 4;
      tribes[1].nation_id = 4;
      c1.tribe = tribes;
      c1.head.tribe_count = 2;

      /* Open ground: native attacker wins → tribe 0's word toward euro 1 = 0
       * (BOTH bytes, friction and attacks). */
      units_set_combat_colonies(&colonies);
      tribes[0].alarm[1].friction = 96;
      tribes[0].alarm[1].attacks = 3;
      {
        const int bx = cx - 3;
        const int by = cy;
        const int bid = units_spawn_allow_stack(&pool, brave3, bx, by);
        const int vid = units_spawn_allow_stack(&pool, soldier3, bx, by);
        ColonizeUnit* b = units_get(&pool, bid);
        ColonizeUnit* v = units_get(&pool, vid);
        if (!b || !v) {
          fprintf(stderr, "1b0e-tension open-ground spawn failed\n");
          units_set_ff_col1(NULL);
          return 1;
        }
        b->nation_id = 4;
        b->home_tribe_id = 0;
        v->nation_id = 1;
        pool.types[brave3].attack = 8;
        pool.types[soldier3].defense = 1;
        if (!units_resolve_land_combat_ff(&pool, bid, vid, NULL, &c1)) {
          fprintf(stderr, "1b0e-tension brave should win on open ground\n");
          units_set_ff_col1(NULL);
          return 1;
        }
        if (col1_tribe_attitude(&tribes[0], 1) != 0) {
          fprintf(
            stderr,
            "1b0e-tension open-ground win must clear the slot, got %d\n",
            col1_tribe_attitude(&tribes[0], 1)
          );
          units_set_ff_col1(NULL);
          return 1;
        }
        units_despawn(&pool, bid);
      }

      /* Colony tile + native attacker LOSES: DOS hands the clear to
       * FUN_5fef_0f14's raid path, so this site leaves the slot alone. */
      col1_tribe_attitude_set(&tribes[1], 1, 77);
      {
        const int bid = units_spawn_allow_stack(&pool, brave3, cx, cy);
        const int vid = units_spawn_allow_stack(&pool, soldier3, cx, cy);
        ColonizeUnit* b = units_get(&pool, bid);
        ColonizeUnit* v = units_get(&pool, vid);
        if (!b || !v) {
          fprintf(stderr, "1b0e-tension colony spawn failed\n");
          units_set_ff_col1(NULL);
          return 1;
        }
        b->nation_id = 4;
        b->home_tribe_id = 1;
        v->nation_id = 1;
        pool.types[brave3].attack = 0;
        pool.types[soldier3].defense = 8;
        if (units_resolve_land_combat_ff(&pool, bid, vid, NULL, &c1)) {
          fprintf(stderr, "1b0e-tension brave should lose at the colony\n");
          units_set_ff_col1(NULL);
          return 1;
        }
        if (col1_tribe_attitude(&tribes[1], 1) != 77) {
          fprintf(
            stderr,
            "1b0e-tension colony loss must NOT clear the slot, got %d\n",
            col1_tribe_attitude(&tribes[1], 1)
          );
          units_set_ff_col1(NULL);
          return 1;
        }
        units_despawn(&pool, vid);
      }

      c1.tribe = NULL;
      c1.head.tribe_count = 0;
      units_set_combat_colonies(NULL);
      units_set_ff_col1(NULL);
      units_set_combat_human_nation(-1);
      fprintf(stderr, "unit_units: 1b0e port-ship fate + DS:0x54f6 discharge ok\n");
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
      (void)units_despawn(&pool, aid);
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
      /* bugs.md: seizure fires only for UNARMED transports (attack 0, as in
       * real NAMES.TXT); a warship loser is damage-or-sink only. */
      pool.types[car].attack = 0;
      pool.types[car].defense = 2;
      /*
       * The seizure arm needs the damage-vs-sink roll to land on "sunk", so
       * keep the winner's guns clear of the loser's hull: DOS rolls
       * 1..(guns+hull) and damages on <= hull (FUN_5fef_0352,
       * viceroy_unpacked.c 99527-99530), so guns == hull is a coin flip that
       * the port's no-RNG path resolves to "damaged". Stock NAMES.TXT has
       * Privateer guns 4 / Caravel hull 4 — exactly that tie.
       */
      pool.types[priv].guns = 12;
      pool.types[car].hull = 4;
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
      (void)units_despawn(&pool, aid);
      if (units_get(&pool, did) && units_get(&pool, did)->active) {
        units_despawn(&pool, did);
      }
      fprintf(stderr, "unit_units: privateer SEIZURE popup ok\n");
    }

    /* Fort fire: miss → MP slow only; hit close → bit7 damage; Drydock repairs. */
    {
      ColonizeColonyPool colonies;
      colonies_init(&colonies);
      colonies_set_occupancy_map(NULL);
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
      ship->moves_left = 4 * UNITS_MP_PER_TILE;
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
      /* bugs.md 255: a fort miss leaves the ship untouched (no MP drain). */
      if (ship->moves_left == 0) {
        fprintf(stderr, "fort miss must NOT drain moves_left (got %d)\n", ship->moves_left);
        return 1;
      }
      units_despawn(&pool, sid_miss);

      /* Close fort hit: defense*2 > atk and atk >= defense (null rng) → bit7. */
      const int sid_hit = units_spawn_allow_stack(&pool, car, wx, wy);
      ship = units_get(&pool, sid_hit);
      ship->nation_id = 1;
      ship->moves_left = 4 * UNITS_MP_PER_TILE;
      ship->col1_unknown15 = 0;
      ship->turns_worked = 0;
      pool.types[car].defense = 3; /* fort atk 4 wins the fight */
      /* Damage-vs-sink uses the loser's @UNIT hull against the fort's strength
       * (4): keep hull clear of it so this asserts the damaged path and not
       * the guns == hull coin flip. See units_ship_damage_vs_sink. */
      pool.types[car].hull = 8;
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
      /* bugs.md 260: combat damage presets the repair TIMER below the
       * threshold (fort winner doubles the bill → remaining = full
       * threshold here) and marks repair_pending. */
      if (ship->turns_worked >= 3 || !ship->repair_pending) {
        fprintf(stderr, "combat damage should preset repair timer (worked=%d pending=%d)\n",
                ship->turns_worked, ship->repair_pending);
        return 1;
      }
      /* The EOT ship tick counts the timer but never clears a repair —
       * the repair tick does that once the threshold is reached. */
      (void)units_tick_ship_build_ready(
        &pool, &colonies, 1, -1, st, sizeof(st), NULL
      );
      ship = units_get(&pool, sid_hit);
      if (!ship || (ship->col1_unknown15 & 0x80u) == 0) {
        fprintf(stderr, "ship-build must leave combat bit7 set\n");
        return 1;
      }
      /* In port (double speed) the timer finishes; the repair tick clears. */
      ship->x = cx;
      ship->y = cy;
      ship->nation_id = 0;
      col->has_building[3] = true;
      int repaired = 0;
      for (int t = 0; t < 4 && !repaired; ++t) {
        (void)units_tick_ship_build_ready(&pool, &colonies, 0, -1, st, sizeof(st), NULL);
        repaired = units_tick_drydock_repair(
          &pool, &colonies, 0, 0, st, sizeof(st), NULL, NULL
        );
      }
      ship = units_get(&pool, sid_hit);
      if (repaired != 1 || !ship || (ship->col1_unknown15 & 0x80u) != 0) {
        fprintf(stderr, "repair timer should clear combat bit7 (repaired=%d)\n", repaired);
        return 1;
      }
      units_despawn(&pool, sid_hit);
      fprintf(stderr, "unit_units: fort bit7 + timed repair ok\n");
    }

    units_set_combat_popups(NULL, NULL);
    units_set_combat_human_nation(-1);
  }

  /* Colony-tile visibility: a colony square never shows a non-selected
   * (idle garrison) unit — only the active/selected unit, and only while
   * it's actually visible (blink on, or mid-move). Player-reported bug on
   * the overland map; see units_top_on_map_tile. */
  {
    int tx = -1;
    int ty = -1;
    for (int y = 5; y < (int)map.height - 5 && tx < 0; ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (!map_tile_is_land(&map, x, y)) {
          continue;
        }
        bool occupied = false;
        for (int i = 0; i < pool.unit_count; ++i) {
          if (pool.units[i].active && pool.units[i].x == x && pool.units[i].y == y) {
            occupied = true;
            break;
          }
        }
        if (!occupied) {
          tx = x;
          ty = y;
          break;
        }
      }
    }
    if (tx < 0) {
      fprintf(stderr, "colony-tile visibility test: no free land tile found\n");
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int saved_selected = pool.selected_id;
    const int garrison = units_spawn_allow_stack(&pool, pioneer, tx, ty);
    const int active = units_spawn_allow_stack(&pool, pioneer, tx, ty);
    pool.selected_id = active;

    /*
     * Blink-off leaves the tile empty even on plain ground: the active unit
     * owns its tile, and letting the rest of the stack stand in for it made
     * the tile alternate between two units so you could not tell which was
     * active (bugs.md). This used to assert the opposite.
     */
    int top = units_top_on_map_tile(&pool, tx, ty, false, &map);
    if (top != -1) {
      fprintf(
        stderr, "non-colony blink-off should leave the tile empty, got %d (garrison %d)\n",
        top, garrison
      );
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* With nothing selected there, the stack's top unit shows as before. */
    pool.selected_id = -1;
    top = units_top_on_map_tile(&pool, tx, ty, false, &map);
    if (top != garrison && top != active) {
      fprintf(stderr, "unselected stack should still draw its top unit, got %d\n", top);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pool.selected_id = active;

    map_occupancy_set_layer2(&map, tx, ty, MAP_OCCUPANCY_HAS_CITY, true);

    /* Colony tile, blink-off: nothing shows — not even the garrison unit. */
    top = units_top_on_map_tile(&pool, tx, ty, false, &map);
    if (top != -1) {
      fprintf(stderr, "colony tile blink-off should hide garrison unit, got %d\n", top);
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    /* Colony tile, blink-on: the selected unit shows normally. */
    top = units_top_on_map_tile(&pool, tx, ty, true, &map);
    if (top != active) {
      fprintf(
        stderr, "colony tile blink-on expected selected unit %d, got %d\n", active, top
      );
      ss_free(&icons);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }

    map_occupancy_set_layer2(&map, tx, ty, MAP_OCCUPANCY_HAS_CITY, false);
    units_despawn(&pool, garrison);
    units_despawn(&pool, active);
    pool.selected_id = saved_selected;
    fprintf(stderr, "unit_units: colony-tile garrison visibility ok\n");
  }

  /* bugs.md: a selected passenger is not on the map, so the tile scan never
   * found it and the ship drew steadily — no blink. The passenger owns its
   * ship's tile like any active unit: own sprite blink-on, empty off. */
  {
    int wx = -1, wy = -1;
    for (int y = 5; y < (int)map.height - 5 && wx < 0; ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (map_tile_is_water(&map, x, y)) {
          wx = x;
          wy = y;
          break;
        }
      }
    }
    if (wx < 0) {
      fprintf(stderr, "pax blink test: no water tile\n");
      return 1;
    }
    const int saved_selected = pool.selected_id;
    const int caravel = units_find_type(&pool, "Caravel");
    const int ship = units_spawn_allow_stack(&pool, caravel, wx, wy);
    const int pax = units_spawn_allow_stack(&pool, pioneer, wx, wy);
    ColonizeUnit* pu = units_get(&pool, pax);
    pu->aboard_ship_id = ship;
    pu->orders = 0;
    ColonizeUnit* su = units_get(&pool, ship);
    su->cargo_count = 1;
    pool.selected_id = pax;
    if (units_top_on_map_tile(&pool, wx, wy, true, &map) != pax) {
      fprintf(stderr, "selected passenger should show on blink-on\n");
      return 1;
    }
    if (units_top_on_map_tile(&pool, wx, wy, false, &map) != -1) {
      fprintf(stderr, "selected passenger tile should be empty off-blink\n");
      return 1;
    }
    pool.selected_id = ship;
    if (units_top_on_map_tile(&pool, wx, wy, true, &map) != ship) {
      fprintf(stderr, "selected ship should still show itself\n");
      return 1;
    }
    units_despawn(&pool, pax);
    units_despawn(&pool, ship);
    pool.selected_id = saved_selected;
    fprintf(stderr, "unit_units: passenger blink ownership ok\n");
  }

  /* DOS ship-switch quirk (bugs.md): the first ship to leave a shared tile
   * scoops the tile's loaded units first come, first served; the rest stay
   * with the remaining ship(s). Awake passengers stay put. */
  {
    int wx = -1, wy = -1;
    for (int y = 5; y < (int)map.height - 5 && wx < 0; ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (map_tile_is_water(&map, x, y)) {
          wx = x;
          wy = y;
          break;
        }
      }
    }
    if (wx < 0) {
      fprintf(stderr, "ship-switch test: no water tile\n");
      return 1;
    }
    const int caravel = units_find_type(&pool, "Caravel");
    /* Spawn order fixes id order: shipA (empty), then shipB with 2 pax. */
    const int shipA = units_spawn_allow_stack(&pool, caravel, wx, wy);
    const int shipB = units_spawn_allow_stack(&pool, caravel, wx, wy);
    const int pax1 = units_spawn_allow_stack(&pool, pioneer, wx, wy);
    const int pax2 = units_spawn_allow_stack(&pool, pioneer, wx, wy);
    ColonizeUnit* sB = units_get(&pool, shipB);
    for (int p = 0; p < 2; ++p) {
      ColonizeUnit* pu = units_get(&pool, p == 0 ? pax1 : pax2);
      pu->aboard_ship_id = shipB;
      pu->orders = UNITS_ORDER_SENTRY;
      pu->moves_left = 0;
      sB->cargo_ids[sB->cargo_count++] = pu->id;
    }
    /* Ship A departs first: takes both of B's sentried passengers. */
    if (units_ship_departure_pickup(&pool, shipA, wx, wy) != 2) {
      fprintf(stderr, "ship-switch: departing ship should scoop both passengers\n");
      return 1;
    }
    ColonizeUnit* sA = units_get(&pool, shipA);
    if (sA->cargo_count != 2 || sB->cargo_count != 0 ||
        units_get(&pool, pax1)->aboard_ship_id != shipA ||
        units_get(&pool, pax2)->aboard_ship_id != shipA) {
      fprintf(stderr, "ship-switch: passengers should ride the departing ship\n");
      return 1;
    }
    /* Awake passenger stays with its own ship. */
    units_get(&pool, pax1)->orders = UNITS_ORDER_NONE;
    if (units_ship_departure_pickup(&pool, shipB, wx, wy) != 1 ||
        units_get(&pool, pax1)->aboard_ship_id != shipA ||
        units_get(&pool, pax2)->aboard_ship_id != shipB) {
      fprintf(stderr, "ship-switch: awake passenger must not transfer\n");
      return 1;
    }
    units_despawn(&pool, pax1);
    units_despawn(&pool, pax2);
    units_despawn(&pool, shipA);
    units_despawn(&pool, shipB);
    fprintf(stderr, "unit_units: ship-switch departure pickup ok\n");
  }

  /* bugs.md: Go To onto a fogged square is always legal — the destination
   * check must not peek under the fog. Seen, it still validates. */
  {
    int wx = -1, wy = -1, lx = -1, ly = -1;
    for (int y = 5; y < (int)map.height - 5 && (wx < 0 || lx < 0); ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (wx < 0 && map_tile_is_water(&map, x, y)) {
          wx = x;
          wy = y;
        } else if (lx < 0 && map_tile_is_land(&map, x, y)) {
          lx = x;
          ly = y;
        }
      }
    }
    if (wx < 0 || lx < 0) {
      fprintf(stderr, "fog goto test: tiles not found\n");
      return 1;
    }
    const int caravel = units_find_type(&pool, "Caravel");
    const int ship = units_spawn_allow_stack(&pool, caravel, wx, wy);
    ColonizeUnit* su = units_get(&pool, ship);
    su->nation_id = 0;
    const size_t li = (size_t)ly * (size_t)map.width + (size_t)lx;
    const uint8_t saved_seen = map.seen[li];
    map.seen[li] = 0; /* fog the land tile for everyone */
    if (!units_set_goto(&pool, ship, &map, lx, ly, NULL)) {
      fprintf(stderr, "fog goto: order onto an unseen tile must be accepted\n");
      return 1;
    }
    units_clear_orders(&pool, ship);
    map.seen[li] = saved_seen; /* fully explored again */
    if (units_set_goto(&pool, ship, &map, lx, ly, NULL)) {
      fprintf(stderr, "fog goto: a SEEN land tile must still refuse a ship goto\n");
      return 1;
    }
    units_despawn(&pool, ship);
    fprintf(stderr, "unit_units: fogged goto destination ok\n");
  }

  /*
   * bugs.md 424: a Go To aimed at an Indian settlement is a move command INTO
   * the village — on arrival its final step must be dispatched through the
   * normal entry flow (game_loop hands it to game_try_unit_move →
   * FUN_4d56_4528 @ACTIONS), not stopped one tile short. The raw pacer has no
   * popup channel and must still refuse to walk in silently.
   */
  {
    int ux = -1, uy = -1, vx = -1, vy = -1;
    for (int y = 5; y < (int)map.height - 5 && ux < 0; ++y) {
      for (int x = 5; x < (int)map.width - 6; ++x) {
        if (map_tile_is_land(&map, x, y) && map_tile_is_land(&map, x + 1, y) &&
            !map_tile_has_city(&map, x, y) && !map_tile_has_city(&map, x + 1, y) &&
            units_id_at(&pool, x, y) < 0 && units_id_at(&pool, x + 1, y) < 0) {
          ux = x;
          uy = y;
          vx = x + 1;
          vy = y;
          break;
        }
      }
    }
    if (ux < 0) {
      fprintf(stderr, "goto-village: no adjacent land pair\n");
      return 1;
    }
    const size_t vi = (size_t)vy * (size_t)map.width + (size_t)vx;
    const uint8_t saved_l2 = map.layer2[vi];
    map.layer2[vi] |= MAP_OCCUPANCY_HAS_CITY; /* native village, no colony */

    const int walker = units_spawn(&pool, pioneer, ux, uy);
    ColonizeUnit* wu = units_get(&pool, walker);
    if (walker < 0 || !wu) {
      fprintf(stderr, "goto-village: spawn failed\n");
      return 1;
    }
    wu->nation_id = 0;
    wu->moves_left = units_max_mp(&pool, walker);
    /* bugs.md 135: the order itself is legal even though the tile is not
     * enterable by a plain settler. */
    if (!units_set_goto(&pool, walker, &map, vx, vy, NULL)) {
      fprintf(stderr, "goto-village: village destination must be accepted\n");
      return 1;
    }
    if (!units_goto_dest_is_village_entry(&pool, walker, &map, NULL)) {
      fprintf(stderr, "goto-village: adjacent arrival must dispatch the entry step\n");
      return 1;
    }
    /* Not adjacent yet → still an ordinary en-route step, no dispatch. */
    wu->x = ux;
    wu->y = uy + 3;
    wu->orders = UNITS_ORDER_GOTO;
    wu->goto_x = (uint8_t)vx;
    wu->goto_y = (uint8_t)vy;
    if (units_goto_dest_is_village_entry(&pool, walker, &map, NULL)) {
      fprintf(stderr, "goto-village: en-route step must not dispatch the entry\n");
      return 1;
    }
    /* Adjacent to a village the path merely brushes past (not the ordered
     * destination) → no dispatch either. */
    wu->x = ux;
    wu->y = uy;
    wu->goto_x = (uint8_t)ux;
    wu->goto_y = (uint8_t)(uy + 2);
    if (units_goto_dest_is_village_entry(&pool, walker, &map, NULL)) {
      fprintf(stderr, "goto-village: only the ORDERED village tile dispatches\n");
      return 1;
    }
    units_despawn(&pool, walker);
    map.layer2[vi] = saved_l2;
    fprintf(stderr, "unit_units: goto village entry dispatch ok\n");
  }

  /*
   * bugs.md 429 / DOS FUN_4720_015c (viceroy_unpacked.c:76010-76026): landfall
   * is offered only to cargo whose spent byte is below its max. A passenger
   * parked at moves_left 0 by boarding is still fresh (DOS spent 0) and may
   * land; one that burnt its allotment this turn stays aboard.
   */
  {
    int wx = -1, wy = -1, lx = -1, ly = -1;
    for (int y = 5; y < (int)map.height - 5 && wx < 0; ++y) {
      for (int x = 5; x < (int)map.width - 5; ++x) {
        if (!map_tile_is_water(&map, x, y) || units_id_at(&pool, x, y) >= 0) {
          continue;
        }
        static const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static const int dy8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        for (int d = 0; d < 8; ++d) {
          const int tx = x + dx8[d];
          const int ty = y + dy8[d];
          if (map_tile_is_land(&map, tx, ty) && !map_tile_has_city(&map, tx, ty) &&
              units_id_at(&pool, tx, ty) < 0) {
            wx = x;
            wy = y;
            lx = tx;
            ly = ty;
            break;
          }
        }
        if (wx >= 0) {
          break;
        }
      }
    }
    if (wx < 0) {
      fprintf(stderr, "landfall-spent: no coast pair\n");
      return 1;
    }
    const int boat = units_spawn(&pool, caravel, wx, wy);
    const int spent_pax = units_spawn_allow_stack(&pool, pioneer, lx, ly);
    const int fresh_pax = units_spawn_allow_stack(&pool, pioneer, lx, ly);
    if (boat < 0 || spent_pax < 0 || fresh_pax < 0 ||
        !units_board(&pool, spent_pax, boat) || !units_board(&pool, fresh_pax, boat)) {
      fprintf(stderr, "landfall-spent: setup failed\n");
      return 1;
    }
    units_get(&pool, boat)->nation_id = 0;
    units_get(&pool, spent_pax)->nation_id = 0;
    units_get(&pool, fresh_pax)->nation_id = 0;
    /* Both parked at 0 by boarding: both eligible, first one wins. */
    if (units_first_landfall_cargo(&pool, boat) != spent_pax) {
      fprintf(stderr, "landfall-spent: parked-fresh cargo must be eligible\n");
      return 1;
    }
    /* Now mark the first one as having spent its allotment this turn. */
    units_get(&pool, spent_pax)->mp_spent_turn = 1;
    if (units_cargo_can_landfall(&pool, spent_pax)) {
      fprintf(stderr, "landfall-spent: spent pax must not be eligible\n");
      return 1;
    }
    if (units_first_landfall_cargo(&pool, boat) != fresh_pax) {
      fprintf(stderr, "landfall-spent: offer must skip to the fresh passenger\n");
      return 1;
    }
    if (units_unload_passenger(&pool, boat, spent_pax, &map, lx, ly, NULL)) {
      fprintf(stderr, "landfall-spent: spent pax must not be put ashore\n");
      return 1;
    }
    if (units_get(&pool, spent_pax)->aboard_ship_id != boat) {
      fprintf(stderr, "landfall-spent: refused pax must stay on the ship\n");
      return 1;
    }
    if (!units_unload_passenger(&pool, boat, fresh_pax, &map, lx, ly, NULL)) {
      fprintf(stderr, "landfall-spent: parked-fresh pax must be able to land\n");
      return 1;
    }
    /* Every passenger spent → no offer at all (DOS leaves reason 0). */
    if (units_first_landfall_cargo(&pool, boat) >= 0) {
      fprintf(stderr, "landfall-spent: all-spent cargo must offer nobody\n");
      return 1;
    }
    units_despawn(&pool, fresh_pax);
    units_despawn(&pool, spent_pax);
    units_despawn(&pool, boat);
    fprintf(stderr, "unit_units: landfall spent-passenger gate ok\n");
  }

  /*
   * P7.2 Fountain of Youth = 8× FUN_38fd_4884(1,0): a 3-way @RECRUIT CHOICE
   * per pick, free passage, no recruit-count bump, chained until 8 landed.
   */
  {
    EuropeScreen feu;
    char eerr[256];
    if (!europe_load(&feu, "COLONIZE", eerr, sizeof(eerr))) {
      fprintf(stderr, "fountain: europe_load failed: %s\n", eerr);
      return 1;
    }
    feu.gold = 123;
    feu.dock_count = 0;
    const uint8_t rc0 = feu.recruit_count;
    AiPopupState fpops;
    ai_popup_init(&fpops);
    units_fountain_youth_enqueue_pick(&feu, &fpops, NULL, 0, 8);
    int picks = 0;
    for (int guard = 0; guard < 20 && ai_popup_busy(&fpops); ++guard) {
      if (!fpops.open && !ai_popup_try_present_next(&fpops)) {
        break;
      }
      if (fpops.current.choice_count != 3 || fpops.current.tag != AI_POPUP_TAG_FOUNTAIN_YOUTH) {
        fprintf(stderr, "fountain: pick %d want 3-way FOUNTAIN_YOUTH choice\n", picks);
        return 1;
      }
      ColonizeInputState in = {0};
      in.last_key = COLONIZE_KEY_DOWN; /* pick slot 1 each time */
      ai_popup_handle_input(&fpops, &in);
      in.last_key = COLONIZE_KEY_ENTER;
      ai_popup_handle_input(&fpops, &in);
      if (!fpops.has_result) {
        fprintf(stderr, "fountain: no result after Enter\n");
        return 1;
      }
      if (!units_fountain_youth_apply_popup(&feu, &fpops, NULL)) {
        fprintf(stderr, "fountain: apply rejected\n");
        return 1;
      }
      ai_popup_consume_result(&fpops);
      picks++;
    }
    if (picks != 8 || feu.dock_count != 8 || feu.gold != 123 || feu.recruit_count != rc0 ||
        ai_popup_busy(&fpops)) {
      fprintf(
        stderr, "fountain: picks=%d dock=%d gold=%d rc=%u/%u busy=%d\n", picks, feu.dock_count,
        feu.gold, feu.recruit_count, rc0, ai_popup_busy(&fpops)
      );
      return 1;
    }
    for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
      if (!feu.pool[i].filled) {
        fprintf(stderr, "fountain: pool slot %d not refilled\n", i);
        return 1;
      }
    }
    europe_free(&feu);
    fprintf(stderr, "fountain of youth 8x free recruit pick ok\n");
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
