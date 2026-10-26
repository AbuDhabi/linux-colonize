#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include "core/ai_king.h"
#include "core/col1_save.h"
#include "core/game_loop.h"
#include "core/game_loop_internal.h"

#include "tests/common/test_assets.h"
#include "../common/test_runner.h"

/*
 * Hall of Fame: the DOS HALLFAME.DAT table (FUN_41f2_0f56: 5 x 42-byte
 * records on disk, 6 slots in memory, ranked by rating) AND the title-menu
 * screen it opens into (a real
 * in_hall_of_fame render state via reports_render_hall_of_fame — not a
 * window-title status string). See docs/port_plan.md Phase 1 /
 * docs/manual_gap.md "Hall of Fame".
 */

/* Minimal title-menu assets: enough for game_create to reach the menu with a
 * "View Hall of Fame" option, nothing else (no wizard pages needed). */
static const char* k_data_dir = "./test-assets-hof";
static const char* k_save_dir = "./test-saves-hof";
static char k_hof_path[512];
static const char* k_fixture_sav = "tests-automated-regression/map_pioneer_native_forest_offer/input.sav";

/* Move DOWN to the Hall of Fame row (@BEGINMENU row 4) and activate it. */
static bool open_hall_of_fame(ColonizeGameState* game) {
  for (int i = 0; i < 4; ++i) {
    if (!test_assets_step(game, COLONIZE_KEY_DOWN)) return false;
  }
  return test_assets_step(game, COLONIZE_KEY_ENTER);
}

static int setup(void) {
  if (!test_assets_create(k_data_dir, TEST_ASSETS_FULL_MENU | TEST_ASSETS_NAMES)) {
    fprintf(stderr, "failed to create test assets\n");
    return 1;
  }
  mkdir(k_save_dir, 0755);
  snprintf(k_hof_path, sizeof(k_hof_path), "%s/HALLFAME.DAT", k_data_dir);
  return 0;
}

/* 1. No HALLFAME.DAT yet: empty table; menu action opens a real screen (not a
 * status-text hack), and Esc closes it back to the title menu. */
static int case_empty_table(void) {
  if (setup() != 0) return 1;
  remove(k_hof_path);

  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed (empty)\n");
    return 1;
  }
  if (game_hof_count(game) != 0) {
    fprintf(stderr, "expected empty HoF table, got %d\n", game_hof_count(game));
    game_destroy(game);
    return 1;
  }
  if (!open_hall_of_fame(game)) {
    fprintf(stderr, "open Hall of Fame failed\n");
    game_destroy(game);
    return 1;
  }
  if (!game_in_hall_of_fame(game) || game_in_menu(game)) {
    fprintf(stderr, "expected Hall of Fame screen open, left title menu\n");
    game_destroy(game);
    return 1;
  }
  /* Idle frames (no key) must not close it or bounce back to the menu —
   * game_update runs every frame, not just on keypress. */
  if (!test_assets_step(game, COLONIZE_KEY_NONE) || !game_in_hall_of_fame(game)) {
    fprintf(stderr, "Hall of Fame screen closed on idle frame\n");
    game_destroy(game);
    return 1;
  }
  if (!test_assets_step(game, COLONIZE_KEY_ESCAPE)) {
    fprintf(stderr, "Esc from Hall of Fame failed\n");
    game_destroy(game);
    return 1;
  }
  if (game_in_hall_of_fame(game) || !game_in_menu(game)) {
    fprintf(stderr, "expected Esc to return to title menu\n");
    game_destroy(game);
    return 1;
  }
  game_destroy(game);
  return 0;
}

/* One 42-byte HALLFAME.DAT record (FUN_41f2_14a8 word order). */
static void put_record(
  uint8_t* r, const char* name, int nation, int declared, int achieved, int year, int season,
  int difficulty, int score, int rating, int tier
) {
  memset(r, 0, 42);
  snprintf((char*)r, 24, "%s", name);
  const int w[9] = {nation, declared, achieved, year, season, difficulty, score, rating, tier};
  for (int i = 0; i < 9; ++i) {
    r[24 + 2 * i] = (uint8_t)(w[i] & 0xff);
    r[25 + 2 * i] = (uint8_t)((w[i] >> 8) & 0xff);
  }
}

static bool write_bytes(const char* path, const uint8_t* buf, size_t n) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  const bool ok = fwrite(buf, 1, n, f) == n;
  return fclose(f) == 0 && ok;
}

/* 2. A DOS-written HALLFAME.DAT loads slot by slot (the 3-entry table found in
 * the shipped COLONIZE/ directory: two Spanish Viceroy entries, one English
 * Discoverer; slots 3-4 empty, nation -1), and a short file loads as empty. */
static int case_dos_file_loads(void) {
  if (setup() != 0) return 1;
  uint8_t buf[210];
  put_record(buf, "Christopher Columbus", 2, 0, 0, 1800, 0, 4, 44, 2, 2);
  put_record(buf + 42, "Christopher Columbus", 2, 0, 0, 1800, 0, 4, 44, 2, 2);
  put_record(buf + 84, "Walter Raleigh", 0, 0, 0, 1600, 0, 0, 7, 0, -1);
  put_record(buf + 126, "", -1, 0, 0, 0, 0, 0, -1, -1, 0);
  put_record(buf + 168, "", -1, 0, 0, 0, 0, 0, -1, -1, 0);
  if (!write_bytes(k_hof_path, buf, sizeof(buf))) {
    fprintf(stderr, "failed to seed HALLFAME.DAT\n");
    return 1;
  }
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed (dos file)\n");
    return 1;
  }
  ColonizeHofEntryView e;
  if (game_hof_count(game) != 3 || !game_hof_entry(game, 2, &e) ||
      strcmp(e.leader, "Walter Raleigh") != 0 || e.year != 1600 || e.score != 7 ||
      e.rating != 0 || e.difficulty != 0 || e.declared || e.achieved ||
      game->hof_entries[2].nation_id != 0 || game->hof_entries[2].tier != -1 ||
      game_hof_entry(game, 3, &e) || game->hof_entries[5].nation_id != -1) {
    fprintf(stderr, "DOS HALLFAME.DAT slots mis-read (count %d)\n", game_hof_count(game));
    game_destroy(game);
    return 1;
  }
  if (!game_hof_entry(game, 0, &e) || e.score != 44 || e.rating != 2 || e.difficulty != 4) {
    fprintf(stderr, "slot 0 mis-read\n");
    game_destroy(game);
    return 1;
  }
  game_destroy(game);

  /* FUN_41f2_0f56: a failed 0xd2-byte fread leaves every slot empty. */
  if (!write_bytes(k_hof_path, buf, 100)) return 1;
  game = game_create(&cfg);
  if (!game || game_hof_count(game) != 0) {
    fprintf(stderr, "short HALLFAME.DAT must load as empty\n");
    if (game) game_destroy(game);
    return 1;
  }
  game_destroy(game);
  remove(k_hof_path);
  return 0;
}

/* 3. DOS insert rule (FUN_41f2_0f56 raw 72604-72630): first slot with a strictly
 * lower rating, ties go below; an entry that outranks none takes slot 5, which
 * the file never stores. The ranked screen renders and Enter closes it. */
static int case_insert_rule_and_screen(void) {
  if (setup() != 0) return 1;
  remove(k_hof_path);
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed (insert)\n");
    return 1;
  }
  static const struct {
    const char* leader;
    int rating;
    int want_slot;
  } ins[] = {
    {"A", 40, 0}, {"B", 60, 0}, {"C", 40, 2}, {"D", 50, 1}, {"E", 10, 4}, {"F", 5, 5}, {"G", 5, 5},
  };
  for (size_t i = 0; i < sizeof(ins) / sizeof(ins[0]); ++i) {
    ColonizeHofEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.leader, sizeof(entry.leader), "%s", ins[i].leader);
    entry.nation_id = 1;
    entry.rating = ins[i].rating;
    entry.score = ins[i].rating * 10;
    const int slot = game_hof_insert(game, &entry);
    if (slot != ins[i].want_slot) {
      fprintf(stderr, "insert %s: slot %d want %d\n", ins[i].leader, slot, ins[i].want_slot);
      game_destroy(game);
      return 1;
    }
  }
  static const char* want_order = "BDACE";
  for (int i = 0; i < 5; ++i) {
    ColonizeHofEntryView e;
    if (!game_hof_entry(game, i, &e) || e.leader[0] != want_order[i]) {
      fprintf(stderr, "slot %d wrong after inserts\n", i);
      game_destroy(game);
      return 1;
    }
  }
  if (!open_hall_of_fame(game) || !game_in_hall_of_fame(game) || game_in_menu(game)) {
    fprintf(stderr, "expected Hall of Fame screen open\n");
    game_destroy(game);
    return 1;
  }
  /* Screen actually renders (background blit + text), not a blank/black
   * frame — weak sanity check that reports_render_hall_of_fame ran. */
  static uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  ColonizePalette palette;
  memset(pixels, 0, sizeof(pixels));
  game_render(game, &fb, &palette);
  bool any_nonzero = false;
  for (size_t i = 0; i < sizeof(pixels) && !any_nonzero; ++i) {
    any_nonzero = pixels[i] != 0;
  }
  if (!any_nonzero) {
    fprintf(stderr, "Hall of Fame screen rendered a blank frame\n");
    game_destroy(game);
    return 1;
  }
  /* Enter closes it back to the title menu (Esc covered in case 1). */
  if (!test_assets_step(game, COLONIZE_KEY_ENTER) || game_in_hall_of_fame(game) ||
      !game_in_menu(game)) {
    fprintf(stderr, "expected Enter to return to title menu\n");
    game_destroy(game);
    return 1;
  }
  game_destroy(game);
  return 0;
}

/* 4. game_hof_save writes the DOS bytes (210 bytes, achieved as 0x5382 & 8,
 * empty slots nation -1) and a reload gets the same slots back. */
static int case_save_round_trip(void) {
  if (setup() != 0) return 1;
  remove(k_hof_path);
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) return 1;
  ColonizeHofEntry entry;
  memset(&entry, 0, sizeof(entry));
  snprintf(entry.leader, sizeof(entry.leader), "Peter Minuit");
  entry.nation_id = 3;
  entry.declared = true;
  entry.achieved = true;
  entry.year = 1776;
  entry.season = 1;
  entry.difficulty = 2;
  entry.score = 900;
  entry.rating = 55;
  entry.tier = 7;
  game_hof_insert(game, &entry);
  game_hof_save(game);
  game_destroy(game);

  uint8_t want[210];
  put_record(want, "Peter Minuit", 3, 1, 8, 1776, 1, 2, 900, 55, 7);
  for (int i = 1; i < 5; ++i) {
    put_record(want + 42 * i, "", -1, 0, 0, 0, 0, 0, -1, -1, 0);
  }
  uint8_t got[256];
  FILE* f = fopen(k_hof_path, "rb");
  const size_t n = f ? fread(got, 1, sizeof(got), f) : 0;
  if (f) fclose(f);
  if (n != sizeof(want) || memcmp(got, want, sizeof(want)) != 0) {
    fprintf(stderr, "HALLFAME.DAT bytes differ (size %zu)\n", n);
    return 1;
  }
  game = game_create(&cfg);
  ColonizeHofEntryView e;
  if (!game || game_hof_count(game) != 1 || !game_hof_entry(game, 0, &e) ||
      strcmp(e.leader, "Peter Minuit") != 0 || !e.declared || !e.achieved || e.year != 1776 ||
      e.difficulty != 2 || e.score != 900 || e.rating != 55 ||
      game->hof_entries[0].season != 1 || game->hof_entries[0].tier != 7) {
    fprintf(stderr, "HALLFAME.DAT round trip mangled\n");
    if (game) game_destroy(game);
    return 1;
  }
  game_destroy(game);
  remove(k_hof_path);
  return 0;
}

/* One frame with a left (or right) button press and no key. */
static bool click_step(ColonizeGameState* game, bool right) {
  ColonizeInputState input = {0};
  input.mouse_left_clicked = !right;
  input.mouse_right_clicked = right;
  input.mouse_x = 160;
  input.mouse_y = 100;
  return game_update(game, &input, 16);
}

static bool at_menu(const ColonizeGameState* game) {
  return game->in_menu && !game->in_hall_of_fame && !game->in_exploits && !game->in_report &&
    !ai_popup_busy(&game->ai_popups);
}

/* The calendar end (KING_WAR_END payload 2: @RETIRING / @RETIRING2): an Up leaves the box up,
 * a letter key closes it, then the score plate and the Hall of Fame (score 0 = no exploits
 * tier) by keys. True when @SCORED is up. */
static bool calendar_end_to_scored(ColonizeGameState* game) {
  game->in_menu = false;
  ai_popup_enqueue_ok_ctx(&game->ai_popups, AI_POPUP_TAG_KING_WAR_END, 0, -1, 2, NULL, "");
  if (!test_assets_step(game, COLONIZE_KEY_NONE) || !test_assets_step(game, COLONIZE_KEY_UP) ||
      !game->ai_popups.open || game->in_report) {
    return false;
  }
  if (!test_assets_step(game, COLONIZE_KEY_A) || game->ai_popups.open || !game->in_report ||
      !test_assets_step(game, COLONIZE_KEY_A) || !game->in_hall_of_fame ||
      !test_assets_step(game, COLONIZE_KEY_A) || game->in_hall_of_fame) {
    return false;
  }
  test_assets_step(game, COLONIZE_KEY_NONE);
  return game->ai_popups.open && game->ai_popups.current.tag == AI_POPUP_TAG_WAR_SCORED &&
    !game->in_menu;
}

/* 6. Every retire-chain screen ends in DOS FUN_281f_03c0 (FUN_41f2_0092 score, 0b70
 * exploits, 0f56 Hall of Fame): any key or either mouse button goes on, one screen per
 * press. The port's Hall of Fame took Esc/Enter only, its score plate Esc/Enter plus the
 * F-key report switch. Also the 1800 end's order and keys (port_bugs.md #1231). */
static int case_click_dismisses_ending_screens(void) {
  if (setup() != 0) return 1;
  remove(k_hof_path);
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed (click)\n");
    return 1;
  }
  int rc = 1;
  /* Title-menu Hall of Fame: left click, right click, Q (not the quit confirm). */
  for (int i = 0; i < 3; ++i) {
    game->menu_selection = 0; /* open_hall_of_fame steps down from row 0 */
    if (!open_hall_of_fame(game) || !game->in_hall_of_fame) {
      fprintf(stderr, "Hall of Fame did not open (pass %d)\n", i);
      goto out;
    }
    const bool ok = (i < 2) ? click_step(game, i == 1) : test_assets_step(game, COLONIZE_KEY_Q);
    if (!ok || !at_menu(game)) {
      fprintf(stderr, "Hall of Fame pass %d did not return to the title menu\n", i);
      goto out;
    }
  }
  /* Retire chain: score -> Hall of Fame (score 0 = no exploits tier) -> menu, clicks only. */
  game->col1_ok = true;
  game->in_menu = false;
  game_open_retire_score(game);
  if (!game->in_report || !click_step(game, true) || game->in_report || !game->in_hall_of_fame) {
    fprintf(stderr, "right click on the retire score did not reach the Hall of Fame\n");
    goto out;
  }
  if (!test_assets_step(game, COLONIZE_KEY_NONE) || !game->in_hall_of_fame) {
    fprintf(stderr, "the score's click also closed the Hall of Fame\n");
    goto out;
  }
  if (!click_step(game, false) || !at_menu(game)) {
    fprintf(stderr, "left click on the retire Hall of Fame did not return to the menu\n");
    goto out;
  }
  /* Exploits screen -> Hall of Fame -> menu. */
  game->in_menu = false;
  game->in_exploits = true;
  if (!click_step(game, true) || game->in_exploits || !game->in_hall_of_fame) {
    fprintf(stderr, "right click on the exploits screen did not reach the Hall of Fame\n");
    goto out;
  }
  if (!test_assets_step(game, COLONIZE_KEY_NONE) || !game->in_hall_of_fame ||
      !click_step(game, false) || !at_menu(game)) {
    fprintf(stderr, "exploits -> Hall of Fame -> menu by clicks failed\n");
    goto out;
  }
  /* The score plate's wait takes any key too, and the main loop drops it (raw 55683-55690):
   * an F-key does not switch reports and Q does not ask to quit. */
  game->in_menu = false;
  game_open_retire_score(game);
  if (!test_assets_step(game, COLONIZE_KEY_F2) || game->in_report || !game->in_hall_of_fame) {
    fprintf(stderr, "F2 on the retire score did not go on to the Hall of Fame\n");
    goto out;
  }
  if (!test_assets_step(game, COLONIZE_KEY_Q) || !at_menu(game)) {
    fprintf(stderr, "Q on the retire Hall of Fame did not return to the menu\n");
    goto out;
  }
  game->in_menu = false;
  game_open_report(game, COLONIZE_REPORT_SCORE);
  if (!test_assets_step(game, COLONIZE_KEY_Q) || game->in_report || game->in_hall_of_fame ||
      ai_popup_busy(&game->ai_popups)) {
    fprintf(stderr, "Q on the F10 score plate did not just close it\n");
    goto out;
  }
  /* 1800 end (FUN_3844_0442 raw 58629-58676, port_bugs.md #1231): @RETIRING -> score ->
   * Hall of Fame -> @SCORED. The @RETIRING box (FUN_6f74_2580, no option rows) closes on
   * any key but Up/Down; Esc on @SCORED ends at the title menu like "That's all.". */
  if (!calendar_end_to_scored(game)) {
    fprintf(stderr, "the 1800 @RETIRING -> score -> Hall of Fame -> @SCORED chain failed\n");
    goto out;
  }
  if (!test_assets_step(game, COLONIZE_KEY_ESCAPE) || !at_menu(game)) {
    fprintf(stderr, "Esc on @SCORED did not end at the title menu\n");
    goto out;
  }
  rc = 0;
out:
  game_destroy(game);
  remove(k_hof_path);
  return rc;
}

/* 7. port_bugs.md #1232: @SCORED "Keep playing anyway." (choice 2) after the calendar end
 * (@RETIRING2 at 1850 asks it like @RETIRING at 1800, FUN_3844_0442 LAB_3844_0b4a) resumes
 * the map with 0x5382 & 0x10 (calendar_latch) set and no endgame latch. That survives a save
 * and reload without re-running the chain, and a later loss ends the game with no second chain
 * (LAB_3844_04ec skips FUN_281f_0574 once the bit is set). */
static int case_keep_playing_survives_reload(void) {
  if (setup() != 0) return 1;
  remove(k_hof_path);
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed (keep playing)\n");
    return 1;
  }
  int rc = 1;
  uint8_t* data = NULL;
  ColonizeCol1Save file;
  ColonizeCol1Save loaded;
  col1_save_init(&file);
  col1_save_init(&loaded);
  game->col1_ok = true;
  game->col1.head.game_options.woi = 1;
  game->col1.head.year = 1850;
  if (!calendar_end_to_scored(game)) {
    fprintf(stderr, "the 1850 @RETIRING2 chain did not end in @SCORED\n");
    goto out;
  }
  if (!test_assets_step(game, COLONIZE_KEY_DOWN) || !test_assets_step(game, COLONIZE_KEY_ENTER) ||
      game->in_menu || ai_popup_busy(&game->ai_popups) ||
      !game->col1.head.game_options.calendar_latch ||
      ai_king_latch_get(&game->col1, AI_KING_ENDGAME_BYTE) != AI_KING_ENDGAME_NONE) {
    fprintf(stderr, "Keep playing did not resume with scoring complete and no endgame latch\n");
    goto out;
  }
  /* Save and reload: the end state (0x5382 and the endgame latch) goes through a real save
   * file's codec; a new session starts with clear port flags. */
  size_t size = 0;
  char err[128] = "";
  if (!col1_save_read_file(k_fixture_sav, &file, err, sizeof(err))) {
    fprintf(stderr, "fixture %s: %s\n", k_fixture_sav, err);
    goto out;
  }
  file.head.game_options = game->col1.head.game_options;
  ai_king_latch_set(&file, AI_KING_ENDGAME_BYTE, ai_king_latch_get(&game->col1, AI_KING_ENDGAME_BYTE));
  if (!col1_save_write_memory(&file, &data, &size, err, sizeof(err)) ||
      !col1_save_read_memory(data, size, &loaded, err, sizeof(err))) {
    fprintf(stderr, "col1 round trip failed: %s\n", err);
    goto out;
  }
  game->col1.head.game_options = loaded.head.game_options;
  ai_king_latch_set(&game->col1, AI_KING_ENDGAME_BYTE, ai_king_latch_get(&loaded, AI_KING_ENDGAME_BYTE));
  if (!game->col1.head.game_options.calendar_latch) {
    fprintf(stderr, "calendar_latch did not survive the save\n");
    goto out;
  }
  game->war_end_retired = false;
  game->scored_after_hof = false;
  for (int i = 0; i < 3; ++i) {
    if (!test_assets_step(game, COLONIZE_KEY_NONE) || game->in_report || game->in_hall_of_fame ||
        game->in_menu || ai_popup_busy(&game->ai_popups)) {
      fprintf(stderr, "the reloaded game re-ran the end chain (frame %d)\n", i);
      goto out;
    }
  }
  ai_king_latch_set(&game->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
  if (!test_assets_step(game, COLONIZE_KEY_NONE) || !at_menu(game)) {
    fprintf(stderr, "a loss after Keep playing did not end at the title menu without a chain\n");
    goto out;
  }
  rc = 0;
out:
  free(data);
  col1_save_free(&file);
  col1_save_free(&loaded);
  game_destroy(game);
  remove(k_hof_path);
  return rc;
}

static const TestCase k_cases[] = {
    {"empty_table", case_empty_table},
    {"dos_file_loads", case_dos_file_loads},
    {"insert_rule_and_screen", case_insert_rule_and_screen},
    {"save_round_trip", case_save_round_trip},
    {"click_dismisses_ending_screens", case_click_dismisses_ending_screens},
    {"keep_playing_survives_reload", case_keep_playing_survives_reload},
};

TEST_MAIN(k_cases)
