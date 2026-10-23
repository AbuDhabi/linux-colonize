/*
 * port_bugs.md #1180 / #1181 guards on popup_field_frame, the DOS text-entry
 * dialog (FUN_291f_0120): the body must be the FONTINTR unbold + shadow markup
 * draw every other popup uses, and the geometry must be the one the asm yields
 * (FUN_6f74_0d44 / 14c6 / 1198 / 1e14, written out in popup.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/fb.h"
#include "core/font_file.h"
#include "core/popup.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

enum { W = 320, H = 200 };

static bool load_font(ColonizeFont* font) {
  char err[128];
  if (!font_file_load("COLONIZE/FONTINTR.FF", font, err, sizeof(err))) {
    fprintf(stderr, "SKIP: FONTINTR.FF: %s\n", err);
    return false;
  }
  return true;
}

/* The wrapped body, label and box of popup_field_frame, redrawn by hand from
 * the geometry it reports; any other pixel difference is stray chrome. */
static int case_field_body_is_unbold_markup(void) {
  ColonizeFont font;
  if (!load_font(&font)) {
    return TR_SKIP;
  }
  static uint8_t a[W * H], b[W * H], c[W * H];
  ColonizeFramebuffer8 fa = {.width = W, .height = H, .pixels = a};
  ColonizeFramebuffer8 fb = {.width = W, .height = H, .pixels = b};
  ColonizeFramebuffer8 fc = {.width = W, .height = H, .pixels = c};
  const char* prompt = "Found {Quebec} here?";
  const uint8_t ink = 15;

  PopupFieldGeom g;
  popup_field_frame(&fa, &font, NULL, NULL, 190, prompt, "Name:", 23, ink, &g);

  for (int pass = 0; pass < 2; ++pass) {
    ColonizeFramebuffer8* f = pass == 0 ? &fb : &fc;
    popup_draw(f, g.frame.x, g.frame.y, g.frame.w, g.frame.h, NULL, NULL, NULL, NULL, NULL, NULL);
    bool emph = false;
    popup_draw_text_markup(
      &font, f, g.frame.x + 5, g.frame.y + 6, prompt, ink, COLONIZE_COL_HILITE, pass == 0, true,
      &emph
    );
    popup_draw_text_markup(
      &font, f, g.frame.x + 5, g.box_y + 3, "Name:", ink, COLONIZE_COL_HILITE, pass == 0, true,
      &emph
    );
    fb_rect_outline(f, g.box_x, g.box_y, g.box_w, g.box_h, ink, ink);
  }
  int rc = 0;
  if (memcmp(a, b, sizeof(a)) != 0) {
    fprintf(stderr, "popup_field_frame body is not the unbold shadowed markup draw\n");
    rc = 1;
  }
  /* Control: the bold default must differ, or this test guards nothing. */
  if (memcmp(b, c, sizeof(b)) == 0) {
    fprintf(stderr, "control failed: unbold and bold draw identical pixels\n");
    rc = 1;
  }
  font_file_free(&font);
  return rc;
}

static int case_field_geometry_is_dos(void) {
  ColonizeFont font;
  if (!load_font(&font)) {
    return TR_SKIP;
  }
  static uint8_t a[W * H];
  ColonizeFramebuffer8 fa = {.width = W, .height = H, .pixels = a};
  const int gh = font.max_height == 6 ? 5 : (int)font.max_height; /* FUN_6f74_0f16 */
  const int label_w = font_text_width(&font, "Name: ");           /* FUN_6f74_0d44: label + " " */
  const int text_w = font_text_width(&font, "W") * 23;            /* DS:0x1f97 "W" * max length */
  int rc = 0;
  /* @width 190 (@LANDHO) and none (@COLONY: the default 0x50). */
  const int widths[2] = {190, 0};
  for (int i = 0; i < 2; ++i) {
    PopupFieldGeom g;
    popup_field_frame(&fa, &font, NULL, NULL, widths[i], "Name this colony?", "Name:", 23, 15, &g);
    int content = widths[i] > 0x50 ? widths[i] : 0x50;
    if (label_w + text_w + 10 > content) {
      content = label_w + text_w + 10; /* FUN_6f74_0d44 raises dialog+0x34 */
    }
    const int rows = (g.frame.h - gh - 20) / (gh + 1); /* H = rows*(gh+1) + gh + 20 */
    const int body_h = rows * (gh + 1);
    const struct {
      const char* what;
      int got;
      int want;
    } checks[] = {
      {"frame width = content + 6", g.frame.w, content + 6},
      {"frame height", g.frame.h, body_h + gh + 20},
      {"frame x = 160 - W/2", g.frame.x, 160 - ((content + 6) >> 1)},
      {"frame y = 100 - H/2", g.frame.y, 100 - ((body_h + gh + 20) >> 1)},
      {"box x = x + 5 + label width", g.box_x, g.frame.x + 5 + label_w},
      {"box y = y + 9 + body", g.box_y, g.frame.y + 9 + body_h},
      {"box w = text width + 6", g.box_w, text_w + 6},
      {"box h = glyph_h + 5", g.box_h, gh + 5},
      {"text x = box + 3", g.text_x, g.box_x + 3},
      {"text y = box + 3", g.text_y, g.box_y + 3},
      {"select bar / hit height = glyph_h + 1", g.text_h, gh + 1},
    };
    for (size_t k = 0; k < sizeof(checks) / sizeof(checks[0]); ++k) {
      if (checks[k].got != checks[k].want) {
        fprintf(stderr, "width %d: %s: want %d got %d\n", widths[i], checks[k].what,
                checks[k].want, checks[k].got);
        rc = 1;
      }
    }
  }
  font_file_free(&font);
  return rc;
}

static const TestCase k_cases[] = {
  {"field_body_is_unbold_markup", case_field_body_is_unbold_markup},
  {"field_geometry_is_dos", case_field_geometry_is_dos},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
