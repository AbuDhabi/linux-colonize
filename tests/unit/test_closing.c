#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/closing.h"
#include "platform/diagnostics.h"

static int failures = 0;

static void check(bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  }
}

static int g_plays[64];
static int g_nplay;
static int g_bgm = -999;

static void cap_play(int id) {
  if (g_nplay < (int)(sizeof g_plays / sizeof g_plays[0])) {
    g_plays[g_nplay++] = id;
  }
}

static void cap_bgm(int id) {
  g_bgm = id;
}

static int last_play(void) {
  return g_nplay > 0 ? g_plays[g_nplay - 1] : -1;
}

static int count_play(int id) {
  int n = 0;
  for (int i = 0; i < g_nplay; ++i) {
    if (g_plays[i] == id) {
      n++;
    }
  }
  return n;
}

static void test_timeline(void) {
  ClosingSeries rows[CLOSING_SERIES_MAX];
  int end_frame = 0;
  const int n = closing_parse_timeline("COLONIZE", rows, CLOSING_SERIES_MAX, &end_frame);
  check(n == 7, "@CLOSING has 7 series");
  check(end_frame == 390, "end marker is frame 390");
  if (n < 7) {
    return;
  }
  check(rows[0].series == CLOSING_SHEET_FIREWORKS, "first series is fireworks");
  check(rows[1].series == CLOSING_SHEET_BELL, "second series is the bell");
  check(rows[2].series == CLOSING_SHEET_ROCK && rows[2].delay == 100, "rock waits 100 ticks");
  check(rows[3].series == CLOSING_SHEET_HAT && rows[3].delay == 16, "hat waits 16 ticks");
  check(rows[4].series == CLOSING_SHEET_LADY, "lady");
  check(rows[5].series == CLOSING_SHEET_MAN, "man");
  check(rows[6].series == CLOSING_SHEET_MIL, "military");
  for (int i = 0; i < n; ++i) {
    check(rows[i].repeats == -1, "live series loop");
    check(rows[i].frame == 1, "live series start at frame 1");
  }
}

static void test_open_and_skip(void) {
  ClosingCinematic c;
  memset(&c, 0, sizeof(c));
  if (!closing_open(&c, "COLONIZE")) {
    fprintf(stderr, "FAIL: closing_open\n");
    failures++;
    return;
  }
  check(c.open, "open");
  check(c.sheet_ok[CLOSING_SHEET_HAT], "hat sheet");
  check(c.sheet_ok[CLOSING_SHEET_FIREWORKS], "fireworks sheet");
  check(c.sheet_ok[CLOSING_SHEET_BELL], "bell sheet");
  check(c.end_frame == 390, "end_frame from TXT");
  check(c.clock == 0, "clock starts at 0");
  check(!c.finished, "starts unfinished");

  ClosingCinematic skip;
  memset(&skip, 0, sizeof(skip));
  if (!closing_open(&skip, "COLONIZE")) {
    fprintf(stderr, "FAIL: closing_open skip\n");
    closing_close(&c);
    failures++;
    return;
  }
  check(memcmp(c.canvas, skip.canvas, sizeof(c.canvas)) == 0, "same background at t0");

  closing_update(&c, CLOSING_FRAME_MS);
  check(c.clock == 1, "one tick advances the clock");
  check(!c.finished, "not done after one tick");
  check(memcmp(c.canvas, skip.canvas, sizeof(c.canvas)) != 0, "sprites appear on tick 1");

  /* Hat start = Frame 1 + Delay 16 = 17. At clock 1 the hat has not entered. */
  uint8_t after_first[320 * 200];
  memcpy(after_first, c.canvas, sizeof(after_first));
  closing_update(&c, CLOSING_FRAME_MS * 15); /* clock 16 */
  check(c.clock == 16, "clock 16, hat still waiting");
  check(memcmp(c.canvas, after_first, sizeof(after_first)) != 0, "other series keep cycling");

  closing_update(&c, CLOSING_FRAME_MS); /* clock 17, hat sprite 0 */
  check(c.clock == 17, "hat start tick");

  closing_skip_to_end(&skip);
  check(skip.finished, "skip finishes");
  check(skip.clock == skip.end_frame, "skip lands on the end marker");

  closing_update(&c, 30000);
  check(c.finished, "timed run completes");
  check(c.clock == c.end_frame, "timed run hits the end marker");
  check(memcmp(c.canvas, skip.canvas, sizeof(c.canvas)) == 0, "skip matches timed end frame");

  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ESCAPE;
  check(closing_handle_input(&c, &in), "open cinematic consumes input");
  check(!c.open, "key closes");

  closing_close(&skip);
}

static void test_sounds(void) {
  ClosingCinematic c;
  memset(&c, 0, sizeof(c));
  g_nplay = 0;
  g_bgm = -999;
  closing_set_sound_hooks(cap_play, cap_bgm);
  if (!closing_open(&c, "COLONIZE")) {
    fprintf(stderr, "FAIL: closing_open sound\n");
    failures++;
    closing_set_sound_hooks(NULL, NULL);
    return;
  }
  check(g_bgm == 0, "CLOSING.EXE clears the VICEROY pool");
  check(g_nplay == 1 && g_plays[0] == CLOSING_BGM_ID, "open plays 0x3d");

  closing_update(&c, CLOSING_FRAME_MS);
  check(c.clock == 1, "clock 1");
  check(count_play(CLOSING_FIREWORK_SOUND_ID) == 0, "no 0x59 on first fireworks tick");
  check(count_play(CLOSING_CHEER_SOUND_ID) == 0, "no 0x5a before hats");

  closing_update(&c, CLOSING_FRAME_MS);
  check(c.clock == 2, "clock 2");
  check(last_play() == CLOSING_FIREWORK_SOUND_ID, "0x59 when fireworks frame is 1");
  check(count_play(CLOSING_FIREWORK_SOUND_ID) == 1, "one firework sting");

  closing_update(&c, CLOSING_FRAME_MS * 15); /* clock 17 */
  check(c.clock == 17, "hat start");
  check(last_play() == CLOSING_CHEER_SOUND_ID, "0x5a on first hat sprite");
  check(count_play(CLOSING_CHEER_SOUND_ID) == 1, "one cheer so far");

  ClosingCinematic skip;
  memset(&skip, 0, sizeof(skip));
  const int before_skip = g_nplay;
  if (!closing_open(&skip, "COLONIZE")) {
    fprintf(stderr, "FAIL: closing_open skip sound\n");
    failures++;
    closing_close(&c);
    closing_set_sound_hooks(NULL, NULL);
    return;
  }
  check(g_nplay == before_skip + 1, "skip open only adds 0x3d");
  closing_skip_to_end(&skip);
  check(g_nplay == before_skip + 1, "skip does not burst SFX");
  closing_close(&skip);
  closing_close(&c);
  closing_set_sound_hooks(NULL, NULL);
}

int main(void) {
  diag_init(0, NULL);
  test_timeline();
  test_open_and_skip();
  test_sounds();
  diag_shutdown();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  return 0;
}
