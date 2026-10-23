/*
 * port_bugs.md #1180 / #1181 guards on popup_field_frame, the DOS text-entry
 * dialog (FUN_291f_0120): the body must be the FONTINTR markup draw every other
 * popup uses (#1191: one shaded pass), and the geometry must be the one the asm yields
 * (FUN_6f74_0d44 / 14c6 / 1198 / 1e14, written out in popup.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_save.h"
#include "core/combat_analysis.h"
#include "core/fb.h"
#include "core/font_file.h"
#include "core/popup.h"
#include "core/reports.h"
#include "core/trade_screen.h"
#include "core/ui_colors.h"
#include "core/unit_stack.h"
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
static int case_field_body_is_markup(void) {
  ColonizeFont font;
  if (!load_font(&font)) {
    return TR_SKIP;
  }
  static uint8_t a[W * H], b[W * H];
  ColonizeFramebuffer8 fa = {.width = W, .height = H, .pixels = a};
  ColonizeFramebuffer8 fb = {.width = W, .height = H, .pixels = b};
  const char* prompt = "Found {Quebec} here?";
  const uint8_t ink = COLONIZE_COL_BASIC;

  PopupFieldGeom g;
  popup_field_frame(&fa, &font, NULL, NULL, 190, prompt, "Name:", 23, ink, NULL, &g);

  popup_draw(&fb, g.frame.x, g.frame.y, g.frame.w, g.frame.h, NULL, NULL, NULL, NULL, NULL, NULL);
  bool emph = false;
  popup_draw_text_markup(
    &font, &fb, g.frame.x + 5, g.frame.y + 6, prompt, ink, COLONIZE_COL_HILITE, NULL, &emph
  );
  popup_draw_text_markup(
    &font, &fb, g.frame.x + 5, g.box_y + 3, "Name:", ink, COLONIZE_COL_HILITE, NULL, &emph
  );
  fb_rect_outline(&fb, g.box_x, g.box_y, g.box_w, g.box_h, ink, ink);
  int rc = 0;
  if (memcmp(a, b, sizeof(a)) != 0) {
    fprintf(stderr, "popup_field_frame body is not the dialog markup draw\n");
    rc = 1;
  }
  font_file_free(&font);
  return rc;
}

/* FUN_1c11_000c glyph layout, decoded here independently of font.c: rows MSB-first,
 * 4 px per byte, ((w - 1) >> 2) + 1 bytes per row; shade s paints map[s], 0 skips. */
static int expect_run(const ColonizeFont* f, uint8_t* px, int x, int y, const char* s,
                      const uint8_t map[4]) {
  for (; *s; ++s) {
    const unsigned char ch = (unsigned char)*s;
    const int w = f->char_widths[ch];
    const int bpr = ((w - 1) >> 2) + 1;
    const uint8_t* g = f->section_data + f->char_offsets[ch];
    for (int r = 0; r < f->max_height; ++r) {
      for (int c = 0; c < w; ++c) {
        const int shade = (g[r * bpr + (c >> 2)] >> (6 - 2 * (c & 3))) & 3;
        if (shade) {
          px[(y + r) * W + x + c] = map[shade];
        }
      }
    }
    x += w;
  }
  return x;
}

/* port_bugs.md #1191: DOS dialog text is one blit per glyph through the shade map
 * FUN_6f74_04f6 loads ({skip, ink, @COLORS enhance 128, @COLORS shadow 47}); no +1,+1
 * drop-shadow pass. purchase.png's 1240 (12,12,12) and 655 (121,73,52) pixels are
 * exactly FONTINTR's shade-3 and shade-2 pixels. */
static int case_dialog_text_is_dos_shaded(void) {
  ColonizeFont font;
  if (!load_font(&font)) {
    return TR_SKIP;
  }
  static uint8_t got[W * H], want[W * H];
  memset(got, 0xcc, sizeof(got));
  memset(want, 0xcc, sizeof(want));
  ColonizeFramebuffer8 fb = {.width = W, .height = H, .pixels = got};
  popup_draw_text_markup(
    &font, &fb, 20, 30, "available.{Which}", COLONIZE_COL_BASIC, COLONIZE_COL_HILITE, NULL, NULL
  );
  const uint8_t base[4] = {0, COLONIZE_COL_BASIC, COLONIZE_COL_ENHANCE, COLONIZE_COL_SHADOW};
  const uint8_t hi[4] = {0, COLONIZE_COL_HILITE, COLONIZE_COL_ENHANCE, COLONIZE_COL_SHADOW};
  expect_run(&font, want, expect_run(&font, want, 20, 30, "available.", base), 30, "Which", hi);
  int n2 = 0, n3 = 0;
  for (int i = 0; i < W * H; ++i) {
    n2 += want[i] == COLONIZE_COL_ENHANCE;
    n3 += want[i] == COLONIZE_COL_SHADOW;
  }
  int rc = 0;
  if (n2 == 0 || n3 == 0) {
    fprintf(stderr, "control: FONTINTR run has %d shade-2 / %d shade-3 pixels\n", n2, n3);
    rc = 1;
  }
  for (int i = 0; i < W * H && !rc; ++i) {
    if (got[i] != want[i]) {
      fprintf(stderr, "(%d,%d) drawn %u, DOS shade map %u\n", i % W, i / W, got[i], want[i]);
      rc = 1;
    }
  }
  font_file_free(&font);
  return rc;
}

/* port_bugs.md #1199: FUN_6f74_0538 6f74:05be draws the byte after '~' as a plain glyph in
 * the other ink (hilite outside {}, base inside) and restores; FUN_6f74_0486 deletes
 * "~{}|" before measuring; '|' ends the text. */
static int case_dialog_tilde_is_dos(void) {
  ColonizeFont font;
  if (!load_font(&font)) {
    return TR_SKIP;
  }
  static uint8_t got[W * H], want[W * H];
  const uint8_t base[4] = {0, COLONIZE_COL_BASIC, COLONIZE_COL_ENHANCE, COLONIZE_COL_SHADOW};
  const uint8_t hi[4] = {0, COLONIZE_COL_HILITE, COLONIZE_COL_ENHANCE, COLONIZE_COL_SHADOW};
  ColonizeFramebuffer8 fb = {.width = W, .height = H, .pixels = got};
  int rc = 0;
  for (int t = 0; t < 4; ++t) {
    static const char* const k_text[] = {"Show ~Indian", "{x~Yz}", "a~#b", "ab|cd"};
    memset(got, 0xcc, sizeof(got));
    memset(want, 0xcc, sizeof(want));
    const int end = popup_draw_text_markup(
      &font, &fb, 20, 30, k_text[t], COLONIZE_COL_BASIC, COLONIZE_COL_HILITE, NULL, NULL
    );
    int x = 20;
    if (t == 0) {
      x = expect_run(&font, want, expect_run(&font, want, x, 30, "Show ", base), 30, "I", hi);
      x = expect_run(&font, want, x, 30, "ndian", base);
    } else if (t == 1) {
      x = expect_run(&font, want, expect_run(&font, want, x, 30, "x", hi), 30, "Y", base);
      x = expect_run(&font, want, x, 30, "z", hi);
    } else if (t == 2) { /* '#' is a glyph once escaped (the run writer skips it) */
      x = expect_run(&font, want, expect_run(&font, want, x, 30, "a", base), 30, "#", hi);
      x = expect_run(&font, want, x, 30, "b", base);
    } else {
      x = expect_run(&font, want, x, 30, "ab", base);
    }
    if (end != x || memcmp(got, want, sizeof(got)) != 0) {
      fprintf(stderr, "\"%s\": end x %d want %d, or pixels are not the 0538 '~' draw\n",
              k_text[t], end, x);
      rc = 1;
    }
  }
  /* An escaped '{' is a glyph, not a brace: what follows stays in the base ink. */
  memset(got, 0xcc, sizeof(got));
  memset(want, 0xcc, sizeof(want));
  popup_draw_text_markup(&font, &fb, 20, 30, "~{xy", COLONIZE_COL_BASIC, COLONIZE_COL_HILITE,
                         NULL, NULL);
  const int xy = 20 + font_draw_glyph_shaded(&font, NULL, 0, 0, '{', hi);
  expect_run(&font, want, xy, 30, "xy", base);
  for (int i = 0; i < W * H && !rc; ++i) {
    if (i % W >= xy && got[i] != want[i]) {
      fprintf(stderr, "\"~{xy\": (%d,%d) drawn %u want %u\n", i % W, i / W, got[i], want[i]);
      rc = 1;
    }
  }
  if (popup_markup_text_width(&font, "Show ~Indian") != font_text_width(&font, "Show Indian") ||
      popup_markup_text_width(&font, "~[ {a}|b") != font_text_width(&font, "[ ab")) {
    fprintf(stderr, "popup_markup_text_width does not delete ~{}| (FUN_6f74_0486)\n");
    rc = 1;
  }
  font_file_free(&font);
  return rc;
}

/* Non-zero-shade pixels of s in font f (the {skip, c, c, c} map paints every one). */
static int ink_pixels(const ColonizeFont* f, const char* s) {
  int n = 0;
  for (; *s; ++s) {
    const unsigned char ch = (unsigned char)*s;
    const int w = f->char_widths[ch];
    const int bpr = ((w - 1) >> 2) + 1;
    const uint8_t* g = f->section_data + f->char_offsets[ch];
    for (int r = 0; r < f->max_height && w > 0; ++r) {
      for (int c = 0; c < w; ++c) {
        n += ((g[r * bpr + (c >> 2)] >> (6 - 2 * (c & 3))) & 3) != 0;
      }
    }
  }
  return n;
}

/* port_bugs.md #1201: Combat Analysis (FUN_636c_0000) and the trade-route screen
 * (FUN_647e_09da) write through FUN_104b_0288 / 02c2 / 0318: one FONTTINY blit through
 * {skip, c, c, c}, no shadow. Combat Analysis: labels / names @COLORS basic, values and
 * base strengths hilite. Trade route: ink 0x0f on a 0x22 fill, grid and OK outline 0. */
static int case_stat_screens_text_is_one_pass(void) {
  ColonizeFont tiny;
  char err[128];
  if (!font_file_load("COLONIZE/FONTTINY.FF", &tiny, err, sizeof(err))) {
    fprintf(stderr, "SKIP: FONTTINY.FF: %s\n", err);
    return TR_SKIP;
  }
  static uint8_t a[W * H], b[W * H];
  ColonizeFramebuffer8 fa = {.width = W, .height = H, .pixels = a};
  ColonizeFramebuffer8 fbb = {.width = W, .height = H, .pixels = b};
  int rc = 0;

  static CombatAnalysisDialog dlg;
  memset(&dlg, 0, sizeof(dlg));
  dlg.open = true;
  dlg.atk_chrome.sprite = dlg.def_chrome.sprite = -1;
  snprintf(dlg.atk_name, sizeof(dlg.atk_name), "Ab");
  snprintf(dlg.def_name, sizeof(dlg.def_name), "Cd");
  dlg.eng.atk_flags.base_combat = 2;
  dlg.eng.def_flags.base_combat = 3;
  snprintf(dlg.atk_rows[0].label, sizeof(dlg.atk_rows[0].label), "Lx");
  snprintf(dlg.atk_rows[0].value, sizeof(dlg.atk_rows[0].value), "50");
  dlg.atk_rows[0].icon_sprite = -1;
  dlg.atk_line_count = 1;
  memset(a, 0, sizeof(a));
  memset(b, 0, sizeof(b));
  combat_analysis_render(&dlg, NULL, NULL, NULL, NULL, NULL, NULL, NULL, COLONIZE_COL_BASIC,
                         COLONIZE_COL_HILITE, NULL, &fa); /* chrome only */
  combat_analysis_render(&dlg, &tiny, NULL, NULL, NULL, NULL, NULL, NULL, COLONIZE_COL_BASIC,
                         COLONIZE_COL_HILITE, NULL, &fbb);
  int n_basic = 0, n_hi = 0, n_other = 0;
  for (int i = 0; i < W * H; ++i) {
    if (a[i] != b[i]) {
      n_basic += b[i] == COLONIZE_COL_BASIC;
      n_hi += b[i] == COLONIZE_COL_HILITE;
      n_other += b[i] != COLONIZE_COL_BASIC && b[i] != COLONIZE_COL_HILITE;
    }
  }
  const int want_basic =
    ink_pixels(&tiny, "AbCdLx") + ink_pixels(&tiny, reports_misc_display_word(75, ""));
  const int want_hi = ink_pixels(&tiny, "2350");
  if (n_other || n_basic != want_basic || n_hi != want_hi) {
    fprintf(stderr, "combat analysis text: %d basic (want %d), %d hilite (want %d), %d other\n",
            n_basic, want_basic, n_hi, want_hi, n_other);
    rc = 1;
  }

  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  snprintf(col1.trade_route[0].name, sizeof(col1.trade_route[0].name), "Run");
  col1.trade_route[0].dest_count = 1;
  col1.trade_route[0].stop[0].colony_index = 999;
  TradeScreen ts;
  trade_screen_init(&ts, NULL);
  snprintf(ts.lab_title, sizeof(ts.lab_title), "TR");
  snprintf(ts.lab_name, sizeof(ts.lab_name), "Nm");
  snprintf(ts.lab_ok, sizeof(ts.lab_ok), "OK");
  trade_screen_open(&ts, 0);
  memset(b, 0xcc, sizeof(b));
  trade_screen_render(&ts, &col1, NULL, "Eu", &tiny, NULL, &fbb);
  memset(a, 0x22, sizeof(a));
  const uint8_t ink[4] = {0, 0x0f, 0x0f, 0x0f};
  /* "TR 1" centred over 0..0x140 at y 5 (FUN_104b_0318). */
  expect_run(&tiny, a, 0xa0 - ((font_text_width(&tiny, "TR 1") - 1) >> 1), 5, "TR 1", ink);
  int n15 = 0;
  for (int i = 0; i < W * H; ++i) {
    n15 += b[i] == 0x0f;
    if (b[i] != 0x22 && b[i] != 0 && b[i] != 0x0f) {
      n_other++;
    }
    if (i / W < 5 + tiny.max_height && a[i] != b[i]) {
      n_other++;
    }
  }
  const int want15 = ink_pixels(&tiny, "TR1Nm RunOK1.  Eu");
  if (n_other || n15 != want15) {
    fprintf(stderr, "trade route: %d ink-15 pixels (want %d), %d off-map / title pixels\n", n15,
            want15, n_other);
    rc = 1;
  }
  font_file_free(&tiny);
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

/*
 * FUN_6f74_14c6 list-dialog height (popup_dos_list_height), port_bugs.md #1187:
 * h = 12 + prose + rows * (glyph_h + 3) + the DS:0x1f66 footer (6 under FONTTINY, else 3).
 * The three DOS reference shots (original_screenshots/europe/, black top rule row to
 * bottom rule row) are the oracle: recruit 103 (4 prose lines, 4 rows, FONTINTR),
 * purchase 119 (2 lines, 7 rows), train 180 (3 lines, 18 rows, FONTTINY).
 */
static int case_list_height_dos(void) {
  int rc = 0;
  ColonizeFont font;
  const bool have_font = load_font(&font);
  const struct {
    const char* what;
    int glyph_h, prose_lines, rows;
    bool footer, tiny;
    int h;
  } k[] = {
    {"recruit.png", 9, 4, 4, true, false, 103},
    {"purchase.png", 9, 2, 7, true, false, 119},
    {"train.png", 5, 3, 18, true, true, 180},
    {"no footer (@REALLYBUY confirm: 1 line, Yes/No)", 9, 1, 2, false, false, 46},
    {"no prose list", 9, 0, 3, false, false, 3 + 6 + 36 + 3},
  };
  for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
    const int h = popup_dos_list_height(
      k[i].glyph_h, k[i].prose_lines * (k[i].glyph_h + 1), k[i].rows, k[i].footer, k[i].tiny
    );
    if (h != k[i].h) {
      fprintf(stderr, "%s: want h %d, got %d\n", k[i].what, k[i].h, h);
      rc = 1;
    }
  }
  /* FUN_6f74_0f16: FONTINTR is 9 high, a 6-row font counts as 5, no font = the 6-row default. */
  if (popup_dos_glyph_h(NULL) != 5 || (have_font && popup_dos_glyph_h(&font) != 9)) {
    fprintf(stderr, "popup_dos_glyph_h\n");
    rc = 1;
  }
  ColonizeFont tiny = {.max_height = 6};
  ColonizeFont big = {.max_height = 9};
  if (popup_dos_glyph_h(&tiny) != 5 || popup_dos_glyph_h(&big) != 9) {
    fprintf(stderr, "popup_dos_glyph_h: 6 -> 5, 9 -> 9\n");
    rc = 1;
  }
  if (have_font) {
    font_file_free(&font);
  }
  return rc;
}

/*
 * port_bugs.md #1188: popup_center_frame (save/load, options, pick music, cheat lists, the unit stack
 * picker; via popup_list_render and directly) places like FUN_6f74_14c6, so an odd size sits 1 px
 * past the halve-the-margin form and a tall dialog is not floored under the menu bar.
 */
static int case_center_frame_is_dos(void) {
  int rc = 0;
  static uint8_t pix[W * H];
  ColonizeFramebuffer8 fb = {.width = W, .height = H, .pixels = pix};
  const struct {
    int w, h, x, y;
  } k[] = {
    {196, 68, 62, 66},  /* even / even */
    {157, 69, 82, 66},  /* odd w (old form 81) and odd h (old form 65) */
    {157, 70, 82, 65},
    {190, 192, 65, 4},  /* tall: no MAP_MENU_BAR_H + 2 floor (old y 10) */
  };
  for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
    PopupFrameGeom g;
    popup_center_frame(&fb, k[i].w, k[i].h, NULL, NULL, &g);
    if (g.x != k[i].x || g.y != k[i].y || g.w != k[i].w || g.h != k[i].h) {
      fprintf(stderr, "popup_center_frame %dx%d: want (%d,%d), got (%d,%d)\n", k[i].w, k[i].h,
              k[i].x, k[i].y, g.x, g.y);
      rc = 1;
    }
  }
  /* The shared list pipeline (save/load, cheat list, options, pick music) goes through it. */
  PopupListMetrics m;
  popup_list_metrics_dos6f74(NULL, &m);
  PopupListGeom lg;
  popup_list_render(&fb, NULL, NULL, NULL, &m, 151, "", 7, 0, NULL, NULL, 15, 14, 138, &lg);
  if (lg.frame.x != 160 - (lg.frame.w >> 1) || lg.frame.y != 100 - (lg.frame.h >> 1)) {
    fprintf(stderr, "popup_list_render frame (%d,%d) %dx%d is not the 14c6 centre\n", lg.frame.x,
            lg.frame.y, lg.frame.w, lg.frame.h);
    rc = 1;
  }
  return rc;
}

/*
 * port_bugs.md #1190: the "Units Present" picker is FUN_2b5a_1b5a's single icon column
 * (box[+0x46] 0, box[+0x4a] 2, flags 0x82): at most 10 rows (raw 42760), rows 16 px from
 * frame_y + 5, h = 12 + 16n, w = widest text + 32, text at frame + 25, the selected row
 * framed in colour 15 from (frame + 4, frame + 4 + 16k) to (frame + 21, frame + 21 + 16k),
 * no title, no bar; the hit test and keys walk the one column.
 */
static int case_units_present_is_dos(void) {
  ColonizeFont font;
  if (!load_font(&font)) {
    return TR_SKIP;
  }
  static ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  snprintf(pool.types[0].name, sizeof(pool.types[0].name), "Colonists");
  pool.types[0].movement = 1;
  pool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  for (int i = 0; i < 12; ++i) {
    const int id = units_spawn_allow_stack(&pool, 0, 3, 3);
    units_get(&pool, id)->nation_id = 0;
  }
  UnitStackPopup dlg;
  memset(&dlg, 0, sizeof(dlg));
  int rc = 0;
  if (!unit_stack_try_open(&dlg, &pool, 3, 3, 0) || dlg.count != 10) {
    fprintf(stderr, "units present: %d rows, want the DOS cap of 10\n", dlg.count);
    rc = 1;
  }
  static uint8_t px[W * H];
  memset(px, 0x11, sizeof(px));
  ColonizeFramebuffer8 fb = {.width = W, .height = H, .pixels = px};
  dlg.selection = 2;
  unit_stack_render(&dlg, &pool, NULL, NULL, &font, NULL, NULL, 1, NULL, NULL, &fb);
  if (dlg.dialog_h != 12 + 16 * 10 || dlg.list_y0 != dlg.dialog_y + 5 || dlg.line_h != 16 ||
      dlg.dialog_x != 160 - (dlg.dialog_w >> 1) || dlg.dialog_y != 100 - (dlg.dialog_h >> 1)) {
    fprintf(stderr, "units present: %dx%d at (%d,%d) list_y0 %d line_h %d\n", dlg.dialog_w,
            dlg.dialog_h, dlg.dialog_x, dlg.dialog_y, dlg.list_y0, dlg.line_h);
    rc = 1;
  }
  const int fx = dlg.dialog_x;
  const int top = dlg.dialog_y + 4 + 16 * 2;
  if (px[top * W + fx + 4] != 15 || px[(top + 17) * W + fx + 21] != 15 ||
      px[(top + 8) * W + fx + 4] != 15 || px[(top - 16) * W + fx + 4] == 15) {
    fprintf(stderr, "units present: no colour-15 frame round row 2's icon\n");
    rc = 1;
  }
  /* Row text: no ink left of frame + 25 on a row (the icon column is empty without icons). */
  for (int y = top + 1; y < top + 17; ++y) {
    for (int x = fx + 6; x < fx + 25; ++x) {
      if (px[y * W + x] == 1) {
        fprintf(stderr, "units present: text ink at x %d, before frame + 25\n", x - fx);
        rc = 1;
        y = top + 17;
        break;
      }
    }
  }
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_RIGHT;
  unit_stack_handle_input(&dlg, &pool, &in, NULL);
  in.last_key = COLONIZE_KEY_DOWN;
  unit_stack_handle_input(&dlg, &pool, &in, NULL);
  if (dlg.selection != 3) {
    fprintf(stderr, "units present: keys moved the selection to %d, want 3\n", dlg.selection);
    rc = 1;
  }
  font_file_free(&font);
  return rc;
}

static const TestCase k_cases[] = {
  {"field_body_is_markup", case_field_body_is_markup},
  {"dialog_text_is_dos_shaded", case_dialog_text_is_dos_shaded},
  {"dialog_tilde_is_dos", case_dialog_tilde_is_dos},
  {"stat_screens_text_is_one_pass", case_stat_screens_text_is_one_pass},
  {"field_geometry_is_dos", case_field_geometry_is_dos},
  {"field_decoration_is_dos", case_field_decoration_is_dos},
  {"place_dos", case_place_dos},
  {"list_height_dos", case_list_height_dos},
  {"center_frame_is_dos", case_center_frame_is_dos},
  {"units_present_is_dos", case_units_present_is_dos},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
