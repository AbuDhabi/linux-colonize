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
  popup_field_frame(&fa, &font, NULL, NULL, 190, prompt, "Name:", 23, ink, NULL, &g);

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
    popup_field_frame(
      &fa, &font, NULL, NULL, widths[i], "Name this colony?", "Name:", 23, 15, NULL, &g
    );
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

/* One-sprite stand-in for an MSSn.SS sheet whose every pixel is `colour`. */
static ColonizeSpriteSheet make_figure(
  ColonizeSprite* sp, int w, int h, int oy, int mode, int ox, uint8_t colour
) {
  memset(sp, 0, sizeof(*sp));
  sp->width = w;
  sp->height = h;
  sp->pixels = malloc((size_t)w * (size_t)h);
  memset(sp->pixels, colour, (size_t)w * (size_t)h);
  ColonizeSpriteSheet sheet;
  memset(&sheet, 0, sizeof(sheet));
  sheet.sprites = sp;
  sheet.sprite_count = 1;
  sheet.place_offset_y = oy;
  sheet.place_mode = mode;
  sheet.place_offset_x = ox;
  return sheet;
}

/* Frame placement + blit of a decorated text-entry dialog: FUN_6f74_14c6
 * (6f74:18ae-1a0c) sizes the assembly, FUN_6f74_248e blits the figure last.
 * port_bugs.md #1183. */
static int case_field_decoration_is_dos(void) {
  ColonizeFont font;
  if (!load_font(&font)) {
    return TR_SKIP;
  }
  enum { FIG = 0x7e };
  static uint8_t plain[W * H], deco[W * H];
  const struct {
    const char* what;
    int fw, fh, oy, mode, ox, width;
  } k[] = {
    {"mode 0 (MSS0 admiral)", 75, 91, 20, 0, 71, 190},
    {"mode 0, total width clamped to 320", 100, 40, 10, 0, 10, 300},
    {"mode 1 (MSS5 nun)", 60, 68, 6, 1, 0, 0},
    {"mode 2", 60, 68, 6, 2, 10, 190},
    {"dropped (total height >= 200)", 75, 180, 0, 0, 71, 190},
  };
  int rc = 0;
  for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
    ColonizeFramebuffer8 fp = {.width = W, .height = H, .pixels = plain};
    ColonizeFramebuffer8 fd = {.width = W, .height = H, .pixels = deco};
    memset(plain, 0, sizeof(plain));
    memset(deco, 0, sizeof(deco));
    ColonizeSprite sp;
    const ColonizeSpriteSheet sheet = make_figure(&sp, k[i].fw, k[i].fh, k[i].oy, k[i].mode, k[i].ox, FIG);
    PopupFieldGeom p, d;
    popup_field_frame(&fp, &font, NULL, NULL, k[i].width, "Name this colony?", "Name:", 23, 15, NULL, &p);
    popup_field_frame(&fd, &font, NULL, NULL, k[i].width, "Name this colony?", "Name:", 23, 15, &sheet, &d);

    const int t = k[i].fh - (k[i].oy < k[i].fh ? k[i].oy : k[i].fh) + p.frame.h;
    int fig_count = 0;
    for (int n = 0; n < W * H; ++n) {
      fig_count += deco[n] == FIG;
    }
    if (t >= H) {
      if (memcmp(plain, deco, sizeof(plain)) != 0 || fig_count != 0 ||
          memcmp(&p, &d, sizeof(p)) != 0) {
        fprintf(stderr, "%s: a dropped figure must leave the dialog undecorated\n", k[i].what);
        rc = 1;
      }
      free(sp.pixels);
      continue;
    }
    const int gy = 100 - (t >> 1);
    int gx, fx;
    int tw = p.frame.w - k[i].ox + k[i].fw;
    int ox = k[i].ox;
    if (k[i].mode == 1) {
      gx = 160 - (k[i].fw >> 1);
      fx = p.frame.x;
    } else {
      if (tw > 320) {
        ox += tw - 320;
        tw = 320;
      }
      if (k[i].mode == 2) {
        fx = 160 - (tw >> 1);
        gx = fx + p.frame.w - ox;
      } else {
        gx = 160 - (tw >> 1);
        fx = gx - ox + k[i].fw;
      }
    }
    const int fy = gy - k[i].oy + k[i].fh;
    const struct {
      const char* what;
      int got, want;
    } checks[] = {
      {"frame x", d.frame.x, fx},
      {"frame y", d.frame.y, fy},
      {"frame w unchanged", d.frame.w, p.frame.w},
      {"frame h unchanged", d.frame.h, p.frame.h},
      {"box x relative to frame", d.box_x - d.frame.x, p.box_x - p.frame.x},
      {"box y relative to frame", d.box_y - d.frame.y, p.box_y - p.frame.y},
      {"text x relative to frame", d.text_x - d.frame.x, p.text_x - p.frame.x},
      {"text y relative to frame", d.text_y - d.frame.y, p.text_y - p.frame.y},
      {"figure top-left pixel", deco[gy * W + gx], FIG},
      {"figure bottom-right pixel", deco[(gy + k[i].fh - 1) * W + gx + k[i].fw - 1], FIG},
      {"figure pixel count", fig_count, k[i].fw * k[i].fh},
    };
    for (size_t c = 0; c < sizeof(checks) / sizeof(checks[0]); ++c) {
      if (checks[c].got != checks[c].want) {
        fprintf(stderr, "%s: %s: want %d got %d\n", k[i].what, checks[c].what, checks[c].want,
                checks[c].got);
        rc = 1;
      }
    }
    free(sp.pixels);
  }
  font_file_free(&font);
  return rc;
}

/*
 * FUN_6f74_14c6 placement (popup_place_dos), port_bugs.md #1186: arithmetic-shift
 * centring, so an odd size lands 1 px down/right of the old halve-the-margin form,
 * and the MSS figure arm. Expected values are worked out by hand from the asm
 * (6f74:16c8-1735, 18ae-1a0c). The three Europe rows are measured off the real DOS
 * reference shots (original_screenshots/europe/ recruit, purchase, train; 2x, black
 * top rule row / 2).
 */
static int case_place_dos(void) {
  int rc = 0;
  ColonizeSprite sp[4];
  const ColonizeSpriteSheet mss2 = make_figure(&sp[0], 122, 84, 6, 1, 0, 1);
  const ColonizeSpriteSheet mss0 = make_figure(&sp[1], 75, 91, 20, 0, 71, 1);
  const ColonizeSpriteSheet mss1 = make_figure(&sp[2], 72, 139, 62, 2, 55, 1);
  const struct {
    const char* what;
    int fw, fh, w, h;
    const ColonizeSpriteSheet* fig;
    int x, y;
    bool figure;
    int fig_x, fig_y;
  } k[] = {
    {"plain even", W, H, 196, 68, NULL, 62, 66, false, 0, 0},
    {"plain odd w, odd h (old form: 81, 65)", W, H, 157, 69, NULL, 82, 66, false, 0, 0},
    {"plain odd w, even h", W, H, 157, 70, NULL, 82, 65, false, 0, 0},
    {"plain, larger map view (fb 640x400)", 640, 400, 157, 69, NULL, 242, 166, false, 0, 0},
    {"recruit.png: 103 px under MSS2, top rule at 88", W, H, 196, 103, &mss2, 62, 88, true, 99, 10},
    {"purchase.png: 119 px under MSS2, top rule at 80", W, H, 196, 119, &mss2, 62, 80, true, 99, 2},
    {"train.png: 180 px, figure dropped, top rule at 10", W, H, 196, 180, &mss2, 62, 10, false, 0, 0},
    {"mode 0 (MSS0)", W, H, 196, 68, &mss0, 64, 102, true, 60, 31},
    {"mode 2 (MSS1)", W, H, 196, 68, &mss1, 54, 105, true, 195, 28},
  };
  for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
    const PopupDosPlace p = popup_place_dos(k[i].fw, k[i].fh, k[i].w, k[i].h, k[i].fig);
    if (p.x != k[i].x || p.y != k[i].y || p.figure != k[i].figure ||
        (p.figure && (p.fig_x != k[i].fig_x || p.fig_y != k[i].fig_y))) {
      fprintf(stderr, "%s: want (%d,%d) fig %d (%d,%d), got (%d,%d) fig %d (%d,%d)\n", k[i].what,
              k[i].x, k[i].y, k[i].figure, k[i].fig_x, k[i].fig_y, p.x, p.y, p.figure, p.fig_x,
              p.fig_y);
      rc = 1;
    }
  }
  /* Mode 0, total width 390 clamped to 320 by widening ox (10 -> 80). */
  const ColonizeSpriteSheet wide = make_figure(&sp[3], 100, 40, 10, 0, 10, 1);
  const PopupDosPlace p = popup_place_dos(W, H, 300, 60, &wide);
  if (!p.figure || p.fig_x != 0 || p.fig_y != 55 || p.x != 20 || p.y != 85) {
    fprintf(stderr, "clamped mode 0: got (%d,%d) fig (%d,%d)\n", p.x, p.y, p.fig_x, p.fig_y);
    rc = 1;
  }
  for (int i = 0; i < 4; ++i) {
    free(sp[i].pixels);
  }
  return rc;
}

static const TestCase k_cases[] = {
  {"field_body_is_unbold_markup", case_field_body_is_unbold_markup},
  {"field_geometry_is_dos", case_field_geometry_is_dos},
  {"field_decoration_is_dos", case_field_decoration_is_dos},
  {"place_dos", case_place_dos},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
