#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ff.h"
#include "core/font.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static int text_right_extent(const ColonizeFont* font, const char* text) {
  ColonizeFramebuffer8 fb;
  fb.width = 64;
  fb.height = 10;
  fb.pixels = calloc((size_t)fb.width * (size_t)fb.height, 1);
  if (!fb.pixels) {
    return -1;
  }
  font_draw_text(font, &fb, 0, 0, text, 15);
  int right = -1;
  for (int x = 0; x < fb.width; ++x) {
    for (int y = 0; y < 8; ++y) {
      if (fb.pixels[y * fb.width + x]) {
        right = x;
        break;
      }
    }
  }
  free(fb.pixels);
  return right;
}

/* FONTSMAL omits '/'; "5/7" must draw wider than jammed "57". */
static int assert_slash_widens(const ColonizeFont* font, const char* label) {
  const int jammed = text_right_extent(font, "57");
  const int priced = text_right_extent(font, "5/7");
  if (jammed < 0 || priced < 0) {
    fprintf(stderr, "%s: oom drawing bid/ask sample\n", label);
    return 0;
  }
  if (priced <= jammed) {
    fprintf(
      stderr,
      "%s: expected '/' to widen \"5/7\" past \"57\" (57→%d, 5/7→%d)\n",
      label,
      jammed,
      priced
    );
    return 0;
  }
  return 1;
}

int main(void) {
  diag_init(0, NULL);

  ColonizeFont font;
  char err[256];
  if (!ff_load("COLONIZE/FONTSMAL.FF", &font, err, sizeof(err))) {
    fprintf(stderr, "ff load failed: %s\n", err);
    return 1;
  }

  fprintf(stderr, "font %ux%u section=%zu\n", font.max_width, font.max_height, font.section_size);
  fprintf(stderr, "A width=%u offset=%u space width=%u\n",
    font.char_widths['A'],
    font.char_offsets['A'],
    font.char_widths[' ']);

  if (font.char_widths['A'] == 0 || font.char_offsets['A'] >= font.section_size) {
    fprintf(stderr, "invalid glyph A metadata\n");
    ff_free(&font);
    return 1;
  }

  if (font.char_widths['/'] != 0) {
    fprintf(stderr, "expected FONTSMAL '/' width 0, got %u\n", font.char_widths['/']);
    ff_free(&font);
    return 1;
  }
  if (!assert_slash_widens(&font, "FONTSMAL")) {
    ff_free(&font);
    return 1;
  }

  ff_free(&font);

  if (!ff_load("COLONIZE/FONTTINY.FF", &font, err, sizeof(err))) {
    fprintf(stderr, "FONTTINY load failed: %s\n", err);
    return 1;
  }
  if (font.char_widths['/'] == 0) {
    fprintf(stderr, "expected FONTTINY '/' glyph\n");
    ff_free(&font);
    return 1;
  }
  if (!assert_slash_widens(&font, "FONTTINY")) {
    ff_free(&font);
    return 1;
  }
  ff_free(&font);

  diag_shutdown();
  return 0;
}
