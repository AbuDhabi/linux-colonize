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
#include "core/map_panel.h"
#include "core/popup.h"
#include "core/reports.h"
#include "core/turn_internal.h"
#include "core/woodcut.h"

#include "../common/ai_fixture.h"
#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NAME "unit_stage_seams"
#include "../common/test_fail.h"
#include "../common/test_catalogs.h"

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

/* 021a's dres is FUN_1000_8908 -> FUN_281f_0718 -> FUN_137f_04b0, which
 * hides a special only on a native settlement tile (FUN_137f_0392). A Euro
 * colony tile (settlement bit, owner nibble 0) keeps its special, so dres
 * must read 1 there. */
static int test_ai_021a_dres_euro_colony_tile(void) {
  for (int t = 0; t < 8; ++t) {
    for (int seed = 1; seed < 256; ++seed) {
      ColonizeWorldMap map;
      if (!fx_map_alloc(&map, 8, 8, t, /*with_seen=*/true)) {
        return fail("map alloc");
      }
      map.prime_resource_seed = (uint16_t)seed;
      ColonizeUnitPool units;
      fx_units_init(&units);
      struct ai_021a_ctx c;
      memset(&c, 0, sizeof(c));
      c.map = &map;
      c.units = &units;
      c.x = 4;
      c.y = 4;
      c.threat_nation = -1;
      (void)ai_021a_dir_tile(&c); /* learn the destination tile */
      /* Only the destination is a colony tile: a settlement next to it
       * would trigger the visit-cooldown skip. */
      map.layer2[c.ny * map.width + c.nx] = MAP_OCCUPANCY_HAS_CITY;
      const Ai021aDirStatus st = ai_021a_dir_tile(&c);
      const int dos_res = map_dos_resource_at(&map, c.nx, c.ny);
      fx_map_free(&map);
      if (st == AI_021A_DIR_OK && dos_res >= 0) {
        if (c.dres != 1) {
          fprintf(stderr, "021a dres: terrain=%d seed=%d dres=%d\n", t, seed, c.dres);
          return fail("021a dres hid a Euro colony tile's special (DOS 04b0 keeps it)");
        }
        return 0;
      }
    }
  }
  return fail("no seed/terrain put a special on the destination tile");
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
/* gap: a deleted colony sits ahead of both stops, so the stops' DOS colony
 * indices (dense, active-only) no longer equal the port ids (campaign 17: a
 * burned colony made the silver wagon service a French colony's slot). */
static int trade_route_arrival_case(bool gap) {
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
  if (gap) {
    fx_colony_add(&game.colonies, 0, 1, 1, 1)->active = false;
  }
  ColonizeColony* load = fx_colony_add(&game.colonies, 0, 5, 4, 1);
  ColonizeColony* other = fx_colony_add(&game.colonies, 0, 6, 4, 1);
  load->stock[COLONIZE_CARGO_SUGAR] = 60;
  /* The load key is the DS:0x84bc sell price x stock; price 0 loads nothing. */
  game.col1.nation[0].trade.euro_price[COLONIZE_CARGO_SUGAR] = 4;
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);

  ColonizeCol1TradeRoute* route = &game.col1.trade_route[0];
  route->dest_count = 2;
  route->stop[0].colony_index = 1; /* DOS index of `other` */
  route->stop[1].colony_index = 0; /* DOS index of `load` */
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

static int test_trade_route_wagon_services_arrival_stop(void) {
  return trade_route_arrival_case(false) || trade_route_arrival_case(true);
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

/* Trade-route seam fixture: 8x8 map filled with `terrain`, one unit type (wagon or
 * caravel, 2 holds), human nation 0, popups ready, route 0 with 2 stops. */
static void trade_seam_init(ColonizeGameState* game, int terrain, bool sea) {
  memset(game, 0, sizeof(*game));
  (void)fx_map_alloc(&game->world_map, 8, 8, terrain, /*with_seen=*/true);
  game->world_map_ok = true;
  game->human_nation = 0;
  game->col1_ok = true;
  fx_units_init(&game->units);
  game->units_ok = true;
  game->units.type_count = 1;
  game->units.types[0].kind_plus1 = (sea ? UNITS_KIND_CARAVEL : UNITS_KIND_WAGON) + 1;
  game->units.types[0].movement = sea ? 4 : 2;
  game->units.types[0].domain = sea ? COLONIZE_UNIT_DOMAIN_SEA : COLONIZE_UNIT_DOMAIN_LAND;
  game->units.types[0].cargo = 2;
  fx_colonies_init(&game->colonies);
  game->colonies_ok = true;
  ai_popup_init(&game->ai_popups);
  game->col1.trade_route[0].dest_count = 2;
  units_set_occupancy_map(&game->world_map);
  colonies_set_occupancy_map(&game->world_map);
}

static void trade_seam_free(ColonizeGameState* game) {
  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game->world_map);
}

static ColonizeUnit* trade_seam_unit(ColonizeGameState* game, int x, int y, int stop) {
  ColonizeUnit* u = units_get(&game->units, units_spawn(&game->units, 0, x, y));
  if (u) {
    u->nation_id = 0;
    u->orders = UNITS_ORDER_TRADE_ROUTE;
    u->follow_unit_id = 0;
    u->col1_counter16 = stop;
    u->goto_x = x;
    u->goto_y = y;
    u->moves = 6;
  }
  return u;
}

static int trade_seam_held(const ColonizeGameState* game, const ColonizeUnit* u, int cargo) {
  int n = 0;
  for (int h = 0; h < units_goods_hold_count(&game->units, u->id); ++h) {
    if (u->hold_goods_type[h] == cargo) {
      n += u->hold_goods_amount[h];
    }
  }
  return n;
}

/* FUN_479b_0b84 raw 77158-77175: route stops hold DOS colony indices. After a colony
 * is gone (a hole in the port pool), DOS index 1 is the third port colony (id 2); the
 * stop must service that colony, not port id 1. */
static int test_trade_route_stop_uses_dos_colony_index(void) {
  ColonizeGameState game;
  trade_seam_init(&game, 2, false);
  ColonizeColony* a = fx_colony_add(&game.colonies, 0, 1, 1, 1);
  ColonizeColony* gone = fx_colony_add(&game.colonies, 0, 3, 1, 1);
  ColonizeColony* c = fx_colony_add(&game.colonies, 0, 5, 1, 1);
  gone->active = false;
  c->stock[COLONIZE_CARGO_SUGAR] = 60;
  game.col1.nation[0].trade.euro_price[COLONIZE_CARGO_SUGAR] = 4;
  ColonizeCol1TradeRoute* route = &game.col1.trade_route[0];
  route->stop[0].colony_index = 1; /* DOS index of c */
  route->stop[0].load_count = 1;
  col1_trade_nibble_set(route->stop[0].load_cargo_nibbles, 0, COLONIZE_CARGO_SUGAR);
  route->stop[1].colony_index = 0;
  ColonizeUnit* w = trade_seam_unit(&game, c->x, c->y, 0);
  int rc = 0;
  if (!w) {
    rc = fail("dos-index wagon spawn");
  } else {
    game_trade_route_retarget(&game, w);
    if (trade_seam_held(&game, w, COLONIZE_CARGO_SUGAR) != 60 ||
        c->stock[COLONIZE_CARGO_SUGAR] != 0) {
      rc = fail("trade stop looked the DOS index up as a port id");
    } else if (w->col1_counter16 != 1 || w->goto_x != a->x || w->goto_y != a->y) {
      rc = fail("dos-index stop did not advance");
    }
  }
  trade_seam_free(&game);
  return rc;
}

/* FUN_479b_0bd0 raw 77234-77238: a unit whose goto is spent but which does not stand
 * on the stop colony's tile (FUN_479b_0b84 exact tile) is not at the stop: it re-aims
 * the SAME stop instead of servicing or advancing. */
static int test_trade_route_off_tile_reaims_same_stop(void) {
  ColonizeGameState game;
  trade_seam_init(&game, 2, false);
  ColonizeColony* a = fx_colony_add(&game.colonies, 0, 2, 2, 1);
  (void)fx_colony_add(&game.colonies, 0, 6, 6, 1);
  a->stock[COLONIZE_CARGO_SUGAR] = 60;
  game.col1.nation[0].trade.euro_price[COLONIZE_CARGO_SUGAR] = 4;
  ColonizeCol1TradeRoute* route = &game.col1.trade_route[0];
  route->stop[0].colony_index = 0;
  route->stop[0].load_count = 1;
  col1_trade_nibble_set(route->stop[0].load_cargo_nibbles, 0, COLONIZE_CARGO_SUGAR);
  route->stop[1].colony_index = 1;
  ColonizeUnit* w = trade_seam_unit(&game, 3, 2, 0); /* next to a, not on it */
  int rc = 0;
  if (!w) {
    rc = fail("off-tile wagon spawn");
  } else {
    w->goto_x = UNITS_GOTO_NONE;
    w->goto_y = UNITS_GOTO_NONE;
    game_trade_route_retarget(&game, w);
    if (w->col1_counter16 != 0 || w->goto_x != a->x || w->goto_y != a->y ||
        a->stock[COLONIZE_CARGO_SUGAR] != 60) {
      rc = fail("off-tile unit serviced or skipped its stop");
    }
  }
  trade_seam_free(&game);
  return rc;
}

/* FUN_479b_0bd0 raw 77317-77334: the stop advances on every arrival BEFORE the
 * all-same-colony @ROUTELOOP check, which then ends the move. */
static int test_trade_route_routeloop_advances_stop(void) {
  if (!test_game_txt()) {
    return TR_SKIP; /* a textless @ROUTELOOP is dropped (ai_popup_enqueue_ok_ctx) */
  }
  ColonizeGameState game;
  trade_seam_init(&game, 2, false);
  game.messages = *test_game_txt(); /* shallow copy, never freed here */
  ColonizeColony* a = fx_colony_add(&game.colonies, 0, 2, 2, 1);
  game.col1.trade_route[0].stop[0].colony_index = 0;
  game.col1.trade_route[0].stop[1].colony_index = 0;
  ColonizeUnit* w = trade_seam_unit(&game, a->x, a->y, 0);
  int rc = 0;
  if (!w) {
    rc = fail("routeloop wagon spawn");
  } else {
    game_trade_route_retarget(&game, w);
    if (w->col1_counter16 != 1 || w->moves != 0 ||
        game.ai_popups.queue_count + game.ai_popups.open < 1) {
      rc = fail("routeloop did not advance the stop before the warning");
    }
  }
  trade_seam_free(&game);
  return rc;
}

/* FUN_479b_0972 raw 77094-77101: in the War of Independence a route ship reaching its
 * Europe stop on the high seas does not sail (silently) and never reaches the dock lane
 * (FUN_479b_0b84(unit, 999) false), so it trades nothing there and keeps the stop. */
static int test_trade_route_woi_europe_stop_no_trade(void) {
  ColonizeGameState game;
  trade_seam_init(&game, T_HIGH_SEAS, true);
  game.europe_ok = true;
  game.col1.head.game_options.woi = 1;
  (void)fx_colony_add(&game.colonies, 0, 1, 1, 1);
  ColonizeCol1TradeRoute* route = &game.col1.trade_route[0];
  route->stop[0].colony_index = 999;
  route->stop[0].unload_count = 1;
  col1_trade_nibble_set(route->stop[0].unload_cargo_nibbles, 0, COLONIZE_CARGO_FURS);
  route->stop[1].colony_index = 0;
  ColonizeUnit* s = trade_seam_unit(&game, 4, 4, 0);
  int rc = 0;
  if (!s || units_load_goods(&game.units, s->id, COLONIZE_CARGO_FURS, 50) != 50) {
    rc = fail("woi ship fixture");
  } else {
    game_trade_route_retarget(&game, s);
    if (s->col1_counter16 != 0 || trade_seam_held(&game, s, COLONIZE_CARGO_FURS) != 50 ||
        game.ai_popups.queue_count != 0 || game.ai_popups.open) {
      rc = fail("WoI Europe stop traded, advanced or popped up");
    }
  }
  trade_seam_free(&game);
  return rc;
}

/* FUN_479b_0b84(unit, 999) raw 77158-77175 is true only on the dock lane: a wagon whose
 * stop is Europe is never there, so FUN_479b_0bd0 keeps the stop (not-arrived arm, raw
 * 77212-77246) instead of advancing. */
static int test_trade_route_europe_stop_off_lane_keeps_stop(void) {
  ColonizeGameState game;
  trade_seam_init(&game, 2, false);
  (void)fx_colony_add(&game.colonies, 0, 1, 1, 1);
  game.col1.trade_route[0].stop[0].colony_index = 999;
  game.col1.trade_route[0].stop[1].colony_index = 0;
  ColonizeUnit* w = trade_seam_unit(&game, 4, 4, 0);
  int rc = 0;
  if (!w) {
    rc = fail("europe-stop wagon spawn");
  } else {
    w->goto_x = UNITS_GOTO_NONE;
    w->goto_y = UNITS_GOTO_NONE;
    game_trade_route_retarget(&game, w);
    if (w->col1_counter16 != 0) {
      rc = fail("off-lane unit advanced past its Europe stop");
    }
  }
  trade_seam_free(&game);
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

/* FUN_465b_0000 raw 75450 / 75792: a human explorer whose rumour changes the
 * unit count (survivors join) skips the FUN_5bfb_3180 contact work, so neither
 * the unmet village next door (FUN_5bfb_022e MET bit) nor the Dutch unit on
 * the other side (FUN_5bfb_153e MET bit) is met on this move. Control: an
 * outcome that leaves the count alone meets both. */
static int test_human_lcr_count_change_skips_contact(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/2, /*with_seen=*/true)) {
    return fail("human lcr map alloc");
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  game.col1_ok = true;
  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].kind_plus1 = UNITS_KIND_COLONIST + 1;
  game.units.types[0].movement = 2;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);
  game.col1.player[0].named_new_world = 1;
  for (int n = 1; n < 4; ++n) {
    game.col1.player[n].control = 1;
  }
  for (uint16_t s = 1; s < 512 && !map_tile_has_rumour(&game.world_map, 3, 3); ++s) {
    game.world_map.prime_resource_seed = s;
  }
  ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.nation_id = 4;
  tribe.x = 4;
  tribe.y = 3;
  game.col1.tribe = &tribe;
  game.col1.head.tribe_count = 1;
  /* The village tile carries the settlement bit and owner nibble that 3180's
   * neighbour read (FUN_281f_06be -> FUN_137f_03e4) resolves. */
  game.world_map.layer2[3 * 8 + 4] |= 0x02u;
  game.world_map.layer3[3 * 8 + 4] = (uint8_t)((game.world_map.layer3[3 * 8 + 4] & 0x0fu) | 0x40u);
  int seen_changed = 0;
  int seen_same = 0;
  const int32_t saved_83a6 = dos_rng_get_83a6();
  int rc = map_tile_has_rumour(&game.world_map, 3, 3) ? 0 : fail("no rumour at (3,3)");
  for (int seed = 1; seed < 400 && rc == 0 && (!seen_changed || !seen_same); ++seed) {
    game.world_map.layer2[3 * 8 + 3] &= (uint8_t)~MAP_LAYER2_LCR_CONSUMED;
    memset(&game.col1.indian[0], 0, sizeof(game.col1.indian[0]));
    ai_popup_init(&game.ai_popups);
    /* The explorer walks onto the rumour, so the move's own 3180 halves
     * (units_sentry_wake_scan) run too. */
    ColonizeUnit* u = units_get(&game.units, units_spawn(&game.units, 0, 3, 4));
    if (!u) {
      rc = fail("human lcr spawn");
      break;
    }
    u->nation_id = 0;
    u->moves = 6;
    const int uid = u->id;
    game.units.selected_id = uid;
    ColonizeUnit* dutch = units_get(&game.units, units_spawn_allow_stack(&game.units, 0, 2, 3));
    if (!dutch) {
      rc = fail("human lcr spawn neighbour");
      break;
    }
    dutch->nation_id = 1;
    /* 3180's facing stamp (raw 98648-98651) is skipped with the rest of 3180. */
    dutch->last_dir = -1;
    /* So is its sentry wake (raw 98628-98646). */
    dutch->orders = UNITS_ORDER_SENTRY;
    const int dutch_id = dutch->id;
    game.col1.head.turn = (uint16_t)seed; /* 153e talks once per pair per turn */
    ai_diplo_talk_reset();
    ai_diplo_write(&game.col1, 0, 1, 0);
    ai_diplo_write(&game.col1, 1, 0, 0);
    /* FUN_65dd_0004 reseeds from the 83a6 word (an earlier case may have set it). */
    dos_rng_set_83a6((int32_t)seed * 12345);
    dos_rng_seed(&game.move_rng, (uint32_t)seed * 12345u);
    const int before = units_active_count(&game.units);
    (void)game_try_unit_move(&game, 3, 3);
    u = units_get(&game.units, uid);
    dutch = units_get(&game.units, dutch_id);
    const bool alive = u && u->active && u->x == 3 && u->y == 3;
    const bool changed = units_active_count(&game.units) != before;
    const bool met = (game.col1.indian[0].euro_diplo[0] & COL1_INDIAN_MET_BIT) != 0;
    const bool euro_met = (ai_diplo_read(&game.col1, 0, 1) & COL1_INDIAN_MET_BIT) != 0;
    const bool faced = dutch && dutch->last_dir >= 0;
    const bool woke = dutch && dutch->orders != UNITS_ORDER_SENTRY;
    if (alive && changed) {
      seen_changed++;
      if (met || euro_met || faced || woke) {
        fprintf(stderr,
                "human lcr: count change (seed %d) still ran contact (tribe %d euro %d faced %d "
                "woke %d)\n",
                seed, met, euro_met, faced, woke);
        rc = 1;
      }
    } else if (alive && !changed) {
      seen_same++;
      if (!met || !euro_met || !faced || !woke) {
        fprintf(stderr,
                "human lcr: unchanged count (seed %d) skipped contact (tribe %d euro %d faced %d "
                "woke %d)\n",
                seed, met, euro_met, faced, woke);
        rc = 1;
      }
    }
    for (int i = 0; i < units_slot_end(&game.units); ++i) {
      if (game.units.units[i].active) {
        units_despawn(&game.units, game.units.units[i].id);
      }
    }
  }
  if (rc == 0 && (!seen_changed || !seen_same)) {
    fprintf(stderr, "human lcr: outcomes not reached (changed %d same %d)\n", seen_changed,
            seen_same);
    rc = 1;
  }
  /* A tail that commits no step (combat, village or colony entry, fortify)
   * jumps to LAB_465b_0bd1 past 3180 (raw 75797): same neighbours, no contact. */
  if (rc == 0) {
    game.world_map.layer2[3 * 8 + 3] |= MAP_LAYER2_LCR_CONSUMED;
    memset(&game.col1.indian[0], 0, sizeof(game.col1.indian[0]));
    ai_popup_init(&game.ai_popups);
    ColonizeUnit* u = units_get(&game.units, units_spawn(&game.units, 0, 3, 3));
    ColonizeUnit* dutch = units_get(&game.units, units_spawn_allow_stack(&game.units, 0, 2, 3));
    if (!u || !dutch) {
      rc = fail("human non-step spawn");
    } else {
      u->nation_id = 0;
      u->moves = 6;
      dutch->nation_id = 1;
      game.units.selected_id = u->id;
      game.col1.head.turn = 1000;
      ai_diplo_talk_reset();
      ai_diplo_write(&game.col1, 0, 1, 0);
      ai_diplo_write(&game.col1, 1, 0, 0);
      game_after_unit_action(&game);
      if ((game.col1.indian[0].euro_diplo[0] & COL1_INDIAN_MET_BIT) != 0 ||
          (ai_diplo_read(&game.col1, 0, 1) & COL1_INDIAN_MET_BIT) != 0) {
        rc = fail("human non-step tail: no 3180 contact without a committed step");
      }
    }
  }
  dos_rng_set_83a6(saved_83a6);
  game.col1.tribe = NULL;
  game.col1.head.tribe_count = 0;
  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/* Step-tail fixture: land everywhere but the ocean column x=2 rows 3..4; an
 * unmet village at (4,3); an AI French unit at (3,2) facing -1. Type 0 =
 * colonist, type 1 = caravel. A FUN_5bfb_3180 run from (3,3) stamps the
 * French unit's facing and meets the village. */
static int step3180_fixture(ColonizeGameState* game, ColonizeCol1Tribe* tribe) {
  trade_seam_init(game, 2, false);
  game->units.type_count = 2;
  game->units.types[0].kind_plus1 = UNITS_KIND_COLONIST + 1;
  game->units.types[0].cargo = 0;
  game->units.types[0].movement = 1;
  game->units.types[1].kind_plus1 = UNITS_KIND_CARAVEL + 1;
  game->units.types[1].movement = 4;
  game->units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  game->units.types[1].cargo = 2;
  game->col1.player[0].named_new_world = 1;
  for (int n = 1; n < 4; ++n) {
    game->col1.player[n].control = 1;
  }
  for (uint16_t s = 1; s < 512 && map_tile_has_rumour(&game->world_map, 3, 3); ++s) {
    game->world_map.prime_resource_seed = s;
  }
  game->world_map.terrain[3 * 8 + 2] = 25; /* ocean */
  game->world_map.terrain[4 * 8 + 2] = 25;
  memset(tribe, 0, sizeof(*tribe));
  tribe->nation_id = 4;
  tribe->x = 4;
  tribe->y = 3;
  game->col1.tribe = tribe;
  game->col1.head.tribe_count = 1;
  game->world_map.layer2[3 * 8 + 4] |= 0x02u;
  game->world_map.layer3[3 * 8 + 4] = (uint8_t)((game->world_map.layer3[3 * 8 + 4] & 0x0fu) | 0x40u);
  ColonizeUnit* fr = units_get(&game->units, units_spawn(&game->units, 0, 3, 2));
  if (!fr || map_tile_has_rumour(&game->world_map, 3, 3) ||
      !map_tile_is_water(&game->world_map, 2, 3)) {
    return -1;
  }
  fr->nation_id = 1;
  fr->last_dir = -1;
  return fr->id;
}

static void step3180_free(ColonizeGameState* game) {
  game->col1.tribe = NULL;
  game->col1.head.tribe_count = 0;
  trade_seam_free(game);
}

/* DOS has no plain O board key: MENU.TXT's only ~O row is Dump Cargo
 * Overboard. O on the map must not board a land unit onto the ship at the
 * cursor. */
static int test_map_o_key_does_not_board(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  int rc = step3180_fixture(&game, &tribe) < 0 ? fail("o key fixture") : 0;
  ColonizeUnit* ship = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 1, 2, 3));
  ColonizeUnit* land = ship ? units_get(&game.units, units_spawn(&game.units, 0, 3, 3)) : NULL;
  if (rc == 0 && (!ship || !land)) {
    rc = fail("o key: spawn");
  }
  if (rc == 0) {
    ship->nation_id = 0;
    land->nation_id = 0;
    land->moves = 3;
    game.units.selected_id = land->id;
    game.map_cursor_x = 2;
    game.map_cursor_y = 3;
    ColonizeInputState in;
    memset(&in, 0, sizeof(in));
    in.last_key = COLONIZE_KEY_O;
    (void)game_update_map_keys(&game, &in);
    if (units_get_const(&game.units, land->id)->aboard_ship_id >= 0) {
      rc = fail("O must not board the land unit onto the ship");
    }
  }
  step3180_free(&game);
  return rc;
}

/* A held 3180 wake dies with its tail. The early return (no selected unit)
 * drops it, so a later non-step tail of the same unit wakes nobody, and
 * units_reset_state (load / new game) clears it too. */
static int test_deferred_wake_does_not_leak(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  const int fr_id = step3180_fixture(&game, &tribe);
  int rc = fr_id < 0 ? fail("wake leak fixture") : 0;
  ColonizeUnit* a = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 0, 3, 3));
  if (rc == 0 && !a) {
    rc = fail("wake leak: spawn");
  }
  if (rc == 0) {
    a->nation_id = 0;
    a->moves = 0;
    const int aid = a->id;
    units_get(&game.units, fr_id)->orders = UNITS_ORDER_SENTRY;
    g_units_wake_deferred_id = aid;
    game.units.selected_id = -1;
    game_after_unit_action(&game);
    game.units.selected_id = aid;
    game_after_unit_action(&game);
    if (units_get_const(&game.units, fr_id)->orders != UNITS_ORDER_SENTRY) {
      rc = fail("wake leak: a held wake survived the early return and fired later");
    }
    g_units_wake_deferred_id = aid;
    units_reset_state();
    if (g_units_wake_deferred_id != -1) {
      rc = fail("wake leak: units_reset_state kept the held wake");
    }
  }
  step3180_free(&game);
  return rc;
}

/* FUN_4720_015c raw 75988-75995: an awake passenger's arrow step onto an
 * adjacent own ship boards that ship instead of being refused as a landfall. */
static int test_human_passenger_ship_to_ship(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  const int fr_id = step3180_fixture(&game, &tribe);
  int rc = fr_id < 0 ? fail("ship-to-ship fixture") : 0;
  ColonizeUnit* a = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 1, 2, 3));
  const int b_id = a ? units_spawn_allow_stack(&game.units, 1, 2, 4) : -1;
  const int pax_id = a ? units_spawn(&game.units, 0, 1, 3) : -1;
  if (rc == 0 && (!a || b_id < 0 || pax_id < 0 || !units_board(&game.units, pax_id, a->id))) {
    rc = fail("ship-to-ship: ships / passenger");
  }
  if (rc == 0) {
    a->nation_id = 0;
    units_get(&game.units, b_id)->nation_id = 0;
    ColonizeUnit* pax = units_get(&game.units, pax_id);
    pax->nation_id = 0;
    pax->orders = UNITS_ORDER_NONE;
    pax->moves = UNITS_MP_PER_TILE;
    game.units.selected_id = pax_id;
    (void)game_try_unit_move(&game, 2, 4);
    pax = units_get(&game.units, pax_id);
    if (!pax || pax->aboard_ship_id != b_id) {
      rc = fail("ship-to-ship: the awake passenger must board the adjacent own ship");
    }
  }
  step3180_free(&game);
  return rc;
}

/* A board step's 07a0 reveal is the passenger's (raw 75783, asm 465b:0aca;
 * radius by mover type) at the ship's tile, whichever unit takes the focus.
 * A colonist (radius 1) boarding a Galleon (radius 2) at (2,3) sees (1,3)
 * but not (2,5). Branch 0: the ship has MP, wakes and takes the focus
 * (FUN_1427_1330 passes, raw 75719-75721). Branch 1: the ship is spent and
 * the focus stays off it. */
static int test_human_board_reveals_with_passenger(void) {
  int rc = 0;
  for (int spent = 0; spent < 2 && rc == 0; ++spent) {
    ColonizeGameState game;
    ColonizeCol1Tribe tribe;
    const int fr_id = step3180_fixture(&game, &tribe);
    rc = fr_id < 0 ? fail("board reveal fixture") : 0;
    game.units.types[1].kind_plus1 = UNITS_KIND_GALLEON + 1;
    /* A ship's second ring shows water only, so make the far probe water. */
    game.world_map.terrain[5 * 8 + 2] = T_OCEAN;
    ColonizeUnit* ship = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 1, 2, 3));
    ColonizeUnit* col = ship ? units_get(&game.units, units_spawn(&game.units, 0, 3, 3)) : NULL;
    if (rc == 0 && (!ship || !col)) {
      rc = fail("board reveal: spawn");
    }
    if (rc == 0 && (map_tile_seen_by(&game.world_map, 1, 3, 0) ||
                    map_tile_seen_by(&game.world_map, 2, 5, 0))) {
      rc = fail("board reveal: (1,3) and (2,5) must start unseen");
    }
    if (rc == 0) {
      ship->nation_id = 0;
      ship->moves = spent ? 0 : 12;
      ship->orders = UNITS_ORDER_SENTRY;
      col->nation_id = 0;
      col->moves = 3;
      game.units.selected_id = col->id;
      (void)game_try_unit_move(&game, 2, 3);
      if (col->aboard_ship_id != ship->id) {
        rc = fail("board reveal: the colonist must board");
      } else if (ship->orders != UNITS_ORDER_NONE) {
        rc = fail("board reveal: the sentried ship must wake");
      } else if ((game.units.selected_id == ship->id) != (spent == 0)) {
        fprintf(stderr, "board reveal: spent=%d focus=%d ship=%d\n", spent,
                game.units.selected_id, ship->id);
        rc = fail("board reveal: only a ship with MP takes the focus");
      } else if (!map_tile_seen_by(&game.world_map, 1, 3, 0)) {
        fprintf(stderr, "board reveal: spent=%d\n", spent);
        rc = fail("board reveal: the passenger's radius 1 must reveal (1,3)");
      } else if (map_tile_seen_by(&game.world_map, 2, 5, 0)) {
        fprintf(stderr, "board reveal: spent=%d\n", spent);
        rc = fail("board reveal: the Galleon's radius 2 must not stand in for the passenger");
      }
    }
    step3180_free(&game);
  }
  return rc;
}

/* With two own hulls on the water tile the focus goes to the tile head
 * (units_465b_board_wake_focus), which need not be the hull the passenger is
 * filed under; the reveal is still the passenger's (raw 75783). */
static int test_human_board_two_hulls_reveals_with_passenger(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  const int fr_id = step3180_fixture(&game, &tribe);
  int rc = fr_id < 0 ? fail("two hulls fixture") : 0;
  game.units.types[1].kind_plus1 = UNITS_KIND_GALLEON + 1;
  game.world_map.terrain[5 * 8 + 2] = T_OCEAN;
  ColonizeUnit* low = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 1, 2, 3));
  const int top_id = low ? units_spawn_allow_stack(&game.units, 1, 2, 3) : -1;
  ColonizeUnit* col = top_id >= 0 ? units_get(&game.units, units_spawn(&game.units, 0, 3, 3)) : NULL;
  if (rc == 0 && (!low || top_id < 0 || !col)) {
    rc = fail("two hulls: spawn");
  }
  if (rc == 0) {
    low->nation_id = 0;
    low->moves = 12;
    units_get(&game.units, top_id)->nation_id = 0;
    units_get(&game.units, top_id)->moves = 12;
    col->nation_id = 0;
    col->moves = 3;
    game.units.selected_id = col->id;
    (void)game_try_unit_move(&game, 2, 3);
    if (col->aboard_ship_id < 0) {
      rc = fail("two hulls: the colonist must board");
    } else if (game.units.selected_id == col->aboard_ship_id) {
      fprintf(stderr, "two hulls: focus %d, filed under %d, head %d\n", game.units.selected_id,
              col->aboard_ship_id, top_id);
      rc = fail("two hulls: the fixture must put the focus on the other hull");
    } else if (!map_tile_seen_by(&game.world_map, 1, 3, 0) ||
               map_tile_seen_by(&game.world_map, 2, 5, 0)) {
      rc = fail("two hulls: the reveal must be the passenger's radius 1");
    }
  }
  step3180_free(&game);
  return rc;
}

/* The ship-to-ship transfer is the same 465b board step (FUN_4720_015c ->
 * 465b), so it reveals with the passenger's radius at the receiving ship's
 * tile too (raw 75783), with MP on the receiving ship (focus moves to it)
 * and without (focus stays on the passenger, who is off the map). */
static int test_human_transfer_reveals_with_passenger(void) {
  int rc = 0;
  for (int spent = 0; spent < 2 && rc == 0; ++spent) {
    ColonizeGameState game;
    ColonizeCol1Tribe tribe;
    const int fr_id = step3180_fixture(&game, &tribe);
    rc = fr_id < 0 ? fail("transfer reveal fixture") : 0;
    game.units.types[1].kind_plus1 = UNITS_KIND_GALLEON + 1;
    game.world_map.terrain[5 * 8 + 2] = T_OCEAN;
    ColonizeUnit* ship = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 1, 2, 3));
    ColonizeUnit* host = ship ? units_get(&game.units, units_spawn(&game.units, 1, 2, 4)) : NULL;
    ColonizeUnit* col = host ? units_get(&game.units, units_spawn(&game.units, 0, 3, 3)) : NULL;
    if (rc == 0 && (!ship || !host || !col || !units_board(&game.units, col->id, host->id))) {
      rc = fail("transfer reveal: spawn / board host");
    }
    if (rc == 0 && (map_tile_seen_by(&game.world_map, 1, 3, 0) ||
                    map_tile_seen_by(&game.world_map, 2, 5, 0))) {
      rc = fail("transfer reveal: (1,3) and (2,5) must start unseen");
    }
    if (rc == 0) {
      ship->nation_id = host->nation_id = col->nation_id = 0;
      ship->moves = spent ? 0 : 12;
      ship->orders = UNITS_ORDER_SENTRY;
      col->orders = UNITS_ORDER_NONE;
      col->moves = 3;
      game.units.selected_id = col->id;
      (void)game_try_unit_move(&game, 2, 3);
      if (col->aboard_ship_id != ship->id) {
        rc = fail("transfer reveal: the passenger must move to the ship");
      } else if ((game.units.selected_id == ship->id) != (spent == 0)) {
        fprintf(stderr, "transfer reveal: spent=%d focus=%d ship=%d\n", spent,
                game.units.selected_id, ship->id);
        rc = fail("transfer reveal: only a ship with MP takes the focus");
      } else if (!map_tile_seen_by(&game.world_map, 1, 3, 0)) {
        fprintf(stderr, "transfer reveal: spent=%d\n", spent);
        rc = fail("transfer reveal: the passenger's radius 1 must reveal (1,3)");
      } else if (map_tile_seen_by(&game.world_map, 2, 5, 0)) {
        fprintf(stderr, "transfer reveal: spent=%d\n", spent);
        rc = fail("transfer reveal: the Galleon's radius 2 must not stand in for the passenger");
      }
    }
    step3180_free(&game);
  }
  return rc;
}

/* A Go To step onto an own ship's tile is the same 465b board step
 * (units_advance_goto_one_step_w -> units_try_move_w -> units_move_board),
 * and the pacer runs the plain game_after_unit_step tail, so the reveal must
 * still be the passenger's (raw 75783), with or without ship MP, and the
 * tail consumes the board record. */
static int test_goto_board_reveals_with_passenger(void) {
  int rc = 0;
  for (int spent = 0; spent < 2 && rc == 0; ++spent) {
    ColonizeGameState game;
    ColonizeCol1Tribe tribe;
    const int fr_id = step3180_fixture(&game, &tribe);
    rc = fr_id < 0 ? fail("goto board fixture") : 0;
    game.units.types[1].kind_plus1 = UNITS_KIND_GALLEON + 1;
    game.world_map.terrain[5 * 8 + 2] = T_OCEAN;
    ColonizeUnit* ship = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 1, 2, 3));
    ColonizeUnit* col = ship ? units_get(&game.units, units_spawn(&game.units, 0, 3, 3)) : NULL;
    if (rc == 0 && (!ship || !col)) {
      rc = fail("goto board: spawn");
    }
    if (rc == 0) {
      ship->nation_id = 0;
      ship->moves = spent ? 0 : 12;
      ship->orders = UNITS_ORDER_SENTRY;
      col->nation_id = 0;
      col->moves = 3;
      col->orders = UNITS_ORDER_GOTO;
      col->goto_x = 2;
      col->goto_y = 3;
      game.units.selected_id = col->id;
      ColonizeInputState input;
      memset(&input, 0, sizeof(input));
      (void)game_update_unit_pacer(&game, &input, 100);
      if (col->aboard_ship_id != ship->id) {
        fprintf(stderr, "goto board: spent=%d pos=(%d,%d) aboard=%d orders=%d\n", spent,
                col->x, col->y, col->aboard_ship_id, col->orders);
        rc = fail("goto board: the Go To step must board the ship");
      } else if (!map_tile_seen_by(&game.world_map, 1, 3, 0)) {
        fprintf(stderr, "goto board: spent=%d\n", spent);
        rc = fail("goto board: the passenger's radius 1 must reveal (1,3)");
      } else if (map_tile_seen_by(&game.world_map, 2, 5, 0)) {
        fprintf(stderr, "goto board: spent=%d\n", spent);
        rc = fail("goto board: the Galleon's radius 2 must not stand in for the passenger");
      } else if (units_last_enter_reason() == COLONIZE_ENTER_BOARD &&
                 units_take_board_passenger() != -1) {
        rc = fail("goto board: the step tail must consume the board record");
      }
    }
    step3180_free(&game);
  }
  return rc;
}

/* @LANDFALL "Make landfall" makes the passenger active (FUN_281f_086c ->
 * FUN_1427_08ea sets DS:0x5392) and jumps to LAB_4720_061c (ndisasm
 * 0x3FE56-0x3FEA3 -> 0xff5c), the FUN_2a1f_0142 commit: the passenger's step
 * runs the FUN_465b_0000 tail, FUN_5bfb_3180 included. */
static int test_human_landfall_runs_3180(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  const int fr_id = step3180_fixture(&game, &tribe);
  int rc = fr_id < 0 ? fail("landfall 3180 fixture") : 0;
  ColonizeUnit* ship = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 1, 2, 3));
  const int pax_id = ship ? units_spawn(&game.units, 0, 1, 3) : -1; /* boards from beside */
  if (rc == 0 && (!ship || pax_id < 0 || !units_board(&game.units, pax_id, ship->id))) {
    rc = fail("landfall 3180: ship / passenger");
  }
  if (rc == 0) {
    ship->nation_id = 0;
    ship->moves = 12;
    units_get(&game.units, pax_id)->nation_id = 0;
    game.units.selected_id = ship->id;
    (void)game_try_unit_move(&game, 3, 3);
    if (units_get_const(&game.units, pax_id)->aboard_ship_id != ship->id) {
      rc = fail("landfall 3180: the step must ask @LANDFALL first");
    }
    game.ai_popups.has_result = true;
    game.ai_popups.result_tag = AI_POPUP_TAG_LANDFALL;
    game.ai_popups.result_cancelled = false;
    game.ai_popups.result_nation_a = ship->id;
    game.ai_popups.result_nation_b = 3;
    game.ai_popups.result_payload = 3;
    game.ai_popups.result_choice_id = 1;
    game_apply_ai_popup_result(&game);
    const ColonizeUnit* pax = units_get_const(&game.units, pax_id);
    if (rc == 0 && (!pax || pax->x != 3 || pax->y != 3)) {
      rc = fail("landfall 3180: passenger did not land at (3,3)");
    } else if (rc == 0 && (game.col1.indian[0].euro_diplo[0] & COL1_INDIAN_MET_BIT) == 0) {
      rc = fail("landfall 3180: the passenger's landfall step must run FUN_5bfb_3180");
    }
  }
  step3180_free(&game);
  return rc;
}

/* A village visit from the next tile is FUN_465b_0000's dwelling dispatch (raw
 * 75486-75490 -> 2a1f_016c, goto LAB_465b_0bd1), past the 3180 tail at raw
 * 75797: the French unit beside the visitor keeps its facing. */
static int test_human_village_visit_no_3180(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  const int fr_id = step3180_fixture(&game, &tribe);
  int rc = fr_id < 0 ? fail("visit 3180 fixture") : 0;
  ColonizeUnit* v = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 0, 3, 3));
  if (rc == 0 && !v) {
    rc = fail("visit 3180: spawn");
  }
  if (rc == 0) {
    v->nation_id = 0;
    v->moves = 3;
    game.units.selected_id = v->id;
    (void)game_try_unit_move(&game, 4, 3);
    const ColonizeUnit* fr = units_get_const(&game.units, fr_id);
    if (v->x != 3 || v->y != 3 || !ai_contact_meet_pending_for_unit(&game.ai_popups, v->id)) {
      rc = fail("visit 3180: the step must open the village menu, not enter");
    } else if (!fr || fr->last_dir >= 0) {
      rc = fail("visit 3180: a village visit must not run FUN_5bfb_3180");
    }
  }
  step3180_free(&game);
  return rc;
}

/* Position of woodcut `id` in the drained FIFO, -1 if absent. */
static int drained_woodcut_index(const int* ids, int n, int id) {
  for (int i = 0; i < n; ++i) {
    if (ids[i] == id) {
      return i;
    }
  }
  return -1;
}

/* FUN_4720_049e runs its @LANDHO scan (raw 76124-76141) only after
 * FUN_2a1f_0142 returned, i.e. after FUN_465b_0000's 3180 tail (raw
 * 75794-75797). A first landing beside an unmet village arms the first-contact
 * woodcut (FUN_5bfb_022e) before the New World one. */
static int test_human_landho_after_3180(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  const int fr_id = step3180_fixture(&game, &tribe);
  int rc = fr_id < 0 ? fail("landho order fixture") : 0;
  ColonizeUnit* m = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 0, 3, 4));
  if (rc == 0 && !m) {
    rc = fail("landho order: spawn");
  }
  if (rc == 0) {
    game.col1.player[0].named_new_world = 0;
    m->nation_id = 0;
    m->moves = 3;
    game.units.selected_id = m->id;
    woodcut_clear_pending();
    (void)game_try_unit_move(&game, 3, 3);
    int ids[8];
    int n = 0;
    while (n < 8 && woodcut_has_pending()) {
      ids[n++] = woodcut_take_pending();
    }
    const int meet = drained_woodcut_index(ids, n, WOODCUT_THE_INCA_NATION);
    const int early = drained_woodcut_index(ids, n, WOODCUT_DISCOVERY_OF_THE_NEW_WORLD);
    const bool queued = ai_popup_busy(&game.ai_popups);
    /* The meet popup is blocking in DOS, so the scan (and its woodcut and
     * name dialog) waits until it is answered. */
    ai_popup_init(&game.ai_popups);
    game_landho_service(&game);
    n = 0;
    while (n < 8 && woodcut_has_pending()) {
      ids[n++] = woodcut_take_pending();
    }
    const int land = drained_woodcut_index(ids, n, WOODCUT_DISCOVERY_OF_THE_NEW_WORLD);
    if (m->x != 3 || m->y != 3) {
      rc = fail("landho order: the colonist did not step to (3,3)");
    } else if (meet < 0 || !queued) {
      fprintf(stderr, "landho order: meet woodcut at %d, popup queued %d\n", meet, (int)queued);
      rc = fail("landho order: the step must run FUN_5bfb_3180 and queue its popup");
    } else if (early >= 0 || land < 0) {
      fprintf(stderr, "landho order: new world before answer %d, after %d\n", early, land);
      rc = fail("landho order: the Land Ho scan must wait for the queued 3180 popup");
    }
  }
  woodcut_clear_pending();
  step3180_free(&game);
  return rc;
}

/* The Land Ho scan sits in FUN_4720_049e's committed-step arm (raw
 * 76123-76141); a non-step tail (fortify, sentry, combat) never scans. */
static int test_human_nonstep_no_landho(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  const int fr_id = step3180_fixture(&game, &tribe);
  int rc = fr_id < 0 ? fail("nonstep landho fixture") : 0;
  ColonizeUnit* m = rc ? NULL : units_get(&game.units, units_spawn(&game.units, 0, 3, 4));
  if (rc == 0 && !m) {
    rc = fail("nonstep landho: spawn");
  }
  if (rc == 0) {
    game.col1.player[0].named_new_world = 0;
    m->nation_id = 0;
    m->moves = 0;
    game.units.selected_id = m->id;
    woodcut_clear_pending();
    game_after_unit_action(&game);
    game_landho_service(&game);
    if (woodcut_has_pending() || game.name_entry.open || game.col1.player[0].named_new_world ||
        game.landho_deferred) {
      rc = fail("nonstep landho: a non-step tail must not run the Land Ho scan");
    }
  }
  woodcut_clear_pending();
  step3180_free(&game);
  return rc;
}

/* FUN_465b_0000 runs 0984 -> FUN_5bfb_3180 (raw 75794-75797) before the
 * Treasure arm (raw 75800-75816), so a Treasure stepping into its own coastal
 * colony beside an unmet village queues @INDIANWELCOME ahead of the King's
 * Galleon offer. */
static int test_human_treasure_offer_after_3180(void) {
  ColonizeGameState game;
  ColonizeCol1Tribe tribe;
  const int fr_id = step3180_fixture(&game, &tribe);
  int rc = fr_id < 0 ? fail("treasure order fixture") : 0;
  game.units.type_count = 3;
  game.units.types[2].kind_plus1 = UNITS_KIND_TREASURE + 1;
  game.units.types[2].movement = 1;
  game.units.types[2].cargo = 0;
  ColonizeColony* port = rc ? NULL : fx_colony_add(&game.colonies, 0, 3, 3, 1);
  ColonizeUnit* t = port ? units_get(&game.units, units_spawn(&game.units, 2, 3, 4)) : NULL;
  if (rc == 0 && (!port || !t)) {
    rc = fail("treasure order: colony / treasure");
  }
  if (rc == 0) {
    port->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
    t->nation_id = 0;
    t->profession = 10; /* +0x315b = gold/100 */
    t->moves = 3;
    game.units.selected_id = t->id;
    (void)game_try_unit_move(&game, 3, 3);
    int welcome = -1;
    int galleon = -1;
    for (int q = 0; q < game.ai_popups.queue_count; ++q) {
      const int tag = game.ai_popups.queue[q].tag;
      if (tag == AI_POPUP_TAG_CONTACT_WELCOME && welcome < 0) {
        welcome = q;
      } else if (tag == AI_POPUP_TAG_KING_GALLEON && galleon < 0) {
        galleon = q;
      }
    }
    if (t->x != 3 || t->y != 3) {
      rc = fail("treasure order: the Treasure did not enter the colony");
    } else if (welcome < 0 || galleon < 0 || welcome > galleon) {
      fprintf(stderr, "treasure order: welcome at %d, galleon at %d\n", welcome, galleon);
      rc = fail("treasure order: the King's Galleon offer must follow FUN_5bfb_3180");
    }
  }
  step3180_free(&game);
  return rc;
}

/* A hull sailing into its own colony beside an unmet village runs the
 * FUN_5bfb_3180 Indian arm (raw 98648-98672: land mover tile, land
 * neighbour, no unit-type test) -> 066c -> 022e first contact (MET bit). */
static int test_ship_in_colony_meets_tribe(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/2, /*with_seen=*/true)) {
    return fail("ship contact map alloc");
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  game.col1_ok = true;
  ai_popup_init(&game.ai_popups);
  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].kind_plus1 = UNITS_KIND_CARAVEL + 1;
  game.units.types[0].movement = 4;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  game.units.types[0].cargo = 2;
  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);
  game.col1.player[0].named_new_world = 1;
  for (int n = 1; n < 4; ++n) {
    game.col1.player[n].control = 1;
  }
  for (uint16_t s = 1; s < 512 && map_tile_has_rumour(&game.world_map, 3, 3); ++s) {
    game.world_map.prime_resource_seed = s;
  }
  game.world_map.terrain[3 * 8 + 2] = 25; /* ocean west of the colony */
  ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.nation_id = 4;
  tribe.x = 4;
  tribe.y = 3;
  game.col1.tribe = &tribe;
  game.col1.head.tribe_count = 1;
  /* Settlement bit and owner nibble for 3180's neighbour read (FUN_137f_03e4). */
  game.world_map.layer2[3 * 8 + 4] |= 0x02u;
  game.world_map.layer3[3 * 8 + 4] = (uint8_t)((game.world_map.layer3[3 * 8 + 4] & 0x0fu) | 0x40u);
  ColonizeColony* port = fx_colony_add(&game.colonies, 0, 3, 3, 1);
  ColonizeUnit* ship = units_get(&game.units, units_spawn(&game.units, 0, 2, 3));
  int rc = 0;
  if (!port || !ship || map_tile_has_rumour(&game.world_map, 3, 3) ||
      !map_tile_is_water(&game.world_map, 2, 3)) {
    rc = fail("ship contact fixture");
  } else {
    port->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
    ship->nation_id = 0;
    ship->moves = 12;
    game.units.selected_id = ship->id;
    (void)game_try_unit_move(&game, 3, 3);
    if (ship->x != 3 || ship->y != 3) {
      rc = fail("ship contact: the ship did not dock");
    } else if ((game.col1.indian[0].euro_diplo[0] & COL1_INDIAN_MET_BIT) == 0) {
      rc = fail("ship contact: a hull in a colony must meet the unmet tribe next door");
    }
  }
  game.col1.tribe = NULL;
  game.col1.head.tribe_count = 0;
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
 * Playtest C18-B1: same with a foreign garrison standing on the colony tile.
 */
static int ship_bumps_foreign_colony_case(bool garrison) {
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
  if (test_game_txt()) { /* textless OKs are dropped (shallow copy, never freed here) */
    game.messages = *test_game_txt();
  }

  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 2;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  game.units.types[0].movement = 4;
  game.units.types[0].cargo = 2;
  game.units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  game.units.types[1].movement = 1;

  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  ColonizeColony* colony = fx_colony_add(&game.colonies, 1, 4, 3, 1);
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);

  if (garrison) {
    ColonizeUnit* g = units_get(&game.units, units_spawn(&game.units, 1, colony->x, colony->y));
    if (g) {
      g->nation_id = 1;
    }
  }
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

static int test_ship_bumps_foreign_colony_trade_arm(void) {
  if (ship_bumps_foreign_colony_case(false) != 0) {
    return 1;
  }
  return ship_bumps_foreign_colony_case(true);
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
    b->home_tribe_index = 0;
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
      f->home_tribe_index = -1;
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
  if (test_game_txt()) { /* textless OKs are dropped (shallow copy, never freed here) */
    game->messages = *test_game_txt();
  }
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
  colony_screen_open_jobs(csv, &game.colonies, &game.world_map, &game.units, col, -1);
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

/* jobs_menu_fixture plus a Master Distiller (@JOB 9 specialty) standing on the fence, selected.
 * Returns the unit id, or -1 on a fixture failure. */
static int fence_jobs_fixture(ColonizeGameState* game, ColonizeMsgCatalog* names) {
  if (!jobs_menu_fixture(game, names)) {
    return -1;
  }
  if (!units_load_types(&game->units, names)) {
    return -1;
  }
  game->units_ok = game->colonies_ok = true;
  const int colonist_t = units_kind_type_index(&game->units, UNITS_KIND_COLONIST);
  ColonizeUnit* u = units_get(&game->units, units_spawn_allow_stack(&game->units, colonist_t, 8, 8));
  if (!u) {
    return -1;
  }
  u->nation_id = 1;
  u->profession = 9; /* @JOB 9, Master Distiller */
  game->colony_screen.selected_colonist = -1;
  game->colony_screen.selected_outside_unit = u->id;
  return u->id;
}

/*
 * A second click (or Enter) on a selected fence unit is FUN_2f2b_44d4 2f2b:4728-4733
 * push 0 -> FUN_2f2b_348c(0), the full jobs list with the leave-as roles after the colony
 * jobs; a field pick admits the unit (FUN_15eb_0fea arm 3) and seats it on the plot 28c8
 * elects, a building pick seats it after the validator, a role pick re-equips it outside,
 * and a full colony (pop 32, 2f2b:349f-34be) gets the leave-as list instead.
 */
static int test_colony_fence_unit_jobs_list(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  int rc = 0;
  /* 1. The list and 2. a field pick. */
  int uid = fence_jobs_fixture(&game, &names);
  if (uid < 0) {
    return fail("fence jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  ColonyScreenView* csv = &game.colony_screen;
  game_colony_open_jobs(&game, col, &game.world_map, -1);
  bool has_farmer = false;
  bool has_role = false;
  for (int i = 0; i < csv->job_count; ++i) {
    has_farmer = has_farmer || csv->job_ids[i] == COLONIZE_JOB_FARMER;
    has_role = has_role || csv->job_ids[i] == UNITS_JOB_COLONIST;
  }
  if (!csv->jobs_open || csv->eject_open || csv->jobs_unit_id != uid || !has_farmer || !has_role) {
    fprintf(stderr, "  jobs_open=%d eject_open=%d unit=%d farmer=%d role=%d\n", csv->jobs_open,
            csv->eject_open, csv->jobs_unit_id, has_farmer, has_role);
    rc = fail("a selected fence unit must open the full jobs list (jobs + roles)");
  } else {
    /* 2f3e's validator (code 0xd, @NODOCKS) runs before 0c36 admits the unit: a Fisherman
     * pick in this dockless colony leaves it outside. */
    game_colony_commit_job(&game, col, COLONIZE_JOB_FISHERMAN);
    if (col->colonist_count != 1 || !units_get_const(&game.units, uid)) {
      rc = fail("a refused field pick (@NODOCKS) must leave the fence unit outside");
    }
    game_colony_open_jobs(&game, col, &game.world_map, -1);
    game_colony_commit_job(&game, col, COLONIZE_JOB_FARMER);
    const ColonizeColonist* c = &col->colonists[1];
    if (col->colonist_count != 2 || units_get_const(&game.units, uid) || col->tiles[3] != 1 ||
        c->field_job != COLONIZE_JOB_FARMER || c->profession != 9 ||
        csv->selected_colonist != 1 || csv->jobs_open) {
      fprintf(stderr, "  pop=%d unit=%p tiles[3]=%d job=%d prof=%d sel=%d\n",
              col->colonist_count, (const void*)units_get_const(&game.units, uid), col->tiles[3],
              c->field_job, c->profession, csv->selected_colonist);
      rc = fail("a fence unit's field pick must admit it onto the plot 28c8 elects");
    }
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);

  /* 3. A building pick, first refused by @MORETHANTHREE, then seated. */
  uid = fence_jobs_fixture(&game, &names);
  if (uid < 0) {
    return fail("fence jobs fixture");
  }
  col = &game.colonies.colonies[0];
  const int shop = colonies_building_row(&game.colonies, COLONY_BUILDING_CARPENTERS_SHOP);
  const int carpenter = colonies_building_occupation(&game.colonies, shop);
  col->colonists[1] = col->colonists[0];
  col->colonists[2] = col->colonists[0];
  col->colonist_count = col->population = 3;
  game_colony_open_jobs(&game, col, &game.world_map, -1);
  game_colony_commit_job(&game, col, carpenter);
  if (col->colonist_count != 3 || !units_get_const(&game.units, uid) ||
      col->colonists[3].active) {
    rc = fail("a refused building pick must leave the fence unit outside");
  }
  col->colonist_count = col->population = 1;
  memset(&col->colonists[1], 0, 2 * sizeof(col->colonists[1]));
  game_colony_open_jobs(&game, col, &game.world_map, -1);
  game_colony_commit_job(&game, col, carpenter);
  if (col->colonist_count != 2 || units_get_const(&game.units, uid) ||
      col->colonists[1].building_type != shop) {
    fprintf(stderr, "  pop=%d building=%d\n", col->colonist_count, col->colonists[1].building_type);
    rc = fail("a fence unit's building pick must admit and seat it");
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);

  /* 4. A role pick re-equips the unit where it stands. */
  uid = fence_jobs_fixture(&game, &names);
  if (uid < 0) {
    return fail("fence jobs fixture");
  }
  col = &game.colonies.colonies[0];
  col->stock[COLONIZE_CARGO_MUSKETS] = 50;
  game_colony_open_jobs(&game, col, &game.world_map, -1);
  game_colony_commit_job(&game, col, UNITS_JOB_COLONIST + COLONIZE_EJECT_SOLDIER);
  const ColonizeUnit* u = units_get_const(&game.units, uid);
  if (col->colonist_count != 1 || !u ||
      u->type_index != units_kind_type_index(&game.units, UNITS_KIND_SOLDIER) ||
      col->stock[COLONIZE_CARGO_MUSKETS] != 0) {
    rc = fail("a fence unit's role pick must arm it outside the colony");
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);

  /* 5. A full colony gets the leave-as list. */
  uid = fence_jobs_fixture(&game, &names);
  if (uid < 0) {
    return fail("fence jobs fixture");
  }
  col = &game.colonies.colonies[0];
  for (int i = 1; i < COLONIZE_COLONY_POP_MAX; ++i) {
    col->colonists[i] = col->colonists[0];
  }
  col->colonist_count = col->population = COLONIZE_COLONY_POP_MAX;
  game_colony_open_jobs(&game, col, &game.world_map, -1);
  if (csv->jobs_open || !csv->eject_open || csv->eject_unit_id != uid) {
    rc = fail("a fence unit in a colony of 32 must get the leave-as list");
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/* The open popup is GAME.TXT @MORETHANTHREE (colonies_emit_more_than_three_chrome). */
static bool popup_is_morethanthree(ColonizeGameState* game) {
  char want[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, "MORETHANTHREE", NULL, "", want, sizeof(want));
  return want[0] != '\0' && game->ai_popups.open &&
         game->ai_popups.current.tag == AI_POPUP_TAG_INFO &&
         strcmp(game->ai_popups.current.body, want) == 0;
}

/*
 * Fence-unit jobs list extras: Clear Specialty on a fence unit (0cae -> 0e8c writes the
 * unit's +0x315b); the own-role row is listed and enabled (348c `local_132 == local_8` -> -2)
 * even where its gate fails; a building drop validates before admitting (2f3e).
 */
static int test_colony_fence_unit_jobs_extras(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  int rc = 0;
  /* 1. Clear Specialty. */
  int uid = fence_jobs_fixture(&game, &names);
  if (uid < 0) {
    return fail("fence jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  game_colony_open_jobs(&game, col, &game.world_map, -1);
  game_colony_commit_job(&game, col, COLONY_JOB_CLEAR_SPECIALTY);
  const AiPopupRequest* cur = &game.ai_popups.current;
  if (!game.ai_popups.open || cur->tag != AI_POPUP_TAG_COLONY_CLEARSPEC || cur->nation_a != -1 ||
      cur->nation_b != uid) {
    rc = fail("Clear Specialty on a fence unit must ask @LOBOTOMIZE for that unit");
  } else {
    game.ai_popups.open = false;
    game.ai_popups.has_result = true;
    game.ai_popups.result_tag = AI_POPUP_TAG_COLONY_CLEARSPEC;
    game.ai_popups.result_cancelled = false;
    game.ai_popups.result_choice_id = 1;
    game.ai_popups.result_nation_a = cur->nation_a;
    game.ai_popups.result_nation_b = cur->nation_b;
    game_apply_ai_popup_result(&game);
    const ColonizeUnit* u = units_get_const(&game.units, uid);
    if (!u || u->profession != COLONIZE_PROF_FREE_COLONIST || col->colonist_count != 1) {
      rc = fail("@LOBOTOMIZE 'yes' must clear the fence unit's specialty");
    }
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);

  /* 2. A blessed Free Colonist (Missionary type) in a churchless colony: the Church gate
   * fails, but the row of its own DS:0x30e job is forced listed and enabled. */
  uid = fence_jobs_fixture(&game, &names);
  if (uid < 0) {
    return fail("fence jobs fixture");
  }
  col = &game.colonies.colonies[0];
  ColonizeUnit* mu = units_get(&game.units, uid);
  mu->type_index = units_kind_type_index(&game.units, UNITS_KIND_MISSIONARY);
  mu->profession = COLONIZE_PROF_FREE_COLONIST;
  const int own = UNITS_JOB_COLONIST + COLONIZE_EJECT_MISSIONARY;
  if (units_type_default_job(mu->type_index) != own) {
    rc = fail("fixture: the Missionary type's DS:0x30e job must be 0x18");
  }
  game_colony_open_jobs(&game, col, &game.world_map, -1);
  const ColonyScreenView* csv = &game.colony_screen;
  int row = -1;
  for (int i = 0; i < csv->job_count; ++i) {
    if (csv->job_ids[i] == own) {
      row = i;
    }
  }
  if (!csv->jobs_open || row < 0 || (csv->jobs_greyed & (1u << row)) != 0) {
    fprintf(stderr, "  open=%d row=%d greyed=0x%x\n", csv->jobs_open, row,
            (unsigned)csv->jobs_greyed);
    rc = fail("a fence unit's own role row must be listed and enabled");
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);

  /* 3. A fence unit dropped on a full Carpenter's Shop stays outside. */
  uid = fence_jobs_fixture(&game, &names);
  if (uid < 0) {
    return fail("fence jobs fixture");
  }
  col = &game.colonies.colonies[0];
  const int shop = colonies_building_row(&game.colonies, COLONY_BUILDING_CARPENTERS_SHOP);
  col->colonists[1] = col->colonists[0];
  col->colonists[2] = col->colonists[0];
  col->colonist_count = col->population = 3;
  game_colony_assign_building_drop(&game, shop);
  if (col->colonist_count != 3 || !units_get_const(&game.units, uid) ||
      !popup_is_morethanthree(&game)) {
    rc = fail("a refused building drop must leave the fence unit outside, with @MORETHANTHREE");
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);

  /* 4. Stockade drop (2f2b:4684-4693): a fence unit whose occupation is a role gets
   * nothing; a colonist still gets the leave-as list. */
  uid = fence_jobs_fixture(&game, &names);
  if (uid < 0) {
    return fail("fence jobs fixture");
  }
  col = &game.colonies.colonies[0];
  game_colony_stockade_drop(&game, col, uid);
  if (game.colony_screen.eject_open) {
    rc = fail("a fence unit dropped on the stockade must open nothing");
  }
  game.colony_screen.selected_outside_unit = -1;
  game.colony_screen.selected_colonist = 0;
  game_colony_stockade_drop(&game, col, -1);
  if (!game.colony_screen.eject_open) {
    rc = fail("a colonist dropped on the stockade must open the leave-as list");
  }
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/*
 * docs/dos_bugs.md DB-15: the validator caps @JOB 9 (Rum Distiller) with `occupation > 8`, so
 * the refusal popup mirror must answer a refused fourth Distiller with @MORETHANTHREE
 * (FUN_2f2b_011e raw 47192), not a silent refusal.
 */
static int test_colony_rum_distiller_refusal_popup(void) {
  static ColonizeGameState game;
  ColonizeMsgCatalog names;
  if (!jobs_menu_fixture(&game, &names)) {
    return fail("jobs fixture");
  }
  ColonizeColony* col = &game.colonies.colonies[0];
  const int shop = colonies_building_row(&game.colonies, COLONY_BUILDING_CARPENTERS_SHOP);
  const int rum = colonies_building_row(&game.colonies, COLONY_BUILDING_RUM_DISTILLERS_HOUSE);
  int rc = 0;
  if (colonies_building_occupation(&game.colonies, rum) != 9) {
    rc = fail("fixture: the Rum Distiller's House must seat @JOB 9");
  }
  col->has_building[rum] = true;
  for (int i = 1; i <= 3; ++i) {
    col->colonists[i] = col->colonists[0];
    col->colonists[i].building_type = rum;
  }
  col->colonist_count = col->population = 4;
  game_colony_assign_building_drop(&game, rum);
  if (col->colonists[0].building_type != shop) {
    rc = fail("the validator must refuse a fourth Rum Distiller");
  } else if (!popup_is_morethanthree(&game)) {
    fprintf(stderr, "  popup open=%d tag=%d\n", game.ai_popups.open,
            (int)game.ai_popups.current.tag);
    rc = fail("a refused fourth Rum Distiller must raise @MORETHANTHREE");
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

/* FUN_2b5a_1b5a raw 42707: ORDERS > Activate ignores a foreign unit under the cursor. */
static int test_activate_ignores_foreign_unit(void) {
  static ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/3, /*with_seen=*/true)) {
    return fail("activate foreign map alloc");
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].movement = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units_set_occupancy_map(&game.world_map);
  const int uid = units_spawn(&game.units, 0, 3, 3);
  ColonizeUnit* u = units_get(&game.units, uid);
  int rc = 0;
  if (!u) {
    rc = fail("activate foreign unit spawn");
  } else {
    u->nation_id = 1;
    u->orders = UNITS_ORDER_FORTIFIED;
    game.units.selected_id = -1;
    game.map_cursor_x = 3;
    game.map_cursor_y = 3;
    game_apply_map_menu_action(&game, MAP_MENU_ACTION_ACTIVATE_UNIT);
    if (u->orders != UNITS_ORDER_FORTIFIED || game.units.selected_id != -1) {
      fprintf(stderr, "  orders %d selected %d\n", u->orders, game.units.selected_id);
      rc = fail("Activate on a foreign unit must not wake or select it");
    }
  }
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/* game_select_tile keeps the despawn step-back (FUN_1427_0824 raw
 * 7834-7836) when the selection just died, and drops it when the player
 * leaves a live selection. */
static int test_select_tile_rotation_retest(void) {
  static ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/3, /*with_seen=*/true)) {
    return fail("select tile retest map alloc");
  }
  game.world_map_ok = true;
  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].movement = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units_set_occupancy_map(&game.world_map);
  const int a = units_spawn(&game.units, 0, 2, 2);
  const int b = units_spawn(&game.units, 0, 3, 3);
  const int c = units_spawn(&game.units, 0, 4, 4);
  int rc = 0;
  game.units.selected_id = b;
  if (!units_despawn(&game.units, b) || game.units.rotation_retest_id != a) {
    rc = fail("despawn of the selection should step back to the previous unit");
  } else {
    game_select_tile(&game, 3, 3);
    if (game.units.rotation_retest_id != a) {
      rc = fail("select tile after a death must keep the step-back");
    } else {
      game.units.selected_id = c;
      game_select_tile(&game, 4, 4);
      if (game.units.rotation_retest_id != -1 || game.units.selected_id != -1) {
        rc = fail("select tile from a live selection must drop the step-back");
      }
    }
  }
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/* View Pieces (FUN_2b5a_0902 raw 42112) leaves 0x5392 on the active unit;
 * the next FUN_2b5a_0e52 re-tests it first (FUN_1427_1410, raw 42291), so
 * Move Pieces returns to it rather than to the first slot. */
static int test_view_pieces_keeps_active_unit(void) {
  static ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/3, /*with_seen=*/true)) {
    return fail("view pieces map alloc");
  }
  game.world_map_ok = true;
  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].movement = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units_set_occupancy_map(&game.world_map);
  int ids[3];
  int rc = 0;
  for (int k = 0; k < 3; ++k) {
    ids[k] = units_spawn(&game.units, 0, 2 + k, 2 + k);
    ColonizeUnit* u = units_get(&game.units, ids[k]);
    if (!u) {
      rc = fail("view pieces spawn");
      break;
    }
    u->nation_id = 0;
    u->moves = 3;
  }
  if (rc == 0) {
    game.units.selected_id = ids[1];
    game.map_cursor_x = 3;
    game.map_cursor_y = 3;
    game_apply_map_menu_action(&game, MAP_MENU_ACTION_VIEW_PIECES);
    if (game.units.selected_id != -1 || !game.view_pieces_mode) {
      rc = fail("View Pieces must drop the selection");
    } else {
      game_apply_map_menu_action(&game, MAP_MENU_ACTION_MOVE_PIECES);
      if (game.units.selected_id != ids[1]) {
        fprintf(stderr, "  selected %d want %d\n", game.units.selected_id, ids[1]);
        rc = fail("Move Pieces after View Pieces must re-test the unit it left");
      }
    }
  }
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/* The other FUN_291f_006c -> FUN_2b5a_0902 paths keep 0x5392 too: the map
 * right-click (FUN_2b5a_3462 raw 46005-46007) and Find Colony, so Move
 * Pieces re-tests the unit they left. */
static int test_right_click_and_find_colony_keep_active_unit(void) {
  static ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/3, /*with_seen=*/true)) {
    return fail("right click map alloc");
  }
  game.world_map_ok = true;
  game.map_menu.open_index = -1;
  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].movement = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units_set_occupancy_map(&game.world_map);
  int ids[3];
  int rc = 0;
  for (int k = 0; k < 3; ++k) {
    ids[k] = units_spawn(&game.units, 0, 2 + k, 2 + k);
    ColonizeUnit* u = units_get(&game.units, ids[k]);
    if (!u) {
      rc = fail("right click spawn");
      break;
    }
    u->nation_id = 0;
    u->moves = 3;
  }
  if (rc == 0) {
    game.units.selected_id = ids[1];
    const int tw = game_map_zoom_tile_px(game.map_zoom);
    ColonizeInputState in;
    memset(&in, 0, sizeof(in));
    in.mouse_right_clicked = true;
    in.mouse_x = 5 * tw + 1;
    in.mouse_y = MAP_VIEW_ORIGIN_Y + 5 * tw + 1;
    (void)game_update_map_menu_bar(&game, &in);
    if (game.units.selected_id != -1 || !game.view_pieces_mode) {
      rc = fail("right-click must enter View Pieces");
    } else {
      game_apply_map_menu_action(&game, MAP_MENU_ACTION_MOVE_PIECES);
      if (game.units.selected_id != ids[1]) {
        rc = fail("Move Pieces after a right-click must re-test the unit it left");
      }
    }
  }
  if (rc == 0) {
    ColonizeColony* c = &game.colonies.colonies[0];
    c->active = true;
    c->x = 5;
    c->y = 5;
    snprintf(c->name, sizeof(c->name), "Fort");
    game.col1.head.curr_nation_map_view = 4;
    game.units.selected_id = ids[2];
    game_find_colony(&game, "fort");
    if (game.units.selected_id != -1 || !game.view_pieces_mode || game.map_cursor_x != 5) {
      rc = fail("Find Colony must enter View Pieces on the colony");
    } else {
      game_apply_map_menu_action(&game, MAP_MENU_ACTION_MOVE_PIECES);
      if (game.units.selected_id != ids[2]) {
        rc = fail("Move Pieces after Find Colony must re-test the unit it left");
      }
    }
  }
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}

/* Edge-scroll fixture: 60x60 land, one land type (1 tile a turn), human
 * France, zoom 0, view centred on (30,30); *ox = the view's left column. No
 * rumour on row 30 columns ox..ox+2. Free with edge_scroll_free. */
static void edge_scroll_init(ColonizeGameState* game, int* ox) {
  memset(game, 0, sizeof(*game));
  (void)fx_map_alloc(&game->world_map, 60, 60, /*terrain_fill=*/2, /*with_seen=*/true);
  game->world_map_ok = true;
  game->human_nation = 0;
  game->col1_ok = true;
  game->col1.player[0].named_new_world = 1;
  ai_popup_clear(&game->ai_popups);
  fx_units_init(&game->units);
  game->units_ok = true;
  game->units.type_count = 1;
  game->units.types[0].movement = 1;
  game->units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  fx_colonies_init(&game->colonies);
  game->colonies_ok = true;
  units_set_occupancy_map(&game->world_map);
  colonies_set_occupancy_map(&game->world_map);
  int cols = 0;
  int rows = 0;
  game_map_zoom_view_size(0, &cols, &rows);
  game->map_zoom = 0;
  game->map_view_x = 30;
  game->map_view_y = 30;
  int oy = 0;
  map_panel_clamp_view_origin(60, 60, 30, 30, cols, rows, ox, &oy);
  for (uint16_t s = 1; s < 512 && (map_tile_has_rumour(&game->world_map, *ox, 30) ||
                                   map_tile_has_rumour(&game->world_map, *ox + 1, 30) ||
                                   map_tile_has_rumour(&game->world_map, *ox + 2, 30));
       ++s) {
    game->world_map.prime_resource_seed = s;
  }
}

static void edge_scroll_free(ColonizeGameState* game) {
  units_set_move_watch(NULL, NULL);
  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game->world_map);
}

/* FUN_112b_0eb6 raw 2627 runs FUN_1984_02fc on the DEPARTURE tile before the
 * slide, and the human step tail does not centre. A unit two columns inside
 * the view's left edge steps west twice: the first step (departure outside the
 * 2-tile margin, arrival inside) must not scroll; the second (departure
 * inside) centres on the departure tile, not the arrival. */
static int test_move_edge_scroll_uses_departure_tile(void) {
  static ColonizeGameState game;
  int ox = 0;
  edge_scroll_init(&game, &ox);
  const int y = 30;
  ColonizeUnit* u = units_get(&game.units, units_spawn(&game.units, 0, ox + 2, y));
  int rc = (ox <= 1 || !u) ? fail("edge scroll: fixture") : 0;
  if (rc == 0) {
    u->nation_id = 0;
    u->moves = 9;
    game.units.selected_id = u->id;
    units_set_move_watch(game_move_watch, &game);
    (void)game_try_unit_move(&game, ox + 1, y);
    if (u->x != ox + 1) {
      rc = fail("edge scroll: first step did not land");
    } else if (game.map_view_x != 30 || game.map_view_y != 30) {
      fprintf(stderr, "  view (%d,%d) after step 1\n", game.map_view_x, game.map_view_y);
      rc = fail("edge scroll: arrival in the margin must not scroll (departure is outside it)");
    }
  }
  if (rc == 0) {
    game.units.selected_id = u->id;
    (void)game_try_unit_move(&game, ox, y);
    if (u->x != ox) {
      rc = fail("edge scroll: second step did not land");
    } else if (game.map_view_x != ox + 1 || game.map_view_y != y) {
      fprintf(stderr, "  view (%d,%d), want (%d,%d)\n", game.map_view_x, game.map_view_y,
              ox + 1, y);
      rc = fail("edge scroll: a departure in the margin centres on the departure tile");
    }
  }
  edge_scroll_free(&game);
  return rc;
}

static int s_board_watch_calls;
static int s_board_watch_to_x;
static bool s_board_watch_aboard;
static int s_board_watch_sprite;

static void board_watch(
  void* user, const ColonizeUnitPool* pool, const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies, int unit_id, int from_x, int from_y, int to_x, int to_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  ++s_board_watch_calls;
  s_board_watch_to_x = to_x;
  s_board_watch_aboard = u && u->aboard_ship_id >= 0 && u->x == to_x && u->y == to_y;
  s_board_watch_sprite = units_map_sprite(pool, unit_id);
  game_move_watch(user, pool, map, colonies, unit_id, from_x, from_y, to_x, to_y);
}

/* A land unit stepping onto its ship is an ordinary FUN_465b_0000 step: the
 * raw 75704 slide (and its departure-tile edge test) runs for the passenger,
 * once, and the passenger is resolvable there (aboard, on the ship's tile).
 * Then raw 75711-75721 activates the ship through FUN_1427_08ea, whose
 * FUN_1984_02fc margin test runs only when FUN_1427_1330 passes (the ship
 * has MP). The ship sits in the margin, the passenger just outside it. A
 * queued popup parks the hand-off, so a spent ship keeps the focus. */
static int test_board_step_watch_and_ship_margin(void) {
  static ColonizeGameState game;
  int rc = 0;
  for (int ship_mp = 12; ship_mp >= 0 && rc == 0; ship_mp -= 12) {
    int ox = 0;
    edge_scroll_init(&game, &ox);
    const int y = 30;
    game.units.type_count = 2;
    game.units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
    game.units.types[1].movement = 4;
    game.units.types[1].cargo = 2;
    game.world_map.terrain[(size_t)y * game.world_map.width + (size_t)(ox + 1)] = 25;
    ColonizeUnit* p = units_get(&game.units, units_spawn(&game.units, 0, ox + 2, y));
    ColonizeUnit* s = p ? units_get(&game.units, units_spawn(&game.units, 1, ox + 1, y)) : NULL;
    if (ox <= 1 || !p || !s || !map_tile_is_water(&game.world_map, ox + 1, y)) {
      rc = fail("board: fixture");
      break;
    }
    p->nation_id = 0;
    p->moves = 3;
    s->nation_id = 0;
    s->moves = ship_mp;
    const int pid = p->id;
    const int sid = s->id;
    game.units.selected_id = pid;
    (void)ai_popup_enqueue_ok(&game.ai_popups, AI_POPUP_TAG_INFO, NULL, "x");
    s_board_watch_calls = 0;
    s_board_watch_aboard = false;
    s_board_watch_sprite = -1;
    units_set_move_watch(board_watch, &game);
    (void)game_try_unit_move(&game, ox + 1, y);
    p = units_get(&game.units, pid);
    /* FUN_465b_0000 raw 75719-75721: the ship becomes active only when
     * FUN_1427_1330 passes (MP left, raw 8741-8762). */
    if (!p || p->aboard_ship_id != sid || (game.units.selected_id == sid) != (ship_mp > 0)) {
      fprintf(stderr, "  ship MP %d: aboard %d, selected %d (ship %d)\n", ship_mp,
              p ? p->aboard_ship_id : -2, game.units.selected_id, sid);
      rc = fail("board: the step must board and hand the focus to a ship with MP left");
    } else if (s_board_watch_calls != 1 || s_board_watch_to_x != ox + 1 ||
               !s_board_watch_aboard || s_board_watch_sprite < 0) {
      fprintf(stderr, "  watch calls %d, to_x %d (want %d), aboard %d, sprite %d\n",
              s_board_watch_calls, s_board_watch_to_x, ox + 1, s_board_watch_aboard,
              s_board_watch_sprite);
      rc = fail("board: the step fires the move watch once, for the resolvable passenger");
    } else {
      const int want_x = ship_mp > 0 ? ox + 1 : 30;
      if (game.map_view_x != want_x || game.map_view_y != y) {
        fprintf(stderr, "  ship MP %d: view (%d,%d), want (%d,%d)\n", ship_mp, game.map_view_x,
                game.map_view_y, want_x, y);
        rc = fail("board: only a ship with MP left takes the margin test");
      }
    }
    ai_popup_clear(&game.ai_popups);
    edge_scroll_free(&game);
  }
  return rc;
}

/* FUN_364b_03f6's fort phantom edge-scrolls on the colony tile (raw 57101),
 * the lunge's origin, not on the defender. The colony tile sits in the
 * margin and the defender just outside it; an own unit on the defender's
 * tile keeps both in live sight. */
static int test_phantom_lunge_edge_scroll_uses_colony_tile(void) {
  static ColonizeGameState game;
  int ox = 0;
  edge_scroll_init(&game, &ox);
  const int y = 30;
  ColonizeUnit* u = units_get(&game.units, units_spawn(&game.units, 0, ox + 2, y));
  int rc = (ox <= 1 || !u) ? fail("phantom lunge: fixture") : 0;
  if (rc == 0) {
    u->nation_id = 0;
    game_phantom_lunge(&game, &game.units, 0, 1, ox + 1, y, ox + 2, y);
    if (game.map_view_x != ox + 1 || game.map_view_y != y) {
      fprintf(stderr, "  view (%d,%d), want (%d,%d)\n", game.map_view_x, game.map_view_y,
              ox + 1, y);
      rc = fail("phantom lunge: the colony tile in the margin centres the view on it");
    }
  }
  edge_scroll_free(&game);
  return rc;
}

/* A unit the player picks keeps the view when it is visible away from the
 * edges: FUN_2b5a_1b5a -> FUN_281f_086c = FUN_1427_08ea runs only the
 * FUN_1984_02fc margin test, so an off-screen pick still centres. */
static int test_player_pick_keeps_view(void) {
  static ColonizeGameState game;
  int ox = 0;
  edge_scroll_init(&game, &ox);
  ColonizeUnit* a = units_get(&game.units, units_spawn(&game.units, 0, 32, 31));
  ColonizeUnit* b = a ? units_get(&game.units, units_spawn(&game.units, 0, 50, 50)) : NULL;
  int rc = (!a || !b) ? fail("player pick: fixture") : 0;
  if (rc == 0) {
    a->nation_id = 0;
    a->moves = 3;
    b->nation_id = 0;
    b->moves = 3;
    game_click_activate_unit(&game, a->id);
    if (game.units.selected_id != a->id) {
      rc = fail("player pick: click must select the unit");
    } else if (game.map_view_x != 30 || game.map_view_y != 30) {
      fprintf(stderr, "  view (%d,%d) after the click\n", game.map_view_x, game.map_view_y);
      rc = fail("player pick: a visible interior unit must not scroll the view");
    }
  }
  if (rc == 0) {
    game_click_activate_unit(&game, b->id);
    if (game.map_view_x != 50 || game.map_view_y != 50) {
      fprintf(stderr, "  view (%d,%d), want (50,50)\n", game.map_view_x, game.map_view_y);
      rc = fail("player pick: an off-screen unit is past every margin and centres");
    }
  }
  edge_scroll_free(&game);
  return rc;
}

/* port_bugs.md #425 speeds up only the sale lines; the colony screen's cargo
 * lines are armed with the same gold kind 1 (FUN_281f_0dc2(1, 0x78, 0), raw
 * 77421 / 77503) and keep DOS's dwell. */
static int test_bar_sale_speedup_only_on_sales(void) {
  static AiPopupState st;
  ai_popup_clear(&st);
  (void)ai_popup_enqueue_bar_message(&st, "Loading 100 Furs onto Caravel");
  (void)ai_popup_enqueue_bar_sale(&st, "100 Furs sold for 300");
  (void)ai_popup_enqueue_bar_sale(&st, "50 Sugar sold for 150");
  int rc = 0;
  (void)ai_popup_bar_service(&st, 1000, false);
  if (st.bar_msg_until_ms != 1000 + AI_POPUP_BAR_MSG_MS) {
    fprintf(stderr, "  cargo line held %u ms\n", (unsigned)(st.bar_msg_until_ms - 1000));
    rc = fail("bar sale: a colony cargo line keeps DOS's dwell");
  }
  (void)ai_popup_bar_service(&st, 1000, true);
  (void)ai_popup_bar_service(&st, 2000, false);
  if (rc == 0 && st.bar_msg_until_ms != 2000 + AI_POPUP_BAR_MSG_MS / AI_POPUP_BAR_SALE_SPEEDUP) {
    fprintf(stderr, "  sale line held %u ms\n", (unsigned)(st.bar_msg_until_ms - 2000));
    rc = fail("bar sale: a sale line with more behind it is sped up");
  }
  (void)ai_popup_bar_service(&st, 2000, true);
  (void)ai_popup_bar_service(&st, 3000, false);
  if (rc == 0 &&
      st.bar_msg_until_ms != 3000 + AI_POPUP_BAR_MSG_LAST_MS / AI_POPUP_BAR_SALE_SPEEDUP) {
    fprintf(stderr, "  last sale line held %u ms\n", (unsigned)(st.bar_msg_until_ms - 3000));
    rc = fail("bar sale: the last sale line of a run is sped up");
  }
  ai_popup_clear(&st);
  return rc;
}

static int s_near_human = -1;

static void near_human_watch(
  void* user, const ColonizeUnitPool* pool, const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies, int unit_id, int from_x, int from_y, int to_x, int to_y
) {
  (void)colonies;
  (void)from_x;
  (void)from_y;
  s_near_human = game_move_is_near_human(
    (const ColonizeGameState*)user, units_get_const(pool, unit_id), map, to_x, to_y
  );
}

/* FUN_465b_0000 raw 75662-75668: the foreign-move show test reads the mover's
 * own vis byte after FUN_281f_07fe (= FUN_1427_0c72, an OR) folded the
 * destination's FUN_281f_0826 mask in. An English unit stepping onto a tile no
 * French unit watches is shown only when France already saw it. */
static int test_foreign_move_shown_by_prior_sighting(void) {
  static ColonizeGameState game;
  int ox = 0;
  edge_scroll_init(&game, &ox);
  const int y = 30;
  ColonizeWorldMap* m = &game.world_map;
  for (int ty = 0; ty < m->height; ++ty) {
    for (int tx = 0; tx < m->width; ++tx) {
      map_set_owner_nibble(m, tx, ty, 0x0f); /* no unit anywhere */
    }
  }
  ColonizeUnit* e = units_get(&game.units, units_spawn(&game.units, 0, ox + 2, y));
  int rc = (!e || !m->layer3) ? fail("foreign show: fixture") : 0;
  if (rc == 0) {
    e->nation_id = 1;
    map_set_owner_nibble(m, ox + 2, y, 1);
    if ((units_vis_mask_for_tile(m, ox + 2, y, 1) & 0x01u) != 0) {
      rc = fail("foreign show: destination must be unwatched by France");
    }
  }
  units_set_move_watch(near_human_watch, &game);
  if (rc == 0) {
    e->col1_vis_mask = 0;
    s_near_human = -1;
    units_move_watch_notify(&game.units, m, &game.colonies, e->id, ox + 1, y);
    if (s_near_human != 0) {
      fprintf(stderr, "  unseen mover: near_human %d\n", s_near_human);
      rc = fail("foreign show: a unit France never saw must not be shown");
    }
  }
  if (rc == 0) {
    e->col1_vis_mask = 0x01u;
    s_near_human = -1;
    units_move_watch_notify(&game.units, m, &game.colonies, e->id, ox + 1, y);
    if (s_near_human != 1) {
      fprintf(stderr, "  seen mover: near_human %d\n", s_near_human);
      rc = fail("foreign show: a unit France already saw stays shown");
    }
  }
  /* The same step through units_try_move_w: the entry byte is captured
   * before the commit recomputes it, and is gone once the watch returns. */
  if (rc == 0) {
    e->col1_vis_mask = 0x01u;
    e->moves = 9;
    s_near_human = -1;
    const ColonizeWorld w =
      world_make(&game.units, &game.colonies, m, &game.col1, true, NULL, NULL);
    (void)units_try_move_w(&w, e->id, ox + 3, y);
    if (e->x != ox + 3) {
      rc = fail("foreign show: the step did not land");
    } else if (s_near_human != 1) {
      fprintf(stderr, "  stepped seen mover: near_human %d\n", s_near_human);
      rc = fail("foreign show: units_try_move_w shows a unit France already saw");
    } else if (units_move_watch_vis_before() != 0) {
      rc = fail("foreign show: the entry byte must not outlive the watch");
    }
  }
  edge_scroll_free(&game);
  return rc;
}

static int s_brave_watch_calls;
static uint8_t s_brave_watch_vis;

static void brave_vis_watch(
  void* user, const ColonizeUnitPool* pool, const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies, int unit_id, int from_x, int from_y, int to_x, int to_y
) {
  (void)user;
  (void)pool;
  (void)map;
  (void)colonies;
  (void)unit_id;
  (void)from_x;
  (void)from_y;
  (void)to_x;
  (void)to_y;
  ++s_brave_watch_calls;
  s_brave_watch_vis = units_move_watch_vis_before();
}

/* The native step commits inline (ai_native_brave_step), so it hands the
 * watch the Brave's vis byte from before units_vis_mask_after_move, the
 * raw 75665 read. On an open map no French unit watches the destination. */
static int test_brave_step_watch_sees_entry_vis(void) {
  int rc = 0;
  int saw_move = 0;
  for (int k = 0; k < 24 && rc == 0 && !saw_move; ++k) {
    ColonizeWorldMap map;
    if (!fx_map_alloc(&map, 12, 12, /*terrain_fill=*/0, /*with_seen=*/true)) {
      return fail("brave vis: map alloc");
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.layer3[i] = 0xf0;
    }
    ColonizeUnitPool units;
    fx_units_init(&units);
    units.type_count = UNITS_KIND_MTD_BRAVE + 2;
    for (int t = 0; t < units.type_count; ++t) {
      units.types[t].movement = 1;
      units.types[t].domain = COLONIZE_UNIT_DOMAIN_LAND;
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
    ColonizeUnit* b = &units.units[0];
    b->id = 1;
    b->active = true;
    b->nation_id = 4;
    b->type_index = UNITS_KIND_BRAVE;
    b->x = 5;
    b->y = 5;
    b->aboard_ship_id = -1;
    b->home_tribe_index = 0;
    b->last_dir = 8;
    b->col1_vis_mask = 0x01u;
    units.next_id = 2;
    units_pool_sync(&units);
    ColonizeDosRng rng;
    dos_rng_seed(&rng, (uint32_t)(k * 12345 + 7));
    s_brave_watch_calls = 0;
    s_brave_watch_vis = 0;
    units_set_move_watch(brave_vis_watch, NULL);
    int steps = 0;
    (void)ai_native_brave_step(
      &units, &map, &col1, &rng, /*nation_id=*/4, /*seed100_init_burns=*/false, b,
      /*max_mp=*/3, /*brave_index=*/0, &steps
    );
    units_set_move_watch(NULL, NULL);
    if (steps > 0 && (b->x != 5 || b->y != 5)) {
      saw_move = 1;
      if (s_brave_watch_calls != 1 || s_brave_watch_vis != 0x01u) {
        fprintf(stderr, "  seed %d: watch calls %d, vis_before 0x%02x (want 0x01)\n", k,
                s_brave_watch_calls, (unsigned)s_brave_watch_vis);
        rc = fail("brave vis: the watch reads the Brave's vis byte from before the step");
      } else if (units_move_watch_vis_before() != 0) {
        rc = fail("brave vis: the entry byte must not outlive the watch");
      }
    }
    fx_map_free(&map);
  }
  if (rc == 0 && !saw_move) {
    return fail("brave vis: no seed made the Brave step — test proved nothing");
  }
  return rc;
}

/* FUN_5fef_1b0e's lunge is the FUN_112b_0eb6 slide from the attacker's tile
 * (raw 100566-100610), so the view edge-scrolls on the attacker's tile and
 * never centres on the defender. */
static int test_combat_edge_scroll_uses_attacker_tile(void) {
  static ColonizeGameState game;
  int ox = 0;
  edge_scroll_init(&game, &ox);
  const int y = 30;
  ColonizeUnit* a = units_get(&game.units, units_spawn(&game.units, 0, ox + 2, y));
  int rc = (ox <= 1 || !a) ? fail("combat scroll: fixture") : 0;
  if (rc == 0) {
    a->nation_id = 0;
    game_combat_watch(&game, &game.units, a->id, ox + 1, y, true);
    if (game.map_view_x != 30 || game.map_view_y != 30) {
      fprintf(stderr, "  view (%d,%d) after the first fight\n", game.map_view_x, game.map_view_y);
      rc = fail("combat scroll: a defender in the margin must not scroll (attacker is outside it)");
    }
  }
  if (rc == 0) {
    a->x = ox + 1;
    game_combat_watch(&game, &game.units, a->id, ox, y, true);
    if (game.map_view_x != ox + 1 || game.map_view_y != y) {
      fprintf(stderr, "  view (%d,%d), want (%d,%d)\n", game.map_view_x, game.map_view_y,
              ox + 1, y);
      rc = fail("combat scroll: an attacker in the margin centres on the attacker's tile");
    }
  }
  edge_scroll_free(&game);
  return rc;
}

/* FUN_5fef_1b0e lunges on its param_4, which FUN_465b_0000 passes as local_1c
 * (raw 75660-75692): a rival-vs-rival fight on ground France has explored but
 * nobody French watches is not shown; the same fight by an attacker France
 * already saw is. */
static int test_combat_lunge_reads_465b_local_1c(void) {
  static ColonizeGameState game;
  int ox = 0;
  edge_scroll_init(&game, &ox);
  const int y = 30;
  ColonizeWorldMap* m = &game.world_map;
  game.col1.player[1].control = 1;
  game.col1.player[2].control = 1;
  ColonizeUnit* a = units_get(&game.units, units_spawn(&game.units, 0, ox + 1, y));
  ColonizeUnit* d = units_get(&game.units, units_spawn(&game.units, 0, ox, y));
  int rc = (ox <= 1 || !a || !d || !m->layer3) ? fail("lunge show: fixture") : 0;
  if (rc == 0) {
    for (int ty = 0; ty < m->height; ++ty) {
      for (int tx = 0; tx < m->width; ++tx) {
        map_set_owner_nibble(m, tx, ty, 0x0f); /* nobody French anywhere */
      }
    }
    map_set_owner_nibble(m, ox + 1, y, 1);
    map_set_owner_nibble(m, ox, y, 2);
    a->nation_id = 1;
    d->nation_id = 2;
    a->col1_vis_mask = 0;
    if ((units_vis_mask_for_tile(m, ox, y, 1) & 0x01u) != 0) {
      rc = fail("lunge show: defender tile must be unwatched by France");
    }
  }
  if (rc == 0) {
    map_reveal_tile(m, ox + 1, y, 0);
    map_reveal_tile(m, ox, y, 0);
    game_combat_watch(&game, &game.units, a->id, ox, y, true);
    if (game.map_view_x != 30 || game.map_view_y != 30) {
      rc = fail("lunge show: an unwatched rival fight must not play (explored is not enough)");
    }
  }
  if (rc == 0) {
    a->col1_vis_mask = 0x01u;
    game_combat_watch(&game, &game.units, a->id, ox, y, true);
    if (game.map_view_x != ox + 1 || game.map_view_y != y) {
      fprintf(stderr, "  view (%d,%d), want (%d,%d)\n", game.map_view_x, game.map_view_y,
              ox + 1, y);
      rc = fail("lunge show: an attacker carrying France's bit plays the lunge");
    }
  }
  /* The other local_1c arms, each from the unshown state: show_entire_map
   * (DS:0x53a2), a human-played attacker, a human-played destination head
   * (raw 75687-75689). */
  for (int arm = 0; arm < 3 && rc == 0; ++arm) {
    a->col1_vis_mask = 0;
    game.map_view_x = 30;
    game.map_view_y = 30;
    game.col1.head.show_entire_map = arm == 0;
    game.col1.player[1].control = arm == 1 ? 0 : 1;
    game.col1.player[2].control = arm == 2 ? 0 : 1;
    game_combat_watch(&game, &game.units, a->id, ox, y, true);
    if (game.map_view_x != ox + 1 || game.map_view_y != y) {
      fprintf(stderr, "  arm %d: view (%d,%d)\n", arm, game.map_view_x, game.map_view_y);
      rc = fail("lunge show: show_entire_map, a human-played attacker or head plays the lunge");
    }
  }
  edge_scroll_free(&game);
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
  /* Tab, FUN_38fd_4f6e raw 66016-66038: step DS:0xf9a, skipping empty panels. */
  eu->harbor_ships = 0;
  eu->dock_count = 1;
  eu->focus = 0;
  in.last_key = COLONIZE_KEY_TAB;
  (void)game_update_europe_screen(&game, &in);
  if (rc == 0 && eu->focus != 2) {
    rc = fail("Europe Tab skips the harbor with no ship in port");
  }
  (void)game_update_europe_screen(&game, &in);
  if (rc == 0 && eu->focus != 0) {
    rc = fail("Europe Tab wraps from the dock to the market");
  }
  eu->harbor_ships = 1;
  eu->dock_count = 0;
  eu->focus = 1;
  (void)game_update_europe_screen(&game, &in);
  if (rc == 0 && eu->focus != 0) {
    rc = fail("Europe Tab skips the empty dock and wraps to the market");
  }
  (void)game_update_europe_screen(&game, &in);
  if (rc == 0 && eu->focus != 1) {
    rc = fail("Europe Tab steps from the market to the harbor");
  }
  eu->harbor_ships = 0;
#undef F1_COLONY
#undef PEDIA_BACK
  game.in_europe = false;
  fx_map_free(&game.world_map);
  assets_msg_free(&names);
  return rc;
}

/*
 * Every FUN_291f_0dd4 repaint ends in FUN_38fd_081c (raw 59178-59179), which zeroes the
 * harbor selection DS:0x9e20/0x9e1c: the open (menu and both arrival auto-opens, raw 68140),
 * the Sail answer Yes or No (FUN_38fd_2a92 raw 60781), the box drag, a purchase
 * (FUN_38fd_4b50 raw 64897) and the return from a Europe article (FUN_38fd_1956).
 */
static int test_europe_repaints_reset_harbor_selection(void) {
  static ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  game.europe_ok = true;
  game.found_open_colony_id = -1;
  game.ff_pedia_after_report = -1;
  game.pedia_return_colony_id = -1;
  ai_popup_init(&game.ai_popups);
  EuropeScreen* eu = &game.europe;
  eu->harbor_ships = 3;
  eu->selected_harbor = 2;
  int rc = 0;
  if (!game_try_enter_europe(&game) || eu->selected_harbor != 0) {
    rc = fail("Europe open selects harbor slot 0");
  }
  game.in_europe = false;
  eu->harbor_ships = 0;
  eu->selected_harbor = 1;
  if (rc == 0 && (!game_try_enter_europe(&game) || eu->selected_harbor != -1)) {
    rc = fail("Europe open with an empty harbor selects nothing");
  }
  game.in_europe = false;
  eu->harbor_ships = 3;
  eu->selected_harbor = 2;
  eu->open_on_dock = true;
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  (void)game_update_services(&game, &in, 16);
  if (rc == 0 && (!game.in_europe || eu->selected_harbor != 0)) {
    rc = fail("deferred arrival open selects harbor slot 0");
  }
  game.in_europe = false;
  eu->selected_harbor = 2;
  ColonizeTurnResult res;
  memset(&res, 0, sizeof(res));
  res.request_europe_open = true;
  game_finish_end_turn(&game, &res);
  if (rc == 0 && (!game.in_europe || eu->selected_harbor != 0)) {
    rc = fail("end-of-turn arrival open selects harbor slot 0");
  }
  game.in_europe = false;
  eu->selected_harbor = 2;
  game.in_pedia = true;
  game.pedia_view = PEDIA_VIEW_ARTICLE;
  game.pedia_return_to_list = false;
  game.pedia_return_colony_id = -1;
  game.pedia_return_europe = true;
  in.last_key = COLONIZE_KEY_ESCAPE;
  (void)game_update_pedia_screen(&game, &in);
  if (rc == 0 && (game.in_pedia || !game.in_europe || eu->selected_harbor != 0)) {
    rc = fail("leaving a Europe article selects harbor slot 0");
  }
  eu->selected_harbor = 2;
  eu->expected_ships = 1;
  eu->expected[0].turns_left = 3;
  ui_drag_begin(&game.ui_drag, UI_DRAG_EUROPE_EXPECTED_SHIP, 0, -1, 0);
  (void)game_europe_drag_drop(&game, EUROPE_BOUND_X + 4, EUROPE_BOUND_Y + 4, false);
  if (rc == 0 && (eu->bound_ships != 1 || eu->selected_harbor != 0)) {
    rc = fail("box drag selects harbor slot 0");
  }
  eu->selected_harbor = 2;
  game.map_confirm = GAME_MAP_CONFIRM_EUROPE_SAIL;
  game.map_confirm_payload = 2;
  game.ai_popups.has_result = true;
  game.ai_popups.result_tag = AI_POPUP_TAG_MAP_CONFIRM;
  game.ai_popups.result_cancelled = false;
  game.ai_popups.result_choice_id = 0;
  game_apply_ai_popup_result(&game);
  if (rc == 0 && (eu->harbor_ships != 3 || eu->selected_harbor != 0)) {
    rc = fail("declined sail selects harbor slot 0");
  }
  eu->selected_harbor = 2;
  eu->purchase_count = 1;
  eu->purchase[0].is_ship = true;
  if (rc == 0 && (!europe_purchase_commit(eu, 0, 0, NULL) || eu->harbor_ships != 4 ||
                  eu->selected_harbor != 0)) {
    rc = fail("purchase selects harbor slot 0");
  }
  game.in_europe = false;
  return rc;
}

/*
 * Only the crossing stamps the nation's return tile: FUN_48d3_007a raw 77602-77607 copies the
 * crossing ship's tile into nation +0x32/+0x33. Queuing an Expected hull (a save import) does
 * not. Fixture: turns_ship_europe_roundtrip's start, French Caravel on the high seas at (50,29).
 */
static int test_crossing_stamps_nation_return_tile(void) {
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
        game, "tests-automated-regression/turns_ship_europe_roundtrip/input.sav", err,
        sizeof(err)
      )) {
    fprintf(stderr, "  %s\n", err);
    game_destroy(game);
    return fail("fixture save load");
  }
  EuropeScreen* eu = &game->europe;
  eu->last_exit_x = 1;
  eu->last_exit_y = 1;
  eu->last_exit_valid = true;
  const int before = eu->expected_ships;
  if (!europe_enqueue_expected(eu, 0, "", NULL, NULL, 0, NULL, NULL, 10, 20, false, 2) ||
      eu->last_exit_x != 1 || eu->last_exit_y != 1) {
    fprintf(stderr, "  enqueue: return tile (%d,%d)\n", eu->last_exit_x, eu->last_exit_y);
    rc = fail("queuing an Expected hull leaves the nation return tile");
  }
  eu->expected_ships = before;
  int sid = -1;
  for (int i = 0; i < units_slot_end(&game->units); ++i) {
    const ColonizeUnit* u = &game->units.units[i];
    if (u->active && u->x == 50 && u->y == 29 && units_is_sea(&game->units, u->id)) {
      sid = u->id;
    }
  }
  if (sid < 0 || !game_ship_sail_to_europe(game, sid) || eu->last_exit_x != 50 ||
      eu->last_exit_y != 29) {
    fprintf(stderr, "  sail %d: return tile (%d,%d)\n", sid, eu->last_exit_x, eu->last_exit_y);
    rc = fail("the crossing stamps the ship's tile as the nation return tile");
  }
  game_destroy(game);
  return rc;
}

/* A new campaign drops the unit hand-off and colony open the previous game
 * left parked, as game_apply_col1_save does. */
static int test_new_campaign_drops_parked_flow(void) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug"};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
    if (game) {
      game_destroy(game);
    }
    return TR_SKIP;
  }
  game->turn_flow_deferred = true;
  game->found_open_colony_id = 3;
  /* An end of turn parked when a lost war's retire chain reached the title. */
  game->turn_proc.step = TURN_PROC_FINISH;
  game->turn_proc.day_advanced = true;
  game->new_game.generate_map = false;
  snprintf(game->new_game.map_file, sizeof(game->new_game.map_file), "%s", "AMER2.MP");
  game_commit_new_campaign(game);
  int rc = 0;
  if (game->turn_flow_deferred || game->found_open_colony_id != -1) {
    rc = fail("new campaign must drop the parked turn flow");
  }
  if (turn_processor_active(&game->turn_proc) || game->turn_proc.day_advanced) {
    rc = fail("new campaign must drop the parked end of turn");
  }
  game_destroy(game);
  return rc;
}

/* An Americas (AMER2) start with seed 100 runs the FUN_684c_08c0 band pick on
 * the loaded map. DOS seed-100 save original_saves/golden_idle_america_seed-100
 * /000001.SAV: 0x190 = 365, England +0x32/+0x33 (47,28), idle human Spain
 * (37,14). */
static int test_new_campaign_amer2_seed100_landfall(void) {
  ColonizeGameConfig cfg = {
    .data_dir = "./COLONIZE", .save_dir = "./build/debug", .rng_seed = 100, .rng_seed_set = true
  };
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
    if (game) {
      game_destroy(game);
    }
    return TR_SKIP;
  }
  game->new_game.path = NEW_GAME_PATH_AMERICA;
  game->new_game.generate_map = false;
  game->new_game.nation = 2;
  game->new_game.difficulty = 4;
  snprintf(game->new_game.map_file, sizeof(game->new_game.map_file), "%s", "AMER2.MP");
  game_commit_new_campaign(game);
  int rc = 0;
  const ColonizeUnit* ship = units_get_const(&game->units, game->units.selected_id);
  if (!game->col1_ok || game->world_map.prime_resource_seed != 365 ||
      game->col1.nation[0].return_from_europe_x != 47 ||
      game->col1.nation[0].return_from_europe_y != 28 ||
      game->col1.nation[2].return_from_europe_x != 37 ||
      game->col1.nation[2].return_from_europe_y != 14 || !ship || ship->x != 37 || ship->y != 14) {
    fprintf(stderr, "  prime %u England (%u,%u) Spain (%u,%u) ship (%d,%d)\n",
            game->world_map.prime_resource_seed, game->col1.nation[0].return_from_europe_x,
            game->col1.nation[0].return_from_europe_y, game->col1.nation[2].return_from_europe_x,
            game->col1.nation[2].return_from_europe_y, ship ? ship->x : -1, ship ? ship->y : -1);
    rc = fail("AMER2 seed-100 landfall through game_commit_new_campaign");
  }
  game_destroy(game);
  return rc;
}

/*
 * Brewster's @RECRUITCHOOSE %STRING0 is the nation's home port: FUN_38fd_4884 raw 64708
 * (param_2 != 0) passes DS:(nation*2 - 0x7c74), the NAMES @HOMEPORT row, to FUN_281f_0438.
 */
static int test_brewster_pick_names_home_port(void) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug"};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
    if (game) {
      game_destroy(game);
    }
    return TR_SKIP;
  }
  int rc = 0;
  const char* port = reports_home_port_name(0);
  ai_popup_clear(&game->ai_popups);
  units_brewster_enqueue_pick(&game->europe, &game->ai_popups, &game->messages, 0);
  if (game->ai_popups.queue_count != 1 || !port[0] ||
      !strstr(game->ai_popups.queue[0].body, port)) {
    fprintf(stderr, "  queue %d port '%s' body '%s'\n", game->ai_popups.queue_count, port,
            game->ai_popups.queue_count > 0 ? game->ai_popups.queue[0].body : "");
    rc = fail("@RECRUITCHOOSE %STRING0 names the home port");
  }
  ai_popup_clear(&game->ai_popups);
  game_destroy(game);
  return rc;
}

/*
 * @HOWMUCH4 STRING1 is the hull's @UNIT type name: FUN_38fd_1fa2 raw 60414-60415 reads
 * `type * 0xe + 0x5230` off the ship's +0x3146, not a per-ship name.
 */
static int test_europe_buy_prompt_names_hull_type(void) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug"};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
    if (game) {
      game_destroy(game);
    }
    return TR_SKIP;
  }
  int rc = 0;
  EuropeScreen* eu = &game->europe;
  const int caravel = units_kind_type_index(&game->units, UNITS_KIND_CARAVEL);
  const ColonizeUnitType* ct = units_type(&game->units, caravel);
  eu->harbor_ships = 0;
  if (!ct || !europe_harbor_push_ex(eu, caravel, "Shipname", NULL, NULL, 0, NULL, NULL)) {
    rc = fail("fixture: Caravel in harbor");
  } else {
    game_europe_open_buy_prompt_for(game, eu->harbor_ships - 1, 0);
    if (!game->howmuch.open || !strstr(game->howmuch.prompt, ct->name) ||
        strstr(game->howmuch.prompt, "Shipname")) {
      fprintf(stderr, "  open %d prompt '%s' want '%s'\n", game->howmuch.open,
              game->howmuch.prompt, ct->name);
      rc = fail("@HOWMUCH4 names the hull's @UNIT type, not the ship's name");
    }
  }
  game_destroy(game);
  return rc;
}

/*
 * A press on a hull under repair selects it but never picks it up: FUN_38fd_2edc raw 61023
 * skips the FUN_38fd_1c64 drag for a damaged hull, so no hull icon rides the cursor. A
 * seaworthy hull's press does pick its icon up.
 */
static int test_europe_repairing_hull_press_no_pickup(void) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug"};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
    if (game) {
      game_destroy(game);
    }
    return TR_SKIP;
  }
  int rc = 0;
  EuropeScreen* eu = &game->europe;
  const int caravel = units_kind_type_index(&game->units, UNITS_KIND_CARAVEL);
  eu->harbor_ships = 0;
  if (!europe_harbor_push_ex(eu, caravel, "", NULL, NULL, 0, NULL, NULL)) {
    rc = fail("fixture: Caravel in harbor");
  }
  int px = -1;
  int py = -1;
  for (int y = 0; rc == 0 && y < 200 && px < 0; ++y) {
    for (int x = 0; x < 320; ++x) {
      const EuropeHitResult h = game_europe_hit(game, x, y);
      if (h.kind == EUROPE_HIT_HARBOR_SHIP && h.index == 0) {
        px = x;
        py = y;
        break;
      }
    }
  }
  if (rc == 0 && px < 0) {
    rc = fail("fixture: harbor hull hit spot");
  }
  game->in_europe = true;
  for (int pass = 0; rc == 0 && pass < 2; ++pass) {
    const bool damaged = pass == 1;
    eu->harbor[0].damaged = damaged;
    eu->selected_harbor = -1;
    game_ui_drag_clear(game);
    ColonizeInputState in;
    memset(&in, 0, sizeof(in));
    in.mouse_x = px;
    in.mouse_y = py;
    in.mouse_left_clicked = true;
    in.mouse_left_down = true;
    (void)game_update_europe_screen(game, &in);
    if (eu->selected_harbor != 0 || game->ui_drag.cursor_ok == damaged) {
      fprintf(stderr, "  damaged %d: selected %d cursor %d\n", damaged, eu->selected_harbor,
              game->ui_drag.cursor_ok);
      rc = fail(damaged ? "a press on a hull under repair picks up no icon"
                        : "a press on a seaworthy hull picks up its icon");
    }
  }
  game_ui_drag_clear(game);
  game->in_europe = false;
  game_destroy(game);
  return rc;
}

/*
 * FUN_38fd_3c86 raw 64101-64120: over a dock unit the press (DS:0x7ec) selects it and puts its
 * caption on the status line (thunk_FUN_291f_0daa = FUN_38fd_3694); the release (DS:0x7f4)
 * over the dock opens the @ARMOPTIONS menu for the left button (thunk_FUN_291f_0e28) and the
 * unit article for the right one (FUN_291f_0942). A release whose press the screen never saw
 * opens nothing.
 */
static int test_europe_dock_menu_on_release(void) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug"};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
    if (game) {
      game_destroy(game);
    }
    return TR_SKIP;
  }
  int rc = 0;
  EuropeScreen* eu = &game->europe;
  eu->harbor_ships = 0;
  eu->dock_count = 1;
  memset(&eu->dock[0], 0, sizeof(eu->dock[0]));
  eu->dock[0].present = true;
  eu->dock[0].profession = UNITS_JOB_NONE;
  int px = -1;
  int py = -1;
  for (int y = 0; y < 200 && px < 0; ++y) {
    for (int x = 0; x < 320; ++x) {
      const EuropeHitResult h = game_europe_hit(game, x, y);
      if (h.kind == EUROPE_HIT_DOCK && h.index == 0) {
        px = x;
        py = y;
        break;
      }
    }
  }
  if (px < 0) {
    rc = fail("fixture: dock slot hit spot");
  }
  game->in_europe = true;
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.mouse_x = px;
  in.mouse_y = py;
  if (rc == 0) {
    eu->status[0] = '\0';
    in.mouse_left_clicked = true;
    in.mouse_left_down = true;
    (void)game_update_europe_screen(game, &in);
    if (eu->menu != EUROPE_MENU_NONE || !eu->status[0] || eu->focus != 2) {
      fprintf(stderr, "  press: menu %d status '%s' focus %d\n", (int)eu->menu, eu->status,
              eu->focus);
      rc = fail("a left press on a dock unit shows its caption, no menu yet");
    }
  }
  if (rc == 0) {
    in.mouse_left_clicked = false;
    in.mouse_left_down = false;
    in.mouse_left_released = true;
    (void)game_update_europe_screen(game, &in);
    if (eu->menu != EUROPE_MENU_DOCK || eu->menu_dock_index != 0) {
      fprintf(stderr, "  release: menu %d\n", (int)eu->menu);
      rc = fail("a left release over the dock opens the dock menu");
    }
    europe_menu_close(eu);
  }
  if (rc == 0) {
    in.mouse_left_released = false;
    in.mouse_right_clicked = true;
    in.mouse_right_down = true;
    (void)game_update_europe_screen(game, &in);
    in.mouse_right_clicked = false;
    in.mouse_right_down = false;
    in.mouse_right_released = true;
    (void)game_update_europe_screen(game, &in);
    if (!game->in_pedia || !game->pedia_return_europe || eu->menu != EUROPE_MENU_NONE) {
      fprintf(stderr, "  right: pedia %d return %d menu %d\n", game->in_pedia,
              game->pedia_return_europe, (int)eu->menu);
      rc = fail("a right release over the dock opens the unit article");
    }
    game->in_pedia = false;
    game->pedia_return_europe = false;
    game->in_europe = true;
  }
  if (rc == 0) {
    in.mouse_right_released = false;
    in.mouse_left_released = true;
    (void)game_update_europe_screen(game, &in);
    if (eu->menu != EUROPE_MENU_NONE) {
      rc = fail("a release with no press on the screen opens nothing");
    }
  }
  if (rc == 0) {
    /* Pressed on the dock, released over the market: nothing opens. */
    in.mouse_left_released = false;
    in.mouse_left_clicked = true;
    in.mouse_left_down = true;
    (void)game_update_europe_screen(game, &in);
    in.mouse_left_clicked = false;
    in.mouse_left_down = false;
    in.mouse_left_released = true;
    in.mouse_x = 162;
    in.mouse_y = 188;
    (void)game_update_europe_screen(game, &in);
    if (eu->menu != EUROPE_MENU_NONE) {
      fprintf(stderr, "  away: menu %d hit %d\n", (int)eu->menu,
              (int)game_europe_hit(game, 162, 188).kind);
      rc = fail("a dock press released over the market opens nothing");
    }
  }
  game->in_europe = false;
  game_destroy(game);
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
 * Esc on @INDIANFOREST issues the order unpaid. FUN_6f74_2580 returns -1 on Esc (raw
 * 116780-116782); FUN_2b5a_123e acts only on 1 (respect) / 2 (pay) (raw 42493/42498), so -1
 * reaches the tail `unit+0x314c = 8` (raw 42511). Same fixture as above.
 */
static int test_indian_land_esc_takes(void) {
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
    game_destroy(game);
    return fail("fixture save load");
  }
  ai_popup_clear(&game->ai_popups);
  int pid = -1;
  for (int i = 0; i < units_slot_end(&game->units); ++i) {
    const ColonizeUnit* u = &game->units.units[i];
    if (u->active && u->x == 45 && u->y == 22 && units_is_pioneer(&game->units, u->id)) {
      pid = u->id;
    }
  }
  game->units.selected_id = pid;
  game->europe.gold = 1000;
  if (pid < 0 || !game_pioneer_land_gate(game, GAME_INDIAN_LAND_FOREST) ||
      !ai_popup_try_present_next(&game->ai_popups)) {
    game_destroy(game);
    return fail("Clear Forest on Tupi forest opens @INDIANFOREST");
  }
  ai_popup_cancel_current(&game->ai_popups);
  game_apply_ai_popup_result(game);
  const ColonizeUnit* pu = units_get(&game->units, pid);
  if (!pu || pu->orders != UNITS_ORDER_CLEAR_PLOW) {
    fprintf(stderr, "  orders %d\n", pu ? pu->orders : -1);
    rc = fail("Esc on @INDIANFOREST clears the forest unpaid (orders 8)");
  }
  if (game->europe.gold != 1000) {
    rc = fail("Esc on @INDIANFOREST pays nothing");
  }
  game_destroy(game);
  return rc;
}

/*
 * FUN_2b5a_1454 (road, raw 42536-42604) and FUN_2b5a_123e (forest, raw 42443-42512) read
 * neither tools nor the current orders, so a Pioneer with under 20 tools already on the order
 * over unbought native land is asked again. Same fixture (Pioneer on Tupi forest (45,22)).
 */
static int indian_land_asks_again(GameIndianLandKind kind, int orders) {
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
  if (!game_load_col1_file(
        game, "tests-automated-regression/map_pioneer_native_forest_offer/input.sav", err,
        sizeof(err)
      )) {
    fprintf(stderr, "  %s\n", err);
    game_destroy(game);
    return fail("fixture save load");
  }
  ai_popup_clear(&game->ai_popups);
  ColonizeUnit* pu = NULL;
  for (int i = 0; i < units_slot_end(&game->units); ++i) {
    ColonizeUnit* u = &game->units.units[i];
    if (u->active && u->x == 45 && u->y == 22 && units_is_pioneer(&game->units, u->id)) {
      pu = u;
    }
  }
  int rc = 0;
  if (!pu) {
    rc = fail("fixture Pioneer at (45,22)");
  } else {
    game->units.selected_id = pu->id;
    pu->orders = orders;
    pu->tools = 0;
    if (!game_pioneer_land_gate(game, kind) || game->ai_popups.queue_count != 1) {
      rc = fail("the native-land question is asked again (no tools / orders gate)");
    }
  }
  game_destroy(game);
  return rc;
}

static int test_indian_road_asks_when_already_building(void) {
  return indian_land_asks_again(GAME_INDIAN_LAND_ROAD, UNITS_ORDER_BUILD_ROAD);
}

static int test_indian_forest_asks_when_already_clearing(void) {
  return indian_land_asks_again(GAME_INDIAN_LAND_FOREST, UNITS_ORDER_CLEAR_PLOW);
}

/*
 * Esc on the @INDIANLAND plot drop seats the worker unpaid. FUN_2f2b_2f3e raw 50372-50373 acts
 * only on 1 (respect, abort) / 2 (pay); -1 carries on into the seat. Fixture:
 * colony_tile_native_land (Quebec (45,24), plot S Tupi-claimed, Tupi at peace).
 */
static int test_indian_land_plot_esc_takes(void) {
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
  if (!game_load_col1_file(
        game, "tests-automated-regression/colony_tile_native_land/input.sav", err, sizeof(err)
      )) {
    fprintf(stderr, "  %s\n", err);
    game_destroy(game);
    return fail("fixture save load");
  }
  ColonizeColony* col = colonies_get_mut(&game->colonies, colonies_id_at(&game->colonies, 45, 24));
  int s_tile = -1;
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    int dx = 0;
    int dy = 0;
    if (colonies_field_tile_delta(i, &dx, &dy) && dx == 0 && dy == 1) {
      s_tile = i;
    }
  }
  int ci = -1;
  for (int i = 0; col && i < col->colonist_count && ci < 0; ++i) {
    if (col->colonists[i].building_type >= 0) {
      ci = i;
    }
  }
  if (!col || s_tile < 0 || ci < 0 || col->tiles[s_tile] >= 0) {
    game_destroy(game);
    return fail("Quebec, a free plot S and an indoor colonist");
  }
  game->in_colony = true;
  game->colony_view_id = col->id;
  game->colony_screen.selected_colonist = ci;
  game->colony_screen.selected_outside_unit = -1;
  ai_popup_clear(&game->ai_popups);
  const int gold_before = game->europe.gold;
  game_colony_area_tile_drop(game, col, &game->world_map, s_tile);
  if (!game->ai_popups.open) {
    (void)ai_popup_try_present_next(&game->ai_popups);
  }
  if (!game->ai_popups.open || game->ai_popups.current.tag != AI_POPUP_TAG_INDIAN_LAND) {
    game_destroy(game);
    return fail("the drop on Tupi-claimed S opens @INDIANLAND");
  }
  ai_popup_cancel_current(&game->ai_popups);
  game_apply_ai_popup_result(game);
  int rc = 0;
  if (col->tiles[s_tile] != ci) {
    fprintf(stderr, "  tiles[S]=%d want %d\n", col->tiles[s_tile], ci);
    rc = fail("Esc on @INDIANLAND seats the worker on the plot");
  }
  if (game->europe.gold != gold_before) {
    rc = fail("Esc on @INDIANLAND pays nothing");
  }
  game_destroy(game);
  return rc;
}

/*
 * campaign8 CN8-B1: clicking (or `activate`) a spent own unit is FUN_2b5a_1b5a + FUN_2b5a_0e52(0):
 * the order clears and the next unit awaiting orders takes control. It used to drop to View
 * Pieces with units still pending, so the turn stalled.
 */
static int test_click_spent_unit_hands_off(void) {
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
    game_destroy(game);
    return fail("fixture save load");
  }
  ai_popup_clear(&game->ai_popups);
  int spent = -1;
  int other = -1;
  for (int i = 0; i < units_slot_end(&game->units); ++i) {
    const ColonizeUnit* u = &game->units.units[i];
    if (!turn_unit_in_rotation(&game->units, u, game->human_nation) ||
        units_orders_skip_turn(u)) {
      continue;
    }
    if (spent < 0) {
      spent = u->id;
    } else if (other < 0) {
      other = u->id;
    }
  }
  ColonizeUnit* s = units_get(&game->units, spent);
  if (!s || other < 0) {
    fprintf(stderr, "  rotation units %d %d\n", spent, other);
    game_destroy(game);
    return fail("fixture has two human units awaiting orders");
  }
  s->moves = 0;
  s->orders = UNITS_ORDER_FORTIFIED;
  game->units.selected_id = -1;
  game->view_pieces_mode = true;
  game_click_activate_unit(game, spent);
  if (s->orders != UNITS_ORDER_NONE || game->units.selected_id < 0 ||
      game->units.selected_id == spent || game->view_pieces_mode) {
    fprintf(stderr, "  orders %d selected %d view %d\n", s->orders, game->units.selected_id,
            game->view_pieces_mode);
    rc = fail("spent unit's order clears and the next pending unit takes control");
  }
  game_destroy(game);
  return rc;
}

/*
 * port_bugs.md #1302: with Game Options End of Turn off, the last unit awaiting orders used up
 * without a hand-off (a colony-screen fence drop despawns it) must still end the turn on the
 * next Move Pieces pass: FUN_2b5a_3b68 re-runs FUN_2b5a_0e52, whose no-unit arm (raw
 * 42308-42312) ends the turn when DS:0x97b0 is set. 0x97b0 is set only once the player was
 * handed a unit (FUN_2b5a_3802), so a turn on which nothing waited stops at the prompt.
 */
static int eot_after_unit_used_up(bool had_control, bool* ended, bool* prompt) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug"};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    if (game) {
      game_destroy(game);
    }
    return TR_SKIP;
  }
  char err[256];
  if (!game_load_col1_file(
        game, "tests-automated-regression/map_pioneer_native_forest_offer/input.sav", err,
        sizeof(err)
      )) {
    fprintf(stderr, "  %s\n", err);
    game_destroy(game);
    return fail("fixture save load");
  }
  ai_popup_clear(&game->ai_popups);
  game->col1.head.game_options.end_of_turn = 0;
  int last = -1;
  for (int i = 0; i < units_slot_end(&game->units); ++i) {
    ColonizeUnit* u = &game->units.units[i];
    if (!turn_unit_in_rotation(&game->units, u, game->human_nation) ||
        units_orders_skip_turn(u)) {
      continue;
    }
    if (last >= 0) {
      u->orders = UNITS_ORDER_FORTIFIED;
    } else {
      last = u->id;
    }
  }
  if (last < 0) {
    game_destroy(game);
    return fail("fixture has a human unit awaiting orders");
  }
  game->units.selected_id = last;
  game->view_pieces_mode = false;
  ColonizeInputState input;
  memset(&input, 0, sizeof(input));
  if (had_control) {
    (void)game_update_unit_pacer(game, &input, 0);
  }
  units_despawn(&game->units, last);
  (void)game_update_unit_pacer(game, &input, 0);
  *ended = turn_processor_active(&game->turn_proc);
  *prompt = game_end_turn_prompt_active(game);
  game_destroy(game);
  return 0;
}

static int test_eot_off_unit_used_up_ends_turn(void) {
  bool ended = false;
  bool prompt = false;
  int rc = eot_after_unit_used_up(true, &ended, &prompt);
  if (rc == TR_SKIP) {
    fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
    return TR_SKIP;
  }
  if (rc == 0 && !ended) {
    rc = fail("End of Turn off: the pass after the last unit is used up ends the turn");
  }
  if (rc == 0 && (eot_after_unit_used_up(false, &ended, &prompt) != 0 || ended || !prompt)) {
    rc = fail("no unit handed to the player this turn: stop at the End of Turn prompt");
  }
  return rc;
}

/*
 * docs/hotseat.md: original_saves/hotseat/hotseat_1522.sav was written by DOS with England at
 * the controls (DS:0x5394 = 0) and France fixed (DS:0x5398 = 1), both control 0, bit 0x80. The
 * load resumes at DS:0x5398 with @MULTINEXT (FUN_130d_0290 raw 6367), and the port's re-save
 * keeps the control table, DS:0x5398 and bit 0x80 (DOS loaded that re-save back as hot-seat).
 */
static int test_hotseat_dos_save_round_trip(void) {
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
  if (!game_load_col1_file(game, "original_saves/hotseat/hotseat_1522.sav", err, sizeof(err))) {
    fprintf(stderr, "  %s\n", err);
    game_destroy(game);
    return fail("DOS hot-seat save load");
  }
  const AiPopupState* p = &game->ai_popups;
  if (game->human_nation != 1 || p->queue_count < 1 || p->queue[0].tag != AI_POPUP_TAG_MULTINEXT) {
    fprintf(stderr, "  human %d queue %d\n", game->human_nation, p->queue_count);
    rc = fail("load resumes France (DS:0x5398) with @MULTINEXT");
  }
  ColonizeCol1Save back;
  col1_save_init(&back);
  if (rc == 0 && (!game_save_col1_named(game, "hotseat_resave.sav", err, sizeof(err)) ||
                  !col1_save_read_file("./build/debug/hotseat_resave.sav", &back, err, sizeof(err)))) {
    fprintf(stderr, "  %s\n", err);
    rc = fail("port re-save");
  }
  if (rc == 0) {
    const ColonizeCol1Head* h = &back.head;
    if (!col1_save_hotseat(&back) || h->human_player != 1 || h->nation_turn != 1 ||
        h->curr_nation_map_view != 1 || back.player[0].control != 0 ||
        back.player[1].control != 0 || back.player[2].control != 1 ||
        back.player[3].control != 1 || col1_save_human_nation(&back) != 1) {
      fprintf(stderr, "  bit %#x 5398 %u 5394 %u 5396 %u control %u %u %u %u\n",
              h->hotseat_woi_redirect_pending, h->human_player, h->nation_turn,
              h->curr_nation_map_view, back.player[0].control, back.player[1].control,
              back.player[2].control, back.player[3].control);
      rc = fail("re-save keeps bit 0x80, DS:0x5398 and the control table");
    }
  }
  col1_save_free(&back);
  remove("./build/debug/hotseat_resave.sav");
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
  if (test_game_txt()) { /* @SOMEBOYCOTT needs its text to queue */
    game->messages = *test_game_txt();
  }
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

/*
 * TB-55: FUN_6662_0f74 runs the near flood only below distance 7. Fixture: the
 * discard campaign's "Hartford Trans" wagon on (47,18), goal Hartford (41,18).
 * The goal flood sends it E onto the road at (48,19) (dx 7); there DOS takes
 * the coarse waypoint flood, which steps back to (47,18), forever. The fixed
 * path keeps distance 7 on the goal flood and the wagon reaches Hartford.
 */
static int test_goto_near_tier_boundary_tb55(void) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug"};
  bool arrived[2] = {false, false};
  int rc = 0;
  for (int k = 0; k < 2 && rc == 0; ++k) {
    dos_buggy_behavior_set(k == 0);
    ColonizeGameState* game = game_create(&cfg);
    if (!game || !game_assets_ok(game)) {
      fprintf(stderr, "SKIP: no COLONIZE/ assets\n");
      if (game) {
        game_destroy(game);
      }
      dos_buggy_behavior_set(true);
      return TR_SKIP;
    }
    char err[256];
    if (!game_load_col1_file(game, "tests-save-misc/tb55_near_tier_wagon.sav", err, sizeof(err))) {
      fprintf(stderr, "  %s\n", err);
      rc = fail("fixture save load");
    }
    ColonizeUnit* wagon = NULL;
    for (int i = 0; rc == 0 && i < units_slot_end(&game->units); ++i) {
      ColonizeUnit* u = &game->units.units[i];
      if (u->active && u->x == 47 && u->y == 18 && u->orders == UNITS_ORDER_TRADE_ROUTE) {
        wagon = u;
      }
    }
    if (rc == 0 && (!wagon || wagon->goto_x != 41 || wagon->goto_y != 18)) {
      rc = fail("route wagon on (47,18) aimed at Hartford (41,18)");
    }
    const ColonizeWorld w = game_world(game);
    for (int step = 0; rc == 0 && step < 24 && !arrived[k]; ++step) {
      int nx = -1;
      int ny = -1;
      if (!units_next_goto_step_w(&w, wagon->id, &nx, &ny)) {
        break;
      }
      wagon->x = nx;
      wagon->y = ny;
      arrived[k] = nx == 41 && ny == 18;
    }
    game_destroy(game);
  }
  dos_buggy_behavior_set(true);
  if (rc == 0 && (arrived[0] || !arrived[1])) {
    fprintf(stderr, "  buggy arrived %d (want 0) fixed arrived %d (want 1)\n", arrived[0],
            arrived[1]);
    rc = fail("TB-55 near tier covers distance 7");
  }
  return rc;
}

/*
 * Every DOS save (FUN_75c2_0288 raw 120103) and load (FUN_75c2_0940 raw 120450)
 * runs FUN_281f_04ca = srand(tick). With live ticks on, both save wrappers and
 * both loaders leave move_rng on the tick, the loaders over the stream RTST
 * restored. With ticks fixed (--seed, tests, replays) a load keeps the RTST
 * stream. Saves go to build/debug/saveload_reseed, removed after.
 */
static int test_saveload_reseeds_from_tick(void) {
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = "./build/debug/saveload_reseed"};
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
  dos_rng_set_live_ticks(true);
  dos_rng_set_tick_word(101);
  game->move_rng.state = 0x12345678u;
  if (rc == 0 && (!game_save_col1_autosave_slot(game, 9, err, sizeof(err)) ||
                  game->move_rng.state != 101u)) {
    fprintf(stderr, "  %s state %08x\n", err, game->move_rng.state);
    rc = fail("autosave reseeds from the tick");
  }
  dos_rng_set_tick_word(202);
  game->move_rng.state = 0x12345678u;
  if (rc == 0 && (!game_save_col1_slot(game, 0, err, sizeof(err)) ||
                  game->move_rng.state != 202u)) {
    fprintf(stderr, "  %s state %08x\n", err, game->move_rng.state);
    rc = fail("slot save reseeds from the tick");
  }
  /* Slot 0's RTST carries state 202; a load ends on the tick instead. */
  dos_rng_set_tick_word(303);
  if (rc == 0 && (!game_load_col1_slot(game, 0, err, sizeof(err)) ||
                  game->move_rng.state != 303u)) {
    fprintf(stderr, "  %s state %08x\n", err, game->move_rng.state);
    rc = fail("slot load reseeds from the tick, not RTST");
  }
  dos_rng_set_tick_word(404);
  if (rc == 0 && (!game_load_col1_file(game, "./build/debug/saveload_reseed/COLONY00.SAV", err,
                                       sizeof(err)) ||
                  game->move_rng.state != 404u)) {
    fprintf(stderr, "  %s state %08x\n", err, game->move_rng.state);
    rc = fail("file load reseeds from the tick, not RTST");
  }
  dos_rng_set_live_ticks(false);
  if (rc == 0 && (!game_load_col1_slot(game, 0, err, sizeof(err)) ||
                  game->move_rng.state != 202u)) {
    fprintf(stderr, "  %s state %08x\n", err, game->move_rng.state);
    rc = fail("fixed ticks: the load keeps the RTST stream");
  }
  dos_rng_set_tick_word(-1);
  remove("./build/debug/saveload_reseed/COLONY00.SAV");
  remove("./build/debug/saveload_reseed/COLONY09.SAV");
  remove("./build/debug/saveload_reseed");
  game_destroy(game);
  return rc;
}

/* An end of turn parked behind FUN_3844_0442's dialogs (after the calendar
 * step, before the day top) must not resume in a loaded world or after the
 * game ends to the title: DOS returns from FUN_130d_0290 on DS:0x53c2 == 0
 * (raw 6511). */
static void park_eot(ColonizeGameState* game) {
  turn_processor_start(&game->turn_proc);
  game->turn_proc.step = TURN_PROC_INDIAN;
  game->turn_proc.nation_cursor = 4;
  game->turn_proc.day_advanced = true;
}

static int test_parked_eot_dropped_on_load_and_title(void) {
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
  park_eot(game);
  if (!game_load_col1_file(game, "tests-save-misc/tb55_near_tier_wagon.sav", err, sizeof(err))) {
    fprintf(stderr, "  %s\n", err);
    rc = fail("fixture save load");
  }
  if (rc == 0 && (turn_processor_active(&game->turn_proc) || game->turn_proc.day_advanced)) {
    rc = fail("a load must drop the parked end of turn");
  }
  if (rc == 0) {
    park_eot(game);
    game_end_to_title(game);
    if (turn_processor_active(&game->turn_proc) || game->turn_proc.day_advanced) {
      rc = fail("ending to the title must drop the parked end of turn");
    }
  }
  game_destroy(game);
  return rc;
}

/* 8x8 land map, a human unit with attack at (3,3), a nation-1 unit at (4,3),
 * nations 1-3 computer-run. Free with edge_scroll_free. */
static int treaty_fixture(ColonizeGameState* game, int* uid) {
  int ox = 0;
  edge_scroll_init(game, &ox);
  game->units.types[0].attack = 2;
  game->units.types[0].defense = 1;
  for (int n = 1; n < 4; ++n) {
    game->col1.player[n].control = 1;
  }
  *uid = units_spawn(&game->units, 0, 3, 3);
  ColonizeUnit* f = units_get(&game->units, units_spawn(&game->units, 0, 4, 3));
  if (*uid < 0 || !f) {
    return 0;
  }
  f->nation_id = 1;
  return 1;
}

/* FUN_465b_0000 raw 75545 tests only the attacker's PEACE bit: a human attack on
 * a peer whose rows carry PEACE|WAR still asks @HAVETREATY. */
static int test_human_peace_war_asks_havetreaty(void) {
  static ColonizeGameState game;
  int uid = -1;
  int rc = 0;
  if (!treaty_fixture(&game, &uid)) {
    rc = fail("treaty fixture");
  } else {
    ai_diplo_write(&game.col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE | AI_DIPLO_WAR);
    ai_diplo_write(&game.col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE | AI_DIPLO_WAR);
    const GameMoveStep step =
      game_move_colony_prompts(&game, units_get(&game.units, uid), uid, 4, 3);
    if (step != GAME_MOVE_RETURN_TRUE || game.ai_popups.queue_count != 1 ||
        game.ai_popups.queue[0].tag != AI_POPUP_TAG_CONTACT_EURO_WAR) {
      fprintf(stderr, "  step %d queue %d\n", (int)step, game.ai_popups.queue_count);
      rc = fail("a human attack under PEACE|WAR asks @HAVETREATY");
    }
  }
  edge_scroll_free(&game);
  return rc;
}

/* FUN_2b5a_1112 raw 42410-42413: Break Treaty on the fortify @HAVETREATY ORs WAR
 * into the partner's row and clears PEACE both ways; no 0x01, no DS:0x53c8 write
 * and no @DECLAREWAR. dos_buggy is on in tests, so the self row gets no WAR
 * (TB-54). */
static int test_fortify_break_treaty_answer(void) {
  static ColonizeGameState game;
  int uid = -1;
  int rc = 0;
  if (!treaty_fixture(&game, &uid)) {
    rc = fail("treaty fixture");
  } else {
    game.col1.head.nation_relation[0] = 5;
    game.col1.head.nation_relation[1] = 5;
    ai_diplo_write(&game.col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_diplo_write(&game.col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
    game.ai_popups.has_result = true;
    game.ai_popups.result_tag = AI_POPUP_TAG_FORTIFY_TREATY;
    game.ai_popups.result_cancelled = false;
    game.ai_popups.result_choice_id = 1;
    game.ai_popups.result_nation_a = uid;
    game.ai_popups.result_nation_b = 1;
    game_apply_ai_popup_result(&game);
    const uint8_t self = ai_diplo_read(&game.col1, 0, 1);
    const uint8_t partner = ai_diplo_read(&game.col1, 1, 0);
    if (self != AI_DIPLO_MET || partner != (AI_DIPLO_MET | AI_DIPLO_WAR) ||
        game.col1.head.nation_relation[0] != 5 || game.col1.head.nation_relation[1] != 5 ||
        game.ai_popups.queue_count != 0) {
      fprintf(stderr, "  self 0x%02x partner 0x%02x queue %d\n", self, partner,
              game.ai_popups.queue_count);
      rc = fail("fortify Break Treaty: partner |= WAR, PEACE cleared, no declare-war popup");
    }
  }
  edge_scroll_free(&game);
  return rc;
}

static const TestCase k_cases[] = {
    {"test_human_peace_war_asks_havetreaty", test_human_peace_war_asks_havetreaty},
    {"test_fortify_break_treaty_answer", test_fortify_break_treaty_answer},
    {"test_parked_eot_dropped_on_load_and_title", test_parked_eot_dropped_on_load_and_title},
    {"test_dos_buggy_default_off_tests_on", test_dos_buggy_default_off_tests_on},
    {"test_saveload_reseeds_from_tick", test_saveload_reseeds_from_tick},
    {"test_goto_near_tier_boundary_tb55", test_goto_near_tier_boundary_tb55},
    {"test_map_pulldown_is_modal", test_map_pulldown_is_modal},
    {"test_turn_year_end_rival_rebels", test_turn_year_end_rival_rebels},
    {"test_ai_contact_raid_alarm_delta", test_ai_contact_raid_alarm_delta},
    {"test_ai_contact_raid_sound_rows", test_ai_contact_raid_sound_rows},
    {"test_ai_021a_dir_tile", test_ai_021a_dir_tile},
    {"test_ai_021a_dres_euro_colony_tile", test_ai_021a_dres_euro_colony_tile},
    {"test_ai_465b_dest_owner", test_ai_465b_dest_owner},
    {"test_ai_brave_field_attack", test_ai_brave_field_attack},
    {"test_game_render_select_palette", test_game_render_select_palette},
    {"test_window_log_fit_line", test_window_log_fit_line},
    {"test_game_colony_unload_whole_hold", test_game_colony_unload_whole_hold},
    {"test_game_colony_outside_role_tools_residue", test_game_colony_outside_role_tools_residue},
    {"test_colony_zoom_hold_survives_pedia_detour", test_colony_zoom_hold_survives_pedia_detour},
    {"test_trade_route_wagon_services_arrival_stop", test_trade_route_wagon_services_arrival_stop},
    {"test_trade_route_stop_uses_dos_colony_index", test_trade_route_stop_uses_dos_colony_index},
    {"test_trade_route_off_tile_reaims_same_stop", test_trade_route_off_tile_reaims_same_stop},
    {"test_trade_route_routeloop_advances_stop", test_trade_route_routeloop_advances_stop},
    {"test_trade_route_woi_europe_stop_no_trade", test_trade_route_woi_europe_stop_no_trade},
    {"test_trade_route_europe_stop_off_lane_keeps_stop", test_trade_route_europe_stop_off_lane_keeps_stop},
    {"test_trade_route_begin_moves_wagon_to_selected_stop", test_trade_route_begin_moves_wagon_to_selected_stop},
    {"test_goto_treasure_gets_king_galleon_offer", test_goto_treasure_gets_king_galleon_offer},
    {"test_human_lcr_count_change_skips_contact", test_human_lcr_count_change_skips_contact},
    {"test_map_o_key_does_not_board", test_map_o_key_does_not_board},
    {"test_deferred_wake_does_not_leak", test_deferred_wake_does_not_leak},
    {"test_human_passenger_ship_to_ship", test_human_passenger_ship_to_ship},
    {"test_human_board_reveals_with_passenger", test_human_board_reveals_with_passenger},
    {"test_human_board_two_hulls_reveals_with_passenger",
     test_human_board_two_hulls_reveals_with_passenger},
    {"test_human_transfer_reveals_with_passenger", test_human_transfer_reveals_with_passenger},
    {"test_goto_board_reveals_with_passenger", test_goto_board_reveals_with_passenger},
    {"test_human_landfall_runs_3180", test_human_landfall_runs_3180},
    {"test_human_village_visit_no_3180", test_human_village_visit_no_3180},
    {"test_human_landho_after_3180", test_human_landho_after_3180},
    {"test_human_nonstep_no_landho", test_human_nonstep_no_landho},
    {"test_human_treasure_offer_after_3180", test_human_treasure_offer_after_3180},
    {"test_ship_in_colony_meets_tribe", test_ship_in_colony_meets_tribe},
    {"test_ship_bumps_foreign_colony_trade_arm", test_ship_bumps_foreign_colony_trade_arm},
    {"test_trade_route_ship_aims_colony_tile", test_trade_route_ship_aims_colony_tile},
    {"test_ai_euro_5952_absorb_colonist", test_ai_euro_5952_absorb_colonist},
    {"test_ai_euro_5952_absorb_soldier", test_ai_euro_5952_absorb_soldier},
    {"test_ai_euro_5952_equip_scout", test_ai_euro_5952_equip_scout},
    {"test_ai_euro_5952_equip_pioneer", test_ai_euro_5952_equip_pioneer},
    {"test_colony_shift_pick_moves_highlight", test_colony_shift_pick_moves_highlight},
    {"test_colony_jobs_menu_seats_indoor_worker", test_colony_jobs_menu_seats_indoor_worker},
    {"test_colony_rum_distiller_refusal_popup", test_colony_rum_distiller_refusal_popup},
    {"test_colony_fence_unit_jobs_list", test_colony_fence_unit_jobs_list},
    {"test_colony_fence_unit_jobs_extras", test_colony_fence_unit_jobs_extras},
    {"test_colony_jobs_menu_open_has_no_side_effects", test_colony_jobs_menu_open_has_no_side_effects},
    {"test_game_world_view", test_game_world_view},
    {"test_goto_place_opens_travelplace_list", test_goto_place_opens_travelplace_list},
    {"test_activate_ignores_foreign_unit", test_activate_ignores_foreign_unit},
    {"test_select_tile_rotation_retest", test_select_tile_rotation_retest},
    {"test_view_pieces_keeps_active_unit", test_view_pieces_keeps_active_unit},
    {"test_right_click_and_find_colony_keep_active_unit",
     test_right_click_and_find_colony_keep_active_unit},
    {"test_move_edge_scroll_uses_departure_tile", test_move_edge_scroll_uses_departure_tile},
    {"test_board_step_watch_and_ship_margin", test_board_step_watch_and_ship_margin},
    {"test_phantom_lunge_edge_scroll_uses_colony_tile",
     test_phantom_lunge_edge_scroll_uses_colony_tile},
    {"test_combat_edge_scroll_uses_attacker_tile", test_combat_edge_scroll_uses_attacker_tile},
    {"test_combat_lunge_reads_465b_local_1c", test_combat_lunge_reads_465b_local_1c},
    {"test_foreign_move_shown_by_prior_sighting", test_foreign_move_shown_by_prior_sighting},
    {"test_brave_step_watch_sees_entry_vis", test_brave_step_watch_sees_entry_vis},
    {"test_player_pick_keeps_view", test_player_pick_keeps_view},
    {"test_bar_sale_speedup_only_on_sales", test_bar_sale_speedup_only_on_sales},
    {"test_colony_list_keys_wrap", test_colony_list_keys_wrap},
    {"test_colony_custom_house_keys", test_colony_custom_house_keys},
    {"test_screen_f1_focus_article", test_screen_f1_focus_article},
    {"test_colony_construction_page_default", test_colony_construction_page_default},
    {"test_colony_construction_two_pages_live", test_colony_construction_two_pages_live},
    {"test_indian_land_offer_row_greyed", test_indian_land_offer_row_greyed},
    {"test_indian_land_esc_takes", test_indian_land_esc_takes},
    {"test_indian_road_asks_when_already_building", test_indian_road_asks_when_already_building},
    {"test_indian_forest_asks_when_already_clearing", test_indian_forest_asks_when_already_clearing},
    {"test_indian_land_plot_esc_takes", test_indian_land_plot_esc_takes},
    {"test_europe_buy_prompt_names_hull_type", test_europe_buy_prompt_names_hull_type},
    {"test_europe_repairing_hull_press_no_pickup", test_europe_repairing_hull_press_no_pickup},
    {"test_europe_dock_menu_on_release", test_europe_dock_menu_on_release},
    {"test_brewster_pick_names_home_port", test_brewster_pick_names_home_port},
    {"test_crossing_stamps_nation_return_tile", test_crossing_stamps_nation_return_tile},
    {"test_new_campaign_drops_parked_flow", test_new_campaign_drops_parked_flow},
    {"test_new_campaign_amer2_seed100_landfall", test_new_campaign_amer2_seed100_landfall},
    {"test_europe_repaints_reset_harbor_selection", test_europe_repaints_reset_harbor_selection},
    {"test_click_spent_unit_hands_off", test_click_spent_unit_hands_off},
    {"test_eot_off_unit_used_up_ends_turn", test_eot_off_unit_used_up_ends_turn},
    {"test_hotseat_dos_save_round_trip", test_hotseat_dos_save_round_trip},
    {"test_europe_unload_all_boycott_any_slot", test_europe_unload_all_boycott_any_slot},
};

TEST_MAIN(k_cases)
