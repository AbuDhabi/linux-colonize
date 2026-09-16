/*
 * Shared single-line text field (core/text_edit.c): caret, selection and the
 * editing keys the leader-name / colony-name boxes are expected to honour.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/text_edit.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

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

/* Opens select-all (DOS field bit 0x80): typing replaces the seed name. */
static void test_select_all_replace(void) {
  char buf[24];
  TextEditState st;
  ColonizeInputState in;

  snprintf(buf, sizeof(buf), "%s", "Walter Raleigh");
  text_edit_reset(&st, buf, true);
  expect_int("select-all lo", text_edit_sel_lo(&st), 0);
  expect_int("select-all hi", text_edit_sel_hi(&st), 14);
  in = typed("J");
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("type over selection", buf, "J");
  expect_int("caret after replace", st.cursor, 1);
}

/* Backspace on a full selection wipes it rather than one char. */
static void test_backspace_over_selection(void) {
  char buf[24];
  TextEditState st;
  ColonizeInputState in;

  snprintf(buf, sizeof(buf), "%s", "Jamestown");
  text_edit_reset(&st, buf, true);
  in = key(COLONIZE_KEY_BACKSPACE, false, false);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_buf("backspace over selection", buf, "");
}

/* Arrows move the caret, Delete/Backspace remove around it, and insert
 * lands at the caret rather than the end. */
static void test_arrows_delete_insert(void) {
  char buf[24];
  TextEditState st;
  ColonizeInputState in;

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
}

/* Shift+arrow extends a selection, typing replaces just that run, and
 * Home/End/Ctrl+A behave as expected on the result. */
static void test_shift_select_and_home_end(void) {
  char buf[24];
  TextEditState st;
  ColonizeInputState in;

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
}

/* Ctrl+Left/Right jump whole words. */
static void test_ctrl_arrow_word_jump(void) {
  char buf[24];
  TextEditState st;
  ColonizeInputState in;

  snprintf(buf, sizeof(buf), "%s", "New Amsterdam");
  text_edit_reset(&st, buf, false);
  in = key(COLONIZE_KEY_LEFT, false, true);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("ctrl+left to word start", st.cursor, 4);
  in = key(COLONIZE_KEY_RIGHT, false, true);
  text_edit_handle_input(&st, buf, sizeof(buf), &in);
  expect_int("ctrl+right to word end", st.cursor, 13);
}

/* Cut / paste round-trips through the process-local clipboard. */
static void test_cut_paste_clipboard(void) {
  char buf[24];
  TextEditState st;
  ColonizeInputState in;

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
}

/* Insertion stops at capacity instead of running off the buffer, and
 * Enter/Esc are reported to the caller rather than swallowed. */
static void test_capacity_clamp_and_confirm_cancel(void) {
  char small[6];
  TextEditState st;
  ColonizeInputState in;

  snprintf(small, sizeof(small), "%s", "abcd");
  text_edit_reset(&st, small, false);
  in = typed("XYZ");
  text_edit_handle_input(&st, small, sizeof(small), &in);
  expect_buf("capacity clamp", small, "abcdX");

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
}

/* These cases accumulate into the shared `g_failures` counter via the
 * expect_int/expect_buf/fail() helpers rather than returning pass/fail
 * directly; wrap each so the table-driven runner can report per-case
 * PASS/FAIL. */
static int case_test_select_all_replace(void) {
  int before = g_failures;
  test_select_all_replace();
  return g_failures != before;
}

static int case_test_backspace_over_selection(void) {
  int before = g_failures;
  test_backspace_over_selection();
  return g_failures != before;
}

static int case_test_arrows_delete_insert(void) {
  int before = g_failures;
  test_arrows_delete_insert();
  return g_failures != before;
}

static int case_test_shift_select_and_home_end(void) {
  int before = g_failures;
  test_shift_select_and_home_end();
  return g_failures != before;
}

static int case_test_ctrl_arrow_word_jump(void) {
  int before = g_failures;
  test_ctrl_arrow_word_jump();
  return g_failures != before;
}

static int case_test_cut_paste_clipboard(void) {
  int before = g_failures;
  test_cut_paste_clipboard();
  return g_failures != before;
}

static int case_test_capacity_clamp_and_confirm_cancel(void) {
  int before = g_failures;
  test_capacity_clamp_and_confirm_cancel();
  return g_failures != before;
}

static const TestCase k_cases[] = {
    {"test_select_all_replace", case_test_select_all_replace},
    {"test_backspace_over_selection", case_test_backspace_over_selection},
    {"test_arrows_delete_insert", case_test_arrows_delete_insert},
    {"test_shift_select_and_home_end", case_test_shift_select_and_home_end},
    {"test_ctrl_arrow_word_jump", case_test_ctrl_arrow_word_jump},
    {"test_cut_paste_clipboard", case_test_cut_paste_clipboard},
    {"test_capacity_clamp_and_confirm_cancel", case_test_capacity_clamp_and_confirm_cancel},
};

int main(void) {
  diag_init(0, NULL);
  int rc = tr_run_main(k_cases, (int)(sizeof(k_cases) / sizeof(k_cases[0])));
  if (rc == 0 && getenv("COLONIZE_TEST_LIST") == NULL) {
    printf("text_edit OK\n");
  }
  return rc;
}
