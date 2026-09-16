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
#include "core/ai_internal.h"
#include "core/col1_save.h"
#include "core/game_loop_internal.h"
#include "core/turn_internal.h"

#include "../common/ai_fixture.h"

#include <stdio.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_stage_seams: FAIL %s\n", msg);
  return 1;
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
  if (ai_contact_raid_alarm_delta(AI_RAID_SCALP) != -16) {
    return fail("raid_alarm_delta SCALP");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_GOLD) != -8) {
    return fail("raid_alarm_delta GOLD");
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

int main(void) {
  int failures = 0;
  failures += test_turn_year_end_rival_rebels();
  failures += test_ai_contact_raid_alarm_delta();
  failures += test_ai_021a_dir_tile();
  failures += test_game_render_select_palette();
  if (failures) {
    fprintf(stderr, "unit_stage_seams: %d test(s) failed\n", failures);
    return 1;
  }
  printf("unit_stage_seams: OK\n");
  return 0;
}
