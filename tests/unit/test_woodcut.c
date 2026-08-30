#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/woodcut.h"
#include "platform/diagnostics.h"

/*
 * Milestone woodcuts: FUN_12fd_006c's once-only bit array + tune pick, and
 * FUN_6f30_0062's WOODFRAM / WDCUTnn / NAMEPLAT / FONT-NP screen. See
 * src/core/woodcut.h.
 */

static int failures = 0;

static void check(bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  }
}

static int tune_calls;
static int last_tune;
static int bgm_calls;
static int last_bgm;

static void fake_play(int id) {
  tune_calls++;
  last_tune = id;
}

static void fake_bgm(int pool) {
  bgm_calls++;
  last_bgm = pool;
}

/* FUN_12fd_0048 / FUN_12fd_000e address DS:0x540a 1-based from event's LSB. */
static void test_once_only_bits(void) {
  ColonizeCol1Save save;
  memset(&save, 0, sizeof(save));
  woodcut_clear_pending();
  woodcut_set_sound_hooks(NULL, NULL);

  check(woodcut_fire(&save, WOODCUT_DISCOVERY_OF_THE_NEW_WORLD), "first fire arms");
  check(save.head.event.discovery_of_the_new_world == 1, "id 1 sets event bit 1");
  check(woodcut_has_pending(), "armed woodcut is queued");
  check(woodcut_take_pending() == WOODCUT_DISCOVERY_OF_THE_NEW_WORLD, "queue returns the id");
  check(!woodcut_has_pending(), "queue drains");

  check(!woodcut_fire(&save, WOODCUT_DISCOVERY_OF_THE_NEW_WORLD), "second fire is a no-op");
  check(!woodcut_has_pending(), "no-op fire queues nothing");

  check(woodcut_fire(&save, WOODCUT_INDIAN_RAID), "id 13 arms");
  check(save.head.event.indian_raid == 1, "id 13 sets event bit 13");

  /* Bits 17-32 live in unknown05 — the same 32-bit array DOS memsets. */
  check(woodcut_fire(&save, 17), "id 17 arms");
  check(save.head.unknown05[0] == 0x01, "id 17 is unknown05[0] bit 0");
  check(woodcut_fire(&save, 25), "id 25 arms");
  check(save.head.unknown05[1] == 0x01, "id 25 is unknown05[1] bit 0");

  check(!woodcut_fire(&save, 0), "id 0 is not a gated id");
  woodcut_clear_pending();
}

/* The FUN_12fd_006c jump table: pool 2 for 0/1/2/9, an event id for 3-6. */
static void test_tune_table(void) {
  ColonizeCol1Save save;
  memset(&save, 0, sizeof(save));
  woodcut_clear_pending();
  woodcut_set_sound_hooks(fake_play, fake_bgm);

  tune_calls = bgm_calls = 0;
  (void)woodcut_fire(&save, WOODCUT_BUILDING_A_COLONY);
  check(bgm_calls == 1 && last_bgm == 2, "id 2 switches to tune pool 2");
  check(tune_calls == 0, "id 2 pushes no event id");

  tune_calls = bgm_calls = 0;
  (void)woodcut_fire(&save, WOODCUT_THE_AZTEC_EMPIRE);
  check(tune_calls == 1 && last_tune == 0x35, "id 4 pushes event 0x35");

  tune_calls = bgm_calls = 0;
  (void)woodcut_fire(&save, WOODCUT_DISCOVERY_OF_THE_PACIFIC_OCEAN);
  check(tune_calls == 1 && last_tune == 0x39, "id 6 pushes event 0x39");

  tune_calls = bgm_calls = 0;
  (void)woodcut_fire(&save, WOODCUT_ENTERING_INDIAN_VILLAGE);
  check(tune_calls == 0 && bgm_calls == 0, "id 7 has no table entry");

  woodcut_set_sound_hooks(NULL, NULL);
  woodcut_clear_pending();
}

static void test_screen(void) {
  static ColonizeWoodcutScreen w;
  if (!woodcut_open(&w, "COLONIZE", WOODCUT_DISCOVERY_OF_THE_NEW_WORLD)) {
    fprintf(stderr, "FAIL: woodcut_open(1)\n");
    failures++;
    return;
  }
  check(w.open, "screen is open");
  check(
    strcmp(w.caption, "DISCOVERY OF THE NEW WORLD") == 0,
    "caption is WOODCUT.TXT @WOODCUT line 1, with no year prefix"
  );
  check(w.palette_ok, "WDCUT01.SS palette is the screen palette");

  /*
   * WOODFRAM.SS is 274x170 with anchor (160,184), so FUN_6f30_002e lands it
   * at (23,15): the canvas is inked there and black in the top-left corner.
   */
  check(w.canvas[0] == 0, "corner outside the frame stays black");
  check(w.canvas[100 * 320 + 160] != 0, "the middle of the frame is drawn");

  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_NONE;
  (void)woodcut_handle_input(&w, &in);
  check(w.open, "no input keeps the screen up");
  in.last_key = COLONIZE_KEY_ENTER;
  (void)woodcut_handle_input(&w, &in);
  check(!w.open, "any key dismisses");

  woodcut_close(&w);
}

static void test_missing_assets(void) {
  static ColonizeWoodcutScreen w;
  memset(&w, 0, sizeof(w));
  check(!woodcut_open(&w, "no-such-dir", 1), "missing art fails cleanly");
  check(!w.open, "failed open leaves the screen closed");
  /* Id 12 has art but no DOS call site; id 14+ has neither. */
  check(!woodcut_open(&w, "COLONIZE", 14), "id 14 has no WDCUT14.SS");
}

int main(void) {
  diag_init(0, NULL);
  test_once_only_bits();
  test_tune_table();
  test_screen();
  test_missing_assets();
  diag_shutdown();
  if (failures) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  printf("ok\n");
  return 0;
}
