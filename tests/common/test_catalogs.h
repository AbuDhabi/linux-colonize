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

/* GAME.TXT @section, or NULL. */
static inline const ColonizeMsgSection* test_game_section(const char* section) {
  const ColonizeMsgCatalog* cat = test_game_txt();
  return cat ? assets_msg_find(cat, section) : NULL;
}

/* Next token-free run (no %TOKEN, {markup}, caret or quote) of a catalog line,
 * starting at *pos: sets *run / *len and moves *pos past it. False at the end. */
static inline bool test_line_next_run(const char* line, size_t* pos, const char** run, size_t* len) {
  const size_t n = strlen(line);
  if (*pos > n) {
    return false;
  }
  size_t k = *pos;
  while (line[k] && line[k] != '%' && line[k] != '{' && line[k] != '}' && line[k] != '^' &&
         line[k] != '"') {
    k++;
  }
  *run = line + *pos;
  *len = k - *pos;
  size_t next = k + 1;
  if (line[k] == '%') { /* a %TOKEN runs to the next non-identifier char */
    while (line[next] == '$' || (line[next] >= 'A' && line[next] <= 'Z') ||
           (line[next] >= '0' && line[next] <= '9')) {
      next++;
    }
  }
  *pos = next;
  return true;
}

/* Longest token-free run found on any content line of the section, copied to
 * out. False if none is >= 8 chars. */
static inline bool test_section_needle(const char* section, char* out, size_t out_size) {
  const ColonizeMsgSection* sec = test_game_section(section);
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
    size_t pos = 0;
    const char* run;
    size_t len;
    while (test_line_next_run(line, &pos, &run, &len)) {
      if (len > best_len) {
        best_ptr = run;
        best_len = len;
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

/* For a section with no 8-char run (@SHIPSUNK: four tokens between three short
 * runs): true when every non-blank run of one content line appears in body in
 * order, and those runs total at least 8 chars. */
static inline bool test_body_has_line_runs(const char* body, const ColonizeMsgSection* sec) {
  for (int i = 0; i < sec->line_count; ++i) {
    const char* line = sec->lines[i];
    if (!line[0] || line[0] == '@') {
      continue;
    }
    const char* at = body;
    size_t total = 0;
    size_t pos = 0;
    const char* run;
    size_t len;
    while (at && test_line_next_run(line, &pos, &run, &len)) {
      if (strspn(run, " ") >= len) {
        continue;
      }
      char piece[COLONIZE_MSG_LINE_LEN];
      if (len >= sizeof(piece)) {
        at = NULL;
        break;
      }
      memcpy(piece, run, len);
      piece[len] = '\0';
      at = strstr(at, piece);
      if (at) {
        at += len;
        total += len;
      }
    }
    if (at && total >= 8) {
      return true;
    }
  }
  return false;
}

/* True when `body` carries the text of GAME.TXT @section. */
static inline bool test_body_is_section(const char* body, const char* section) {
  char needle[COLONIZE_MSG_LINE_LEN];
  if (!body) {
    return false;
  }
  if (test_section_needle(section, needle, sizeof(needle))) {
    return strstr(body, needle) != NULL;
  }
  const ColonizeMsgSection* sec = test_game_section(section);
  return sec && test_body_has_line_runs(body, sec);
}

#endif /* COLONIZE_TESTS_COMMON_TEST_CATALOGS_H */
