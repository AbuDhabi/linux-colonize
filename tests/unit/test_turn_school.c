/* Slice of the former tests/unit/test_turn.c (split by feature 2026-09-23):
 * schoolhouse training arms (veteran, cap, fail, profession, criminal, indentured, phase H). */
#include "test_turn_common.h"

/*
 * bugs.md #380: a Veteran Soldier teaching in a College turns a Free Colonist
 * who is working a FIELD TILE (not sitting in the school) into a Veteran
 * Soldier after 6 turns. The old port needed profession == @JOB 18 for the
 * teacher, required students to be inside the school, and handed out the
 * teacher's field_job (always -1 in a building), so it graduated nobody.
 */
static int unit_train_veteran_soldier(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "College");
  pool.building_type_count = 1;

  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Boston");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  /* Teacher: Veteran Soldier (@JOB 21, school level 2 -> 6 turns). */
  col->colonists[0].active = true;
  col->colonists[0].profession = 21;
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 5; /* one tick -> 6 == need */
  /* Student: Free Colonist out on a field tile, not in the school. */
  col->colonists[1].active = true;
  col->colonists[1].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[1].building_type = -1;
  col->colonists[1].field_job = COLONIZE_JOB_FARMER;
  col->colonists[1].turns_in_job = 0;
  col->colonist_count = 2;
  col->population = 2;
  pool.colony_count = 1;

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&pool, col, NULL, &prod, NULL);
  if (col->colonists[1].profession != 21) {
    fprintf(
      stderr,
      "vetsoldier: field student want Veteran Soldier(21) got %d\n",
      col->colonists[1].profession
    );
    return 1;
  }
  if (col->colonists[0].turns_in_job != 0) {
    fprintf(stderr, "vetsoldier: teacher counter should reset\n");
    return 1;
  }

  /* Level 2 needs 6 turns, not the Schoolhouse's 4: 5 turns must not graduate. */
  col->colonists[1].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[0].turns_in_job = 4; /* one tick -> 5 < 6 */
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&pool, col, NULL, &prod, NULL);
  if (col->colonists[1].profession != COLONIZE_PROF_FREE_COLONIST) {
    fprintf(stderr, "vetsoldier: graduated early at 5 of 6 turns\n");
    return 1;
  }

  /* Indian Converts are not students in DOS (0x1b is absent from the list). */
  col->colonists[1].profession = COLONIZE_PROF_CONVERT;
  col->colonists[0].turns_in_job = 5;
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&pool, col, NULL, &prod, NULL);
  if (col->colonists[1].profession != COLONIZE_PROF_CONVERT) {
    fprintf(stderr, "vetsoldier: Indian Convert must not be educated\n");
    return 1;
  }
  fprintf(stderr, "unit_turn: Veteran Soldier education ok\n");
  return 0;
}

/*
 * bugs.md #580 tick half: DOS FUN_364b_0688 (raw 57510-57535) caps the
 * graduation loop with the bare literal `local_6e < 3` and picks teachers on
 * occupation 0x12 + the level test only -- it never consults the colony's
 * owned school tier. So a Schoolhouse-only colony that somehow seats four
 * teachers still graduates three of them in one tick, and a level-2 specialist
 * seated in a Schoolhouse still teaches. The owned-tier cap lives in the
 * work-assign validator alone (overlays.c:60455-60484).
 */
static int unit_school_tick_cap_is_three(void) {
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
  snprintf(col->name, sizeof(col->name), "Boston");
  col->building_in_production = -1;
  col->has_building[0] = true; /* owned tier 1 only */
  col->stock[COLONIZE_CARGO_FOOD] = 200;
  /* Four ready level-1 teachers (Expert Farmer, @JOB level 1 -> 4 turns). */
  for (int i = 0; i < 4; ++i) {
    col->colonists[i].active = true;
    col->colonists[i].profession = COLONIZE_JOB_FARMER;
    col->colonists[i].building_type = 0;
    col->colonists[i].field_job = -1;
    col->colonists[i].turns_in_job = 3; /* one tick -> 4 == need */
  }
  /* Five Free Colonist students out on tiles. */
  for (int i = 4; i < 9; ++i) {
    col->colonists[i].active = true;
    col->colonists[i].profession = COLONIZE_PROF_FREE_COLONIST;
    col->colonists[i].building_type = -1;
    col->colonists[i].field_job = COLONIZE_JOB_FARMER;
    col->colonists[i].turns_in_job = 0;
  }
  col->colonist_count = 9;
  col->population = 9;
  pool.colony_count = 1;

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&pool, col, NULL, &prod, NULL);

  int graduated = 0;
  for (int i = 4; i < 9; ++i) {
    if (col->colonists[i].profession == COLONIZE_JOB_FARMER) {
      graduated++;
    }
  }
  if (graduated != 3) {
    fprintf(stderr, "school cap: want 3 graduates in one tick, got %d\n", graduated);
    return 1;
  }
  /* The first three teachers qualified and were zeroed; the fourth never
   * entered the `local_6e < 3` body, so its counter keeps running. */
  for (int i = 0; i < 3; ++i) {
    if (col->colonists[i].turns_in_job != 0) {
      fprintf(stderr, "school cap: teacher %d counter should reset\n", i);
      return 1;
    }
  }
  if (col->colonists[3].turns_in_job == 0) {
    fprintf(stderr, "school cap: 4th teacher must not consume his turn\n");
    return 1;
  }

  /* A level-2 specialist seated in a Schoolhouse still teaches: the tick has
   * no @NEEDCOLLEGE gate. */
  memset(col->colonists, 0, sizeof(col->colonists));
  col->colonists[0].active = true;
  col->colonists[0].profession = 21; /* Veteran Soldier, level 2 -> 6 turns */
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 5;
  col->colonists[1].active = true;
  col->colonists[1].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[1].building_type = -1;
  col->colonists[1].field_job = COLONIZE_JOB_FARMER;
  col->colonist_count = 2;
  col->population = 2;
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&pool, col, NULL, &prod, NULL);
  if (col->colonists[1].profession != 21) {
    fprintf(
      stderr,
      "school cap: Schoolhouse must still teach level 2, got %d\n",
      col->colonists[1].profession
    );
    return 1;
  }
  fprintf(stderr, "unit_turn: school tick cap ok\n");
  return 0;
}

/* Phase G @TRAINFAIL when ready teacher has no eligible students. */
static int unit_trainfail(void) {
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
  snprintf(col->name, sizeof(col->name), "Roanoke");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_JOB_FARMER; /* @JOB level 1 teacher */
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 3; /* one tick → 4 ≥ need */
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "trainfail: GAME.TXT load failed\n");
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
  if (strstr(eu.status, "No students") == NULL) {
    fprintf(stderr, "trainfail: status want No students got '%s'\n", eu.status);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Roanoke") == NULL &&
       strstr(pops.queue[0].body, "teacher") == NULL &&
       strstr(pops.queue[0].body, "specialty") == NULL &&
       strstr(pops.queue[0].body, "students") == NULL)) {
    fprintf(
      stderr,
      "trainfail: TRAINFAIL popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_turn: TRAINFAIL chrome ok\n");
  return 0;
}

/* Phase G @TRAINPROFESSION when school graduation assigns a specialty. */
static int unit_trainprofession(void) {
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
  snprintf(col->name, sizeof(col->name), "Plymouth");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_JOB_FARMER; /* @JOB level 1 teacher */
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 3; /* one tick → 4 ≥ need */
  col->colonists[1].active = true;
  col->colonists[1].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[1].building_type = 0;
  col->colonists[1].field_job = -1;
  col->colonists[1].turns_in_job = 0;
  col->colonist_count = 2;
  col->population = 2;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "trainprof: GAME.TXT load failed\n");
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
  if (col->colonists[1].profession != COLONIZE_JOB_FARMER) {
    fprintf(
      stderr,
      "trainprof: student want Farmer(%d) got %d\n",
      COLONIZE_JOB_FARMER,
      col->colonists[1].profession
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  if (strstr(eu.status, "trained") == NULL) {
    fprintf(stderr, "trainprof: status want trained got '%s'\n", eu.status);
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Plymouth") == NULL &&
       strstr(pops.queue[0].body, "Farmer") == NULL &&
       strstr(pops.queue[0].body, "profession") == NULL &&
       strstr(pops.queue[0].body, "learned") == NULL)) {
    fprintf(
      stderr,
      "trainprof: TRAINPROFESSION popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_turn: TRAINPROFESSION chrome ok\n");
  return 0;
}

/* Phase G ladder: Criminal → Indentured + @TRAINCRIMINAL. */
static int unit_traincriminal(void) {
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
  snprintf(col->name, sizeof(col->name), "Boston");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_JOB_FARMER; /* @JOB level 1 teacher */
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 3;
  col->colonists[1].active = true;
  col->colonists[1].profession = COLONIZE_PROF_CRIMINAL;
  col->colonists[1].building_type = 0;
  col->colonists[1].field_job = -1;
  col->colonists[1].turns_in_job = 0;
  col->colonist_count = 2;
  col->population = 2;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "traincriminal: GAME.TXT load failed\n");
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
  if (col->colonists[1].profession != COLONIZE_PROF_INDENTURED) {
    fprintf(
      stderr,
      "traincriminal: want Indentured(%d) got %d\n",
      COLONIZE_PROF_INDENTURED,
      col->colonists[1].profession
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Boston") == NULL &&
       strstr(pops.queue[0].body, "criminal") == NULL &&
       strstr(pops.queue[0].body, "Criminal") == NULL &&
       strstr(pops.queue[0].body, "indentured") == NULL)) {
    fprintf(
      stderr,
      "traincriminal: TRAINCRIMINAL popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_turn: TRAINCRIMINAL chrome ok\n");
  return 0;
}

/* Phase G ladder: Indentured → Free + @TRAININDENTURED. */
static int unit_trainindentured(void) {
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
  snprintf(col->name, sizeof(col->name), "Salem");
  col->building_in_production = -1;
  col->has_building[0] = true;
  col->stock[COLONIZE_CARGO_FOOD] = 50;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_JOB_FARMER; /* @JOB level 1 teacher */
  col->colonists[0].building_type = 0;
  col->colonists[0].field_job = -1;
  col->colonists[0].turns_in_job = 3;
  col->colonists[1].active = true;
  col->colonists[1].profession = COLONIZE_PROF_INDENTURED;
  col->colonists[1].building_type = 0;
  col->colonists[1].field_job = -1;
  col->colonists[1].turns_in_job = 0;
  col->colonist_count = 2;
  col->population = 2;
  pool.colony_count = 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "trainindentured: GAME.TXT load failed\n");
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
  if (col->colonists[1].profession != COLONIZE_PROF_FREE_COLONIST) {
    fprintf(
      stderr,
      "trainindentured: want Free(%d) got %d\n",
      COLONIZE_PROF_FREE_COLONIST,
      col->colonists[1].profession
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  if (pops.queue_count < 1 ||
      (strstr(pops.queue[0].body, "Salem") == NULL &&
       strstr(pops.queue[0].body, "indentured") == NULL &&
       strstr(pops.queue[0].body, "free") == NULL &&
       strstr(pops.queue[0].body, "Free") == NULL)) {
    fprintf(
      stderr,
      "trainindentured: TRAININDENTURED popup weak q=%d body='%s'\n",
      pops.queue_count,
      pops.queue_count > 0 ? pops.queue[0].body : ""
    );
    assets_msg_free(&game_txt);
    return 1;
  }
  assets_msg_free(&game_txt);
  fprintf(stderr, "unit_turn: TRAININDENTURED chrome ok\n");
  return 0;
}

/* Phase H @TRAINPROFESSION when Free Colonist discovers field skill. */
static int unit_phase_h_trainprofession(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  snprintf(col->name, sizeof(col->name), "Concord");
  col->building_in_production = -1;
  col->stock[COLONIZE_CARGO_FOOD] = 500;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[0].building_type = -1;
  col->colonists[0].field_job = COLONIZE_JOB_COTTON_PLANTER;
  col->colonist_count = 1;
  col->population = 1;
  col->tiles[0] = 0;
  pool.colony_count = 1;

  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "phaseh: GAME.TXT load failed\n");
    return 1;
  }

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.head.year = 1492;
  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  AiPopupState pops;
  ai_popup_init(&pops);
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 1);
  int discovered = 0;
  for (unsigned t = 0; t < 5000u; ++t) {
    col1.head.turn = (uint16_t)(t & 0xffffu);
    col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
    /* raw 57595: `0 < job < 5` — Farmer (0) is excluded. */
    col->colonists[0].field_job = COLONIZE_JOB_COTTON_PLANTER;
    col->colonists[0].turns_in_job = 3;
    col->stock[COLONIZE_CARGO_FOOD] = 500;
    eu.status[0] = '\0';
    ai_popup_init(&pops);
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&pool), .map=(ColonizeWorldMap*)(NULL), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng), .europe=(EuropeScreen*)(&eu)}, 0, &prod, &pops, &game_txt);
    if (col->colonists[0].profession == COLONIZE_JOB_COTTON_PLANTER) {
      discovered = 1;
      /* bugs.md #771: raw 57606-57607 writes only the profession byte; the
       * education counter survives (3 + this turn's raw 57505 tick = 4). */
      if (col->colonists[0].turns_in_job != 4) {
        fprintf(stderr, "phaseh: turns_in_job reset to %d\n", col->colonists[0].turns_in_job);
        assets_msg_free(&game_txt);
        return 1;
      }
      if (pops.queue_count < 1 ||
          (strstr(pops.queue[0].body, "Concord") == NULL &&
           strstr(pops.queue[0].body, "Farmer") == NULL &&
           strstr(pops.queue[0].body, "learned") == NULL &&
           strstr(pops.queue[0].body, "profession") == NULL)) {
        fprintf(
          stderr,
          "phaseh: TRAINPROFESSION popup weak q=%d body='%s' status='%s'\n",
          pops.queue_count,
          pops.queue_count > 0 ? pops.queue[0].body : "",
          eu.status
        );
        assets_msg_free(&game_txt);
        return 1;
      }
      break;
    }
  }
  assets_msg_free(&game_txt);
  if (!discovered) {
    fprintf(stderr, "phaseh: no field skill discover in 5000 ticks\n");
    return 1;
  }
  fprintf(stderr, "unit_turn: Phase H TRAINPROFESSION chrome ok\n");
  return 0;
}

/* bugs.md #770: FUN_4962_0606 (raw 78332-78374) counts the nation's MAP
 * units before its colonists, so a Master Cotton Planter standing outside any
 * colony still closes the on-the-job Cotton Planter roll for the nation. */
static int unit_phase_h_map_expert_blocks_learn(void) {
  ColonizeColonyPool pool;
  colonies_init(&pool);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  col->nation_id = 0;
  col->building_in_production = -1;
  col->colonists[0].active = true;
  col->colonists[0].profession = COLONIZE_PROF_FREE_COLONIST;
  col->colonists[0].building_type = -1;
  col->colonists[0].field_job = COLONIZE_JOB_COTTON_PLANTER;
  col->colonist_count = 1;
  col->population = 1;
  pool.colony_count = 1;

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 14;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Scout");
  const int uid = units_spawn(&units, 0, 1, 1);
  ColonizeUnit* u = units_get(&units, uid);
  if (!u) {
    fprintf(stderr, "phaseh2: spawn failed\n");
    return 1;
  }
  units_set_nation(u, 0);
  u->profession = COLONIZE_JOB_COTTON_PLANTER;

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.head.year = 1492;
  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 1);
  for (unsigned t = 0; t < 5000u; ++t) {
    col1.head.turn = (uint16_t)(t & 0xffffu);
    col->stock[COLONIZE_CARGO_FOOD] = 500;
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    /* Through the real EOT call site (turn_colony.c), which is where the
     * pool was dropped. */
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.human_nation = 0;
    ctx.units = &units;
    ctx.colonies = &pool;
    ctx.europe = &eu;
    ctx.col1 = &col1;
    ctx.col1_ok = true;
    ctx.rng = &rng;
    turn_run_colony_eot(&ctx, &prod);
    if (col->colonists[0].profession == COLONIZE_JOB_COTTON_PLANTER) {
      fprintf(stderr, "phaseh2: learned Cotton Planter at tick %u despite a map expert\n", t);
      return 1;
    }
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_phase_h_map_expert_blocks_learn", unit_phase_h_map_expert_blocks_learn},
    {"unit_train_veteran_soldier", unit_train_veteran_soldier},
    {"unit_school_tick_cap_is_three", unit_school_tick_cap_is_three},
    {"unit_trainfail", unit_trainfail},
    {"unit_trainprofession", unit_trainprofession},
    {"unit_traincriminal", unit_traincriminal},
    {"unit_trainindentured", unit_trainindentured},
    {"unit_phase_h_trainprofession", unit_phase_h_trainprofession},
};
TEST_MAIN(k_cases)
