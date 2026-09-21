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

static const TestCase k_cases[] = {
    {"empty_table", case_empty_table},
    {"legacy_format", case_legacy_format},
    {"ranked_rerank_and_screen", case_ranked_rerank_and_screen},
    {"overflow_cap", case_overflow_cap},
};

TEST_MAIN(k_cases)
