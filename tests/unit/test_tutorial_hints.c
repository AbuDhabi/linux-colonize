/*
 * Tutorial hints (src/core/game_tutorial.c): one case per DOS gate / latch of
 * GAME.TXT @TUTORIAL1-4 and @TUTORIAL7-19. Each case checks that the hint is
 * queued with its GAME.TXT body (tokens filled exactly as the port's DOS read
 * says), that its COL1 head tut bit latches, that the latch keeps it from
 * firing twice, and that the Tutorial Hints option (DS:0x5382 bit 7) gates it
 * (@TUTORIAL18 has no gate). The DOS sites are cited in game_tutorial.c.
 */
#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_preview.h"
#include "core/colony_yield.h"
#include "core/game_dialogs.h"
#include "core/game_loop_internal.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/units.h"
#include "core/units_move.h"

#include "../common/ai_fixture.h"
#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NAME "unit_tutorial_hints"
#include "../common/test_fail.h"

#define FX_W 24
#define FX_H 24

static ColonizeMsgCatalog s_names;
static bool s_names_ok = false;

/* All-plains board with an open-sea column at x = 1 (water region 1), the
 * human (nation 0) on turn 5, Tutorial Hints on. */
static ColonizeGameState* fx_game(void) {
  if (!s_names_ok) {
    assets_msg_init(&s_names);
    if (!assets_msg_load_file(&s_names, "COLONIZE/NAMES.TXT")) {
      return NULL;
    }
    reports_names_load_catalogs("COLONIZE");
    s_names_ok = true;
  }
  ColonizeGameState* g = calloc(1, sizeof(*g));
  if (!g) {
    return NULL;
  }
  assets_msg_init(&g->messages);
  if (!assets_msg_load_file(&g->messages, "COLONIZE/GAME.TXT")) {
    free(g);
    return NULL;
  }
  ai_popup_init(&g->ai_popups);
  fx_units_init(&g->units);
  fx_colonies_init(&g->colonies);
  if (!units_load_types(&g->units, &s_names) || !colonies_load_buildings(&g->colonies, &s_names) ||
      !fx_map_alloc(&g->world_map, FX_W, FX_H, T_PLAINS, false)) {
    assets_msg_free(&g->messages);
    free(g);
    return NULL;
  }
  for (int y = 0; y < FX_H; ++y) {
    for (int x = 0; x < FX_W; ++x) {
      const int i = y * FX_W + x;
      if (x <= 1) {
        g->world_map.terrain[i] = T_OCEAN;
        g->world_map.layer3[i] = 0xf1; /* open sea: water region 1 */
      } else {
        g->world_map.layer3[i] = 0xf2; /* unowned land, continent 2 */
      }
    }
  }
  g->units_ok = true;
  g->world_map_ok = true;
  g->col1_ok = true;
  g->human_nation = 0;
  g->colony_view_id = -1;
  g->col1.head.turn = 5;
  g->col1.head.difficulty = 1;
  g->col1.head.game_options.tutorial_hints = 1;
  return g;
}

static void fx_free(ColonizeGameState* g) {
  if (!g) {
    return;
  }
  fx_map_free(&g->world_map);
  assets_msg_free(&g->messages);
  free(g);
}

static int fx_spawn(ColonizeGameState* g, ColonizeUnitKind kind, int x, int y) {
  return units_spawn_allow_stack(&g->units, units_kind_type_index(&g->units, kind), x, y);
}

static ColonizeColony* fx_colony(ColonizeGameState* g, int x, int y, int pop) {
  ColonizeColony* c = fx_colony_add(&g->colonies, 0, x, y, pop);
  snprintf(c->name, sizeof(c->name), "%s", "Testville");
  for (int i = 0; i < pop; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].unit_type_index = units_kind_type_index(&g->units, UNITS_KIND_COLONIST);
    c->colonists[i].profession = UNITS_JOB_NONE;
    c->colonists[i].building_type = -1;
    c->colonists[i].field_job = -1;
  }
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    c->tiles[t] = -1;
  }
  return c;
}

/* Popups raised so far: queued plus the one a colony-screen hint presented. */
static int popup_total(const ColonizeGameState* g) {
  return g->ai_popups.queue_count + (g->ai_popups.open ? 1 : 0);
}

/* The newest popup (queued, else the open one) is `section` filled with `tok`. */
static const AiPopupRequest* popup_last(const ColonizeGameState* g) {
  if (g->ai_popups.queue_count > 0) {
    return &g->ai_popups.queue[g->ai_popups.queue_count - 1];
  }
  return g->ai_popups.open ? &g->ai_popups.current : NULL;
}

static bool queued_last_is(const ColonizeGameState* g, const char* section, const PopupMsgTokens* tok) {
  const AiPopupRequest* req = popup_last(g);
  if (!req) {
    return false;
  }
  char want[AI_POPUP_BODY_LEN];
  popup_msg_fill(&g->messages, section, tok, "", want, sizeof(want));
  (void)popup_msg_take_pending_graphic();
  if (!want[0] || strcmp(req->body, want) != 0) {
    fprintf(stderr, "  want %s: \"%s\"\n  got: \"%s\"\n", section, want, req->body);
    return false;
  }
  return true;
}

static PopupMsgTokens no_tok(void) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  return tok;
}

/* Runs the piece-selected chain on `uid` and expects exactly one new popup. */
static bool chain_fires(ColonizeGameState* g, int uid, const char* section, const PopupMsgTokens* tok) {
  const int before = popup_total(g);
  game_tutorial_piece_selected(g, uid);
  return popup_total(g) == before + 1 && queued_last_is(g, section, tok);
}

static bool chain_silent(ColonizeGameState* g, int uid) {
  const int before = popup_total(g);
  game_tutorial_piece_selected(g, uid);
  return popup_total(g) == before;
}

#define FX_OPEN(g)                                                                   \
  ColonizeGameState* g = fx_game();                                                  \
  if (!g) {                                                                          \
    fprintf(stderr, "SKIP " TEST_NAME ": COLONIZE/NAMES.TXT or GAME.TXT missing\n"); \
    return TR_SKIP;                                                                  \
  }

/* @TUTORIAL1: turn 0 on Discoverer only; latch DS:0x5386 bit 4 (tut2.nr1). */
static int test_tutorial1_turn0_discoverer(void) {
  FX_OPEN(g);
  int rc = 0;
  const int ship = fx_spawn(g, UNITS_KIND_CARAVEL, 1, 10);
  g->col1.head.turn = 0;
  g->col1.head.tut3.nr11 = 1; /* the lone caravel would otherwise read as TUTORIAL11 */
  if (!chain_silent(g, ship)) {
    rc = fail("TUTORIAL1 must need difficulty 0");
  }
  g->col1.head.difficulty = 0;
  g->col1.head.game_options.tutorial_hints = 0;
  if (rc == 0 && !chain_silent(g, ship)) {
    rc = fail("hints off must silence the chain");
  }
  g->col1.head.game_options.tutorial_hints = 1;
  PopupMsgTokens tok = no_tok();
  tok.string0 = units_type(&g->units, units_get_const(&g->units, ship)->type_index)->name;
  if (rc == 0 && (!chain_fires(g, ship, "TUTORIAL1", &tok) || !g->col1.head.tut2.nr1)) {
    rc = fail("TUTORIAL1 on the turn-0 caravel, latch tut2.nr1");
  }
  if (rc == 0 && popup_last(g)->graphic_mss != 0) {
    rc = fail("TUTORIAL1 carries the MSS0 admiral (FUN_281f_0652(0x8b3, 0))");
  }
  if (rc == 0 && !chain_silent(g, ship)) {
    rc = fail("TUTORIAL1 latched: no second showing");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL11: an empty ship alone on its tile before turn 20; tut3.nr11. */
static int test_tutorial11_lone_empty_ship(void) {
  FX_OPEN(g);
  int rc = 0;
  const int ship = fx_spawn(g, UNITS_KIND_CARAVEL, 1, 10);
  ColonizeUnit* su = units_get(&g->units, ship);
  su->hold_goods_type[0] = 0;
  su->hold_goods_amount[0] = 50;
  if (!chain_silent(g, ship)) {
    rc = fail("TUTORIAL11 must want empty holds");
  }
  su->hold_goods_amount[0] = 0;
  g->col1.head.turn = 20;
  if (rc == 0 && !chain_silent(g, ship)) {
    rc = fail("TUTORIAL11 ends at turn 20");
  }
  g->col1.head.turn = 19;
  PopupMsgTokens tok = no_tok();
  tok.string0 = units_type(&g->units, su->type_index)->name;
  tok.string1 = reports_home_port_name(0);
  if (rc == 0 && (!chain_fires(g, ship, "TUTORIAL11", &tok) || !g->col1.head.tut3.nr11)) {
    rc = fail("TUTORIAL11 on a lone empty caravel, latch tut3.nr11");
  }
  if (rc == 0 && !chain_silent(g, ship)) {
    rc = fail("TUTORIAL11 latched");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL13: pioneers ashore before turn 20 with no colony; tut1.nr13. */
static int test_tutorial13_pioneer_ashore(void) {
  FX_OPEN(g);
  int rc = 0;
  const int on_sea = fx_spawn(g, UNITS_KIND_PIONEER, 1, 5);
  if (!chain_silent(g, on_sea)) {
    rc = fail("TUTORIAL13 must want land");
  }
  const int pio = fx_spawn(g, UNITS_KIND_PIONEER, 10, 10);
  fx_colony(g, 20, 20, 1);
  if (rc == 0 && !chain_silent(g, pio)) {
    rc = fail("TUTORIAL13 must want no colony yet");
  }
  g->colonies.colonies[0].active = false;
  PopupMsgTokens tok = no_tok();
  if (rc == 0 && (!chain_fires(g, pio, "TUTORIAL13", &tok) || !g->col1.head.tut1.nr13)) {
    rc = fail("TUTORIAL13 on pioneers ashore, latch tut1.nr13");
  }
  if (rc == 0 && popup_last(g)->graphic_mss != 3) {
    rc = fail("TUTORIAL13 carries MSS3");
  }
  if (rc == 0 && !chain_silent(g, pio)) {
    rc = fail("TUTORIAL13 latched");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL14: soldiers ashore before turn 20; tut1.nr14. */
static int test_tutorial14_soldier_ashore(void) {
  FX_OPEN(g);
  int rc = 0;
  const int sol = fx_spawn(g, UNITS_KIND_SOLDIER, 10, 10);
  g->col1.head.turn = 20;
  if (!chain_silent(g, sol)) {
    rc = fail("TUTORIAL14 ends at turn 20");
  }
  g->col1.head.turn = 3;
  PopupMsgTokens tok = no_tok();
  if (rc == 0 && (!chain_fires(g, sol, "TUTORIAL14", &tok) || !g->col1.head.tut1.nr14)) {
    rc = fail("TUTORIAL14 on soldiers ashore, latch tut1.nr14");
  }
  if (rc == 0 && !chain_silent(g, sol)) {
    rc = fail("TUTORIAL14 latched");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL15: colonists standing in a colony before turn 40; tut1.nr15. */
static int test_tutorial15_colonist_in_colony(void) {
  FX_OPEN(g);
  int rc = 0;
  fx_colony(g, 10, 10, 1);
  const int col = fx_spawn(g, UNITS_KIND_COLONIST, 10, 10);
  g->col1.head.turn = 40;
  if (!chain_silent(g, col)) {
    rc = fail("TUTORIAL15 ends at turn 40");
  }
  g->col1.head.turn = 39;
  PopupMsgTokens tok = no_tok();
  tok.string0 = "Testville";
  if (rc == 0 && (!chain_fires(g, col, "TUTORIAL15", &tok) || !g->col1.head.tut1.nr15)) {
    rc = fail("TUTORIAL15 names the colony, latch tut1.nr15");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL3: pioneers on a good coastal first-colony site with a special
 * resource in the 3x3 (TUTORIAL13 already latched, as it comes first). */
static int test_tutorial3_first_colony_site(void) {
  FX_OPEN(g);
  int rc = 0;
  g->world_map.prime_resource_seed = 0x1234;
  g->col1.head.tut1.nr13 = 1;
  /* A pioneer at x = 2 touches the x = 1 sea column; find a row whose 3x3
   * carries a resource FUN_2b5a_0070 maps to a cargo. */
  int py = -1;
  for (int y = 2; y < FX_H - 2 && py < 0; ++y) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        const int r = map_resource_type_for_yield(&g->world_map, 2 + dx, y + dy);
        if (r >= 2 && r <= 13 && r != 7 && r != 11) {
          py = y;
        }
      }
    }
  }
  if (py < 0) {
    fx_free(g);
    return fail("fixture: no special resource next to the coast");
  }
  const int pio = fx_spawn(g, UNITS_KIND_PIONEER, 2, py);
  ColonizeColony* near = fx_colony(g, 3, py + 1, 1);
  near->nation_id = 1; /* any nation's colony next door (FUN_281f_0614(.., -1, -1)) */
  if (!chain_silent(g, pio)) {
    rc = fail("TUTORIAL3 must not fire next to a colony");
  }
  near->active = false;
  const int inland = fx_spawn(g, UNITS_KIND_PIONEER, 12, py);
  if (rc == 0 && !chain_silent(g, inland)) {
    rc = fail("TUTORIAL3 must want open sea next door");
  }
  const int before = popup_total(g);
  game_tutorial_piece_selected(g, pio);
  if (rc == 0 && (popup_total(g) != before + 1 || !g->col1.head.tut2.nr3)) {
    rc = fail("TUTORIAL3 on the coastal resource site, latch tut2.nr3");
  }
  if (rc == 0 && popup_last(g)->graphic_mss != 3) {
    rc = fail("TUTORIAL3 carries MSS3");
  }
  if (rc == 0 && !chain_silent(g, pio)) {
    rc = fail("TUTORIAL3 latched");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL8: a Free Colonist / Indentured Servant once a tribe is met; tut3.nr8. */
static int test_tutorial8_unskilled_after_contact(void) {
  FX_OPEN(g);
  int rc = 0;
  const int col = fx_spawn(g, UNITS_KIND_COLONIST, 10, 10);
  units_get(&g->units, col)->profession = UNITS_JOB_NONE;
  if (!chain_silent(g, col)) {
    rc = fail("TUTORIAL8 must want a met tribe");
  }
  g->col1.nation[0].relation_by_indian[3] = 0x60;
  PopupMsgTokens tok = no_tok();
  tok.string0 = reports_job_short_name(0x13);
  if (rc == 0 && (!chain_fires(g, col, "TUTORIAL8", &tok) || !g->col1.head.tut3.nr8)) {
    rc = fail("TUTORIAL8 names @JOB row 0x13 for a Free Colonist, latch tut3.nr8");
  }
  if (rc == 0 && !chain_silent(g, col)) {
    rc = fail("TUTORIAL8 latched");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL9: pioneers next to their colony on a roadless forest; tut3.nr9. */
static int test_tutorial9_road_hint(void) {
  FX_OPEN(g);
  int rc = 0;
  fx_colony(g, 10, 10, 1);
  g->world_map.terrain[10 * FX_W + 11] = T_PLAINS | T_FOREST_BIT;
  const int pio = fx_spawn(g, UNITS_KIND_PIONEER, 11, 10);
  map_tile_set_road(&g->world_map, 11, 10, true);
  if (!chain_silent(g, pio)) {
    rc = fail("TUTORIAL9 must want a roadless tile");
  }
  map_tile_set_road(&g->world_map, 11, 10, false);
  PopupMsgTokens tok = no_tok();
  if (rc == 0 && (!chain_fires(g, pio, "TUTORIAL9", &tok) || !g->col1.head.tut3.nr9)) {
    rc = fail("TUTORIAL9 next to the colony, latch tut3.nr9");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL10: pioneers next to their colony on unplowed open land; tut3.nr10. */
static int test_tutorial10_plow_hint(void) {
  FX_OPEN(g);
  int rc = 0;
  fx_colony(g, 10, 10, 1);
  const int far = fx_spawn(g, UNITS_KIND_PIONEER, 13, 10);
  if (!chain_silent(g, far)) {
    rc = fail("TUTORIAL10 must want the colony adjacent");
  }
  const int pio = fx_spawn(g, UNITS_KIND_PIONEER, 11, 11);
  map_tile_set_plowed(&g->world_map, 11, 11, true);
  if (rc == 0 && !chain_silent(g, pio)) {
    rc = fail("TUTORIAL10 must want an unplowed tile");
  }
  map_tile_set_plowed(&g->world_map, 11, 11, false);
  PopupMsgTokens tok = no_tok();
  if (rc == 0 && (!chain_fires(g, pio, "TUTORIAL10", &tok) || !g->col1.head.tut3.nr10)) {
    rc = fail("TUTORIAL10 on plains next to the colony, latch tut3.nr10");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL19: an Indian Convert; tut1.nr19, now behind the chain's option gate. */
static int test_tutorial19_convert(void) {
  FX_OPEN(g);
  int rc = 0;
  const int cv = fx_spawn(g, UNITS_KIND_COLONIST, 10, 10);
  units_get(&g->units, cv)->profession = UNITS_JOB_CONVERT;
  g->col1.head.game_options.tutorial_hints = 0;
  if (!chain_silent(g, cv)) {
    rc = fail("TUTORIAL19 is gated by the option (FUN_2b5a_0e52 raw 42330)");
  }
  g->col1.head.game_options.tutorial_hints = 1;
  units_get(&g->units, cv)->orders = UNITS_ORDER_SENTRY;
  if (rc == 0 && !chain_silent(g, cv)) {
    rc = fail("the chain wants an order-free piece (+0x314c == 0)");
  }
  units_get(&g->units, cv)->orders = UNITS_ORDER_NONE;
  PopupMsgTokens tok = no_tok();
  if (rc == 0 && (!chain_fires(g, cv, "TUTORIAL19", &tok) || !g->col1.head.tut1.nr19)) {
    rc = fail("TUTORIAL19 on a Convert, latch tut1.nr19");
  }
  if (rc == 0 && !chain_silent(g, cv)) {
    rc = fail("TUTORIAL19 latched");
  }
  fx_free(g);
  return rc;
}

/* The chain stops at the first hint: turn-0 pioneers get @TUTORIAL1 only. */
static int test_chain_one_hint_per_activation(void) {
  FX_OPEN(g);
  int rc = 0;
  g->col1.head.turn = 0;
  g->col1.head.difficulty = 0;
  const int pio = fx_spawn(g, UNITS_KIND_PIONEER, 10, 10);
  PopupMsgTokens tok = no_tok();
  tok.string0 = units_type(&g->units, units_get_const(&g->units, pio)->type_index)->name;
  if (!chain_fires(g, pio, "TUTORIAL1", &tok) || g->col1.head.tut1.nr13) {
    rc = fail("TUTORIAL1 first, TUTORIAL13 not in the same activation");
  }
  if (rc == 0 && !chain_fires(g, pio, "TUTORIAL13", &(PopupMsgTokens){0})) {
    rc = fail("the next activation reaches TUTORIAL13");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL2: the land-ho tail, option-gated, no latch of its own. */
static int test_tutorial2_landho(void) {
  FX_OPEN(g);
  int rc = 0;
  g->col1.head.game_options.tutorial_hints = 0;
  game_tutorial_landho(g);
  if (g->ai_popups.queue_count != 0) {
    rc = fail("TUTORIAL2 is option-gated");
  }
  g->col1.head.game_options.tutorial_hints = 1;
  game_tutorial_landho(g);
  PopupMsgTokens tok = no_tok();
  if (rc == 0 && (g->ai_popups.queue_count != 1 || !queued_last_is(g, "TUTORIAL2", &tok))) {
    rc = fail("TUTORIAL2 after the New World is named");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL4: colony-screen bring-up; %STRING0 = the @CARGO row of colonist 0's
 * occupation (a Carpenter reads Hammers), %STRING1 = the last other field
 * cargo his plot yields, else Food; tut2.nr4. */
static int tut4_strings(ColonizeGameState* g, ColonizeColony* c, const char* s0, const char* s1) {
  g->col1.head.tut2.nr4 = 0;
  ai_popup_init(&g->ai_popups);
  game_tutorial_colony_opened(g, c->id);
  PopupMsgTokens tok = no_tok();
  tok.string0 = s0;
  tok.string1 = s1;
  return g->col1.head.tut2.nr4 && queued_last_is(g, "TUTORIAL4", &tok);
}

static int test_tutorial4_colony_jobs(void) {
  FX_OPEN(g);
  int rc = 0;
  ColonizeColony* c = fx_colony(g, 10, 10, 1);
  g->in_colony = true;
  g->colony_view_id = c->id;
  char cargo[20][32];
  for (int i = 0; i < 20; ++i) {
    const char* n = reports_names_field("CARGO", i, 0);
    snprintf(cargo[i], sizeof(cargo[i]), "%s", n ? n : "");
  }
  c->colonists[0].building_type = colonies_building_row(&g->colonies, COLONY_BUILDING_CARPENTERS_SHOP);
  if (colonies_colonist_occupation_job(&g->colonies, &c->colonists[0]) != COLONIZE_PROF_CARPENTER ||
      !tut4_strings(g, c, cargo[0x10], cargo[0])) {
    rc = fail("TUTORIAL4 for a Carpenter names Hammers and Food, latch tut2.nr4");
  }
  /* A farmer on the plains plot east of the colony: Food, then the last
   * nonzero plains field yield other than Food. */
  int plot = -1;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    int dx = 0;
    int dy = 0;
    if (plot < 0 && colonies_field_tile_delta(t, &dx, &dy) && dx == 1 && dy == 0) {
      plot = t;
    }
  }
  c->colonists[0].building_type = -1;
  c->colonists[0].field_job = 0;
  c->tiles[plot] = 0;
  int other = 0;
  for (int k = 1; k < 8; ++k) {
    if (colony_yield_terrain_class_base(T_PLAINS, k) != 0) {
      other = k;
    }
  }
  if (rc == 0 && (other == 0 || !tut4_strings(g, c, cargo[0], cargo[other]))) {
    rc = fail("TUTORIAL4 for a plains farmer names Food and the plot's last other cargo");
  }
  const int n = popup_total(g);
  game_tutorial_colony_opened(g, c->id);
  if (rc == 0 && popup_total(g) != n) {
    rc = fail("TUTORIAL4 latched");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL12: a ship in the colony's tile chain at the bring-up; tut3.nr12. */
static int test_tutorial12_ship_in_port(void) {
  FX_OPEN(g);
  int rc = 0;
  ColonizeColony* c = fx_colony(g, 10, 10, 1);
  g->col1.head.tut2.nr4 = 1;
  g->in_colony = true;
  g->colony_view_id = c->id;
  game_tutorial_colony_opened(g, c->id);
  if (g->ai_popups.queue_count != 0 || g->ai_popups.open || g->col1.head.tut3.nr12) {
    rc = fail("TUTORIAL12 must want a ship in port");
  }
  fx_spawn(g, UNITS_KIND_CARAVEL, 10, 10);
  game_tutorial_colony_opened(g, c->id);
  PopupMsgTokens tok = no_tok();
  tok.string0 = "Testville";
  if (rc == 0 && (!g->col1.head.tut3.nr12 || !queued_last_is(g, "TUTORIAL12", &tok))) {
    rc = fail("TUTORIAL12 names the colony, latch tut3.nr12");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL7: an accepted assignment in a colony of more than two with no
 * Stockade; tut3.nr7. */
static int test_tutorial7_stockade_hint(void) {
  FX_OPEN(g);
  int rc = 0;
  ColonizeColony* c = fx_colony(g, 10, 10, 2);
  g->in_colony = true;
  g->colony_view_id = c->id;
  game_tutorial_colony_assigned(g);
  if (g->ai_popups.queue_count != 0 || g->col1.head.tut3.nr7) {
    rc = fail("TUTORIAL7 wants more than two colonists");
  }
  ColonizeColony* big = fx_colony(g, 14, 14, 3);
  g->colony_view_id = big->id;
  const int stockade = colonies_building_row(&g->colonies, COLONY_BUILDING_STOCKADE);
  if (stockade < 0) {
    fx_free(g);
    return fail("fixture: no Stockade row");
  }
  big->has_building[stockade] = true;
  game_tutorial_colony_assigned(g);
  if (rc == 0 && (g->ai_popups.queue_count != 0 || g->col1.head.tut3.nr7)) {
    rc = fail("TUTORIAL7 wants no Stockade");
  }
  big->has_building[stockade] = false;
  game_tutorial_colony_assigned(g);
  PopupMsgTokens tok = no_tok();
  tok.string0 = "Testville";
  if (rc == 0 && (!g->col1.head.tut3.nr7 || !queued_last_is(g, "TUTORIAL7", &tok))) {
    rc = fail("TUTORIAL7 names the colony, latch tut3.nr7");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL16: the colony screen's refresh with food gross below demand; tut1.nr16. */
static int test_tutorial16_food_short(void) {
  FX_OPEN(g);
  int rc = 0;
  ColonizeColony* c = fx_colony(g, 10, 10, 1);
  g->in_colony = true;
  g->colony_view_id = c->id;
  ColonizeWorld w = game_world(g);
  ColonizeColonyPreview p;
  colony_preview_compute_w(&w, c, &p);
  if (p.food_produced < p.food_consumed) {
    fx_free(g);
    return fail("fixture: one idle colonist must be fed by the commons");
  }
  game_tutorial_colony_refresh(g);
  if (g->ai_popups.queue_count != 0 || g->col1.head.tut1.nr16) {
    rc = fail("TUTORIAL16 wants a food shortfall");
  }
  ColonizeColony* hungry = fx_colony(g, 16, 16, 8);
  g->colony_view_id = hungry->id;
  colony_preview_compute_w(&w, hungry, &p);
  if (rc == 0 && p.food_produced >= p.food_consumed) {
    fx_free(g);
    return fail("fixture: eight idle colonists must starve");
  }
  g->col1.head.game_options.tutorial_hints = 0;
  game_tutorial_colony_refresh(g);
  if (rc == 0 && (g->ai_popups.queue_count != 0 || g->col1.head.tut1.nr16)) {
    rc = fail("TUTORIAL16 is option-gated");
  }
  g->col1.head.game_options.tutorial_hints = 1;
  game_tutorial_colony_refresh(g);
  PopupMsgTokens tok = no_tok();
  if (rc == 0 && (!g->col1.head.tut1.nr16 || !queued_last_is(g, "TUTORIAL16", &tok))) {
    rc = fail("TUTORIAL16 on a food shortfall, latch tut1.nr16");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL17: the European Status bring-up; tut1.nr17. */
static int test_tutorial17_europe_open(void) {
  FX_OPEN(g);
  int rc = 0;
  snprintf(g->col1.player[0].country_name, sizeof(g->col1.player[0].country_name), "%s", "Newland");
  game_tutorial_europe_opened(g);
  PopupMsgTokens tok = no_tok();
  tok.string0 = reports_home_port_name(0);
  tok.string1 = reports_nation_country_phrase(0, 0);
  tok.string2 = "Newland";
  if (!g->col1.head.tut1.nr17 || !queued_last_is(g, "TUTORIAL17", &tok)) {
    rc = fail("TUTORIAL17 with home port, country and New World name, latch tut1.nr17");
  }
  game_tutorial_europe_opened(g);
  if (rc == 0 && g->ai_popups.queue_count != 1) {
    rc = fail("TUTORIAL17 latched");
  }
  fx_free(g);
  return rc;
}

/* @TUTORIAL18: a plain drag the treasury cannot pay for 100 of is refused,
 * every time, whatever the option says. */
static int test_tutorial18_cannot_afford(void) {
  FX_OPEN(g);
  int rc = 0;
  g->units_ok = false; /* six-hold fallback for the harbor ship */
  g->europe_ok = true;
  g->col1.head.game_options.tutorial_hints = 0;
  EuropeScreen* eu = &g->europe;
  eu->cargo_count = 16;
  snprintf(eu->cargo[COLONIZE_CARGO_TOOLS].name, sizeof(eu->cargo[0].name), "%s", "x");
  eu->cargo[COLONIZE_CARGO_TOOLS].ask = 2;
  eu->harbor_ships = 1;
  eu->gold = 200;
  g->col1.nation[0].gold = 200;
  if (game_tutorial_europe_cannot_afford(g, 0, COLONIZE_CARGO_TOOLS) ||
      g->ai_popups.queue_count != 0) {
    rc = fail("200 gold pays 100 tools at 2");
  }
  eu->gold = 199;
  g->col1.nation[0].gold = 199;
  char name[32];
  snprintf(name, sizeof(name), "%s", reports_names_field("CARGO", COLONIZE_CARGO_TOOLS, 0));
  PopupMsgTokens tok = no_tok();
  tok.string0 = name;
  tok.number0 = 2;
  tok.has_number0 = true;
  tok.number1 = 199;
  tok.has_number1 = true;
  for (int i = 0; rc == 0 && i < 2; ++i) {
    if (!game_tutorial_europe_cannot_afford(g, 0, COLONIZE_CARGO_TOOLS) ||
        g->ai_popups.queue_count != i + 1 || !queued_last_is(g, "TUTORIAL18", &tok)) {
      rc = fail("199 gold: refused with TUTORIAL18, every time, option off");
    }
  }
  fx_free(g);
  return rc;
}

/* The latches are COL1 head bytes DS:0x5380 / 0x5386 / 0x5387: they survive a
 * save round trip. */
static int test_latches_round_trip_save(void) {
  ColonizeCol1Save* a = calloc(1, sizeof(*a));
  ColonizeCol1Save* b = calloc(1, sizeof(*b));
  int rc = 0;
  if (!a || !b) {
    rc = fail("alloc");
    goto out;
  }
  char err[256];
  if (!col1_save_read_file("test-saves-col1/COLONY00.SAV", a, err, sizeof(err))) {
    free(a);
    free(b);
    fprintf(stderr, "SKIP " TEST_NAME ": test-saves-col1/COLONY00.SAV: %s\n", err);
    return TR_SKIP;
  }
  a->head.tut1.nr13 = a->head.tut1.nr14 = a->head.tut1.nr15 = 1;
  a->head.tut1.nr16 = a->head.tut1.nr17 = a->head.tut1.nr19 = 1;
  a->head.tut2.nr1 = a->head.tut2.nr3 = a->head.tut2.nr4 = 1;
  a->head.tut3.nr7 = a->head.tut3.nr8 = a->head.tut3.nr9 = a->head.tut3.nr10 = 1;
  a->head.tut3.nr11 = a->head.tut3.nr12 = 1;
  uint8_t* data = NULL;
  size_t size = 0;
  if (!col1_save_write_memory(a, &data, &size, err, sizeof(err)) ||
      !col1_save_read_memory(data, size, b, err, sizeof(err))) {
    fprintf(stderr, "  %s\n", err);
    rc = fail("col1 save round trip");
  } else if (memcmp(&a->head.tut1, &b->head.tut1, 1) != 0 ||
             memcmp(&a->head.tut2, &b->head.tut2, 1) != 0 ||
             memcmp(&a->head.tut3, &b->head.tut3, 1) != 0) {
    rc = fail("tut latch bytes must round-trip");
  }
  free(data);
out:
  col1_save_free(a);
  col1_save_free(b);
  free(a);
  free(b);
  return rc;
}

static const TestCase k_cases[] = {
    {"test_tutorial1_turn0_discoverer", test_tutorial1_turn0_discoverer},
    {"test_tutorial11_lone_empty_ship", test_tutorial11_lone_empty_ship},
    {"test_tutorial13_pioneer_ashore", test_tutorial13_pioneer_ashore},
    {"test_tutorial14_soldier_ashore", test_tutorial14_soldier_ashore},
    {"test_tutorial15_colonist_in_colony", test_tutorial15_colonist_in_colony},
    {"test_tutorial3_first_colony_site", test_tutorial3_first_colony_site},
    {"test_tutorial8_unskilled_after_contact", test_tutorial8_unskilled_after_contact},
    {"test_tutorial9_road_hint", test_tutorial9_road_hint},
    {"test_tutorial10_plow_hint", test_tutorial10_plow_hint},
    {"test_tutorial19_convert", test_tutorial19_convert},
    {"test_chain_one_hint_per_activation", test_chain_one_hint_per_activation},
    {"test_tutorial2_landho", test_tutorial2_landho},
    {"test_tutorial4_colony_jobs", test_tutorial4_colony_jobs},
    {"test_tutorial12_ship_in_port", test_tutorial12_ship_in_port},
    {"test_tutorial7_stockade_hint", test_tutorial7_stockade_hint},
    {"test_tutorial16_food_short", test_tutorial16_food_short},
    {"test_tutorial17_europe_open", test_tutorial17_europe_open},
    {"test_tutorial18_cannot_afford", test_tutorial18_cannot_afford},
    {"test_latches_round_trip_save", test_latches_round_trip_save},
};

TEST_MAIN(k_cases)
