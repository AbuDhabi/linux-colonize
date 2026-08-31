#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/declaration.h"
#include "platform/diagnostics.h"

/*
 * FUN_43f7_160a Declaration-of-Independence signing cinematic: DECOIND.PIK
 * plus one DEC-UPP{A..Z}/DEC-LOW{a..z}.SS sheet per letter, closed by
 * DEC-SQIG.SS when the run reaches x = 0xdc. See src/core/declaration.h.
 */

static int failures = 0;

static void check(bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  }
}

static void test_title_case(void) {
  char a[32];
  snprintf(a, sizeof(a), "UNITED COLONIES");
  declaration_title_case(a);
  check(strcmp(a, "United Colonies") == 0, "strlwr + word-initial upcase");

  char b[32];
  snprintf(b, sizeof(b), "les provinces-unies");
  declaration_title_case(b);
  check(strcmp(b, "Les Provinces-Unies") == 0, "hyphen is a word boundary");

  char c[32];
  c[0] = '\0';
  declaration_title_case(c);
  check(c[0] == '\0', "empty name survives");
}

static void test_run_layout(void) {
  DeclarationCinematic d;
  memset(&d, 0, sizeof(d));
  if (!declaration_open(&d, "COLONIZE", "United Colonies")) {
    fprintf(stderr, "FAIL: declaration_open(United Colonies)\n");
    failures++;
    return;
  }
  check(strcmp(d.name, "United Colonies") == 0, "signature text is title-cased");
  check(d.glyph_count > 0, "glyph run is non-empty");
  check(d.glyphs[0].x == DECLARATION_START_X, "run starts at DOS x = 0x7e");
  check(d.glyphs[0].y == DECLARATION_START_Y, "run starts at DOS y = 0x94");
  check(d.glyphs[0].sheet == 'U' - 'A', "first glyph is the upper-case sheet");
  check(d.glyphs[0].frames == 10, "upper-case letters play 10 frames");
  check(d.glyphs[1].sheet == 26 + ('n' - 'a'), "second glyph is the lower-case sheet");
  check(d.glyphs[1].frames == 7, "lower-case letters play 7 frames");
  /* DOS local_5a: the signature slants up as it is written. */
  check(d.glyphs[1].y == DECLARATION_START_Y - 3, "upper-case advances y by -3");
  check(d.glyphs[2].y == d.glyphs[1].y - 2, "lower-case advances y by -2");

  /* "United Colonies" overruns the signature line, so DOS closes with
   * DEC-SQIG and stops before the remaining letters. */
  const DeclarationGlyph* last = &d.glyphs[d.glyph_count - 1];
  check(last->sheet == DECLARATION_SQUIGGLE_SHEET, "overrun ends on the flourish");
  check(last->x >= DECLARATION_END_X, "flourish only once x reached 0xdc");
  check(d.glyph_count < (int)strlen(d.name), "run stops early on overrun");

  /* A space contributes a 3px gap with no sprite (DOS local_6 = 3). */
  bool saw_gap = false;
  for (int i = 1; i < d.glyph_count; ++i) {
    if (d.glyphs[i].sheet < 0) {
      saw_gap = true;
      check(d.glyphs[i].frames == 0, "gap glyph draws nothing");
      check(d.glyphs[i + 1].x == d.glyphs[i].x + 3, "gap advances x by 3");
      check(d.glyphs[i + 1].y == d.glyphs[i].y - 1, "gap advances y by -1");
    }
  }
  check(saw_gap, "the space in the name is a gap glyph");
  declaration_close(&d);
  check(!d.open, "close clears open");
}

static void test_animation_and_skip(void) {
  DeclarationCinematic run;
  DeclarationCinematic skip;
  memset(&run, 0, sizeof(run));
  memset(&skip, 0, sizeof(skip));
  if (!declaration_open(&run, "COLONIZE", "Free States") ||
      !declaration_open(&skip, "COLONIZE", "Free States")) {
    fprintf(stderr, "FAIL: declaration_open(Free States)\n");
    failures++;
    return;
  }

  /* Nothing drawn yet: the canvases are the bare parchment and match. */
  check(memcmp(run.canvas, skip.canvas, sizeof(run.canvas)) == 0, "same background");
  check(!run.finished, "starts unfinished");

  declaration_update(&run, 8);
  check(memcmp(run.canvas, skip.canvas, sizeof(run.canvas)) == 0, "under one frame draws nothing");
  declaration_update(&run, 30);
  check(!run.finished, "38 ms is not the whole animation");
  check(memcmp(run.canvas, skip.canvas, sizeof(run.canvas)) != 0, "ink appears");

  /* 5 s is far past the DOS run length (8.213 ms per frame). */
  declaration_update(&run, 5000);
  check(run.finished, "animation completes");

  declaration_skip_to_end(&skip);
  check(skip.finished, "skip finishes");
  check(
    memcmp(run.canvas, skip.canvas, sizeof(run.canvas)) == 0,
    "fast-forward lands on the same pixels as the timed run"
  );

  /* Finished + keypress closes; the same key while running only skips. */
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ESCAPE;
  check(declaration_handle_input(&run, &in), "open cinematic consumes input");
  check(!run.open, "keypress on a finished cinematic closes it");

  declaration_close(&run);
  declaration_close(&skip);
}

static void test_missing_assets(void) {
  DeclarationCinematic d;
  memset(&d, 0, sizeof(d));
  check(
    !declaration_open(&d, "no-such-dir", "United Colonies"),
    "missing DECOIND.PIK fails cleanly"
  );
  check(!d.open, "failed open leaves the cinematic closed");
}

int main(void) {
  diag_init(0, NULL);
  test_title_case();
  test_run_layout();
  test_animation_and_skip();
  test_missing_assets();
  diag_shutdown();
  if (failures) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  printf("ok\n");
  return 0;
}
