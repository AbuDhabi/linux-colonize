#include "test_units_common.h"


/* Helper: Drydock repair emits @REFIT. */
/*
 * bugs.md #462/#463/#464/#465 — coastal fort fire (FUN_364b_03f6, raw
 * 57016-57113) and the damaged-ship tail of FUN_5fef_0352:
 *
 *   #465  one salvo per neighbour tile, aimed at the FIRST ship in that
 *         tile's stack (raw 57068-57076), not at every hull on it.
 *   #464  the outcome is presented through the 1b0e fizzle, so the
 *         dissolve hook sees phase 0 then phase 1 around the despawn.
 *   #463  a damaged hull with no own Drydock/Shipyard colony is placed at
 *         DOS's off-map Europe slot (asm 5fef:0bc0-5fef:0cf9,
 *         `loser_nation - 0x14` in both coordinates) on the spot — it does
 *         not wait on its tile for an end-of-turn router.
 *   #462  which is also why the sprite is gone by the time the dissolve's
 *         "after" frame is drawn.
 */
static int s_dissolve_phases[8];
static int s_dissolve_count;

static void unit_test_dissolve_hook(void* user, int phase) {
  (void)user;
  if (s_dissolve_count < (int)(sizeof(s_dissolve_phases) / sizeof(s_dissolve_phases[0]))) {
    s_dissolve_phases[s_dissolve_count++] = phase;
  }
}

static int unit_fort_fire_dissolve_and_europe(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Merchantman");
  pool.types[0].movement = 5;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  pool.types[0].defense = 2; /* Fort strength 4 >= 2 → the fort wins */
  pool.types[0].hull = 10;   /* hull >= guns → damaged, not sunk (no-rng) */

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "fortfire2: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  map.terrain[2 * 8 + 2] = 2; /* the colony's own land tile */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Fort");
  snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name), "Drydock");
  colonies.building_type_count = 2;
  ColonizeColony* col = &colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 0;
  col->x = 2;
  col->y = 2;
  col->population = 3;
  col->has_building[0] = true; /* Fort, strength 4, no Drydock anywhere */
  snprintf(col->name, sizeof(col->name), "Jamestown");
  colonies.colony_count = 1;

  ColonizeCol1Save c1;
  memset(&c1, 0, sizeof(c1));
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    c1.head.founding_father[i] = -1;
  }
  ai_diplo_declare_war(&c1, 0, 1);

  /* Two hostile hulls on ONE water tile: DOS fires at the stack's first ship
   * only. */
  const int s1 = units_spawn_allow_stack(&pool, 0, 3, 2);
  const int s2 = units_spawn_allow_stack(&pool, 0, 3, 2);
  if (s1 < 0 || s2 < 0) {
    fprintf(stderr, "fortfire2: ship spawn failed\n");
    map_free(&map);
    return 1;
  }
  units_get(&pool, s1)->nation_id = 1;
  units_get(&pool, s2)->nation_id = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  units_set_combat_colonies(&colonies);
  units_set_combat_human_nation(1); /* the SHIPS are the human's */
  units_set_combat_europe(&eu);
  s_dissolve_count = 0;
  units_set_combat_dissolve(unit_test_dissolve_hook, NULL);

  int rc = 0;
  char st[96];
  st[0] = '\0';
  (void)units_coastal_fort_fire_pulse_w(
    &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map,
                     .col1 = &c1, .col1_ok = true, .rng = NULL},
    1, st, sizeof(st)
  );

  /* #464: phase 0 then phase 1, exactly one pair (one salvo). */
  if (s_dissolve_count != 2 || s_dissolve_phases[0] != 0 || s_dissolve_phases[1] != 1) {
    fprintf(stderr, "fortfire2: want dissolve 0,1 got %d phases (%d,%d)\n",
            s_dissolve_count, s_dissolve_phases[0], s_dissolve_count > 1 ? s_dissolve_phases[1] : -1);
    rc = 1;
  }
  /* #465: the second hull on the tile was never fired at. */
  const ColonizeUnit* u2 = units_get(&pool, s2);
  if (rc == 0 && (!u2 || !u2->active || u2->x != 3 || u2->y != 2)) {
    fprintf(stderr, "fortfire2: second hull on the tile should be untouched\n");
    rc = 1;
  }
  /* #462/#463: the damaged hull left the map for Europe on the spot. */
  const ColonizeUnit* u1 = units_get(&pool, s1);
  if (rc == 0 && u1 && u1->active) {
    fprintf(stderr, "fortfire2: damaged hull still on the map at (%d,%d)\n", u1->x, u1->y);
    rc = 1;
  }
  if (rc == 0 && eu.expected_ships != 1) {
    fprintf(stderr, "fortfire2: want 1 Europe-bound hull got %d\n", eu.expected_ships);
    rc = 1;
  }
  if (rc == 0 && eu.expected[0].turns_left < 1) {
    fprintf(stderr, "fortfire2: repair wait must be at least a turn (got %d)\n",
            eu.expected[0].turns_left);
    rc = 1;
  }

  /* An own Drydock port takes the hull instead — no Europe trip. */
  if (rc == 0) {
    col->has_building[1] = true; /* Drydock at Jamestown */
    colonies.colonies[1] = *col;
    colonies.colonies[1].id = 1;
    colonies.colonies[1].x = 6;
    colonies.colonies[1].y = 6;
    colonies.colonies[1].nation_id = 1; /* the ships' own repair port */
    colonies.colonies[1].has_building[0] = false;
    snprintf(colonies.colonies[1].name, sizeof(colonies.colonies[1].name), "Plymouth");
    colonies.colony_count = 2;
    col->has_building[1] = false;
    map.terrain[6 * 8 + 6] = 2;
    const int s3 = units_spawn_allow_stack(&pool, 0, 3, 2);
    units_get(&pool, s3)->nation_id = 1;
    units_despawn(&pool, s2);
    (void)units_coastal_fort_fire_pulse_w(
      &(ColonizeWorld){.units = &pool, .colonies = &colonies, .map = &map,
                       .col1 = &c1, .col1_ok = true, .rng = NULL},
      1, st, sizeof(st)
    );
    const ColonizeUnit* u3 = units_get(&pool, s3);
    if (!u3 || !u3->active || u3->x != 6 || u3->y != 6) {
      fprintf(stderr, "fortfire2: drydock hull should sit at Plymouth, got (%d,%d) active=%d\n",
              u3 ? u3->x : -1, u3 ? u3->y : -1, u3 ? u3->active : 0);
      rc = 1;
    }
    if (rc == 0 && eu.expected_ships != 1) {
      fprintf(stderr, "fortfire2: drydock hull must not sail to Europe\n");
      rc = 1;
    }
  }

  units_set_combat_dissolve(NULL, NULL);
  units_set_combat_europe(NULL);
  units_set_combat_colonies(NULL);
  units_set_combat_human_nation(-1);
  map_free(&map);
  if (rc == 0) {
    fprintf(stderr, "unit_units: fort fire dissolve + damaged-to-Europe ok\n");
  }
  return rc;
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
static int g_event_sfx_ids[8];

static void unit_event_sfx_play_mock(int id) {
  if (id >= SOUND_EVENT_ID_BASE) {
    if (g_event_sfx_calls < (int)(sizeof(g_event_sfx_ids) / sizeof(g_event_sfx_ids[0]))) {
      g_event_sfx_ids[g_event_sfx_calls] = id;
    }
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
    units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did);
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
    units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did);
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
    units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did);
    if (g_event_sfx_calls == 0) {
      fprintf(stderr, "combat_sfx: human defence must stay audible\n");
      rc = 1;
    }
  }

  /* 5fef:2271 and 232e-23a7: a native attacks a human defender with a
   * typed cue, followed by the generic cue. Row 21 is Mounted Brave. */
  if (rc == 0) {
    pool.types[0].kind_plus1 = 22;
    g_event_sfx_calls = 0;
    const int aid = units_spawn_allow_stack(&pool, 0, 4, 3);
    const int did = units_spawn_allow_stack(&pool, 1, 5, 3);
    units_get(&pool, aid)->nation_id = 4;
    units_get(&pool, did)->nation_id = 0;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 10);
    units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=&pool, .col1=&col1, .col1_ok=true, .rng=&rng}, aid, did);
    if (g_event_sfx_calls != 2 || g_event_sfx_ids[0] != 0x50 || g_event_sfx_ids[1] != 0x41) {
      fprintf(stderr, "combat_sfx: native typed/generic sequence got %d calls (%d,%d)\n",
              g_event_sfx_calls, g_event_sfx_ids[0], g_event_sfx_ids[1]);
      rc = 1;
    }
  }
  /* Once woodcut 13 has fired, DOS suppresses the generic cue when Combat
   * Analysis is off; the typed cue still plays. */
  if (rc == 0) {
    g_event_sfx_calls = 0;
    const int aid = units_spawn_allow_stack(&pool, 0, 4, 2);
    const int did = units_spawn_allow_stack(&pool, 1, 5, 2);
    units_get(&pool, aid)->nation_id = 4;
    units_get(&pool, did)->nation_id = 0;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 12);
    units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=&pool, .col1=&col1, .col1_ok=true, .rng=&rng}, aid, did);
    if (g_event_sfx_calls != 1 || g_event_sfx_ids[0] != 0x50) {
      fprintf(stderr, "combat_sfx: repeat native attack got %d calls (first %d)\n",
              g_event_sfx_calls, g_event_sfx_ids[0]);
      rc = 1;
    }
  }
  /* 5fef:234e-236a: cavalry row 7 selects 0x4c without a typed cue. */
  if (rc == 0) {
    pool.types[0].kind_plus1 = 8;
    g_event_sfx_calls = 0;
    const int aid = units_spawn_allow_stack(&pool, 0, 4, 4);
    const int did = units_spawn_allow_stack(&pool, 1, 5, 4);
    units_get(&pool, aid)->nation_id = 0;
    units_get(&pool, did)->nation_id = 1;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 11);
    units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=&pool, .col1=&col1, .col1_ok=true, .rng=&rng}, aid, did);
    if (g_event_sfx_calls != 1 || g_event_sfx_ids[0] != 0x4c) {
      fprintf(stderr, "combat_sfx: cavalry attack got %d calls (first %d)\n",
              g_event_sfx_calls, g_event_sfx_ids[0]);
      rc = 1;
    }
  }
  /* 5fef:2577-259f: a losing land attacker emits a second cue. */
  if (rc == 0) {
    g_event_sfx_calls = 0;
    const int aid = units_spawn_allow_stack(&pool, 1, 4, 5);
    const int did = units_spawn_allow_stack(&pool, 0, 5, 5);
    units_get(&pool, aid)->nation_id = 0;
    units_get(&pool, did)->nation_id = 1;
    (void)units_resolve_land_combat_ff_w(
      &(ColonizeWorld){.units=&pool, .col1=&col1, .col1_ok=true, .rng=NULL}, aid, did
    );
    if (g_event_sfx_calls != 2 || g_event_sfx_ids[0] != 0x41 || g_event_sfx_ids[1] != 0x40) {
      fprintf(stderr, "combat_sfx: land loss got %d calls (%d,%d)\n",
              g_event_sfx_calls, g_event_sfx_ids[0], g_event_sfx_ids[1]);
      rc = 1;
    }
  }
  /* 5fef:3040-3063: an AI colony burning cannot emit the human burn cue. */
  if (rc == 0) {
    g_event_sfx_calls = 0;
    units_combat_notify_colony_burned(&col1, "AI colony", 1, "Natives");
    if (g_event_sfx_calls != 0) {
      fprintf(stderr, "combat_sfx: AI colony burn emitted %d cues\n", g_event_sfx_calls);
      rc = 1;
    }
  }
  if (rc == 0) {
    g_event_sfx_calls = 0;
    units_combat_notify_colony_burned(&col1, "Human colony", 0, "Natives");
    if (g_event_sfx_calls != 1 || g_event_sfx_ids[0] != 0x53) {
      fprintf(stderr, "combat_sfx: human colony burn got %d cues (first %d)\n",
              g_event_sfx_calls, g_event_sfx_ids[0]);
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
  units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL), .rng=(ColonizeDosRng*)(&rng0)}, aid, did);

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
  units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL), .rng=(ColonizeDosRng*)(&rng1)}, aid, did);
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
  units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL), .rng=(ColonizeDosRng*)(&rng2)}, aid, did);
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
    units_get(&pool, aid)->moves = UNITS_MP_PER_TILE;
    units_get(&pool, did)->nation_id = 4;
    units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(&rng)}, aid, 5, 5);
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
    units_get(&pool, aid)->moves = UNITS_MP_PER_TILE;
    units_get(&pool, did)->nation_id = 4;
    units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(&rng)}, aid, 3, 2);
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
    units_get(&pool, aid)->moves = UNITS_MP_PER_TILE;
    units_get(&pool, did)->nation_id = 4;
    units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(&rng)}, aid, 5, 5);
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
    units_get(&pool, aid)->moves = UNITS_MP_PER_TILE;
    if (!units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(&rng)}, aid, 4, 4)) {
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
    brave->moves = 0; /* natives: SPENT byte — 0 = fresh full allotment */
    (void)units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(&rng)}, bid, 4, 4);

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
      br2->moves = 0; /* spent byte: fresh */
      (void)units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .colonies=(ColonizeColonyPool*)(&colonies), .map=(ColonizeWorldMap*)(&map), .rng=(ColonizeDosRng*)(&rng)}, b2, 4, 4);
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
 * Razing a village (FUN_5fef_1b0e dwelling arm → FUN_4d56_00e0), bugs.md
 * #550-#552:
 *  - 00e0 raw 81307: only settlement bit 0x02 is cleared; a real road stays,
 *    the implied village road art goes (#550).
 *  - 1b0e raw 100667-100672: the conqueror's own mission comes back as a
 *    Missionary on the site, Jesuit bit → profession 0x18; a rival's mission
 *    is lost (#551).
 *  - 00e0 raw 81310-81319: Indian units whose home village (+0x314a) is the
 *    razed one are deleted wherever they stand; other villages' braves stay
 *    and their home index shifts down (#552).
 */
static int unit_village_raze_tail(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "raze: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "raze: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  const int brave_t = units_kind_type_index(&pool, UNITS_KIND_BRAVE);
  const int miss_t = units_kind_type_index(&pool, UNITS_KIND_MISSIONARY);
  if (brave_t < 0 || miss_t < 0) {
    fprintf(stderr, "raze: Brave/Missionary type rows missing\n");
    return 1;
  }

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 10;
  map.height = 10;
  map.tile_count = 100;
  map.terrain = calloc(100, 1);
  map.layer2 = calloc(100, 1);
  map.layer3 = calloc(100, 1);
  map.improve = calloc(100, 1);
  ColonizeCol1Tribe* tribes = calloc(3, sizeof(ColonizeCol1Tribe));
  if (!map.terrain || !map.layer2 || !map.layer3 || !map.improve || !tribes) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    free(map.improve);
    free(tribes);
    return 1;
  }
  for (int i = 0; i < 100; ++i) {
    map.terrain[i] = 2; /* plains */
    map.layer3[i] = 0xf1;
  }
  units_set_occupancy_map(&map);

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  memset(col1.head.founding_father, 0xff, sizeof(col1.head.founding_father));
  col1.stuff.tribe_village_counts[0] = 3;
  col1.head.tribe_count = 3;
  col1.tribe = tribes;
  const int vx[3] = {3, 6, 1};
  const int vy[3] = {3, 6, 6};
  /* 0: own plain mission; 1: rival (Dutch) Jesuit mission; 2: own Jesuit. */
  const uint8_t mission[3] = {0u, (uint8_t)(1u | COL1_TRIBE_MISSION_JESUIT_BIT),
                              (uint8_t)(0u | COL1_TRIBE_MISSION_JESUIT_BIT)};
  for (int i = 0; i < 3; ++i) {
    tribes[i].x = (uint8_t)vx[i];
    tribes[i].y = (uint8_t)vy[i];
    tribes[i].nation_id = 4;
    tribes[i].population = 1;
    tribes[i].mission = mission[i];
    map.layer2[vy[i] * 10 + vx[i]] = MAP_OCCUPANCY_HAS_CITY;
    map.layer3[vy[i] * 10 + vx[i]] = 0x41;
  }
  map.improve[6 * 10 + 6] = MAP_IMPROVE_ROAD; /* a real road under village 1 */

  const int b_home0 = units_spawn_allow_stack(&pool, brave_t, 5, 5);
  const int b_home1 = units_spawn_allow_stack(&pool, brave_t, 5, 6);
  const int b_free = units_spawn_allow_stack(&pool, brave_t, 5, 7);
  units_get(&pool, b_home0)->nation_id = 4;
  units_get(&pool, b_home0)->home_tribe_id = 0;
  units_get(&pool, b_home1)->nation_id = 4;
  units_get(&pool, b_home1)->home_tribe_id = 1;
  units_get(&pool, b_free)->nation_id = 4;
  units_get(&pool, b_free)->home_tribe_id = -1;

  ColonizeWorld w = {.units = &pool, .map = &map, .col1 = &col1, .col1_ok = true};
  int rc = 0;

  /* Raze village 0 (own plain mission). */
  if (!units_try_native_settlement_fallout_w(&w, 0, 4, 3, 3, 500)) {
    fprintf(stderr, "raze: village 0 not destroyed\n");
    rc = 1;
  }
  const ColonizeUnit* u = units_get_const(&pool, b_home0);
  if (rc == 0 && u && u->active) {
    fprintf(stderr, "raze #552: brave bound to the razed village survived\n");
    rc = 1;
  }
  u = units_get_const(&pool, b_home1);
  if (rc == 0 && (!u || !u->active || u->home_tribe_id != 0)) {
    fprintf(stderr, "raze #552: other village's brave must stay, home 1 -> 0\n");
    rc = 1;
  }
  u = units_get_const(&pool, b_free);
  if (rc == 0 && (!u || !u->active)) {
    fprintf(stderr, "raze #552: unbound brave must stay\n");
    rc = 1;
  }
  if (rc == 0 && (map_tile_has_city(&map, 3, 3) || map_tile_has_road(&map, 3, 3))) {
    fprintf(stderr, "raze #550: razed site must lose the settlement bit (no road art)\n");
    rc = 1;
  }
  int found_prof[3] = {-1, -1, -1};
  int found_n[3] = {0, 0, 0};
  for (int k = 0; k < 3 && rc == 0; ++k) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* m = &pool.units[i];
      if (m->active && m->type_index == miss_t && m->x == vx[k] && m->y == vy[k]) {
        found_n[k]++;
        found_prof[k] = m->profession;
        if (m->nation_id != 0) {
          fprintf(stderr, "raze #551: returned missionary nation %d want 0\n", m->nation_id);
          rc = 1;
        }
      }
    }
    if (k == 0) {
      if (found_n[0] != 1 || found_prof[0] == UNITS_JOB_MISSIONARY) {
        fprintf(stderr, "raze #551: want one plain Missionary (n=%d prof=%d)\n", found_n[0],
                found_prof[0]);
        rc = 1;
      }
      /* Raze village 1 (rival Jesuit mission; now index 0) and 2 (own Jesuit). */
      if (rc == 0 && (!units_try_native_settlement_fallout_w(&w, 0, 4, 6, 6, 500) ||
                      !units_try_native_settlement_fallout_w(&w, 0, 4, 1, 6, 500))) {
        fprintf(stderr, "raze: villages 1/2 not destroyed\n");
        rc = 1;
      }
    }
  }
  if (rc == 0 && found_n[1] != 0) {
    fprintf(stderr, "raze #551: a rival nation's mission must not come back\n");
    rc = 1;
  }
  if (rc == 0 && (found_n[2] != 1 || found_prof[2] != UNITS_JOB_MISSIONARY)) {
    fprintf(stderr, "raze #551: want one Jesuit Missionary (n=%d prof=%d)\n", found_n[2],
            found_prof[2]);
    rc = 1;
  }
  if (rc == 0 && (map_tile_has_city(&map, 6, 6) || !map_tile_has_road(&map, 6, 6))) {
    fprintf(stderr, "raze #550: a real road under a razed village must stay\n");
    rc = 1;
  }
  u = units_get_const(&pool, b_home1);
  if (rc == 0 && u && u->active) {
    fprintf(stderr, "raze #552: brave of razed village 1 survived\n");
    rc = 1;
  }

  units_set_occupancy_map(NULL);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  free(map.improve);
  free(tribes);
  if (rc == 0) {
    fprintf(stderr, "unit_units: village raze tail (#550-#552) ok\n");
  }
  return rc;
}

/*
 * FUN_5fef_1b0e raw 100573-100577: a plain Brave (@UNIT type 0x13) attacking
 * the Artillery (@UNIT type 0xb) of a HUMAN-controlled European never wins,
 * whatever the roll — DOS forces `bVar8 = false` and latches `local_ca`. An
 * AI-controlled European (control != 0) is NOT covered by the gate.
 */
/*
 * bugs.md #756 / #757 — FUN_5fef_0352 raw 99435-99436: the artillery
 * damage-then-destroy ladder runs only when
 *   (winner type < 0xd || winner type > 0x12) && local_2a == 0,
 * local_2a = FUN_281f_0768 (ocean / high seas) of the WINNER's tile OR'd with
 * the LOSER's tile. Gate failed -> the gun falls to the plain despawn tail at
 * raw 99711: destroyed outright, no damaged bit. Also asserts #757: the damage
 * arm writes bit7 ONLY (raw 99475-99482) and must not zero the gun's MP.
 */
static int unit_artillery_loss_water_gate(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "arty_water: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "arty_water: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int arty = units_find_type(&pool, "Artillery");
  const int drag = units_find_type(&pool, "Dragoons");
  assets_msg_free(&names);
  if (arty < 0 || drag < 0) {
    fprintf(stderr, "arty_water: types missing\n");
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
    map.terrain[i] = 2;    /* plains */
    map.layer3[i] = 0xf1;  /* no owner, continent 1 (never all-lake) */
  }
  units_set_occupancy_map(&map);
  units_set_combat_colonies(NULL);

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  memset(col1.head.founding_father, 0xff, sizeof(col1.head.founding_father));
  col1.head.difficulty = 2;
  col1.player[0].control = 1;
  col1.player[1].control = 1;
  units_set_ff_col1(&col1);
  units_set_combat_human_nation(-1);

  int rc = 0;
  /* Leg 1 — plain land fight: the losing gun takes bit7 and survives. */
  {
    const int aid = units_spawn_allow_stack(&pool, arty, 2, 2);
    const int did = units_spawn_allow_stack(&pool, drag, 3, 2);
    if (aid < 0 || did < 0) {
      fprintf(stderr, "arty_water: spawn failed\n");
      rc = 1;
    } else {
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      a->nation_id = 0;
      d->nation_id = 1;
      a->moves = UNITS_MP_PER_TILE;
      /* NULL rng: atk_wins = atk_strength >= def_strength. The open-field
       * >>2 on the artillery attacker makes it lose. */
      (void)units_resolve_land_combat_ff_w(
        &(ColonizeWorld){.units = &pool, .col1 = &col1, .col1_ok = true, .rng = NULL}, aid, did);
      a = units_get(&pool, aid);
      if (!a || !a->active) {
        fprintf(stderr, "arty_water: land loss destroyed the gun (want damaged)\n");
        rc = 1;
      } else if ((a->col1_flags15 & 0x80u) == 0) {
        fprintf(stderr, "arty_water: land loss did not set the damaged bit\n");
        rc = 1;
      } else if (a->moves == 0) {
        /* bugs.md #757 */
        fprintf(stderr, "arty_water: damage arm zeroed MP (DOS raw 99475-99482 does not)\n");
        rc = 1;
      }
      if (units_get(&pool, aid)) {
        (void)units_despawn(&pool, aid);
      }
      (void)units_despawn(&pool, did);
    }
  }

  /* Leg 2 — same fight with local_2a set (both tiles ocean): destroyed. */
  if (rc == 0) {
    /* Only the LOSER's tile needs to be water — DOS ORs the two probes. The
     * winner's tile stays plains so the strength comparison is unchanged
     * from leg 1 and the artillery still loses. */
    map.terrain[4 * 8 + 2] = 0x19; /* ocean */
    const int aid = units_spawn_allow_stack(&pool, arty, 2, 4);
    const int did = units_spawn_allow_stack(&pool, drag, 3, 4);
    if (aid < 0 || did < 0) {
      fprintf(stderr, "arty_water: water spawn failed\n");
      rc = 1;
    } else {
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      a->nation_id = 0;
      d->nation_id = 1;
      a->moves = UNITS_MP_PER_TILE;
      (void)units_resolve_land_combat_ff_w(
        &(ColonizeWorld){.units = &pool, .col1 = &col1, .col1_ok = true, .rng = NULL}, aid, did);
      a = units_get(&pool, aid);
      if (a && a->active) {
        fprintf(stderr, "arty_water: gun survived a loss with local_2a set "
                        "(want destroyed, raw 99435-99436)\n");
        rc = 1;
        (void)units_despawn(&pool, aid);
      }
      (void)units_despawn(&pool, did);
    }
  }

  units_set_occupancy_map(NULL);
  units_set_ff_col1(NULL);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  if (rc == 0) {
    fprintf(stderr, "unit_units: artillery loss water/hull gate (#756/#757) ok\n");
  }
  return rc;
}

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
      if (units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did)) {
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
      if (units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did)) {
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
      if (units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did)) {
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
        won[i] = is_naval ? units_resolve_naval_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did)
                          : units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&c1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did);
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
/* bugs.md #659: FUN_5fef_0352 Royal-flagged hull loss lowers tax_rate via the
 * DS:0x978d table read one entry late (Caravel 4, Merchantman 6, Galleon 3,
 * Privateer 8, Frigate 0); Artillery is skipped by the `0 < k` test. */
static int s_tax_hook_nation = -1;
static int s_tax_hook_value = -1;
static void unit_tax_hook(void* user, int nation_id, int new_tax) {
  (void)user;
  s_tax_hook_nation = nation_id;
  s_tax_hook_value = new_tax;
}
static int unit_royal_loss_tax_cut(void) {
  static const struct { const char* name; int tax; int want_cut; int want_hook; } k[] = {
    {"Caravel", 30, 4, 1},   {"Merchantman", 30, 6, 1}, {"Galleon", 30, 3, 1},
    {"Privateer", 30, 8, 1}, {"Frigate", 30, 0, 0},     {"Artillery", 30, 0, 0},
    {"Caravel", 3, 3, 1},    {"Caravel", 0, 0, 0},
  };
  for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    pool.type_count = 1;
    snprintf(pool.types[0].name, sizeof(pool.types[0].name), "%s", k[i].name);
    ColonizeUnit u;
    memset(&u, 0, sizeof(u));
    u.type_index = 0;
    u.nation_id = 1;
    u.col1_flags15 = 0x40;
    ColonizeCol1Save c1;
    memset(&c1, 0, sizeof(c1));
    c1.nation[1].tax_rate = (uint8_t)k[i].tax;
    units_reset_hooks();
    units_set_tax_change_hook(unit_tax_hook, NULL);
    s_tax_hook_nation = -1;
    s_tax_hook_value = -1;
    const int cut = units_royal_loss_tax_cut(&c1, &pool, &u);
    const int want_tax = k[i].tax - k[i].want_cut;
    if (cut != k[i].want_cut || (int)c1.nation[1].tax_rate != want_tax ||
        (k[i].want_hook && (s_tax_hook_nation != 1 || s_tax_hook_value != want_tax)) ||
        (!k[i].want_hook && s_tax_hook_nation != -1)) {
      fprintf(stderr, "royaltax: %s tax %d: cut %d (want %d) tax %d hook %d/%d\n",
              k[i].name, k[i].tax, cut, k[i].want_cut, (int)c1.nation[1].tax_rate,
              s_tax_hook_nation, s_tax_hook_value);
      units_reset_hooks();
      return 1;
    }
    /* Not Royal-flagged: nothing. */
    u.col1_flags15 = 0;
    c1.nation[1].tax_rate = 30;
    if (units_royal_loss_tax_cut(&c1, &pool, &u) != 0 || c1.nation[1].tax_rate != 30) {
      fprintf(stderr, "royaltax: unflagged %s cut\n", k[i].name);
      units_reset_hooks();
      return 1;
    }
  }
  units_reset_hooks();
  fprintf(stderr, "unit_units: #659 royal hull loss tax cut ok\n");
  return 0;
}

int main(void) {
  diag_init(0, NULL);
  if (unit_royal_loss_tax_cut() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_combat_music_sting() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_fort_fire_dissolve_and_europe() != 0) {
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
  if (unit_artillery_loss_water_gate() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_village_raze_tail() != 0) {
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
  diag_shutdown();
  return 0;
}
