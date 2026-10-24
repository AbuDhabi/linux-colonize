#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/font_file.h"
#include "core/font.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#include "../common/test_runner.h"

static int text_right_extent_with(
  const ColonizeFont* font, const char* text,
  void (*draw)(const ColonizeFont*, ColonizeFramebuffer8*, int, int, const char*, uint8_t)
) {
  ColonizeFramebuffer8 fb;
  fb.width = 64;
  fb.height = 10;
  fb.pixels = calloc((size_t)fb.width * (size_t)fb.height, 1);
  if (!fb.pixels) {
    return -1;
  }
  draw(font, &fb, 0, 0, text, 15);
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

static int text_right_extent(const ColonizeFont* font, const char* text) {
  return text_right_extent_with(font, text, font_draw_text);
}

/* Pixels of text drawn at (1, 1) into a cleared 64x12 frame (out holds 64 * 12). */
static void text_pixels(
  const ColonizeFont* font, const char* text, uint8_t* out,
  void (*draw)(const ColonizeFont*, ColonizeFramebuffer8*, int, int, const char*, uint8_t)
) {
  ColonizeFramebuffer8 fb = {.width = 64, .height = 12, .pixels = out};
  memset(out, 0, 64 * 12);
  draw(font, &fb, 1, 1, text, 15);
}

/*
 * port_bugs.md #1229: FUN_1c11_000c (raw 17468-17471) draws a char whose face width byte
 * is 0 as nothing and does not advance, on every text path. FONTSMAL has no '/', so DOS
 * shows "5/7" as "57"; only font_draw_text_port (port-only surfaces) keeps the builtin
 * 5x7 stand-in.
 */
static int assert_lacking_glyph_is_dos(const ColonizeFont* font, const char* label, char ch) {
  char with[4] = {'5', ch, '7', '\0'};
  static uint8_t got[64 * 12];
  static uint8_t want[64 * 12];
  text_pixels(font, with, got, font_draw_text);
  text_pixels(font, "57", want, font_draw_text);
  int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  const bool inked = font_glyph_ink_bounds(font, (unsigned char)ch, &min_x, &min_y, &max_x, &max_y);
  const int port_w = font_text_width_port(font, with);
  const int port_right = text_right_extent_with(font, with, font_draw_text_port);
  if (memcmp(got, want, sizeof(got)) != 0 || font_text_width(font, with) != font_text_width(font, "57") ||
      font_text_width_skip(font, with, FONT_SKIP_NONE) != font_text_width(font, "57") || inked ||
      port_w != font_text_width(font, "57") + 6 || port_right <= text_right_extent(font, "57")) {
    fprintf(
      stderr, "%s: '%c' must draw nothing with no advance (inked %d, port width %d, port right %d)\n",
      label, ch, inked, port_w, port_right
    );
    return 0;
  }
  return 1;
}

/* FONTTINY has '/'; "5/7" must draw wider than "57". */
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

static int case_fontsmal(void) {
  diag_init(0, NULL);

  ColonizeFont font;
  char err[256];
  if (!font_file_load("COLONIZE/FONTSMAL.FF", &font, err, sizeof(err))) {
    fprintf(stderr, "ff load failed: %s\n", err);
    diag_shutdown();
    return 1;
  }

  fprintf(stderr, "font %ux%u section=%zu\n", font.max_width, font.max_height, font.section_size);
  fprintf(stderr, "A width=%u offset=%u space width=%u\n",
    font.char_widths['A'],
    font.char_offsets['A'],
    font.char_widths[' ']);

  if (font.char_widths['A'] == 0 || font.char_offsets['A'] >= font.section_size) {
    fprintf(stderr, "invalid glyph A metadata\n");
    font_file_free(&font);
    diag_shutdown();
    return 1;
  }

  if (font.char_widths['/'] != 0) {
    fprintf(stderr, "expected FONTSMAL '/' width 0, got %u\n", font.char_widths['/']);
    font_file_free(&font);
    diag_shutdown();
    return 1;
  }
  if (!assert_lacking_glyph_is_dos(&font, "FONTSMAL", '/')) {
    font_file_free(&font);
    diag_shutdown();
    return 1;
  }

  font_file_free(&font);
  diag_shutdown();
  return 0;
}

static int case_fonttiny(void) {
  diag_init(0, NULL);

  ColonizeFont font;
  char err[256];
  if (!font_file_load("COLONIZE/FONTTINY.FF", &font, err, sizeof(err))) {
    fprintf(stderr, "FONTTINY load failed: %s\n", err);
    diag_shutdown();
    return 1;
  }
  if (font.char_widths['/'] == 0) {
    fprintf(stderr, "expected FONTTINY '/' glyph\n");
    font_file_free(&font);
    diag_shutdown();
    return 1;
  }
  if (!assert_slash_widens(&font, "FONTTINY") || !assert_lacking_glyph_is_dos(&font, "FONTTINY", '=') ||
      !assert_lacking_glyph_is_dos(&font, "FONTTINY", '*')) {
    font_file_free(&font);
    diag_shutdown();
    return 1;
  }
  font_file_free(&font);

  diag_shutdown();
  return 0;
}

static const TestCase k_cases[] = {
    {"case_fontsmal", case_fontsmal},
    {"case_fonttiny", case_fonttiny},
};

TEST_MAIN(k_cases)
