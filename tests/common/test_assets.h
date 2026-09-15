#ifndef COLONIZE_TESTS_COMMON_TEST_ASSETS_H
#define COLONIZE_TESTS_COMMON_TEST_ASSETS_H

/*
 * Synthetic COLONIZE/ data tree for the tests that drive the real game loop
 * without the original game files (duplication audit TT-19 / TT-20 / TT-21).
 * test_game_flow.c, test_new_game.c and test_hall_of_fame.c each carried
 * their own copy of the file writer, the 1024-byte VICEROY.PAL generator and
 * a variant of the tree itself; the variants differ only in which GAME.TXT
 * blocks and how much NAMES.TXT they need, so those are flags.
 */

#include <stdbool.h>

#include "core/game_loop.h"

enum {
  /* Title menu with all five DOS options (NEW WORLD / AMERICA / LOAD /
   * Hall of Fame / Exit) and a literal version string. Without it the menu
   * is test_game_flow's minimal three-option block with "%STRING0". */
  TEST_ASSETS_FULL_MENU = 1u << 0,
  /* @AMERICA..@BUILD2: the new-game wizard pages. */
  TEST_ASSETS_WIZARD_PAGES = 1u << 1,
  /* NAMES.TXT with one @LEADERNAME row, @SCENARIO and one @UNIT row. */
  TEST_ASSETS_NAMES = 1u << 2,
  /* NAMES.TXT with all four leaders and three unit rows (wizard needs them). */
  TEST_ASSETS_NAMES_FULL = 1u << 3
};

bool test_assets_write_text_file(const char* path, const char* content);
bool test_assets_write_palette(const char* path);

/* Creates `dir` (ignoring "already exists") and writes MODULES.DB, ERRORS.DB,
 * MENU.TXT, GAME.TXT, VICEROY.PAL and — with either NAMES flag — NAMES.TXT. */
bool test_assets_create(const char* dir, unsigned flags);

/* One keypress through the real game loop. */
bool test_assets_step(ColonizeGameState* game, ColonizeKey key);

#endif /* COLONIZE_TESTS_COMMON_TEST_ASSETS_H */
