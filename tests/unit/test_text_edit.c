/*
 * Shared single-line text field (core/text_edit.c): caret, selection and the
 * editing keys the leader-name / colony-name boxes are expected to honour.
 */
#include <stdio.h>
#include <string.h>

#include "core/text_edit.h"
#include "platform/diagnostics.h"

static int g_failures;

static void fail(const char* what, const char* got, const char* want) {
  fprintf(stderr, "%s: got \"%s\", want \"%s\"\n", what, got, want);
  ++g_failures;
}

static void expect_buf(const char* what, const char* buf, const char* want) {
  if (strcmp(buf, want) != 0) {
    fail(what, buf, want);
  }
}

static void expect_int(const char* what, int got, int want) {
  if (got != want) {
    fprintf(stderr, "%s: got %d, want %d\n", what, got, want);
    ++g_failures;
  }
}

static ColonizeInputState key(ColonizeKey k, bool shift, bool ctrl) {
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = k;
  in.shift_held = shift;
  in.ctrl_held = ctrl;
  return in;
}

static ColonizeInputState typed(const char* text) {
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  const int n = (int)strlen(text);
  in.text_input_len = n < COLONIZE_TEXT_INPUT_MAX - 1 ? n : COLONIZE_TEXT_INPUT_MAX - 1;
  memcpy(in.text_input, text, (size_t)in.text_input_len);
  in.text_input[in.text_input_len] = '\0';
  return in;
}

int main(void) {
  diag_init(0, NULL);

  char buf[24];
  TextEditState st;
  ColonizeInputState in;

  /* Opens select-all (DOS field bit 0x80): typing replaces the seed name. */
  snprintf(buf, sizeof(buf), "%s", "Walter Raleigh");
  text_edit_reset(&st, buf, true);
  expect_int("select-all lo", text_edit_sel_lo(&st), 0);
  expect_int("select-all hi", text_edit_sel_hi(&st), 14);
  in = typed("J");
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("type over selection", buf, "J");
  expect_int("caret after replace", st.cursor, 1);

  /* Backspace on a full selection wipes it rather than one char. */
  snprintf(buf, sizeof(buf), "%s", "Jamestown");
  text_edit_reset(&st, buf, true);
  in = key(COLONIZE_KEY_BACKSPACE, false, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("backspace over selection", buf, "");

  /* Arrows move the caret; Delete removes forward. */
  snprintf(buf, sizeof(buf), "%s", "abcd");
  text_edit_reset(&st, buf, false);
  expect_int("reset caret at end", st.cursor, 4);
  in = key(COLONIZE_KEY_LEFT, false, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("two lefts", st.cursor, 2);
  in = key(COLONIZE_KEY_DELETE, false, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("delete forward", buf, "abd");
  in = key(COLONIZE_KEY_BACKSPACE, false, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("backspace back", buf, "ad");

  /* Insert at the caret, not at the end. */
  in = typed("XY");
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("insert at caret", buf, "aXYd");
  expect_int("caret after insert", st.cursor, 3);

  /* Shift+arrow extends; typing then replaces just that run. */
  snprintf(buf, sizeof(buf), "%s", "Plymouth");
  text_edit_reset(&st, buf, false);
  in = key(COLONIZE_KEY_LEFT, true, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("shift-left selection", text_edit_sel_hi(&st) - text_edit_sel_lo(&st), 2);
  in = typed("er");
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("replace shift selection", buf, "Plymouer");

  /* Home / End, and Ctrl+A. */
  in = key(COLONIZE_KEY_HOME, false, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("home", st.cursor, 0);
  if (text_edit_has_selection(&st)) {
    fprintf(stderr, "home should collapse the selection\n");
    ++g_failures;
  }
  in = key(COLONIZE_KEY_END, true, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("shift-end selects to end", text_edit_sel_hi(&st), 8);
  in = key(COLONIZE_KEY_LEFT, false, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("plain left collapses to selection start", st.cursor, 0);
  in = key(COLONIZE_KEY_A, false, true);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("ctrl+a lo", text_edit_sel_lo(&st), 0);
  expect_int("ctrl+a hi", text_edit_sel_hi(&st), 8);

  /* Ctrl+Left/Right jump whole words. */
  snprintf(buf, sizeof(buf), "%s", "New Amsterdam");
  text_edit_reset(&st, buf, false);
  in = key(COLONIZE_KEY_LEFT, false, true);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("ctrl+left to word start", st.cursor, 4);
  in = key(COLONIZE_KEY_RIGHT, false, true);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("ctrl+right to word end", st.cursor, 13);

  /* Cut / paste round-trips through the process-local clipboard. */
  text_edit_set_clipboard(NULL);
  snprintf(buf, sizeof(buf), "%s", "Quebec");
  text_edit_reset(&st, buf, true);
  in = key(COLONIZE_KEY_X, false, true);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("ctrl+x empties", buf, "");
  in = typed("Fort ");
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  in = key(COLONIZE_KEY_V, false, true);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("ctrl+v pastes", buf, "Fort Quebec");

  /* Insertion stops at capacity instead of running off the buffer. */
  char small[6];
  snprintf(small, sizeof(small), "%s", "abcd");
  text_edit_reset(&st, small, false);
  in = typed("XYZ");
  text_edit_handle_input(&st, small, sizeof(small), &in);
  expect_buf("capacity clamp", small, "abcdX");

  /* Enter / Esc are reported, not swallowed. */
  in = key(COLONIZE_KEY_ENTER, false, false);
  expect_int(
    "enter confirms",
    (int)text_edit_handle_input(&st, small, sizeof(small), &in),
    (int)TEXT_EDIT_ACTION_CONFIRM
  );
  in = key(COLONIZE_KEY_ESCAPE, false, false);
  expect_int(
    "escape cancels",
    (int)text_edit_handle_input(&st, small, sizeof(small), &in),
    (int)TEXT_EDIT_ACTION_CANCEL
  );

  if (g_failures > 0) {
    fprintf(stderr, "text_edit: %d failure(s)\n", g_failures);
    return 1;
  }
  printf("text_edit OK\n");
  return 0;
}
