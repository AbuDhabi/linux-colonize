/*
 * Stage-seam smoke: the 2026-09-16 splits made ai_euro_act_*, game_update_*,
 * game_render_*, game_move_*, ai_contact_raid_*, ai_king_0982_*, ai_021a_*,
 * turn_step_* and turn_year_end_* file-local (static). core/internal.h's
 * COLONIZE_INTERNAL macro (COLONIZE_TESTING flips it to external linkage on
 * colonize_core, see CMakeLists.txt) plus each module's <module>_internal.h
 * now let a test call one stage directly instead of only through its
 * dispatcher. This file exercises one stage per header to prove the seam
 * actually works, not to re-cover behaviour the module's own tests already
 * pin.
 */
#include "core/ai_contact_internal.h"
#include "core/ai_euro_internal.h"
#include "core/ai_internal.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/game_dialogs.h"
#include "core/font_file.h"
#include "core/colony_screen.h"
#include "core/dos_buggy.h"
#include "core/game_loop_internal.h"
#include "core/popup.h"
#include "core/turn_internal.h"

#include "../common/ai_fixture.h"
#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NAME "unit_stage_seams"
#include "../common/test_fail.h"

/* dos_buggy_behavior: off at load (the opencol state); the test-binary
 * constructor (test_name_kinds.c, default priority, runs after this one) turns
 * it on before main(). */
static bool s_dos_buggy_at_load = true;
__attribute__((constructor(101))) static void capture_dos_buggy_at_load(void) {
  s_dos_buggy_at_load = dos_buggy_behavior();
}

static int test_dos_buggy_default_off_tests_on(void) {
  if (s_dos_buggy_at_load) {
    return fail("dos_buggy_behavior on before any setter ran");
  }
  if (!dos_buggy_behavior()) {
    return fail("test harness did not turn dos_buggy_behavior on");
  }
  return 0;
}

/* turn_year_end_rival_rebels: pure function of (rebel_sentiment, census_pop_proxy). */
static int test_turn_year_end_rival_rebels(void) {
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.nation[1].rebel_sentiment = 50;
  col1.stuff.census_pop_proxy[1] = 20;
  const int v = turn_year_end_rival_rebels(&col1, 1);
  if (v != 10) { /* 50 * 20 / 100 */
    fprintf(stderr, "  got %d want 10\n", v);
    return fail("turn_year_end_rival_rebels formula");
  }
  /* Out-of-range rival is a no-op 0, not a crash. */
  if (turn_year_end_rival_rebels(&col1, 99) != 0) {
    return fail("turn_year_end_rival_rebels range guard");
  }
  return 0;
}

/* ai_contact_raid_alarm_delta: pure per-kind table, DOS-literal deltas. */
static int test_ai_contact_raid_alarm_delta(void) {
  if (ai_contact_raid_alarm_delta(AI_RAID_NOTHING) != 0) {
    return fail("raid_alarm_delta NOTHING");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_STORES) != -4) {
    return fail("raid_alarm_delta STORES");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_SHIP) != -16) {
    return fail("raid_alarm_delta SHIP");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_BURN) != -12) {
    return fail("raid_alarm_delta BURN");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_GOLD) != -8) {
    return fail("raid_alarm_delta GOLD");
  }
  return 0;
}

/* FUN_5fef_0f14's five human-victim raid outcome cue arms. */
static int test_ai_contact_raid_sound_rows(void) {
  const AiRaidChrome* row = ai_contact_raid_chrome_row(AI_RAID_NOTHING, 0);
  if (!row || row->sound != 0x5b || row->sound2 != -1) {
    return fail("raid sound NOTHING must be 0x5b");
  }
  row = ai_contact_raid_chrome_row(AI_RAID_STORES, 0);
  if (!row || row->sound != 0x4f || row->sound2 != -1) {
    return fail("raid sound STORES must be 0x4f");
  }
  row = ai_contact_raid_chrome_row(AI_RAID_BURN, 1);
  if (!row || row->sound != 0x53 || row->sound2 != -1) {
    return fail("raid sound BURN must be 0x53");
  }
  row = ai_contact_raid_chrome_row(AI_RAID_SHIP, 0);
  if (!row || row->sound != 0x4b || row->sound2 != 0x4d) {
    return fail("raid sound SHIP must be 0x4b then 0x4d");
  }
  row = ai_contact_raid_chrome_row(AI_RAID_GOLD, 0);
  if (!row || row->sound != 0x4e || row->sound2 != -1) {
    return fail("raid sound GOLD must be 0x4e");
  }
  return 0;
}

/* ai_021a_dir_tile on a hand-built tile ring: plain terrain, no owner, no
 * village — dest tile should score as unowned/self and never SKIP. */
static int test_ai_021a_dir_tile(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);

  struct ai_021a_ctx c;
  memset(&c, 0, sizeof(c));
  c.map = &map;
  c.units = &units;
  c.col1 = NULL;
  c.colonies = NULL;
  c.col = NULL;
  c.village = NULL;
  c.nation_id = 0;
  c.x = 4;
  c.y = 4;
  c.d = 0; /* east: MAP_DIR8 order used elsewhere; any in-range dir works */
  c.threat_nation = -1;
  c.vx = 0;
  c.vy = 0;

  const Ai021aDirStatus st = ai_021a_dir_tile(&c);
  fx_map_free(&map);

  if (st != AI_021A_DIR_OK) {
    return fail("ai_021a_dir_tile unexpectedly skipped a plain land tile");
  }
  /* Fresh layer3 is zero everywhere, so the destination reads as owned by
   * nation 0 == c.nation_id -> "not foreign", no grudge/hostile. */
  if (c.owner != c.nation_id) {
    return fail("ai_021a_dir_tile owner mismatch on zeroed layer3");
  }
  if (c.hostile != 0 || c.grudge != 0) {
    return fail("ai_021a_dir_tile hostile/grudge should be clear for own tile");
  }
  return 0;
}

/* port_bugs.md #553: FUN_465b_0000 local_4 — the settlement owner of a Euro
 * colony tile, overridden by the stack head's nation. ai_native_brave_step
 * uses it to keep a Brave from walking INTO a foreign colony (DOS attacks or
 * exhausts instead; it never co-locates). */
static int test_ai_465b_dest_owner(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  const size_t ci = (size_t)4 * map.width + 5;
  for (size_t i = 0; i < (size_t)map.width * map.height; ++i) {
    map.layer3[i] = 0xf0; /* unowned */
  }
  map.layer2[ci] |= 0x02u;  /* settlement bit */
  map.layer3[ci] = 0x21;    /* owner nibble 2 (Spain) */
  int rc = 0;
  if (ai_465b_dest_owner(&map, &units, 5, 4) != 2) {
    rc = fail("empty Spanish colony tile must read owner 2");
  }
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 3, 3) != -1) {
    rc = fail("empty unowned land must read -1");
  }
  ColonizeUnit* g = &units.units[0];
  g->active = true;
  g->id = units.next_id++;
  g->x = 5;
  g->y = 4;
  g->aboard_ship_id = -1;
  g->nation_id = 2;
  units_pool_sync(&units);
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 5, 4) != 2) {
    rc = fail("garrisoned colony must read the garrison's nation");
  }
  /* A lone Euro unit on open ground is foreign to a Brave too. */
  g->x = 3;
  g->y = 3;
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 3, 3) != 2) {
    rc = fail("unit-held open tile must read the unit's nation");
  }
  ColonizeUnit* later = &units.units[1];
  later->active = true;
  later->id = units.next_id++;
  later->x = 3;
  later->y = 3;
  later->aboard_ship_id = -1;
  later->nation_id = 3;
  units_pool_sync(&units);
  units_tile_stack_arrive(&units, g->id);
  units_tile_stack_arrive(&units, later->id);
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 3, 3) != 3) {
    rc = fail("mixed stack must use latest arrival's nation");
  }
  units_tile_stack_arrive(&units, g->id);
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 3, 3) != 2) {
    rc = fail("lower-slot returning unit must become stack head");
  }
  fx_map_free(&map);
  return rc;
}

/* game_render_select_palette with no display dependency: an in_menu-only
 * game state with none of the other screen flags set must pick game->palette
 * verbatim (the first arm of the cascade). */
static int test_game_render_select_palette(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  game.in_menu = true;
  game.palette.rgb[1][0] = 11;
  game.palette.rgb[1][1] = 22;
  game.palette.rgb[1][2] = 33;

  ColonizeFramebuffer8 fb;
  memset(&fb, 0, sizeof(fb));

  ColonizePalette out;
  memset(&out, 0, sizeof(out));

  game_render_select_palette(&game, &fb, &out, /*render_log_counter=*/1);

  if (out.rgb[1][0] != 11 || out.rgb[1][1] != 22 || out.rgb[1][2] != 33) {
    return fail("game_render_select_palette did not pick game->palette for in_menu");
  }
  return 0;
}

/*
 * window_log_fit_line: a line that does not fit keeps the first two words, the
 * last two and the {emphasised} ones, with "..." for each dropped run.
 */
static int test_window_log_fit_line(void) {
  ColonizeFont font;
  memset(&font, 0, sizeof(font));
  char err[128];
  if (!font_file_load("COLONIZE/FONTTINY.FF", &font, err, sizeof(err))) {
    fprintf(stderr, "SKIP popup_wrap_seam: COLONIZE/FONTTINY.FF not present\n");
    return TR_SKIP; /* no data dir in this checkout — the seam is still compiled */
  }

  const char* text =
    "Colony: the people of {Jamestown} are alarmed by filler filler filler filler end here";
  char out[160];
  int rc = 0;

  window_log_fit_line(&font, text, 4000, out, sizeof(out));
  if (strcmp(out, text) != 0) {
    rc = fail("a line that fits must be left alone");
  }

  window_log_fit_line(&font, text, 150, out, sizeof(out));
  if (popup_markup_text_width(&font, out) > 150) {
    fprintf(stderr, "  got \"%s\" (%d px)\n", out, popup_markup_text_width(&font, out));
    rc = fail("fitted line still wider than the strip");
  }
  if (strncmp(out, "Colony: the", 11) != 0 || !strstr(out, "{Jamestown}") ||
      !strstr(out, "end here") || !strstr(out, "...")) {
    fprintf(stderr, "  got \"%s\"\n", out);
    rc = fail("fitted line lost a first/last/emphasised word, or the ellipsis");
  }

  /* Squeezed hard, the emphasis goes too, but the braces stay balanced. */
  window_log_fit_line(&font, text, 60, out, sizeof(out));
  if (popup_markup_text_width(&font, out) > 60) {
    fprintf(stderr, "  got \"%s\"\n", out);
    rc = fail("hard-squeezed line still too wide");
  }
  int open_braces = 0;
  for (const char* p = out; *p; ++p) {
    if (*p == '{') {
      open_braces++;
    } else if (*p == '}') {
      open_braces--;
    }
    if (open_braces < 0) {
      break;
    }
  }
  if (open_braces != 0) {
    fprintf(stderr, "  got \"%s\"\n", out);
    rc = fail("elision left unbalanced emphasis markup");
  }

  /* A full-length popup body (the @REBELMAJORITY text) must keep its ending:
   * the log stores whole bodies and elides in the middle. */
  const char* sol =
    "Sons of Liberty membership in {Jamestown} is up to 75%, Your Excellency. "
    "A majority of the colonists there support the idea of independence from "
    "England! All colonists in Jamestown gain +1 to their base production "
    "abilities.";
  window_log_fit_line(&font, sol, 316, out, sizeof(out));
  if (popup_markup_text_width(&font, out) > 316) {
    fprintf(stderr, "  got \"%s\"\n", out);
    rc = fail("SoL line still wider than the strip");
  }
  if (strncmp(out, "Sons of", 7) != 0 || !strstr(out, "production abilities.")) {
    fprintf(stderr, "  got \"%s\"\n", out);
    rc = fail("SoL line lost its opening or its ending");
  }

  /* Word wrap: given room, a long entry flows over several rows intact; given
   * one row, the tail is elided into it. */
  static char wrapped[WINDOW_LOG_MAX_LINES][WINDOW_LOG_LINE_LEN];
  const int n = window_log_wrap(&font, sol, 316, 4, wrapped);
  if (n < 2) {
    rc = fail("long entry did not wrap over several rows");
  }
  char joined[WINDOW_LOG_LINE_LEN * 4] = {0};
  for (int i = 0; i < n; ++i) {
    if (popup_markup_text_width(&font, wrapped[i]) > 316) {
      fprintf(stderr, "  row %d: \"%s\"\n", i, wrapped[i]);
      rc = fail("wrapped row wider than the strip");
    }
    const size_t used = strlen(joined);
    snprintf(joined + used, sizeof(joined) - used, "%s ", wrapped[i]);
  }
  if (strstr(joined, "...") != NULL) {
    fprintf(stderr, "  got \"%s\"\n", joined);
    rc = fail("entry that fits the rows must not be elided");
  }
  if (!strstr(joined, "{Jamestown}") || !strstr(joined, "abilities.")) {
    fprintf(stderr, "  got \"%s\"\n", joined);
    rc = fail("wrapped entry lost emphasis or its ending");
  }
  if (window_log_wrap(&font, sol, 316, 1, wrapped) != 1 ||
      !strstr(wrapped[0], "...") || !strstr(wrapped[0], "abilities.")) {
    fprintf(stderr, "  got \"%s\"\n", wrapped[0]);
    rc = fail("single-row wrap must elide and keep the ending");
  }

  font_file_free(&font);
  return rc;
}

/* port_bugs.md #948: amount 0 is the normal whole-hold unload request. */
static int test_game_colony_unload_whole_hold(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  colonies_init(&game.colonies);
  game.colony_view_id = 0;
  ColonizeColony* colony = &game.colonies.colonies[0];
  colony->id = 0;
  colony->active = true;
  colony->nation_id = 0;
  game.colonies.colony_count = 1;

  fx_units_init(&game.units);
  game.units.type_count = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  game.units.types[0].movement = 4;
  game.units.types[0].cargo = 2;
  snprintf(game.units.types[0].name, sizeof(game.units.types[0].name), "Test Ship");
  const int uid = units_spawn(&game.units, 0, 5, 5);
  if (uid < 0 || units_load_goods(&game.units, uid, COLONIZE_CARGO_SUGAR, 60) != 60) {
    return fail("whole-hold unload fixture");
  }

  const int moved = game_colony_unload_hold_commit(&game, uid, 0, 0, NULL);
  if (moved != 60 || colony->stock[COLONIZE_CARGO_SUGAR] != 60) {
    return fail("zero amount must unload the whole hold");
  }
  return 0;
}

/* FUN_15eb_1068 raw 11232-11243: a body's tools byte is banked only through a
 * Pioneer's job cargo list. A Scout carrying the residue of a Pioneer stripped
 * in Europe must not re-equip as a Pioneer for free; a real Pioneer still
 * returns its tools to the warehouse. */
static int test_game_colony_outside_role_tools_residue(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  ColonizeUnitPool* units = calloc(1, sizeof(*units));
  ColonizeColonyPool* pool = calloc(1, sizeof(*pool));
  int rc = 0;
  if (!units || !pool || !assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    rc = fail("tools-residue fixture");
    goto out;
  }
  units_reset(units);
  colonies_init(pool);
  if (!units_load_types(units, &names) || !colonies_load_buildings(pool, &names)) {
    rc = fail("tools-residue catalogs");
    goto out;
  }
  ColonizeColony* colony = &pool->colonies[0];
  colony->id = 0;
  colony->active = true;
  colony->x = 4;
  colony->y = 4;
  pool->colony_count = 1;
  const int scout = units_spawn(units, units_kind_type_index(units, UNITS_KIND_SCOUT), 4, 4);
  const int pioneer = units_spawn_allow_stack(units, units_kind_type_index(units, UNITS_KIND_PIONEER), 4, 4);
  ColonizeUnit* su = units_get(units, scout);
  ColonizeUnit* pu = units_get(units, pioneer);
  if (!su || !pu) {
    rc = fail("tools-residue spawn");
    goto out;
  }
  su->tools = 100;
  pu->tools = 100;
  if (game_colony_apply_outside_role(pool, colony, units, scout, COLONIZE_EJECT_PIONEER) ||
      colony->stock[COLONIZE_CARGO_TOOLS] != 0) {
    rc = fail("a Scout's leftover tools byte must not equip a Pioneer");
    goto out;
  }
  if (!game_colony_apply_outside_role(pool, colony, units, pioneer, COLONIZE_EJECT_COLONIST) ||
      colony->stock[COLONIZE_CARGO_TOOLS] != 100 || pu->tools != 0) {
    rc = fail("a Pioneer's tools go back to the warehouse");
  }
out:
  assets_msg_free(&names);
  free(units);
  free(pool);
  return rc;
}

/* port_bugs.md #950: a terrain-pedia detour from a zoomed colony is still part of
 * that colony visit. Queued EOT chrome must remain held until the player
 * leaves the restored colony screen, not merely until in_colony temporarily
 * drops while the article owns the display. */
static int test_colony_zoom_hold_survives_pedia_detour(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  game.found_open_colony_id = -1;
  game.ff_pedia_after_report = -1;
  fx_colonies_init(&game.colonies);
  if (!fx_colony_add(&game.colonies, 0, 4, 4, 1)) {
    return fail("zoom-hold colony fixture");
  }
  ai_popup_init(&game.ai_popups);
  if (!ai_popup_enqueue_ok(&game.ai_popups, AI_POPUP_TAG_INFO, NULL, "queued")) {
    return fail("zoom-hold popup fixture");
  }

  game.colony_zoom_popup_hold = true;
  game.in_pedia = true;
  game.pedia_view = PEDIA_VIEW_ARTICLE;
  game.pedia_return_to_list = false;
  game.pedia_return_colony_id = 0;

  ColonizeInputState input;
  memset(&input, 0, sizeof(input));
  input.last_key = COLONIZE_KEY_NONE;
  (void)game_update_services(&game, &input, 16);
  if (!game.colony_zoom_popup_hold || game.ai_popups.open) {
    units_reset_hooks();
    return fail("pedia detour released zoom hold");
  }

  input.last_key = COLONIZE_KEY_ESCAPE;
  if (game_update_pedia_screen(&game, &input) != GAME_UPDATE_RETURN_TRUE ||
      game.in_pedia || !game.in_colony || game.colony_view_id != 0) {
    units_reset_hooks();
    return fail("pedia detour did not restore colony");
  }

  memset(&input, 0, sizeof(input));
  input.last_key = COLONIZE_KEY_NONE;
  (void)game_update_services(&game, &input, 16);
  if (!game.colony_zoom_popup_hold || game.ai_popups.open) {
    units_reset_hooks();
    return fail("restored colony did not retain zoom hold");
  }

  game.in_colony = false;
  (void)game_update_services(&game, &input, 16);
  if (game.colony_zoom_popup_hold || !game.ai_popups.open) {
    units_reset_hooks();
    return fail("leaving colony did not release queued popup");
  }
  units_reset_hooks();
  return 0;
}

/* campaign4 FoodToIsabella: DOS stores the trade-route cursor in +0x315b,
 * but the runtime keeps its unpacked stop index in col1_counter16 (+0x315a).
 * A wagon's ordinary colony-arrival reset must not erase that runtime cursor
 * before game_trade_route_retarget services the stop. */
static int test_trade_route_wagon_services_arrival_stop(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/2, /*with_seen=*/true)) {
    return fail("trade-route map alloc");
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  game.col1_ok = true;

  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].kind_plus1 = UNITS_KIND_WAGON + 1;
  game.units.types[0].movement = 2;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  game.units.types[0].cargo = 2;

  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  ColonizeColony* load = fx_colony_add(&game.colonies, 0, 5, 4, 1);
  ColonizeColony* other = fx_colony_add(&game.colonies, 0, 6, 4, 1);
  load->stock[COLONIZE_CARGO_SUGAR] = 60;
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);

  ColonizeCol1TradeRoute* route = &game.col1.trade_route[0];
  route->dest_count = 2;
  route->stop[0].colony_index = (uint16_t)other->id;
  route->stop[1].colony_index = (uint16_t)load->id;
  route->stop[1].load_count = 1;
  col1_trade_nibble_set(
    route->stop[1].load_cargo_nibbles, 0, COLONIZE_CARGO_SUGAR
  );

  const int uid = units_spawn(&game.units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&game.units, uid);
  if (!wagon) {
    colonies_set_occupancy_map(NULL);
    units_set_occupancy_map(NULL);
    fx_map_free(&game.world_map);
    return fail("trade-route wagon spawn");
  }
  wagon->nation_id = 0;
  wagon->orders = UNITS_ORDER_TRADE_ROUTE;
  wagon->follow_unit_id = 0;
  wagon->col1_counter16 = 1;
  wagon->goto_x = load->x;
  wagon->goto_y = load->y;

  const ColonizeWorld w = fx_world(
    &game.units, &game.colonies, &game.world_map, &game.col1, NULL, NULL
  );
  int rc = 0;
  if (!units_try_move_w(&w, uid, load->x, load->y)) {
    rc = fail("trade-route wagon could not enter load colony");
  } else {
    game_trade_route_retarget(&game, wagon);
    int sugar = 0;
    for (int h = 0; h < units_goods_hold_count(&game.units, uid); ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_SUGAR) {
        sugar += wagon->hold_goods_amount[h];
      }
    }
    if (sugar != 60 || load->stock[COLONIZE_CARGO_SUGAR] != 0) {
      rc = fail("trade-route arrival serviced the wrong stop");
    } else if (wagon->col1_counter16 != 0 ||
               wagon->goto_x != other->x || wagon->goto_y != other->y) {
      rc = fail("trade-route arrival did not advance to the next stop");
    }
  }

  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/* DOS FUN_479b_0bd0 raw 77234-77238 copies the DS:0x8542 colony record's
 * x/y into the unit's goto pair whatever the unit's domain: a route ship
 * aims at the colony TILE and docks there (which is what puts passengers
 * ashore), never at an adjacent water tile. */
static int test_trade_route_ship_aims_colony_tile(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/2, /*with_seen=*/true)) {
    return fail("trade-route ship map alloc");
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  game.col1_ok = true;

  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].kind_plus1 = UNITS_KIND_CARAVEL + 1;
  game.units.types[0].movement = 4;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  game.units.types[0].cargo = 2;

  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  ColonizeColony* port = fx_colony_add(&game.colonies, 0, 5, 4, 1);
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);

  ColonizeCol1TradeRoute* route = &game.col1.trade_route[0];
  route->dest_count = 2;
  route->stop[0].colony_index = (uint16_t)port->id;
  route->stop[1].colony_index = 999;

  const int uid = units_spawn(&game.units, 0, 2, 4);
  ColonizeUnit* ship = units_get(&game.units, uid);
  int rc = 0;
  if (!ship) {
    rc = fail("trade-route ship spawn");
  } else {
    ship->nation_id = 0;
    ship->orders = UNITS_ORDER_TRADE_ROUTE;
    ship->follow_unit_id = 0;
    if (!game_trade_route_aim_stop(&game, ship, 0)) {
      rc = fail("trade-route ship could not aim colony stop");
    } else if (ship->goto_x != port->x || ship->goto_y != port->y) {
      rc = fail("trade-route ship stopped short of the colony tile");
    }
  }

  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/* DOS FUN_2b5a_1e66 raw 42862-42864 assigns order 2 and immediately calls
 * FUN_479b_0bd0(unit, 1). Starting a full-MP wagon at the other stop must
 * therefore retain control long enough to begin moving, even when another
 * idle unit is waiting in the activation queue. */
static int test_trade_route_begin_moves_wagon_to_selected_stop(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/2, /*with_seen=*/true)) {
    return fail("trade-route begin map alloc");
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  game.col1_ok = true;

  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].kind_plus1 = UNITS_KIND_WAGON + 1;
  game.units.types[0].movement = 2;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  game.units.types[0].cargo = 2;

  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  ColonizeColony* colony1 = fx_colony_add(&game.colonies, 0, 3, 3, 1);
  ColonizeColony* colony2 = fx_colony_add(&game.colonies, 0, 4, 3, 1);
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);

  ColonizeCol1TradeRoute* route = &game.col1.trade_route[0];
  route->dest_count = 2;
  route->stop[0].colony_index = (uint16_t)colony1->id;
  route->stop[1].colony_index = (uint16_t)colony2->id;

  const int wagon_id = units_spawn(&game.units, 0, colony1->x, colony1->y);
  const int other_id = units_spawn(&game.units, 0, 6, 6);
  ColonizeUnit* wagon = units_get(&game.units, wagon_id);
  ColonizeUnit* other = units_get(&game.units, other_id);
  int rc = 0;
  if (!wagon || !other) {
    rc = fail("trade-route begin wagon spawn");
  } else {
    wagon->nation_id = 0;
    wagon->moves = 6;
    other->nation_id = 0;
    other->moves = 6;
    game.units.selected_id = wagon_id;
    game.map_cursor_x = wagon->x;
    game.map_cursor_y = wagon->y;

    game_trade_begin_at_stop(&game, 0, 1);
    if (wagon->orders != UNITS_ORDER_TRADE_ROUTE || wagon->moves != 6 ||
        wagon->goto_x != colony2->x || wagon->goto_y != colony2->y) {
      rc = fail("trade-route begin did not preserve MP and aim Colony 2");
    } else if (game.units.selected_id != wagon_id) {
      rc = fail("trade-route begin handed control away before movement");
    } else {
      ColonizeInputState input;
      memset(&input, 0, sizeof(input));
      (void)game_update_unit_pacer(&game, &input, 100);
      if (wagon->x != colony2->x || wagon->y != colony2->y || wagon->moves >= 6) {
        rc = fail("trade-route wagon did not move toward Colony 2");
      }
    }
  }

  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}


/*
 * Playtest C2-B2: a Treasure walking into a coastal colony under Go To must get
 * the King's Galleon offer, exactly as an arrow-key step does (FUN_479b_0972
 * walks a human goto through FUN_465b_0000, raw 75800-75815).
 */
static int test_goto_treasure_gets_king_galleon_offer(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/2, /*with_seen=*/true)) {
    return fail("goto treasure map alloc");
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  game.col1_ok = true;
  ai_popup_clear(&game.ai_popups);

  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].kind_plus1 = UNITS_KIND_TREASURE + 1;
  game.units.types[0].movement = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  ColonizeColony* colony = fx_colony_add(&game.colonies, 0, 4, 3, 1);
  colony->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);

  const int tid = units_spawn(&game.units, 0, 3, 3);
  ColonizeUnit* t = units_get(&game.units, tid);
  int rc = 0;
  if (!t) {
    rc = fail("goto treasure spawn");
  } else {
    t->nation_id = 0;
    t->moves = 3;
    t->profession = 10; /* 1000 gold (+0x315b * 100) */
    t->orders = UNITS_ORDER_GOTO;
    t->goto_x = (uint8_t)colony->x;
    t->goto_y = (uint8_t)colony->y;
    game.units.selected_id = tid;
    game.map_cursor_x = t->x;
    game.map_cursor_y = t->y;
    ColonizeInputState input;
    memset(&input, 0, sizeof(input));
    (void)game_update_unit_pacer(&game, &input, 100);
    bool offered = false;
    for (int q = 0; q < game.ai_popups.queue_count; ++q) {
      offered |= game.ai_popups.queue[q].tag == AI_POPUP_TAG_KING_GALLEON &&
        game.ai_popups.queue[q].payload == tid;
    }
    if (t->x != colony->x || t->y != colony->y) {
      rc = fail("goto treasure did not reach the colony");
    } else if (!offered) {
      rc = fail("goto treasure arrival raised no King's Galleon offer");
    }
  }

  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/*
 * Playtest C3-B4: a ship bumping a foreign Euro colony must reach the
 * FUN_5f7a_0662 trade arm (FUN_4720_015c leaves the step valid, raw
 * 76005-76006) instead of stopping silently in the landfall probe. With no
 * peace treaty the arm answers @TRADEATWAR and spends the whole allotment.
 */
static int test_ship_bumps_foreign_colony_trade_arm(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/25, /*with_seen=*/true)) {
    return fail("ship trade map alloc");
  }
  for (int i = 0; i < 64; ++i) {
    game.world_map.layer3[i] = 1;
  }
  game.world_map.terrain[3 * 8 + 4] = 2;
  game.world_map.layer3[3 * 8 + 4] = 0xf0;
  game.world_map_ok = true;
  game.human_nation = 0;
  game.col1_ok = true;
  ai_popup_clear(&game.ai_popups);

  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  game.units.types[0].movement = 4;
  game.units.types[0].cargo = 2;

  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  ColonizeColony* colony = fx_colony_add(&game.colonies, 1, 4, 3, 1);
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);

  const int sid = units_spawn(&game.units, 0, 3, 3);
  ColonizeUnit* ship = units_get(&game.units, sid);
  int rc = 0;
  if (!ship || units_load_goods(&game.units, sid, COLONIZE_CARGO_FURS, 50) != 50) {
    rc = fail("ship trade spawn");
  } else {
    ship->nation_id = 0;
    ship->moves = 12;
    game.units.selected_id = sid;
    (void)game_try_unit_move(&game, colony->x, colony->y);
    const bool asked = game.ai_popups.open && game.ai_popups.current.tag == AI_POPUP_TAG_INFO;
    if (ship->x != 3 || ship->y != 3) {
      rc = fail("ship entered the foreign colony");
    } else if (ship->moves != 0 || !asked) {
      rc = fail("ship bump did not run the foreign-trade arm");
    }
  }

  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/*
 * ai_euro_5952_absorb_equip — FUN_5952_035e's absorption arm, Colonist case
 * (raw 94271-94274, an unconditional take-in) and its shared outer gate
 * `+0x1b & 0x10` (NEEDS_COLONISTS, raw 94231). Hosted in the colony tick as
 * a tile re-scan since 2026-09-18, so the seam is colony-side, not unit-side.
 */
static int test_ai_euro_5952_absorb_colonist(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("absorb map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, /*nation=*/1, 4, 4, /*pop=*/2);

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;

  const int uid = units_spawn_allow_stack(&units, 0, 4, 4);
  ColonizeUnit* u = units_get(&units, uid);
  if (!u) {
    fx_map_free(&map);
    return fail("absorb spawn colonist");
  }
  u->nation_id = 1;
  u->moves = UNITS_MP_PER_TILE;

  int labor = 0;
  /* Gate closed: the colony does not want colonists -> nothing happens. */
  c->ai_flags = 0;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 2 || !units_get(&units, uid)) {
    fx_map_free(&map);
    return fail("absorb changed state with the outer gate closed");
  }

  /* Gate open: the Colonist case has no test of its own. */
  c->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  const int pop_after = (int)colonies.colonies[0].population;
  const ColonizeUnit* gone = units_get(&units, uid);
  if (pop_after != 3) {
    fx_map_free(&map);
    return fail("absorption must add the colonist to the colony");
  }
  if (gone && gone->active) {
    fx_map_free(&map);
    return fail("the absorbed unit must leave the map");
  }
  if (labor != 0) {
    fx_map_free(&map);
    return fail("only the Soldier case touches iStack_76");
  }

  /* DOS re-scans the tile stack after every absorption (iStack_32), so two
   * colonists standing on the tile are BOTH taken in by one tick. */
  const int a1 = units_spawn_allow_stack(&units, 0, 4, 4);
  const int a2 = units_spawn_allow_stack(&units, 0, 4, 4);
  for (int i = 0; i < 2; ++i) {
    ColonizeUnit* w = units_get(&units, i == 0 ? a1 : a2);
    if (!w) {
      fx_map_free(&map);
      return fail("absorb spawn pair");
    }
    w->nation_id = 1;
    w->moves = UNITS_MP_PER_TILE;
    w->profession = i == 0 ? 8 : 26;
  }
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  const int pop2 = (int)colonies.colonies[0].population;
  fx_map_free(&map);
  if (pop2 != 5) {
    return fail("the tile re-scan must absorb every eligible unit in one tick");
  }
  if (c->colonists[3].profession != 26 || c->colonists[4].profession != 8) {
    return fail("absorption must enumerate the physical chain, latest arrival first");
  }
  return 0;
}

/*
 * ai_euro_5952_absorb_equip — the Soldier/Dragoon case (raw 94239-94256).
 * Gate disjuncts: (b) the arriving unit is a specialist other than a Veteran
 * Soldier AND one of the two census cells aiStack_68[0x13]/[0x15] is non-zero,
 * (c) +0x1b bit 2 (MILITARY_SURPLUS). Absorbing clears bit 2, refunds the
 * muskets to the colony stock, keeps the profession, consumes one census cell
 * ([0x15] first, else [0x13]) and increments the tick-local iStack_76.
 */
static int test_ai_euro_5952_absorb_soldier(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("absorb-soldier map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldiers");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, /*nation=*/1, 4, 4, /*pop=*/2);
  c->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  c->stock[COLONIZE_CARGO_MUSKETS] = 0;

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;

  /* Census: one ordinary colonist to spare, no Veteran Soldier. */
  ai_euro_5952_set_absorb_census(c->id, /*nonexpert=*/1, /*vet_soldier=*/0);

  /* A Veteran Soldier (profession 0x15) is excluded from disjunct (b). */
  const int vid = units_spawn_allow_stack(&units, 0, 4, 4);
  ColonizeUnit* v = units_get(&units, vid);
  if (!v) {
    fx_map_free(&map);
    return fail("absorb-soldier spawn veteran");
  }
  v->nation_id = 1;
  v->profession = UNITS_JOB_SOLDIER;
  v->muskets = 50;
  int labor = 0;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 2) {
    fx_map_free(&map);
    return fail("a Veteran Soldier must not be absorbed through disjunct (b)");
  }

  /* MILITARY_SURPLUS (disjunct c) takes the same unit in, clears the bit,
   * refunds the muskets and keeps the profession. */
  c->ai_flags |= COLONIZE_COLONY_AI_MILITARY_SURPLUS;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 3 ||
      (c->ai_flags & COLONIZE_COLONY_AI_MILITARY_SURPLUS) != 0 ||
      c->stock[COLONIZE_CARGO_MUSKETS] != 50 ||
      (int)c->colonists[2].profession != UNITS_JOB_SOLDIER) {
    fx_map_free(&map);
    return fail("surplus absorption must clear bit 2, refund muskets, keep profession");
  }
  if (labor != 1) {
    fx_map_free(&map);
    return fail("the Soldier case must increment the tick-local iStack_76");
  }

  /* That absorption consumed a census cell too (raw 94248-94255), so re-seed
   * one before exercising disjunct (b). */
  ai_euro_5952_set_absorb_census(c->id, /*nonexpert=*/1, /*vet_soldier=*/0);

  /* Disjunct (b): a Hardy Pioneer carrying muskets, with census[0x13] = 1.
   * The absorption consumes that cell, so the second one on the tile stays
   * out even though DOS's re-scan sees it in the same tick. */
  for (int round = 0; round < 2; ++round) {
    const int pid = units_spawn_allow_stack(&units, 0, 4, 4);
    ColonizeUnit* p = units_get(&units, pid);
    if (!p) {
      fx_map_free(&map);
      return fail("absorb-soldier spawn specialist");
    }
    p->nation_id = 1;
    p->profession = UNITS_JOB_PIONEER; /* expert, != 0x15 */
    p->muskets = 50;
  }
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 4) {
    fx_map_free(&map);
    return fail("exactly one specialist must be absorbed per free census cell");
  }
  if (labor != 2) {
    fx_map_free(&map);
    return fail("the specialist absorption must also bump iStack_76");
  }
  fx_map_free(&map);
  return 0;
}

/* fx_colony_add sets population/colonist_count only; the equip arm ejects a
 * real colonist, so the slots have to be live. */
static void fx_colony_fill_colonists(ColonizeColony* c) {
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    c->tiles[i] = -1;
  }
  for (int i = 0; i < (int)c->colonist_count && i < COLONIZE_COLONY_POP_MAX; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].profession = COLONIZE_PROF_FREE_COLONIST;
    c->colonists[i].field_job = -1;
  }
}

/*
 * ai_euro_5952_absorb_equip — the equip arm's SCOUT target (raw 94277-94285).
 * DOS: `if (0x65 < stock[horses]) { if (pop < 10 && pop < wanted_size) skip;
 * if (!(+0x1b & 0x10)) { local_8e = 0x16; local_136 = 1; } }`. With muskets
 * at 0 the Soldier/Dragoon arm cannot override, and with pop <= 10 the
 * local_90 conjunct short-circuits before its RNG draw, so this case is
 * fully deterministic and needs no rng.
 */
static int test_ai_euro_5952_equip_scout(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("equip-scout map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Scouts");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* pop 8 = ai_euro_colony_wanted_size's live answer, so `pop < wanted` is
   * false and the DOS `goto LAB_0de5` skip does NOT fire. */
  ColonizeColony* c = fx_colony_add(&colonies, /*nation=*/1, 4, 4, /*pop=*/8);
  c->ai_flags = 0;
  c->stock[COLONIZE_CARGO_MUSKETS] = 0;
  c->stock[COLONIZE_CARGO_HORSES] = 0x65; /* exactly the threshold: no arm */
  fx_colony_fill_colonists(c);

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;

  int labor = 0;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 8) {
    fx_map_free(&map);
    return fail("horses == 0x65 must not arm the Scout target (DOS tests 0x65 <)");
  }

  c->stock[COLONIZE_CARGO_HORSES] = 0x66;
  c->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS; /* +0x1b & 0x10 blocks it */
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 8) {
    fx_map_free(&map);
    return fail("NEEDS_COLONISTS must veto the Scout target");
  }

  c->ai_flags = 0;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 7) {
    fx_map_free(&map);
    return fail("the Scout target must re-type one colonist out of the colony");
  }
  if (c->stock[COLONIZE_CARGO_HORSES] != 0x66 - UNITS_EQUIP_HORSES) {
    fx_map_free(&map);
    return fail("FUN_15eb_1068 must charge the Scout's horses to the stock");
  }
  int scouts = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (u->active && u->x == 4 && u->y == 4) {
      scouts++;
    }
  }
  fx_map_free(&map);
  if (scouts != 1) {
    return fail("exactly one Scout must be produced per tick (local_136 is a flag)");
  }
  return 0;
}

/*
 * ai_euro_5952_absorb_equip — the equip arm's PIONEER target (raw
 * 94300-94303): `local_90 && unit_type_counts[nation][2] == 0 && local_10 &&
 * 0x13 < stock[tools]`. local_90 carries DOS's RNG(0,3) conjunct, so the
 * arm fires on roughly a quarter of the ticks; the test drives a fixed seed
 * for a fixed number of ticks and asserts the tools gate flips the outcome
 * from "some Pioneers" to "none", which is the gate this piece adds.
 */
static int equip_pioneer_run(int tools, int ticks) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return -1;
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Pioneers");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, /*nation=*/1, 4, 4, /*pop=*/20);
  c->ai_flags = 0;
  c->stock[COLONIZE_CARGO_MUSKETS] = 0;  /* Soldier/Dragoon arm stays shut */
  c->stock[COLONIZE_CARGO_HORSES] = 0;   /* Scout arm stays shut */
  c->stock[COLONIZE_CARGO_TOOLS] = tools;
  fx_colony_fill_colonists(c);

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.rng = &rng;

  int made = 0;
  int labor = 0;
  for (int t = 0; t < ticks; ++t) {
    const int before = (int)c->population;
    c->stock[COLONIZE_CARGO_TOOLS] = tools; /* one Pioneer's worth per tick */
    ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
    if ((int)c->population < before) {
      made++;
    }
  }
  fx_map_free(&map);
  return made;
}

static int test_ai_euro_5952_equip_pioneer(void) {
  const int open = equip_pioneer_run(/*tools=*/100, /*ticks=*/40);
  if (open < 0) {
    return fail("equip-pioneer map alloc");
  }
  if (open <= 0) {
    return fail("the Pioneer target must fire when tools > 0x13 and local_90 rolls");
  }
  const int shut = equip_pioneer_run(/*tools=*/0x13, /*ticks=*/40);
  if (shut != 0) {
    return fail("tools == 0x13 must veto the Pioneer target (DOS tests 0x13 <)");
  }
  return 0;
}


/*
 * port_bugs.md #822: FUN_465b_0000 routes a Brave step onto a foreign stack-head
 * tile with >= 3 thirds left into FUN_5fef_1b0e (raw 75467-75479,
 * 75631-75634, 75692) — the attack resolves and the Brave exhausts in place.
 * The port only exhausted, so a Brave beside a lone Euro unit stood forever.
 * Surround a full-MP Brave with foreign Soldiers: whichever direction 021a
 * picks, a committed step is an attack, so somebody must die.
 */
static int test_ai_brave_field_attack(void) {
  int rc = 0;
  int saw_move = 0;
  for (int k = 0; k < 24 && rc == 0 && !saw_move; ++k) {
    ColonizeWorldMap map;
    if (!fx_map_alloc(&map, 12, 12, /*terrain_fill=*/0, /*with_seen=*/true)) {
      return fail("brave-attack map alloc");
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.layer3[i] = 0xf0; /* unowned, continent 0 */
    }
    ColonizeUnitPool units;
    fx_units_init(&units);
    units.type_count = UNITS_KIND_MTD_BRAVE + 2;
    for (int t = 0; t < units.type_count; ++t) {
      units.types[t].movement = 1;
      units.types[t].attack = 2;
      units.types[t].defense = 2;
      units.types[t].domain = COLONIZE_UNIT_DOMAIN_LAND;
      snprintf(units.types[t].name, sizeof(units.types[t].name), "t%d", t);
    }
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    ColonizeCol1Tribe tribe;
    memset(&tribe, 0, sizeof(tribe));
    tribe.x = 5;
    tribe.y = 5;
    tribe.nation_id = 4;
    tribe.population = 3;
    col1.tribe = &tribe;
    col1.head.tribe_count = 1;
    col1.indian[0].euro_diplo[1] = 1; /* met — no first-contact ceremony */

    ColonizeUnit* b = &units.units[0];
    b->id = 1;
    b->active = true;
    b->nation_id = 4;
    b->type_index = UNITS_KIND_BRAVE;
    b->x = 5;
    b->y = 5;
    b->aboard_ship_id = -1;
    b->home_tribe_id = 0;
    b->moves = 0; /* Braves store thirds SPENT (conventions.md) */
    b->last_dir = 8;
    for (int d = 0; d < 8; ++d) {
      ColonizeUnit* f = &units.units[1 + d];
      f->id = 2 + d;
      f->active = true;
      f->nation_id = 1;
      f->type_index = 1;
      f->x = 5 + MAP_DIR8_DX[d];
      f->y = 5 + MAP_DIR8_DY[d];
      f->aboard_ship_id = -1;
      f->home_tribe_id = -1;
      f->moves = 1;
    }
    units.next_id = 10;
    units_pool_sync(&units);
    int before = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      before += units.units[i].active ? 1 : 0;
    }

    ColonizeDosRng rng;
    dos_rng_seed(&rng, (uint32_t)(k * 12345 + 7)); /* spread — tiny-seed trap */
    int steps = 0;
    (void)ai_native_brave_step(
      &units, &map, &col1, &rng, /*nation_id=*/4, /*seed100_init_burns=*/false, b,
      /*max_mp=*/3, /*brave_index=*/0, &steps
    );
    int after = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      after += units.units[i].active ? 1 : 0;
    }
    if (steps > 0) {
      saw_move = 1;
      if (after != before - 1) {
        fprintf(stderr, "  seed %d: %d -> %d active units\n", k, before, after);
        rc = fail("a committed Brave step onto a foreign tile must resolve combat");
      } else if (b->active && (b->x != 5 || b->y != 5)) {
        rc = fail("a surviving Brave attacker must stay on its own tile");
      } else if (b->active && b->moves != 3) {
        rc = fail("an attacking Brave must exhaust its full allotment (0934)");
      }
    }
    fx_map_free(&map);
  }
  if (rc == 0 && !saw_move) {
    return fail("no seed made the Brave commit a step — test proved nothing");
  }
  return rc;
}

/*
 * Shift pick in the construction picker: the picker stays open, so its
 * highlight must follow the row the pick acted on — the started project when
 * production was idle (nothing queued, or the current project already built),
 * the queued row once something is really in production. A refused pick moves
 * nothing.
 */
static int test_colony_shift_pick_moves_highlight(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    return fail("NAMES.TXT load");
  }
  colonies_init(&game.colonies);
  if (!colonies_load_buildings(&game.colonies, &names)) {
    assets_msg_free(&names);
    return fail("@BUILDING load");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  col->id = 1;
  col->active = true;
  col->nation_id = 0;
  col->population = 4;
  game.colonies.colony_count = 1;
  game.colony_view_id = col->id;

  const int carpenter = colonies_find_building(&game.colonies, "Carpenter's Shop");
  const int church = colonies_find_building(&game.colonies, "Church");
  const int stockade = colonies_find_building(&game.colonies, "Stockade");
  if (carpenter < 0 || church < 0 || stockade < 0) {
    assets_msg_free(&names);
    return fail("building rows");
  }

  int rc = 0;
  for (int pass = 0; pass < 2 && rc == 0; ++pass) {
    /* pass 0: nothing in production. pass 1: current project already built. */
    memset(col->has_building, 0, sizeof(col->has_building));
    colonies_build_queue_clear(&game.colonies, col->id);
    if (pass == 0) {
      col->building_in_production = -1;
    } else {
      col->has_building[carpenter] = true;
      col->building_in_production = carpenter;
    }
    const ColoniesBuildableOpts bopts = game_colony_buildable_opts(&game);
    colony_screen_open_construction(&game.colony_screen, &game.colonies, col->id, &bopts);
    ColonyScreenView* csv = &game.colony_screen;
    int want_church = -1;
    int want_stockade = -1;
    int want_carpenter = -1;
    for (int i = 0; i < csv->buildable_count; ++i) {
      if (csv->buildable_ids[i] == church) {
        want_church = i + 1;
      } else if (csv->buildable_ids[i] == stockade) {
        want_stockade = i + 1;
      } else if (csv->buildable_ids[i] == carpenter) {
        want_carpenter = i + 1;
      }
    }
    /* Nothing in production opens on the (no production) row; a built current
     * project stays listed (FUN_15eb_3650 tail, port_bugs.md #1218) and has the bar. */
    if (csv->construction_selection != (pass == 0 ? 0 : want_carpenter) ||
        (pass == 1 && want_carpenter <= 0)) {
      rc = fail("the picker must open on the current project's row");
      break;
    }
    if (want_church <= 0 || want_stockade <= 0) {
      rc = fail("picker fixture must list Church and Stockade");
      break;
    }
    /* Idle: the pick starts the project and the highlight follows it. */
    game_colony_queue_construction(&game, church);
    if (col->building_in_production != church || col->build_queue_count != 0) {
      rc = fail("a Shift pick on an idle colony must start the project");
      break;
    }
    if (csv->construction_selection != want_church) {
      rc = fail("the highlight must follow the started project");
      break;
    }
    /* Busy: the pick queues and the highlight follows the queued row. */
    game_colony_queue_construction(&game, stockade);
    if (colonies_build_queue_pos(&game.colonies, col->id, stockade) != 1) {
      rc = fail("a Shift pick with a live project must queue it");
      break;
    }
    if (csv->construction_selection != want_stockade) {
      rc = fail("the highlight must follow the queued row");
      break;
    }
    /* Refused (already queued): nothing moves. */
    game_colony_queue_construction(&game, church);
    if (csv->construction_selection != want_stockade) {
      rc = fail("a refused Shift pick must not move the highlight");
      break;
    }
    colony_screen_close_construction(csv);
  }
  assets_msg_free(&names);
  return rc;
}

/* One Prairie plot (ring 3, unowned, unbought) among ocean plots; colonist 0 works in the
 * Carpenter's Shop of nation 1's colony 0. False on a fixture load failure. */
static bool jobs_menu_fixture(ColonizeGameState* game, ColonizeMsgCatalog* names) {
  memset(game, 0, sizeof(*game));
  assets_msg_init(names);
  if (!assets_msg_load_file(names, "COLONIZE/NAMES.TXT")) {
    return false;
  }
  fx_colonies_init(&game->colonies);
  if (!colonies_load_buildings(&game->colonies, names) ||
      !fx_map_alloc(&game->world_map, 16, 16, 0x19, false)) {
    assets_msg_free(names);
    return false;
  }
  game->world_map_ok = true;
  for (size_t i = 0; i < game->world_map.tile_count; ++i) {
    game->world_map.layer3[i] = 0xf1; /* unowned open ocean */
  }
  int dx = 0;
  int dy = 0;
  colonies_field_tile_delta(3, &dx, &dy);
  game->world_map.terrain[(8 + dy) * 16 + 8 + dx] = 3;
  game->world_map.layer3[(8 + dy) * 16 + 8 + dx] = 0xf0;
  game->world_map.terrain[8 * 16 + 8] = 3;
  game->world_map.layer3[8 * 16 + 8] = 0xf0;
  ColonizeColony* col = &game->colonies.colonies[0];
  col->id = 0;
  col->active = true;
  col->nation_id = 1;
  col->x = 8;
  col->y = 8;
  col->population = 1;
  col->colonist_count = 1;
  col->building_in_production = -1;
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    col->tiles[i] = -1;
  }
  const int shop = colonies_building_row(&game->colonies, COLONY_BUILDING_CARPENTERS_SHOP);
  col->has_building[shop] = true;
  ColonizeColonist* c = &col->colonists[0];
  c->active = true;
  c->unit_type_index = -1;
  c->profession = COLONIZE_PROF_FREE_COLONIST;
  c->field_job = -1;
  c->building_type = shop;
  c->turns_in_job = 5;
  game->colonies.colony_count = 1;
  game->colony_view_id = col->id;
  game->in_colony = true;
  game->colony_screen.selected_colonist = 0;
  game->colony_screen.selected_outside_unit = -1;
  return true;
}

/*
 * port_bugs.md #1204: a building worker who picks a field job from the jobs menu is seated
 * on the plot FUN_15eb_28c8(colonist, job) elects (FUN_2f2b_348c raw 50860-50863), never on
 * plot 0 (the old Enter fallback).
 */
static int test_colony_jobs_menu_seats_indoor_worker(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  if (!jobs_menu_fixture(&game, &names)) {
    return fail("jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  ColonizeColonist* c = &col->colonists[0];
  ColonyScreenView* csv = &game.colony_screen;
  int rc = 0;
  colony_screen_open_jobs(csv, &game.colonies, &game.world_map, col, -1);
  if (!csv->jobs_open || csv->jobs_tile_index != -1) {
    rc = fail("Enter / a building-worker click must open the jobs menu with no plot");
  } else {
    game_colony_commit_job(&game, col, COLONIZE_JOB_FARMER);
    if (col->tiles[3] != 0 || c->field_job != COLONIZE_JOB_FARMER || c->building_type >= 0 ||
        col->tiles[0] >= 0) {
      fprintf(stderr, "  tiles[0]=%d tiles[3]=%d job=%d building=%d\n", col->tiles[0],
              col->tiles[3], c->field_job, c->building_type);
      rc = fail("a building worker's field pick must take the plot 28c8 elects");
    }
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/*
 * docs/dos_bugs.md DB-54 (port_bugs.md #1205): DOS's jobs menu stamps each elected plot and
 * zeroes the job counter just by opening; the port's opening changes nothing.
 */
static int test_colony_jobs_menu_open_has_no_side_effects(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  if (!jobs_menu_fixture(&game, &names)) {
    return fail("jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  ColonizeColonist* c = &col->colonists[0];
  const int shop = c->building_type;
  int dx = 0;
  int dy = 0;
  colonies_field_tile_delta(3, &dx, &dy);
  const size_t plot = (size_t)(8 + dy) * 16 + 8 + dx;
  const uint8_t layer3_before = game.world_map.layer3[plot];
  const uint8_t layer2_before = game.world_map.layer2[plot];
  c->turns_in_job = 7;
  game_colony_open_jobs(&game, col, &game.world_map, -1);
  int rc = 0;
  if (!game.colony_screen.jobs_open || game.world_map.layer3[plot] != layer3_before ||
      game.world_map.layer2[plot] != layer2_before || c->turns_in_job != 7 ||
      c->building_type != shop || c->field_job >= 0 || col->tiles[3] >= 0) {
    fprintf(stderr, "  open %d owner %x layer2 %02x turns %d building %d job %d tile %d\n",
            (int)game.colony_screen.jobs_open, game.world_map.layer3[plot] >> 4,
            game.world_map.layer2[plot], c->turns_in_job, c->building_type, c->field_job,
            col->tiles[3]);
    rc = fail("opening the jobs menu must not stamp plots or zero the job counter (DB-54)");
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/*
 * port_bugs.md #1208: ORDERS Go to Place (0x321) is the same FUN_647e_01c6 list as Go to Port
 * (case 2b5a:28d4 -> FUN_2b5a_1dfc): @TRAVELPLACE, no Europe row, own colonies on the unit's
 * continent (FUN_647e_0094), the colony the unit stands on left out.
 */
static int test_goto_place_opens_travelplace_list(void) {
  static ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/3, /*with_seen=*/true)) {
    return fail("goto place map alloc");
  }
  for (size_t i = 0; i < game.world_map.tile_count; ++i) {
    game.world_map.layer3[i] = 0xf2; /* unowned land, continent 2 */
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  static char lines[][COLONIZE_MSG_LINE_LEN] = {
    "@width=190", "@default=1", "Select a colony to travel to:"
  };
  ColonizeMsgSection sec;
  memset(&sec, 0, sizeof(sec));
  snprintf(sec.name, sizeof(sec.name), "TRAVELPLACE");
  sec.lines = lines;
  sec.line_count = 3;
  game.messages.sections = &sec;
  game.messages.section_count = 1;
  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].movement = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  fx_colony_add(&game.colonies, 0, 2, 2, 1);
  fx_colony_add(&game.colonies, 0, 5, 2, 1);
  fx_colony_add(&game.colonies, 1, 5, 5, 1); /* foreign */
  fx_colony_add(&game.colonies, 0, 2, 5, 1);
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);
  const int uid = units_spawn(&game.units, 0, 2, 2);
  ColonizeUnit* u = units_get(&game.units, uid);
  int rc = 0;
  if (!u) {
    rc = fail("goto place unit spawn");
  } else {
    u->nation_id = 0;
    game.units.selected_id = uid;
    game.goto_port_pending_unit = -1;
    game_apply_map_menu_action(&game, MAP_MENU_ACTION_GOTO_PLACE);
    const CheatListDialog* cl = &game.cheat_list;
    if (!cl->open || cl->kind != CHEAT_LIST_KIND_GOTO_PORT ||
        strcmp(cl->prompt, "Select a colony to travel to:") != 0 || cl->option_count != 2 ||
        cl->option_ids[0] != 1 || cl->option_ids[1] != 3 || game.goto_port_pending_unit != uid) {
      fprintf(stderr, "  open %d prompt '%s' rows %d [%d %d]\n", (int)cl->open, cl->prompt,
              cl->option_count, cl->option_ids[0], cl->option_ids[1]);
      rc = fail("Go to Place: @TRAVELPLACE list of the other own colonies, no Europe row");
    }
  }
  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/*
 * port_bugs.md #1209: the colony lists are FUN_6f74_2580 lists; a paged jobs / construction
 * list's "(More)" row is a row like the others (348c adds it last on page 0, first on page 1).
 */
static int test_colony_list_keys_wrap(void) {
  static ColonyScreenView v;
  memset(&v, 0, sizeof(v));
  v.jobs_open = true;
  v.job_count = 20; /* > 0x10: page 0 = jobs 0..15 + More, page 1 = More + jobs 16..19 */
  for (int i = 0; i < v.job_count; ++i) {
    v.job_ids[i] = i;
  }
  colony_screen_list_key(&v, COLONIZE_KEY_UP);
  if (!v.more_selected || v.jobs_selection != 0) {
    return fail("jobs page 0: Up on the first row wraps to the More row");
  }
  colony_screen_list_key(&v, COLONIZE_KEY_UP);
  if (v.more_selected || v.jobs_selection != 15) {
    return fail("jobs page 0: Up from More is the last job row");
  }
  colony_screen_list_key(&v, COLONIZE_KEY_DOWN);
  colony_screen_list_key(&v, COLONIZE_KEY_DOWN);
  if (v.more_selected || v.jobs_selection != 0) {
    return fail("jobs page 0: Down from More wraps to the first row");
  }
  colony_screen_jobs_next_page(&v);
  if (!v.more_selected || v.jobs_selection != 16) {
    return fail("jobs page 1 opens on its first row, More");
  }
  colony_screen_list_key(&v, COLONIZE_KEY_UP);
  if (v.more_selected || v.jobs_selection != 19) {
    return fail("jobs page 1: Up on More wraps to the last job");
  }
  colony_screen_list_key(&v, COLONIZE_KEY_DOWN);
  colony_screen_list_key(&v, COLONIZE_KEY_DOWN);
  if (v.more_selected || v.jobs_selection != 16) {
    return fail("jobs page 1: Down from the last job wraps to More, then job 16");
  }
  memset(&v, 0, sizeof(v));
  v.eject_open = true;
  v.eject_role_count = 3;
  v.eject_role_enabled[0] = true;
  v.eject_role_enabled[2] = true;
  colony_screen_list_key(&v, COLONIZE_KEY_DOWN);
  if (v.eject_selection != 2) {
    return fail("leave-as: Down skips the greyed row");
  }
  colony_screen_list_key(&v, COLONIZE_KEY_DOWN);
  if (v.eject_selection != 0) {
    return fail("leave-as: Down on the last row wraps");
  }
  memset(&v, 0, sizeof(v));
  v.construction_open = true;
  v.buildable_count = 3;
  colony_screen_list_key(&v, COLONIZE_KEY_UP);
  if (v.construction_selection != 3 || v.more_selected) {
    return fail("construction: Up on the first row wraps to the last");
  }
  memset(&v, 0, sizeof(v));
  v.custom_house_open = true;
  v.custom_house_count = 16;
  colony_screen_list_key(&v, COLONIZE_KEY_UP);
  if (v.custom_house_selection != 15) {
    return fail("Custom House: Up on the first row wraps to the last (port_bugs.md #1213)");
  }
  return 0;
}

/*
 * port_bugs.md #1213: @CUSTOM is a FUN_6f74_2580 @checkbox box. The bar opens on row 0, Up /
 * Down move it, Space and F1 flip the barred row and keep the box up, any other key does
 * nothing (C does not swap it for the construction list), Enter closes it.
 */
static int test_colony_custom_house_keys(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  if (!jobs_menu_fixture(&game, &names)) {
    return fail("jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  ColonyScreenView* csv = &game.colony_screen;
  col->custom_house_bits = 0;
  col->has_building[colonies_building_row(&game.colonies, COLONY_BUILDING_CUSTOM_HOUSE)] = true;
  colony_screen_open_custom_house(csv, col, NULL);
  const int last = csv->custom_house_count - 1;
  ColonizeInputState in;
#define CH_PRESS(k) (memset(&in, 0, sizeof(in)), in.last_key = (k), (void)game_update(&game, &in, 16))
  int rc = 0;
  if (!csv->custom_house_open || csv->custom_house_selection != 0 || last < 1) {
    rc = fail("the Custom House box opens with the bar on row 0");
  }
  CH_PRESS(COLONIZE_KEY_UP);
  if (rc == 0 && csv->custom_house_selection != last) {
    rc = fail("Up on row 0 wraps to the last row");
  }
  CH_PRESS(COLONIZE_KEY_SPACE);
  if (rc == 0 && (col->custom_house_bits != 1u << csv->custom_house_cargo_ids[last] ||
                  !csv->custom_house_open)) {
    rc = fail("Space flips the barred row and keeps the box up");
  }
  CH_PRESS(COLONIZE_KEY_DOWN);
  CH_PRESS(COLONIZE_KEY_F1);
  if (rc == 0 && (csv->custom_house_selection != 0 ||
                  col->custom_house_bits != (1u << csv->custom_house_cargo_ids[last] |
                                             1u << csv->custom_house_cargo_ids[0]))) {
    rc = fail("Down wraps to row 0 and F1 flips it");
  }
  CH_PRESS(COLONIZE_KEY_C);
  if (rc == 0 && (!csv->custom_house_open || csv->construction_open)) {
    rc = fail("C does nothing while the Custom House box is up");
  }
  CH_PRESS(COLONIZE_KEY_ENTER);
  if (rc == 0 && (csv->custom_house_open || !game.in_colony)) {
    rc = fail("Enter closes the Custom House box, not the colony screen");
  }
#undef CH_PRESS
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/*
 * F1 on the bare colony screen opens the article of the DS:0x32e focus panel and comes back
 * (FUN_2f2b_6372 raw 55622-55650); on the Europe screen the DS:0xf9a panel's (FUN_38fd_4f6e
 * raw 66119-66135).
 */
static int test_screen_f1_focus_article(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  if (!jobs_menu_fixture(&game, &names)) {
    return fail("jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  ColonyScreenView* csv = &game.colony_screen;
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  int rc = 0;
#define F1_COLONY() (in.last_key = COLONIZE_KEY_F1, (void)game_update_colony_screen(&game, &in))
#define PEDIA_BACK() (in.last_key = COLONIZE_KEY_ESCAPE, (void)game_update_pedia_screen(&game, &in))
  F1_COLONY(); /* static focus 0, cursor 2,2: the colony tile */
  if (!game.in_pedia || game.pedia_category != PEDIA_CAT_TERRAIN ||
      game.pedia_index != map_pedia_terrain_index_at(&game.world_map, col->x, col->y) ||
      game.pedia_return_colony_id != col->id) {
    rc = fail("F1 with the area focus opens the colony tile's terrain article");
  }
  PEDIA_BACK();
  if (rc == 0 && (game.in_pedia || !game.in_colony || game.colony_view_id != col->id)) {
    rc = fail("leaving the article returns to the colony screen");
  }
  csv->focus = COLONY_FOCUS_COLONISTS;
  F1_COLONY();
  if (rc == 0 && (game.pedia_category != PEDIA_CAT_JOB ||
                  game.pedia_index != colonies_colonist_occupation_job(&game.colonies, &col->colonists[0]))) {
    rc = fail("F1 with the colonist focus opens the occupation's skill article");
  }
  PEDIA_BACK();
  csv->focus = COLONY_FOCUS_WAREHOUSE;
  csv->selected_cargo = 5;
  F1_COLONY();
  if (rc == 0 && (game.pedia_category != PEDIA_CAT_CARGO || game.pedia_index != 5)) {
    rc = fail("F1 with the warehouse focus opens the cargo article");
  }
  PEDIA_BACK();
  csv->focus = COLONY_FOCUS_SHIPS;
  F1_COLONY();
  if (rc == 0 && (game.in_pedia || !game.in_colony)) {
    rc = fail("F1 with the ship focus and no ship in port does nothing (DS:0x33c == 0)");
  }
  csv->focus = COLONY_FOCUS_UNITS;
  csv->multi_mode = COLONY_MULTI_PRODUCTION;
  in.last_key = COLONIZE_KEY_NONE;
  (void)game_update_colony_screen(&game, &in);
  if (rc == 0 && csv->focus != COLONY_FOCUS_COLONISTS) {
    rc = fail("off the Units tab the Units-pane focus becomes the colonist focus");
  }
  game.in_colony = false;

  EuropeScreen* eu = &game.europe;
  game.in_europe = true;
  eu->focus = 0;
  eu->selected_market = 3;
  in.last_key = COLONIZE_KEY_F1;
  (void)game_update_europe_screen(&game, &in);
  if (rc == 0 && (!game.in_pedia || game.pedia_category != PEDIA_CAT_CARGO ||
                  game.pedia_index != 3 || !game.pedia_return_europe)) {
    rc = fail("Europe F1 with the market focus opens the cargo article");
  }
  PEDIA_BACK();
  if (rc == 0 && (game.in_pedia || !game.in_europe)) {
    rc = fail("leaving the article returns to the Europe screen");
  }
  eu->focus = 2;
  eu->dock_count = 1;
  eu->dock[0].dos_type = EUROPE_DOCK_TYPE_PIONEERS;
  eu->dock_selected = 4; /* past the dock: FUN_38fd_07c6 reads it as 0 */
  in.last_key = COLONIZE_KEY_F1;
  (void)game_update_europe_screen(&game, &in);
  if (rc == 0 && (game.pedia_category != PEDIA_CAT_UNIT ||
                  game.pedia_index != EUROPE_DOCK_TYPE_PIONEERS)) {
    rc = fail("Europe F1 with the dock focus opens the dock unit's article");
  }
  PEDIA_BACK();
  eu->focus = 1;
  in.last_key = COLONIZE_KEY_F1;
  (void)game_update_europe_screen(&game, &in);
  if (rc == 0 && game.in_pedia) {
    rc = fail("Europe F1 with the harbor focus and no ship does nothing (DS:0xfa2 == 0)");
  }
#undef F1_COLONY
#undef PEDIA_BACK
  game.in_europe = false;
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/*
 * port_bugs.md #1214: the encroachment popup keeps DOS's "offer gold" row and greys it when the
 * treasury is short (FUN_2b5a_123e raw 42484-42488: FUN_291f_01b6(box, 2, 1)). Fixture: the
 * map_pioneer_native_forest_offer start (the Pioneer on Tupi forest (45,22), offer 422).
 */
static int test_indian_land_offer_row_greyed(void) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug"};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
    if (game) {
      game_destroy(game);
    }
    return TR_SKIP;
  }
  char err[256];
  int rc = 0;
  if (!game_load_col1_file(
        game, "tests-automated-regression/map_pioneer_native_forest_offer/input.sav", err,
        sizeof(err)
      )) {
    fprintf(stderr, "  %s\n", err);
    rc = fail("fixture save load");
  }
  const uint32_t golds[2] = {421, 422};
  for (int k = 0; k < 2 && rc == 0; ++k) {
    ai_popup_clear(&game->ai_popups);
    game->units.selected_id = -1;
    for (int i = 0; i < units_slot_end(&game->units); ++i) {
      const ColonizeUnit* u = &game->units.units[i];
      if (u->active && u->x == 45 && u->y == 22 && units_is_pioneer(&game->units, u->id)) {
        game->units.selected_id = u->id;
      }
    }
    game->europe.gold = golds[k];
    if (!game_pioneer_land_gate(game, GAME_INDIAN_LAND_FOREST) ||
        game->ai_popups.queue_count != 1) {
      rc = fail("Clear Forest on Tupi forest queues @INDIANFOREST");
      break;
    }
    const AiPopupRequest* r = &game->ai_popups.queue[0];
    const uint32_t want = k == 0 ? 1u << 1 : 0u;
    if (r->choice_count != 3 || r->choice_ids[1] != 2 || r->disabled_mask != want) {
      fprintf(stderr, "  gold %u: rows %d id1 %d mask %u\n", golds[k], r->choice_count,
              r->choice_ids[1], r->disabled_mask);
      rc = fail("the offer row stays, greyed only while gold < price");
    }
    /* port_bugs.md #1220: DS:0x1f5c = DS:0x8d52 (the Tupi, slot 7) before the box
     * (ndisasm 0x222a7): the chief portrait of the DOSBox-X capture. */
    if (r->portrait_tribe != 7) {
      fprintf(stderr, "  portrait tribe %d\n", r->portrait_tribe);
      rc = fail("@INDIANFOREST wears the Tupi chief");
    }
  }
  game_destroy(game);
  return rc;
}

/*
 * port_bugs.md #1215, FUN_2f2b_5bd2: a two-page construction list bars the current project on
 * the page that lists it, else the "(More)" row toward it (docs/dos_bugs.md DB-60 / DB-63:
 * DOS shows no bar there), on the first showing and after every page turn. The view is
 * hand-built: the port's buildable list tops out at 22 rows (port_bugs.md #1218).
 */
static int test_colony_construction_page_default(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  if (!jobs_menu_fixture(&game, &names)) {
    return fail("jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  ColonyScreenView* csv = &game.colony_screen;
  int rc = 0;
  colony_screen_open_construction(csv, &game.colonies, col->id, NULL);
  if (csv->construction_selection != 0 || csv->construction_default_row != 0) {
    rc = fail("an idle colony's one-page list bars the clear row");
  }
  csv->buildable_count = 25; /* 26 rows: page 0 = entries 0..15 + More, page 1 = More + 16..25 */
  for (int i = 0; i < csv->buildable_count; ++i) {
    csv->buildable_ids[i] = 100 + i;
  }
  const int p1 = 0x12; /* a page-1 project */
  csv->construction_default_row = p1;
  csv->construction_page = 0;
  csv->construction_selection = 0;
  csv->more_selected = true; /* what the open gives it (DB-63) */
  const int before = col->building_in_production;
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  (void)game_update(&game, &in, 16);
  if (rc == 0 && (csv->construction_page != 1 || csv->construction_selection != p1 ||
                  csv->more_selected || !csv->construction_open ||
                  col->building_in_production != before)) {
    rc = fail("Enter on page 0's (More) turns to the page that bars the project");
  }
  colony_screen_construction_next_page(csv);
  if (rc == 0 && (csv->construction_page != 0 || !csv->more_selected)) {
    rc = fail("back on page 0 without the project: bar on its (More)");
  }
  colony_screen_list_key(csv, COLONIZE_KEY_UP);
  if (rc == 0 && (csv->construction_selection != 15 || csv->more_selected)) {
    rc = fail("Up from (More) takes page 0's last entry");
  }
  csv->construction_default_row = 3; /* a page-0 project */
  colony_screen_construction_next_page(csv);
  if (rc == 0 && (csv->construction_page != 1 || !csv->more_selected)) {
    rc = fail("page 1 without the project: bar on its back (More)");
  }
  colony_screen_construction_next_page(csv);
  if (rc == 0 && (csv->construction_selection != 3 || csv->more_selected)) {
    rc = fail("page 0 bars the project it lists");
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/*
 * port_bugs.md #1218 / #1215 on a live list: a colony with all seven unit
 * projects and fourteen buildings open, whose current project (the Shipyard)
 * was just built, keeps that row (FUN_15eb_3650 tail), so the CHANGE box has 23
 * rows and two pages; the bar follows the #1215 rule across the page turns.
 */
static int test_colony_construction_two_pages_live(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  if (!jobs_menu_fixture(&game, &names)) {
    return fail("jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  ColonyScreenView* csv = &game.colony_screen;
  static const ColonizeBuildingRow k_own[] = {
    COLONY_BUILDING_TOWN_HALL, COLONY_BUILDING_WEAVERS_HOUSE, COLONY_BUILDING_TOBACCONISTS_HOUSE,
    COLONY_BUILDING_RUM_DISTILLERS_HOUSE, COLONY_BUILDING_FUR_TRADERS_HOUSE,
    COLONY_BUILDING_BLACKSMITHS_HOUSE, COLONY_BUILDING_DOCKS, COLONY_BUILDING_DRYDOCK,
    COLONY_BUILDING_SHIPYARD, COLONY_BUILDING_ARMORY,
  };
  for (size_t i = 0; i < sizeof(k_own) / sizeof(k_own[0]); ++i) {
    col->has_building[colonies_building_row(&game.colonies, k_own[i])] = true;
  }
  col->population = 10;
  const int shipyard = colonies_building_row(&game.colonies, COLONY_BUILDING_SHIPYARD);
  col->building_in_production = shipyard;
  ColoniesBuildableOpts bopts = game_colony_buildable_opts(&game);
  bopts.has_adam_smith = true;
  bopts.has_peter_stuyvesant = true;
  colony_screen_open_construction(csv, &game.colonies, col->id, &bopts);
  int row = -1;
  for (int i = 0; i < csv->buildable_count; ++i) {
    if (csv->buildable_ids[i] == shipyard) {
      row = i + 1;
    }
  }
  int rc = 0;
  if (csv->buildable_count + 1 != 23 || row <= 0 || row >= 0x10) {
    rc = fail("the built Shipyard keeps its row: 23 rows, on page 0");
  }
  if (rc == 0 && (csv->construction_selection != row || csv->more_selected)) {
    rc = fail("page 0 opens with the bar on the current project");
  }
  colony_screen_construction_next_page(csv);
  if (rc == 0 && (csv->construction_page != 1 || !csv->more_selected)) {
    rc = fail("page 1 without the project: bar on its back (More) (DB-63)");
  }
  colony_screen_construction_next_page(csv);
  if (rc == 0 && (csv->construction_selection != row || csv->more_selected)) {
    rc = fail("back on page 0 the bar is on the project again");
  }
  /* port_bugs.md #1223: picking that "(COMPLETE)" row keeps the project and clears +0x1c bit
   * 0x80 with no message (FUN_2f2b_5bd2 raw 52665-52667). */
  col->colony_flags |= COLONIZE_COLONY_FLAG_BUILD_COMPLETE;
  game.colony_view_id = col->id;
  game_colony_commit_construction(&game, shipyard);
  if (rc == 0 && (col->building_in_production != shipyard ||
                  (col->colony_flags & COLONIZE_COLONY_FLAG_BUILD_COMPLETE) ||
                  ai_popup_busy(&game.ai_popups) || csv->construction_open)) {
    rc = fail("the (COMPLETE) pick keeps the project silently");
  }
  /* A page-1 current project (the last unit project; the Town Hall unbuilt keeps 23 rows):
   * the list opens on page 0 with the bar on its "(More)" (DB-63). */
  col->has_building[colonies_building_row(&game.colonies, COLONY_BUILDING_TOWN_HALL)] = false;
  col->building_in_production = csv->buildable_ids[csv->buildable_count - 1];
  colony_screen_open_construction(csv, &game.colonies, col->id, &bopts);
  if (rc == 0 && (csv->buildable_count + 1 != 23 || csv->construction_page != 0 ||
                  !csv->more_selected)) {
    rc = fail("a page-1 project opens page 0 with the bar on (More)");
  }
  colony_screen_close_construction(csv);
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/* port_bugs.md #1091: an open map pull-down is modal (DOS runs it as a blocking
 * loop), so game_modal_open is true and the unit/turn hand-off defers. */
static int test_map_pulldown_is_modal(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return 1;
  }
  int rc = 0;
  game->map_menu.loaded = true;
  game->map_menu.open_index = -1;
  if (game_modal_open(game) || !game_turn_flow_allowed(game)) {
    fprintf(stderr, "closed pull-down must not block\n");
    rc = 1;
  }
  game->map_menu.open_index = 2;
  if (!game_modal_open(game) || game_turn_flow_allowed(game) ||
      !game_defer_turn_flow(game) || !game->turn_flow_deferred) {
    fprintf(stderr, "open pull-down must block and defer turn flow\n");
    rc = 1;
  }
  /* Map / screen hotkeys pressed behind the open pull-down do nothing. */
  game->map_menu.menu_count = 3;
  game->map_menu.menus[2].visible = true;
  game->turn_flow_deferred = false;
  const ColonizeKey keys[] = {COLONIZE_KEY_Q, COLONIZE_KEY_E, COLONIZE_KEY_TILDE,
                              COLONIZE_KEY_P, COLONIZE_KEY_F1, COLONIZE_KEY_RIGHT};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
    ColonizeInputState input;
    memset(&input, 0, sizeof(input));
    input.last_key = keys[i];
    (void)game_update(game, &input, 16);
    if (game->map_menu.open_index != 2 || ai_popup_busy(&game->ai_popups) ||
        game->in_europe || game->in_debug_atlas || game->in_pedia || game->in_report) {
      fprintf(stderr, "hotkey %zu reached the map behind the pull-down\n", i);
      rc = 1;
    }
  }
  /* Esc still closes the menu itself. */
  {
    ColonizeInputState input;
    memset(&input, 0, sizeof(input));
    input.last_key = COLONIZE_KEY_ESCAPE;
    (void)game_update(game, &input, 16);
    if (game->map_menu.open_index != -1) {
      fprintf(stderr, "Esc did not close the pull-down\n");
      rc = 1;
    }
  }
  free(game);
  return rc;
}

/* port_bugs.md #1110: game_world builds the one live view; a pool whose load flag
 * is false is NULL, col1_ok follows the game, human_nation is filled. */
static int test_game_world_view(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return 1;
  }
  game->human_nation = 2;
  game->units_ok = true;
  game->ai_rng_seed = 100;
  ColonizeWorld w = game_world(game);
  int rc = 0;
  if (w.units != &game->units || w.map || w.col1 || w.col1_ok || w.europe ||
      w.colonies != &game->colonies || w.rng != &game->move_rng || !w.rng_reseed_set ||
      w.rng_reseed != 100 || w.human_nation != 2 || w.ai_popups) {
    fprintf(stderr, "game_world: unloaded pools must be NULL\n");
    rc = 1;
  }
  game->col1_ok = true;
  game->world_map_ok = true;
  game->europe_ok = true;
  w = game_world(game);
  if (w.col1 != &game->col1 || !w.col1_ok || w.map != &game->world_map ||
      w.europe != &game->europe) {
    fprintf(stderr, "game_world: loaded pools must be set\n");
    rc = 1;
  }
  if (world_make(NULL, NULL, NULL, NULL, false, NULL, NULL).human_nation != -1) {
    fprintf(stderr, "world_make: human_nation must default to -1\n");
    rc = 1;
  }
  free(game);
  return rc;
}

/* docs/dos_bugs.md DB-58: Europe ship menu row 3 (Unload all cargo, FUN_38fd_2bfe)
 * raises @SOMEBOYCOTT for a boycotted hold in any slot, not only slot 0. */
static int unload_all_boycott_notices(int boycotted_cargo) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return -1;
  }
  game->europe_ok = true;
  game->human_nation = 0;
  game->col1.nation[0].boycott_bitmap = (uint16_t)(1u << boycotted_cargo);
  EuropeScreen* eu = &game->europe;
  eu->harbor_ships = 1;
  eu->harbor[0].hold_goods_type[0] = COLONIZE_CARGO_MUSKETS;
  eu->harbor[0].hold_goods_amount[0] = 100;
  eu->harbor[0].hold_goods_type[1] = COLONIZE_CARGO_CLOTH;
  eu->harbor[0].hold_goods_amount[1] = 100;
  game->ai_popups.has_result = true;
  game->ai_popups.result_tag = AI_POPUP_TAG_EUROPE_SHIP_MENU;
  game->ai_popups.result_nation_a = 0;
  game->ai_popups.result_choice_id = 3;
  game_apply_ai_popup_result(game);
  const int n = game->ai_popups.queue_count;
  free(game);
  return n;
}

static int test_europe_unload_all_boycott_any_slot(void) {
  int rc = 0;
  if (unload_all_boycott_notices(COLONIZE_CARGO_CLOTH) != 1) {
    fprintf(stderr, "unload all: boycotted slot 1 must raise @SOMEBOYCOTT\n");
    rc = 1;
  }
  if (unload_all_boycott_notices(COLONIZE_CARGO_MUSKETS) != 1) {
    fprintf(stderr, "unload all: boycotted slot 0 must raise @SOMEBOYCOTT\n");
    rc = 1;
  }
  if (unload_all_boycott_notices(COLONIZE_CARGO_SILVER) != 0) {
    fprintf(stderr, "unload all: no boycotted hold, no notice\n");
    rc = 1;
  }
  return rc;
}

static const TestCase k_cases[] = {
    {"test_dos_buggy_default_off_tests_on", test_dos_buggy_default_off_tests_on},
    {"test_map_pulldown_is_modal", test_map_pulldown_is_modal},
    {"test_turn_year_end_rival_rebels", test_turn_year_end_rival_rebels},
    {"test_ai_contact_raid_alarm_delta", test_ai_contact_raid_alarm_delta},
    {"test_ai_contact_raid_sound_rows", test_ai_contact_raid_sound_rows},
    {"test_ai_021a_dir_tile", test_ai_021a_dir_tile},
    {"test_ai_465b_dest_owner", test_ai_465b_dest_owner},
    {"test_ai_brave_field_attack", test_ai_brave_field_attack},
    {"test_game_render_select_palette", test_game_render_select_palette},
    {"test_window_log_fit_line", test_window_log_fit_line},
    {"test_game_colony_unload_whole_hold", test_game_colony_unload_whole_hold},
    {"test_game_colony_outside_role_tools_residue", test_game_colony_outside_role_tools_residue},
    {"test_colony_zoom_hold_survives_pedia_detour", test_colony_zoom_hold_survives_pedia_detour},
    {"test_trade_route_wagon_services_arrival_stop", test_trade_route_wagon_services_arrival_stop},
    {"test_trade_route_begin_moves_wagon_to_selected_stop", test_trade_route_begin_moves_wagon_to_selected_stop},
    {"test_goto_treasure_gets_king_galleon_offer", test_goto_treasure_gets_king_galleon_offer},
    {"test_ship_bumps_foreign_colony_trade_arm", test_ship_bumps_foreign_colony_trade_arm},
    {"test_trade_route_ship_aims_colony_tile", test_trade_route_ship_aims_colony_tile},
    {"test_ai_euro_5952_absorb_colonist", test_ai_euro_5952_absorb_colonist},
    {"test_ai_euro_5952_absorb_soldier", test_ai_euro_5952_absorb_soldier},
    {"test_ai_euro_5952_equip_scout", test_ai_euro_5952_equip_scout},
    {"test_ai_euro_5952_equip_pioneer", test_ai_euro_5952_equip_pioneer},
    {"test_colony_shift_pick_moves_highlight", test_colony_shift_pick_moves_highlight},
    {"test_colony_jobs_menu_seats_indoor_worker", test_colony_jobs_menu_seats_indoor_worker},
    {"test_colony_jobs_menu_open_has_no_side_effects", test_colony_jobs_menu_open_has_no_side_effects},
    {"test_game_world_view", test_game_world_view},
    {"test_goto_place_opens_travelplace_list", test_goto_place_opens_travelplace_list},
    {"test_colony_list_keys_wrap", test_colony_list_keys_wrap},
    {"test_colony_custom_house_keys", test_colony_custom_house_keys},
    {"test_screen_f1_focus_article", test_screen_f1_focus_article},
    {"test_colony_construction_page_default", test_colony_construction_page_default},
    {"test_colony_construction_two_pages_live", test_colony_construction_two_pages_live},
    {"test_indian_land_offer_row_greyed", test_indian_land_offer_row_greyed},
    {"test_europe_unload_all_boycott_any_slot", test_europe_unload_all_boycott_any_slot},
};

TEST_MAIN(k_cases)
