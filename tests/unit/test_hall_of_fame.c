#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include "core/game_loop.h"
#include "core/game_loop_internal.h"

#include "tests/common/test_assets.h"
#include "../common/test_runner.h"

/*
 * Hall of Fame: ranked HOF.TXT table (score|leader|nation|year|difficulty,
 * highest score first; backward-compat with the old single-integer-per-line
 * stub; re-ranks an unsorted/hand-edited file; caps at COLONIZE_HOF_MAX (10)
 * entries on load) AND the title-menu screen it opens into (a real
 * in_hall_of_fame render state via reports_render_hall_of_fame — not a
 * window-title status string). See docs/port_plan.md Phase 1 /
 * docs/manual_gap.md "Hall of Fame".
 */

/* Minimal title-menu assets: enough for game_create to reach the menu with a
 * "View Hall of Fame" option, nothing else (no wizard pages needed). */
static const char* k_data_dir = "./test-assets-hof";
static const char* k_save_dir = "./test-saves-hof";
static char k_hof_path[512];

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
  snprintf(k_hof_path, sizeof(k_hof_path), "%s/HOF.TXT", k_data_dir);
  return 0;
}

/* 1. No HOF.TXT yet: empty table; menu action opens a real screen (not a
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

/* 2. Legacy single-integer-per-line HOF.TXT still loads (as one entry). */
static int case_legacy_format(void) {
  if (setup() != 0) return 1;
  if (!test_assets_write_text_file(k_hof_path, "1200\n")) {
    fprintf(stderr, "failed to seed legacy HOF.TXT\n");
    return 1;
  }
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed (legacy)\n");
    return 1;
  }
  if (game_hof_count(game) != 1) {
    fprintf(stderr, "expected 1 legacy entry, got %d\n", game_hof_count(game));
    game_destroy(game);
    return 1;
  }
  ColonizeHofEntryView e;
  if (!game_hof_entry(game, 0, &e) || e.score != 1200) {
    fprintf(stderr, "legacy entry score mismatch\n");
    game_destroy(game);
    return 1;
  }
  game_destroy(game);
  return 0;
}

/* 3. Unsorted, multi-field HOF.TXT re-ranks highest-first on load, and the
 * ranked screen actually renders + Enter closes it. */
static int case_ranked_rerank_and_screen(void) {
  if (setup() != 0) return 1;
  if (!test_assets_write_text_file(
        k_hof_path,
        "800|Ann Bonny|English|1700|2\n"
        "1500|Jacques Cartier|French|1750|3\n"
        "1100|Miguel de Soto|Spanish|1720|1\n"
      )) {
    fprintf(stderr, "failed to seed ranked HOF.TXT\n");
    return 1;
  }
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed (ranked)\n");
    return 1;
  }
  if (game_hof_count(game) != 3) {
    fprintf(stderr, "expected 3 ranked entries, got %d\n", game_hof_count(game));
    game_destroy(game);
    return 1;
  }
  static const struct {
    int score;
    const char* leader;
    const char* nation;
    int year;
  } want[3] = {
    {1500, "Jacques Cartier", "French", 1750},
    {1100, "Miguel de Soto", "Spanish", 1720},
    {800, "Ann Bonny", "English", 1700},
  };
  for (int i = 0; i < 3; ++i) {
    ColonizeHofEntryView e;
    if (!game_hof_entry(game, i, &e)) {
      fprintf(stderr, "rank %d entry missing\n", i);
      game_destroy(game);
      return 1;
    }
    if (e.score != want[i].score || strcmp(e.leader, want[i].leader) != 0 ||
        strcmp(e.nation, want[i].nation) != 0 || e.year != want[i].year) {
      fprintf(
        stderr,
        "rank %d mismatch: got %d %s (%s) %d want %d %s (%s) %d\n",
        i,
        e.score,
        e.leader,
        e.nation,
        e.year,
        want[i].score,
        want[i].leader,
        want[i].nation,
        want[i].year
      );
      game_destroy(game);
      return 1;
    }
  }
  if (!open_hall_of_fame(game)) {
    fprintf(stderr, "open Hall of Fame (ranked) failed\n");
    game_destroy(game);
    return 1;
  }
  if (!game_in_hall_of_fame(game) || game_in_menu(game)) {
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
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) {
    fprintf(stderr, "Enter from Hall of Fame failed\n");
    game_destroy(game);
    return 1;
  }
  if (game_in_hall_of_fame(game) || !game_in_menu(game)) {
    fprintf(stderr, "expected Enter to return to title menu\n");
    game_destroy(game);
    return 1;
  }
  game_destroy(game);
  return 0;
}

/* 4. Table caps at COLONIZE_HOF_MAX (10): 12 distinct scores, lowest 2
 * dropped, order still highest-first. */
static int case_overflow_cap(void) {
  if (setup() != 0) return 1;
  FILE* f = fopen(k_hof_path, "wb");
  if (!f) {
    fprintf(stderr, "failed to seed overflow HOF.TXT\n");
    return 1;
  }
  for (int i = 1; i <= 12; ++i) {
    fprintf(f, "%d|Leader%d|Nation|1700|0\n", i * 100, i);
  }
  fclose(f);
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed (overflow)\n");
    return 1;
  }
  if (game_hof_count(game) != 10) {
    fprintf(stderr, "expected cap of 10 entries, got %d\n", game_hof_count(game));
    game_destroy(game);
    return 1;
  }
  ColonizeHofEntryView top, bottom;
  if (!game_hof_entry(game, 0, &top) || top.score != 1200) {
    fprintf(stderr, "expected top score 1200\n");
    game_destroy(game);
    return 1;
  }
  if (!game_hof_entry(game, 9, &bottom) || bottom.score != 300) {
    fprintf(stderr, "expected 10th-place score 300 (100/200 dropped), got %d\n", bottom.score);
    game_destroy(game);
    return 1;
  }
  game_destroy(game);
  remove(k_hof_path);
  return 0;
}

/* 5. #1115: an EMPTY leader field must not drop the fields after it, and the
 * row survives a game_hof_save (temp file + rename) -> reload round trip. */
static int case_empty_leader_round_trip(void) {
  if (setup() != 0) return 1;
  if (!test_assets_write_text_file(k_hof_path, "900||Dutch|1710|2|55|1|0|3\n")) {
    fprintf(stderr, "failed to seed empty-leader HOF.TXT\n");
    return 1;
  }
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  for (int pass = 0; pass < 2; ++pass) {
    ColonizeGameState* game = game_create(&cfg);
    if (!game) {
      fprintf(stderr, "game_create failed (empty leader pass %d)\n", pass);
      return 1;
    }
    ColonizeHofEntryView e;
    if (game_hof_count(game) != 1 || !game_hof_entry(game, 0, &e) || e.score != 900 ||
        e.leader[0] != '\0' || strcmp(e.nation, "Dutch") != 0 || e.year != 1710 ||
        e.difficulty != 2 || e.rating != 55 || !e.declared || e.achieved) {
      fprintf(stderr, "empty-leader row mangled on pass %d\n", pass);
      game_destroy(game);
      return 1;
    }
    if (pass == 0) {
      game_hof_save(game);
    }
    game_destroy(game);
  }
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

/* 6. Every retire-chain screen ends in DOS FUN_281f_03c0 (FUN_41f2_0092 score, 0b70
 * exploits, 0f56 Hall of Fame): any key or either mouse button goes on, one screen per
 * press. The port's Hall of Fame took Esc/Enter only. */
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
  rc = 0;
out:
  game_destroy(game);
  remove(k_hof_path);
  return rc;
}

static const TestCase k_cases[] = {
    {"empty_table", case_empty_table},
    {"legacy_format", case_legacy_format},
    {"ranked_rerank_and_screen", case_ranked_rerank_and_screen},
    {"overflow_cap", case_overflow_cap},
    {"empty_leader_round_trip", case_empty_leader_round_trip},
    {"click_dismisses_ending_screens", case_click_dismisses_ending_screens},
};

TEST_MAIN(k_cases)
