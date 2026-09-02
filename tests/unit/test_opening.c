#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/opening.h"
#include "platform/diagnostics.h"

static int failures = 0;

static void check(bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  }
}

static void test_timeline(void) {
  OpeningSeries rows[OPENING_SERIES_MAX];
  int end_frame = 0;
  const int n = opening_parse_timeline("COLONIZE", rows, OPENING_SERIES_MAX, &end_frame);
  check(n == 11, "@OPENING has 11 series");
  check(end_frame == 891, "end marker is frame 891");
  if (n < 11) {
    return;
  }
  check(rows[0].series == OPENING_SHEET_WIND1 && rows[0].base_x == 640, "wind 1 at world 640");
  check(rows[2].series == OPENING_SHEET_SUN && rows[2].frame == 40, "sun at 40");
  check(rows[8].series == OPENING_SHEET_BONK && rows[8].frame == 701, "bonk at landfall");
  check(rows[9].series == OPENING_SHEET_GUY, "guy");
  check(rows[10].series == OPENING_SHEET_LOGO && rows[10].frame == 767, "logo");
}

static void test_credits_and_path(void) {
  OpeningCredit credits[OPENING_CREDIT_MAX];
  const int n = opening_parse_credits("COLONIZE", credits, OPENING_CREDIT_MAX);
  check(n >= 10, "@CREDITS has plates");
  if (n > 0) {
    check(credits[0].start_frame == 25 && credits[0].series == 0 && credits[0].sprite == 0,
          "first credit is 1-based sprite 1 → 0");
  }
  int xs[OPENING_PATH_MAX];
  int ys[OPENING_PATH_MAX];
  const int p = opening_parse_path("COLONIZE", xs, ys, OPENING_PATH_MAX);
  check(p == 701, "PATH.DAT has 701 points");
  if (p > 0) {
    check(xs[0] == 868 && ys[0] == 89, "ship starts at 868,89");
    check(xs[p - 1] == 161 && ys[p - 1] == 114, "ship ends at 161,114");
  }
}

static int g_play_id = -1;

static void test_capture_play(int id) {
  g_play_id = id;
}

static void test_open_skip_and_motion(void) {
  OpeningCinematic o;
  memset(&o, 0, sizeof(o));
  g_play_id = -1;
  opening_set_sound_hooks(test_capture_play, NULL);
  if (!opening_open(&o, "COLONIZE")) {
    fprintf(stderr, "FAIL: opening_open\n");
    failures++;
    return;
  }
  check(o.open, "open");
  check(g_play_id == 0x34, "OPENING.EXE cue is 0x34");
  check(o.scene_ok && o.border_ok, "panorama + border");
  check(o.ship_ok, "ship sheet");
  check(o.path_count == 701, "path loaded");
  {
    int bonk_at = -1;
    for (int i = 0; i < o.series_count; ++i) {
      if (o.series[i].series == OPENING_SHEET_BONK) {
        bonk_at = o.series[i].frame;
        break;
      }
    }
    check(bonk_at == o.path_count, "OPENSHIP path ends where OPENBONK starts");
  }
  {
    const ColonizeSprite* ship = &o.ship.sprites[0];
    const ColonizeSpriteSheet* bonk = &o.sheets[OPENING_SHEET_BONK];
    check(bonk && o.sheet_ok[OPENING_SHEET_BONK] && bonk->sprite_count > 0, "bonk sheet");
    if (ship && bonk && bonk->sprite_count > 0) {
      const ColonizeSprite* still = &bonk->sprites[bonk->sprite_count - 1];
      const int ship_x =
        o.path_x[o.path_count - 1] - (ship->width >> 1) + OPENING_SHIP_X_ALIGN;
      const int ship_y = o.path_y[o.path_count - 1] - (ship->height >> 1);
      const int still_x = still->anchor_x - (still->width >> 1);
      const int still_y = still->anchor_y - still->height + 1;
      check(ship_x == still_x, "landed ship lines up with bonk still in x");
      check(ship_y == still_y, "landed ship lines up with bonk still in y");
    }
  }
  check(o.end_frame == 891, "end_frame from TXT");
  check(o.clock == 0, "clock starts at 0");
  check(!o.finished, "starts unfinished");
  check(o.skip_presses == 0, "no skip yet");
  check(o.mps_logo_ok && o.mps_logo.sprite_count == 16, "MPSLOGO.SS");
  check(o.mps_name_ok && o.mps_name.sprite_count == 29, "MPSNAME.SS");
  check(o.logo_phase, "starts on the MPS logo");

  uint8_t logo0[320 * 200];
  memcpy(logo0, o.canvas, sizeof(logo0));

  opening_update(&o, OPENING_FRAME_MS * 2);
  check(o.logo_phase && o.logo_clock == 2, "logo ticks");
  check(memcmp(o.canvas, logo0, sizeof(logo0)) != 0, "logo spun");

  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ESCAPE;
  check(opening_handle_input(&o, &in), "consumes input");
  check(o.open && !o.finished && !o.logo_phase, "one key skips logo into sailing");
  check(o.clock == 0 && o.skip_presses == 0, "sailing skip not armed");

  uint8_t first[320 * 200];
  memcpy(first, o.canvas, sizeof(first));

  opening_update(&o, OPENING_FRAME_MS * 2);
  check(o.clock == 2, "two sailing ticks");
  check(memcmp(o.canvas, first, sizeof(first)) != 0, "ship moved a pixel by tick 2");

  in.last_key = COLONIZE_KEY_ESCAPE;
  check(opening_handle_input(&o, &in), "first sailing key");
  check(o.open && !o.finished, "first key does not skip");
  check(o.skip_presses == 1, "one press armed");

  in.last_key = COLONIZE_KEY_ENTER;
  check(opening_handle_input(&o, &in), "second key");
  check(!o.finished, "second key holds the last frame");
  check(o.clock == o.end_frame, "skip jumps to the end marker");
  check(o.open, "still open until the owner closes");

  opening_update(&o, OPENING_HOLD_MS - 1);
  check(!o.finished, "hold not done yet");
  opening_update(&o, 1);
  check(o.finished, "1s hold finishes");

  opening_close(&o);
  check(!o.open, "close");
}

static void test_timed_run_reaches_end(void) {
  OpeningCinematic o;
  memset(&o, 0, sizeof(o));
  if (!opening_open(&o, "COLONIZE")) {
    fprintf(stderr, "FAIL: opening_open timed\n");
    failures++;
    return;
  }
  opening_update(&o, OPENING_FRAME_MS * (OPENING_LOGO_END_FRAME + 2));
  check(!o.logo_phase, "logo ends on its own");
  check(o.clock == 0, "sailing clock starts after logo");
  opening_update(&o, OPENING_FRAME_MS * 900);
  check(!o.finished, "end frame holds before the menu");
  check(o.clock == o.end_frame, "lands on end marker");
  opening_update(&o, OPENING_HOLD_MS - 1);
  check(!o.finished, "hold not done yet");
  opening_update(&o, 1);
  check(o.finished, "1s hold then finished");
  opening_close(&o);
}

int main(void) {
  diag_init(0, NULL);
  test_timeline();
  test_credits_and_path();
  test_open_skip_and_motion();
  test_timed_run_reaches_end();
  diag_shutdown();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  return 0;
}
