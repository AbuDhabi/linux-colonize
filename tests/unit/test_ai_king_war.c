/* Slice of the former tests/unit/test_ai_king.c (split by feature 2026-09-23):
 * King war events, non-combat gate, 43f7_0082 spawn types. */
#include "test_ai_king_common.h"

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
  /* Human 0 at peace with 2 only (1 is the Crown/REF slot for human 0); 3 unmet. */
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
 * bugs.md: a crown LAND unit that cannot fight must not march or attack.
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
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("noncombat: alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* land */
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
 * bugs.md #505 — FUN_43f7_0082 (raw 73519-73543) unit pick. k 0/1:
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

static const TestCase k_cases[] = {
    {"test_king_new_war_event", test_king_new_war_event},
    {"test_king_noncombat_never_attacks", test_king_noncombat_never_attacks},
    {"case_43f7_0082_spawn_types", case_43f7_0082_spawn_types},
};
TEST_MAIN(k_cases)
