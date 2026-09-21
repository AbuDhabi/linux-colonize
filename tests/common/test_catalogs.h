#ifndef COLONIZE_TESTS_COMMON_TEST_CATALOGS_H
#define COLONIZE_TESTS_COMMON_TEST_CATALOGS_H

/*
 * Real-catalog access for tests.
 *
 * The port compiles none of the game's wording, so a test cannot recognise a
 * popup by a sentence typed into the test either. These helpers read the
 * shipped COLONIZE/GAME.TXT (cwd = repo root) and answer "is this body the
 * text of @SECTION?" from the catalog itself.
 */

#include <stdbool.h>
#include <string.h>

#include "core/assets.h"

static inline const ColonizeMsgCatalog* test_game_txt(void) {
  static ColonizeMsgCatalog cat;
  static int state = 0; /* 0 untried, 1 ok, -1 failed */
  if (state == 0) {
    assets_msg_init(&cat);
    state = assets_msg_load_file(&cat, "COLONIZE/GAME.TXT") ? 1 : -1;
  }
  return state == 1 ? &cat : NULL;
}

/* The shipped NAMES.TXT, for contexts that read @ACTIONS / @LEVELS / ... */
static inline const ColonizeMsgCatalog* test_names_txt(void) {
  static ColonizeMsgCatalog cat;
  static int state = 0;
  if (state == 0) {
    assets_msg_init(&cat);
    state = assets_msg_load_file(&cat, "COLONIZE/NAMES.TXT") ? 1 : -1;
  }
  return state == 1 ? &cat : NULL;
}

/* The shipped LABELS.TXT / MENU.TXT (screen chrome reads them). */
static inline const ColonizeMsgCatalog* test_labels_txt(void) {
  static ColonizeMsgCatalog cat;
  static int state = 0;
  if (state == 0) {
    assets_msg_init(&cat);
    state = assets_msg_load_file(&cat, "COLONIZE/LABELS.TXT") ? 1 : -1;
  }
  return state == 1 ? &cat : NULL;
}

static inline const ColonizeMsgCatalog* test_menu_txt(void) {
  static ColonizeMsgCatalog cat;
  static int state = 0;
  if (state == 0) {
    assets_msg_init(&cat);
    state = assets_msg_load_file(&cat, "COLONIZE/MENU.TXT") ? 1 : -1;
  }
  return state == 1 ? &cat : NULL;
}

/* Longest token-free run (no %TOKEN, {markup}, caret or quote) found on any
 * content line of the section, copied to out. False if none is >= 8 chars. */
static inline bool test_section_needle(const char* section, char* out, size_t out_size) {
  const ColonizeMsgCatalog* cat = test_game_txt();
  const ColonizeMsgSection* sec = cat ? assets_msg_find(cat, section) : NULL;
  if (!sec || !out || out_size == 0) {
    return false;
  }
  const char* best_ptr = NULL;
  size_t best_len = 0;
  for (int i = 0; i < sec->line_count; ++i) {
    const char* line = sec->lines[i];
    if (!line[0] || line[0] == '@') {
      continue;
    }
    size_t start = 0;
    const size_t n = strlen(line);
    for (size_t k = 0; k <= n; ++k) {
      const char c = line[k];
      if (c == '\0' || c == '%' || c == '{' || c == '}' || c == '^' || c == '"') {
        if (k - start > best_len) {
          best_ptr = line + start;
          best_len = k - start;
        }
        start = k + 1;
        if (c == '%') { /* a %TOKEN runs to the next non-identifier char */
          while (line[start] == '$' || (line[start] >= 'A' && line[start] <= 'Z') ||
                 (line[start] >= '0' && line[start] <= '9')) {
            start++;
          }
          k = start - 1;
        }
      }
    }
  }
  if (!best_ptr || best_len < 8) {
    return false;
  }
  if (best_len >= out_size) {
    best_len = out_size - 1;
  }
  memcpy(out, best_ptr, best_len);
  out[best_len] = '\0';
  return true;
}

/* True when `body` carries the text of GAME.TXT @section. */
static inline bool test_body_is_section(const char* body, const char* section) {
  char needle[COLONIZE_MSG_LINE_LEN];
  return body && test_section_needle(section, needle, sizeof(needle)) &&
         strstr(body, needle) != NULL;
}

#endif /* COLONIZE_TESTS_COMMON_TEST_CATALOGS_H */
