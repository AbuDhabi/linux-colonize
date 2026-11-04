/* Slice of the former tests/unit/test_ai_king.c (split by feature 2026-09-23):
 * King war events, non-combat gate, 43f7_0082 spawn types. */
#include "test_ai_king_common.h"
#include "core/dos_buggy.h"
#include "core/settings.h"

/* FUN_38fd_5930 @KINGNEWWAR: Crown cancels peace with a random peer, grants gold + vets. */
static int test_king_new_war_event(void) {
  ColonizeCol1Save c;
  col1_save_init(&c);
  c.head.difficulty = 0;
  for (int i = 0; i < 4; ++i) {
    c.player[i].control = (i == 0) ? 0 : 1;
    memset(&c.nation[i], 0, sizeof(c.nation[i]));
  }
  memset(c.head.founding_father, -1, sizeof(c.head.founding_father));
  snprintf(c.player[2].country_name, sizeof(c.player[2].country_name), "Spain");
  /* Human 0 at peace with 2 only; 1 and 3 unmet. */
  ai_diplo_or_both(&c, 0, 2, (uint8_t)(AI_DIPLO_MET | AI_DIPLO_PEACE));
  c.nation[0].gold = 50;
  uint32_t turn = 500; /* (0+2)*500 > 799 */
  AiPopupState pops;
  ai_popup_clear(&pops);
  ColonizeDosRng rng;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.col1 = &c;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.turn_number = &turn;
  ctx.human_nation = 0;
  ctx.ai_popups = &pops;
  int fired = 0;
  dos_rng_seed(&rng, 100u); /* one continuous stream (LCG warm-up bias on tiny seeds) */
  for (int i = 0; i < 4000 && !fired; ++i) {
    fired = ai_king_new_war_event(&ctx);
  }
  if (!fired) {
    return fail("KINGNEWWAR should fire for some seed (roll <= difficulty)");
  }
  if (c.nation[0].gold != 150) {
    fprintf(stderr, "unit_ai_king: newwar gold %u\n", (unsigned)c.nation[0].gold);
    return fail("KINGNEWWAR should grant (difficulty+1)*100 gold when combat totals are equal");
  }
  if ((c.nation[0].euro_relation[2] & AI_DIPLO_PEACE) || (c.nation[2].euro_relation[0] & AI_DIPLO_PEACE)) {
    return fail("KINGNEWWAR should clear PEACE both ways");
  }
  if (!(ai_diplo_read(&c, 0, 2) & AI_DIPLO_CROWN_ARMED) || !(ai_diplo_read(&c, 0, 2) & AI_DIPLO_MET)) {
    return fail("KINGNEWWAR should set CROWN_ARMED (0x10) and keep MET");
  }
  if (pops.queue_count != 1 || pops.queue[0].tag != AI_POPUP_TAG_KING_TAX ||
      pops.queue[0].payload != 100) {
    return fail("KINGNEWWAR should enqueue one OK popup carrying the gold grant");
  }
  /* Franklin suppresses it entirely. */
  ai_diplo_or_both(&c, 0, 2, AI_DIPLO_PEACE);
  c.nation[0].founding_fathers[FF_BENJAMIN_FRANKLIN / 8] |= (uint8_t)(1u << (FF_BENJAMIN_FRANKLIN % 8));
  c.nation[0].gold = 50;
  for (int i = 0; i < 2000; ++i) {
    if (ai_king_new_war_event(&ctx)) {
      return fail("Franklin must suppress KINGNEWWAR");
    }
  }
  /* A met-but-unpeaced peer blocks it. */
  c.nation[0].founding_fathers[FF_BENJAMIN_FRANKLIN / 8] = 0;
  c.nation[0].euro_relation[3] = AI_DIPLO_MET; /* raw: met, no peace (or_both would default PEACE) */
  for (int i = 0; i < 2000; ++i) {
    if (ai_king_new_war_event(&ctx)) {
      return fail("a met-but-unpeaced peer must block KINGNEWWAR");
    }
  }
  fprintf(stderr, "unit_ai_king: KINGNEWWAR ok\n");
  return 0;
}

/*
 * port_bugs.md: a crown LAND unit that cannot fight must not march or attack.
 * A Wagon Train changes hands when the REF takes a port
 * (ai_king_try_capture_at flips every civilian on the tile to the crown), and
 * the King's column had no combat-role gate — so a "Tory Wagon Train" walked
 * out and opened combat on the human's units. DOS gates every combat entry on
 * the @UNIT attack byte (DS:0x5236); ai_euro_try_attack already did.
 */
static int test_king_noncombat_never_attacks(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 0;
  ai_king_latch_clear(&col1);
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    memset(&col1.nation[i], 0, sizeof(col1.nation[i]));
  }
  col1.head.game_options.woi = 1;
  col1.head.colony_count = 1;
  col1.colony = calloc(1, sizeof(ColonizeCol1Colony));
  if (!col1.colony) {
    return fail("noncombat: alloc colony");
  }
  col1.colony[0].nation_id = 0;
  col1.colony[0].x = 5;
  col1.colony[0].y = 5;
  col1.colony[0].population = 4;
  col1.colony[0].rebel_dividend = 60;
  col1.colony[0].rebel_divisor = 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("noncombat: alloc map");
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].cargo = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Continental Army");
  units.types[1].movement = 1;
  units.types[1].attack = 4;
  units.types[1].defense = 4;

  const int crown = ai_king_crown_nation(0);
  const int wagon = units_spawn_allow_stack(&units, 0, 7, 7);
  const int rebel = units_spawn_allow_stack(&units, 1, 8, 7);
  ColonizeUnit* wu = units_get(&units, wagon);
  ColonizeUnit* ru = units_get(&units, rebel);
  if (!wu || !ru) {
    return fail("noncombat: spawn");
  }
  units_set_nation(wu, crown);
  wu->orders = UNITS_ORDER_NONE;
  wu->goto_x = -1;
  wu->goto_y = -1;
  units_set_nation(ru, 0);
  ru->orders = UNITS_ORDER_FORTIFY;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 5;
  c->y = 5;
  c->population = 4;
  c->colonist_count = 4;
  snprintf(c->name, sizeof(c->name), "Jamestown");
  colonies.colony_count = 1;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7u);
  uint16_t year = 1776;
  uint16_t autumn = 0;
  uint32_t turn = 1;
  char status[128];
  status[0] = '\0';
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.human_nation = 0;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.rng = &rng;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.turn_number = &turn;
  ctx.status = status;
  ctx.status_size = sizeof(status);

  for (int t = 0; t < 4; ++t) {
    ai_king_ref_pre_euro_beat(&ctx);
    ai_king_human_2022_beat(&ctx);
    ai_king_nation_turn(&ctx);
    ColonizeUnit* w = units_get(&units, wagon);
    ColonizeUnit* r = units_get(&units, rebel);
    if (!r || !r->active || r->nation_id != 0) {
      return fail("crown Wagon Train must not be able to attack a human unit");
    }
    if (!w || !w->active) {
      return fail("crown Wagon Train should survive its own idle turn");
    }
    if (w->x != 7 || w->y != 7) {
      fprintf(stderr, "unit_ai_king: Tory wagon moved to (%d,%d)\n", w->x, w->y);
      return fail("crown Wagon Train must not join the REF hunt");
    }
    w->moves = units_max_mp(&units, wagon);
  }

  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  free(col1.colony);
  col1.colony = NULL;
  map_free(&map);
  fprintf(stderr, "unit_ai_king: crown non-combat gate ok\n");
  return 0;
}

/*
 * port_bugs.md #505 — FUN_43f7_0082 (raw 73519-73543) unit pick. k 0/1:
 *   nation > 3 || player[nation].control != 0  -> Regulars (6) / Cavalry (8)
 *   control == 0, independence declared        -> Cont. Army (9) / Cont. Cav. (7)
 *   control == 0, not yet declared             -> Dragoons (4), both slots
 * k == 2 -> Man-O-War (0x12), k == 3 -> Artillery (0xb), regardless of nation.
 */
static int case_43f7_0082_spawn_types(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    return fail("0082: NAMES.TXT load failed");
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  if (!units_load_types(&units, &names)) {
    assets_msg_free(&names);
    return fail("0082: units_load_types failed");
  }
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 12, 12, err, sizeof(err))) {
    assets_msg_free(&names);
    return fail("0082: map_alloc failed");
  }
  for (int i = 0; i < 12 * 12; ++i) {
    map.terrain[i] = 2;
    map.layer3[i] = 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[0].control = 0;
  col1.player[1].control = 1;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.units = &units;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;

  const int ty_regulars = units_find_type(&units, "Regulars");
  const int ty_cavalry = units_find_type(&units, "Cavalry");
  const int ty_army = units_find_type(&units, "Cont. Army");
  const int ty_cav = units_find_type(&units, "Cont. Cav.");
  const int ty_drag = units_find_type(&units, "Dragoons");
  const int ty_mow = units_find_type(&units, "Man-O-War");
  const int ty_art = units_find_type(&units, "Artillery");

  struct {
    int nation;
    int declared;
    int k;
    int want;
    const char* what;
  } cases[] = {
    {1, 1, 0, ty_regulars, "AI-controlled slot -> Regulars"},
    {1, 1, 1, ty_cavalry, "AI-controlled slot -> Cavalry"},
    {0, 1, 0, ty_army, "player + declared -> Cont. Army"},
    {0, 1, 1, ty_cav, "player + declared -> Cont. Cav."},
    {0, 0, 0, ty_drag, "player, pre-declaration -> Dragoons"},
    {0, 0, 1, ty_drag, "player, pre-declaration -> Dragoons"},
    {1, 1, 2, ty_mow, "k=2 -> Man-O-War"},
    {1, 0, 3, ty_art, "k=3 -> Artillery"},
  };
  int rc = 0;
  int x = 1;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    col1.head.game_options.woi = cases[i].declared ? 1u : 0u;
    const int uid = ai_king_10f0_spawn_unit(&ctx, cases[i].nation, cases[i].k, x, 1);
    const ColonizeUnit* u = uid >= 0 ? units_get_const(&units, uid) : NULL;
    if (!u || u->type_index != cases[i].want) {
      fprintf(stderr, "unit_ai_king: 0082[%s] got type=%d want=%d\n", cases[i].what,
              u ? u->type_index : -1, cases[i].want);
      rc = 1;
    }
    if (uid >= 0) {
      (void)units_despawn(&units, uid);
    }
    x = (x + 2) % 10 + 1;
  }
  map_free(&map);
  assets_msg_free(&names);
  if (rc != 0) {
    return fail("0082 spawn-type table");
  }
  fprintf(stderr, "unit_ai_king: FUN_43f7_0082 spawn types ok\n");
  return 0;
}

/*
 * port_bugs.md #878e: DS:0x9456[nation]'s "one crown hull away at a time" gate
 * (FUN_521d_20e6 disjunct `DS:0x9456[nation] != 0`, viceroy_unpacked.c
 * raw 89717-89720) is bumped the instant a hull is ordered home, not only
 * once it lands on the high-seas tile — so a SECOND crown Man-O-War must be
 * blocked from taking the sail-home arm while a first one is still mid
 * transit (orders == UNITS_ORDER_AI_SAIL, not yet on a High Seas tile).
 * Before the fix, ai_king_crown_ships_in_europe_lane only looked at the
 * live tile, which was free again by the time a second hull acted (the
 * first hull's own call had already moved it off the high-seas tile it
 * hadn't reached, or despawned it if it had) — so the gate never fired.
 */
static int case_878e_sail_home_one_at_a_time(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    return fail("878e: NAMES.TXT load failed");
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  if (!units_load_types(&units, &names)) {
    assets_msg_free(&names);
    return fail("878e: units_load_types failed");
  }
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  const int W = 40, H = 4;
  if (!map_alloc(&map, W, H, err, sizeof(err))) {
    assets_msg_free(&names);
    return fail("878e: map_alloc failed");
  }
  /* Rows 1 and 2 are open ocean corridors; the column just inside the east
   * rim is High Seas (terrain 0x1a = 26; 015e never reads a rim tile as High
   * Seas, FUN_137f_000a), everything else plain ocean (25), far enough that a
   * 4-tile move budget can't reach it in one act. */
  for (int i = 0; i < W * H; ++i) {
    map.terrain[i] = 25;
  }
  map.terrain[1 * W + (W - 2)] = 26;
  map.terrain[2 * W + (W - 2)] = 26;

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[0].control = 0;
  col1.player[1].control = 1;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
  col1.head.expeditionary_force[0] = 3; /* land pools stocked */
  col1.head.expeditionary_force[2] = 0; /* MoW pool spent -> take the sail-home arm */

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.units = &units;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;

  const int crown = 1;
  const int mow_a_id = units_spawn_allow_stack(&units, units_find_type(&units, "Man-O-War"), 2, 1);
  const int mow_b_id = units_spawn_allow_stack(&units, units_find_type(&units, "Man-O-War"), 2, 2);
  ColonizeUnit* mow_a = units_get(&units, mow_a_id);
  ColonizeUnit* mow_b = units_get(&units, mow_b_id);
  if (!mow_a || !mow_b) {
    map_free(&map);
    assets_msg_free(&names);
    return fail("878e: MoW spawn failed");
  }
  ColonizeUnit* mows[2] = {mow_a, mow_b};
  for (int i = 0; i < 2; ++i) {
    ColonizeUnit* mow = mows[i];
    mow->nation_id = crown;
    mow->cargo_count = 0;
    mow->moves = 4 * UNITS_MP_PER_TILE;
    mow->orders = UNITS_ORDER_NONE;
    mow->goto_x = -1;
    mow->goto_y = -1;
  }

  if (!ai_king_mow_sail_home_20e6(&ctx, mow_a, crown)) {
    map_free(&map);
    assets_msg_free(&names);
    return fail("878e: first empty crown MoW should take the sail-home arm");
  }
  mow_a = units_get(&units, mow_a_id);
  if (!mow_a || !mow_a->active || mow_a->orders != UNITS_ORDER_AI_SAIL ||
      map_tile_is_high_seas(&map, mow_a->x, mow_a->y)) {
    map_free(&map);
    assets_msg_free(&names);
    return fail(
      "878e: first hull should still be mid-transit (AI_SAIL, off the high-seas tile) after one act"
    );
  }

  /* Second hull, alone on its own tile, must be BLOCKED while the first is
   * still away -- the DOS "one hull in the Europe lane at a time" gate. */
  mow_b->moves = 4 * UNITS_MP_PER_TILE;
  const int second_result = ai_king_mow_sail_home_20e6(&ctx, mow_b, crown);
  mow_b = units_get(&units, mow_b_id);
  if (second_result != 0 || !mow_b || !mow_b->active || mow_b->orders == UNITS_ORDER_AI_SAIL) {
    map_free(&map);
    assets_msg_free(&names);
    return fail("878e: second crown MoW must not sail home while the first is still in the Europe lane");
  }

  map_free(&map);
  assets_msg_free(&names);
  fprintf(stderr, "unit_ai_king: 878e sail-home one-at-a-time ok\n");
  return 0;
}

/* ===== Tolerated-DOS-bug fixes (docs/tolerated_dos_bugs.md), flag off ===== */

/* Minimal 0982 fixture: 4 x 34 map, column x=0 ocean, the rest land on
 * continent 1; types 0 Regular, 1 Man-O-War. Crown 1, human 0. */
typedef struct {
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  ColonizeWorldMap map;
  ColonizeTurnContext ctx;
} TbInvFx;

static int tb_inv_fx_init(TbInvFx* f) {
  memset(f, 0, sizeof(*f));
  fx_units_init(&f->units);
  f->units.type_count = 2;
  f->units.types[0].kind_plus1 = UNITS_KIND_REGULAR + 1;
  f->units.types[0].movement = 1;
  f->units.types[0].attack = 3;
  f->units.types[0].defense = 2;
  f->units.types[1].kind_plus1 = UNITS_KIND_MAN_O_WAR + 1;
  f->units.types[1].movement = 4;
  f->units.types[1].domain = 1;
  f->units.types[1].cargo = 6;
  colonies_init(&f->colonies);
  colonies_set_occupancy_map(NULL);
  char err[128];
  if (!map_alloc(&f->map, 4, 34, err, sizeof(err))) {
    return 0;
  }
  for (int y = 0; y < 34; ++y) {
    for (int x = 0; x < 4; ++x) {
      f->map.terrain[y * 4 + x] = x == 0 ? 25 : 1;
      f->map.layer3[y * 4 + x] = x == 0 ? 0xf0 : 0xf1;
    }
  }
  f->ctx.units = &f->units;
  f->ctx.colonies = &f->colonies;
  f->ctx.map = &f->map;
  f->ctx.human_nation = 0;
  return 1;
}

static ColonizeColony* tb_inv_add_colony(TbInvFx* f, int x, int y, int colonists) {
  const int i = f->colonies.colony_count++;
  ColonizeColony* c = &f->colonies.colonies[i];
  c->id = i;
  c->active = true;
  c->nation_id = 0;
  c->x = x;
  c->y = y;
  c->population = colonists;
  c->colonist_count = colonists;
  c->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
  return c;
}

static void tb_inv_run(TbInvFx* f, uint16_t* force) {
  struct ai_king_0982_ctx w;
  memset(&w, 0, sizeof(w));
  w.ctx = &f->ctx;
  w.crown = 1;
  w.human = 0;
  w.force = force;
  ai_king_0982_invasion(&w);
}

/* TB-39: an eleventh coastal colony is scored and can be the target. */
static int case_tb39_invasion_past_ten_colonies(void) {
  TbInvFx f;
  if (!tb_inv_fx_init(&f)) {
    return fail("tb39: map_alloc");
  }
  for (int i = 0; i < 11; ++i) {
    tb_inv_add_colony(&f, 1, 3 * i + 1, i == 10 ? 10 : 1);
  }
  uint16_t force[4] = {6, 0, 1, 0};
  dos_buggy_behavior_set(false);
  tb_inv_run(&f, force);
  dos_buggy_behavior_set(true);
  int mow_y = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &f.units.units[i];
    if (u->active && u->nation_id == 1 && u->type_index == 1) {
      mow_y = u->y;
    }
  }
  map_free(&f.map);
  if (mow_y < 30) {
    fprintf(stderr, "tb39: MoW y=%d (want next to colony 10 at y=31)\n", mow_y);
    return fail("tb39: fixed 0982 must consider the eleventh coastal colony");
  }
  return 0;
}

/* Count crown Regulars on land after one 0982 wave against a colony whose
 * neighbour holds three crown Regulars (muskets 550 -> garrison 7; the wave caps at 6 and floors at 3). */
static int tb37_landed(bool buggy) {
  TbInvFx f;
  if (!tb_inv_fx_init(&f)) {
    return -1;
  }
  ColonizeColony* c = tb_inv_add_colony(&f, 1, 10, 1);
  c->stock[COLONIZE_CARGO_MUSKETS] = 550;
  for (int k = 0; k < 3; ++k) {
    ColonizeUnit* u = units_get(&f.units, units_spawn_allow_stack(&f.units, 0, 2, 10));
    if (u) {
      units_set_nation(u, 1);
    }
  }
  uint16_t force[4] = {6, 0, 1, 0};
  dos_buggy_behavior_set(buggy);
  tb_inv_run(&f, force);
  dos_buggy_behavior_set(true);
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &f.units.units[i];
    if (u->active && u->nation_id == 1 && u->type_index == 0) {
      n++;
    }
  }
  map_free(&f.map);
  return n - 3;
}

/* TB-37: the neighbour's own crown units reduce the garrison need. */
static int case_tb37_garrison_reads_neighbour_head(void) {
  const int dos = tb37_landed(true);
  const int fixed = tb37_landed(false);
  if (dos != 6 || fixed != 4) {
    fprintf(stderr, "tb37: landed dos=%d fixed=%d (want 6/4)\n", dos, fixed);
    return fail("tb37: fixed 0982 must count the neighbour tile's crown units");
  }
  return 0;
}

/* TB-38: a stale crown owner nibble on an empty tile is not crown-held. */
static int case_tb38_land_ignores_stale_nibble(void) {
  TbInvFx f;
  memset(&f, 0, sizeof(f));
  fx_units_init(&f.units);
  f.units.type_count = 1;
  f.units.types[0].kind_plus1 = UNITS_KIND_REGULAR + 1;
  f.units.types[0].movement = 1;
  f.units.types[0].attack = 3;
  f.units.types[0].defense = 2;
  char err[128];
  if (!map_alloc(&f.map, 8, 8, err, sizeof(err))) {
    return fail("tb38: map_alloc");
  }
  for (int i = 0; i < 64; ++i) {
    f.map.terrain[i] = 1;
    f.map.layer3[i] = 0xf1;
  }
  const int crown = 1;
  ColonizeUnit* held = units_get(&f.units, units_spawn_allow_stack(&f.units, 0, 4, 4));
  if (!held) {
    map_free(&f.map);
    return fail("tb38: spawn");
  }
  units_set_nation(held, crown);
  f.map.layer3[4 * 8 + 4] = (uint8_t)((crown << 4) | 1);
  f.map.layer3[2 * 8 + 4] = (uint8_t)((crown << 4) | 1); /* stale: (4,2) is empty */
  ColonizeColony col;
  memset(&col, 0, sizeof(col));
  col.x = 5;
  col.y = 3;
  f.ctx.units = &f.units;
  f.ctx.map = &f.map;
  uint16_t force[4] = {3, 0, 0, 0};
  dos_buggy_behavior_set(false);
  ai_king_0982_land_troops(&f.ctx, crown, force, &col, map_continent_id_at(&f.map, 4, 3), 0, 3, 3,
                           3);
  dos_buggy_behavior_set(true);
  int at_held = 0;
  int elsewhere = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &f.units.units[i];
    if (u->active && u->nation_id == crown) {
      if (u->x == 4 && u->y == 4) {
        at_held++;
      } else {
        elsewhere++;
      }
    }
  }
  units_set_occupancy_map(NULL);
  map_free(&f.map);
  if (at_held != 4 || elsewhere != 0) {
    fprintf(stderr, "tb38: held=%d elsewhere=%d (want 4/0)\n", at_held, elsewhere);
    return fail("tb38: fixed landing must not treat a stale-nibble empty tile as crown-held");
  }
  return 0;
}

/* ===== divergences.hard_king (docs/settings.md) ===== */

static void hard_king_set(bool on) {
  ColonizeSettings st = *settings_get();
  st.hard_king = on;
  settings_set(&st);
}

static int count_crown(const TbInvFx* f, int type_index, int* ys, int ys_cap) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &f->units.units[i];
    if (u->active && u->nation_id == 1 && u->type_index == type_index) {
      if (ys && n < ys_cap) {
        ys[n] = u->y;
      }
      n++;
    }
  }
  return n;
}

/* Waves per turn = difficulty + 1 while hulls last, each at another colony. */
static int hard_king_waves(int difficulty, int* ys) {
  static ColonizeCol1Save col1;
  TbInvFx f;
  if (!tb_inv_fx_init(&f)) {
    return -1;
  }
  tb_inv_add_colony(&f, 1, 5, 2);
  tb_inv_add_colony(&f, 1, 15, 2);
  tb_inv_add_colony(&f, 1, 25, 2);
  memset(&col1, 0, sizeof(col1));
  col1.head.difficulty = (uint8_t)difficulty;
  col1.head.expeditionary_force[0] = 12;
  col1.head.expeditionary_force[2] = 3;
  ai_king_set_independence(&col1, 1);
  f.ctx.col1 = &col1;
  f.ctx.col1_ok = true;
  hard_king_set(true);
  ai_king_ref_wave(&f.ctx);
  hard_king_set(false);
  const int n = count_crown(&f, 1, ys, 5);
  map_free(&f.map);
  return n;
}

static int case_hard_king_waves_per_turn(void) {
  int ys[5] = {0};
  if (hard_king_waves(0, ys) != 1) {
    return fail("hard_king: Discoverer lands one wave a turn");
  }
  const int n = hard_king_waves(4, ys);
  if (n != 3) {
    fprintf(stderr, "hard_king: Viceroy hulls=%d (want 3: pool of 3)\n", n);
    return fail("hard_king: Viceroy lands a wave per hull, up to 5");
  }
  int band[3] = {0};
  for (int i = 0; i < 3; ++i) {
    band[ys[i] / 10] = 1;
  }
  if (!band[0] || !band[1] || !band[2]) {
    return fail("hard_king: the three waves must hit three different colonies");
  }
  return 0;
}

/* The target's own units no longer steer the landing to empty tiles. */
static int hard_king_soldier_survives(bool hard) {
  TbInvFx f;
  memset(&f, 0, sizeof(f));
  fx_units_init(&f.units);
  f.units.type_count = 1;
  f.units.types[0].kind_plus1 = UNITS_KIND_REGULAR + 1;
  f.units.types[0].movement = 1;
  f.units.types[0].attack = 3;
  f.units.types[0].defense = 2;
  char err[128];
  if (!map_alloc(&f.map, 8, 8, err, sizeof(err))) {
    return -1;
  }
  for (int i = 0; i < 64; ++i) {
    f.map.terrain[i] = 1;
    f.map.layer3[i] = 0xf1;
  }
  const int sid = units_spawn_allow_stack(&f.units, 0, 4, 3);
  units_set_nation(units_get(&f.units, sid), 0);
  ColonizeColony col;
  memset(&col, 0, sizeof(col));
  col.x = 5;
  col.y = 3;
  f.ctx.units = &f.units;
  f.ctx.map = &f.map;
  f.ctx.human_nation = 0;
  uint16_t force[4] = {3, 0, 0, 0};
  hard_king_set(hard);
  dos_buggy_behavior_set(false); /* TB-38: crown-held = a live crown unit, not the nibble */
  ai_king_0982_land_troops(&f.ctx, 1, force, &col, map_continent_id_at(&f.map, 4, 3), 0, 3, 4, 4);
  dos_buggy_behavior_set(true);
  hard_king_set(false);
  const ColonizeUnit* s = units_get_const(&f.units, sid);
  const int alive = s && s->active && s->nation_id == 0;
  units_set_occupancy_map(NULL);
  map_free(&f.map);
  return alive;
}

static int case_hard_king_lands_on_units(void) {
  if (hard_king_soldier_survives(false) != 1) {
    return fail("hard_king off: DOS landing avoids the soldier's tile");
  }
  if (hard_king_soldier_survives(true) != 0) {
    return fail("hard_king on: the landing seizes the soldier's tile");
  }
  return 0;
}

/* A soldier two tiles out makes its colony the less attractive target. */
static int hard_king_target_y(bool hard) {
  TbInvFx f;
  if (!tb_inv_fx_init(&f)) {
    return -1;
  }
  tb_inv_add_colony(&f, 1, 5, 3);
  tb_inv_add_colony(&f, 1, 20, 3);
  units_set_nation(units_get(&f.units, units_spawn_allow_stack(&f.units, 0, 3, 20)), 0);
  uint16_t force[4] = {6, 0, 1, 0};
  hard_king_set(hard);
  tb_inv_run(&f, force);
  hard_king_set(false);
  int y = -1;
  (void)count_crown(&f, 1, &y, 1);
  map_free(&f.map);
  return y;
}

static int case_hard_king_area_garrison(void) {
  const int off = hard_king_target_y(false);
  const int on = hard_king_target_y(true);
  if (off < 15 || on < 0 || on > 10) {
    fprintf(stderr, "hard_king: MoW y off=%d on=%d (want ~20 / ~5)\n", off, on);
    return fail("hard_king: 5x5 soldiers must count against the colony's attractiveness");
  }
  return 0;
}

/*
 * FUN_38fd_5930 raw 68340 skips self and DS:0x53d2 only. Before succession
 * 0x53d2 = -1, so nation 1 (the port's fallback crown for human 0) counts:
 * peace with it alone lets the event fire, and met-no-peace blocks it.
 */
static int case_newwar_counts_fallback_crown(void) {
  ColonizeCol1Save c;
  col1_save_init(&c);
  c.head.difficulty = 0;
  for (int i = 0; i < 4; ++i) {
    c.player[i].control = (i == 0) ? 0 : 1;
    memset(&c.nation[i], 0, sizeof(c.nation[i]));
  }
  memset(c.head.founding_father, -1, sizeof(c.head.founding_father));
  if (c.head.crown_nation_id != -1) {
    return fail("newwar: col1_save_init must leave crown_nation_id = -1");
  }
  ai_diplo_or_both(&c, 0, 1, (uint8_t)(AI_DIPLO_MET | AI_DIPLO_PEACE));
  uint32_t turn = 500;
  ColonizeDosRng rng;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.col1 = &c;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.turn_number = &turn;
  ctx.human_nation = 0;
  dos_rng_seed(&rng, 100u);
  int fired = 0;
  for (int i = 0; i < 4000 && !fired; ++i) {
    fired = ai_king_new_war_event(&ctx);
  }
  if (!fired) {
    return fail("newwar: peace with nation 1 alone must let KINGNEWWAR fire before succession");
  }
  /* Nation 1 met, no peace; peace with 2: DOS blocks. */
  c.nation[0].euro_relation[1] = AI_DIPLO_MET;
  c.nation[1].euro_relation[0] = AI_DIPLO_MET;
  ai_diplo_or_both(&c, 0, 2, (uint8_t)(AI_DIPLO_MET | AI_DIPLO_PEACE));
  for (int i = 0; i < 2000; ++i) {
    if (ai_king_new_war_event(&ctx)) {
      return fail("newwar: met-no-peace nation 1 must block KINGNEWWAR before succession");
    }
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"case_newwar_counts_fallback_crown", case_newwar_counts_fallback_crown},
    {"test_king_new_war_event", test_king_new_war_event},
    {"test_king_noncombat_never_attacks", test_king_noncombat_never_attacks},
    {"case_43f7_0082_spawn_types", case_43f7_0082_spawn_types},
    {"case_878e_sail_home_one_at_a_time", case_878e_sail_home_one_at_a_time},
    {"case_tb39_invasion_past_ten_colonies", case_tb39_invasion_past_ten_colonies},
    {"case_tb37_garrison_reads_neighbour_head", case_tb37_garrison_reads_neighbour_head},
    {"case_tb38_land_ignores_stale_nibble", case_tb38_land_ignores_stale_nibble},
    {"case_hard_king_waves_per_turn", case_hard_king_waves_per_turn},
    {"case_hard_king_lands_on_units", case_hard_king_lands_on_units},
    {"case_hard_king_area_garrison", case_hard_king_area_garrison},
};
TEST_MAIN(k_cases)
