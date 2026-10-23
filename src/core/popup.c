#include "core/popup.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <stdio.h>

#include "core/assets.h"
#include "core/fb.h"
#include "core/font.h"
#include "core/popup_msg.h"
#include "core/ui_colors.h"

void popup_colors_from_ui(ColonizePopupColors* out) {
  if (!out) {
    return;
  }
  out->outer = 0;
  out->mid = COLONIZE_COL_BORDER0;
  out->light = COLONIZE_COL_BORDER1;
  out->dark = COLONIZE_COL_BORDER2;
}

void popup_colors_remap(
  ColonizePopupColors* colors,
  const ColonizePalette* source_palette,
  const ColonizePalette* target_palette
) {
  if (!colors) {
    return;
  }
  colors->outer = 0;
  if (!source_palette || !target_palette) {
    return;
  }
  const uint8_t mid = colors->mid;
  const uint8_t light = colors->light;
  const uint8_t dark = colors->dark;
  colors->mid = assets_palette_nearest_rgb(
    target_palette,
    source_palette->rgb[mid][0],
    source_palette->rgb[mid][1],
    source_palette->rgb[mid][2]
  );
  colors->light = assets_palette_nearest_rgb(
    target_palette,
    source_palette->rgb[light][0],
    source_palette->rgb[light][1],
    source_palette->rgb[light][2]
  );
  colors->dark = assets_palette_nearest_rgb(
    target_palette,
    source_palette->rgb[dark][0],
    source_palette->rgb[dark][1],
    source_palette->rgb[dark][2]
  );
}

/* Inclusive-corner fill (x0..x1, y0..y1) over the shared clipped primitive. */
static void popup_fill_rect(
  ColonizeFramebuffer8* fb, int x0, int y0, int x1, int y1, uint8_t color
) {
  fb_fill_rect(fb, x0, y0, x1 - x0 + 1, y1 - y0 + 1, color);
}

void popup_draw(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  const ColonizeSpriteSheet* tile,
  const ColonizePopupColors* colors,
  int* out_inner_x,
  int* out_inner_y,
  int* out_inner_w,
  int* out_inner_h
) {
  ColonizePopupColors local;
  if (!colors) {
    popup_colors_from_ui(&local);
    colors = &local;
  }

  if (out_inner_x) {
    *out_inner_x = 0;
  }
  if (out_inner_y) {
    *out_inner_y = 0;
  }
  if (out_inner_w) {
    *out_inner_w = 0;
  }
  if (out_inner_h) {
    *out_inner_h = 0;
  }

  if (!framebuffer || !framebuffer->pixels || w <= 0 || h <= 0) {
    return;
  }

  if (tile && tile->sprite_count > 0) {
    ss_tile_rect(tile, x, y, w, h, framebuffer);
  } else {
    popup_fill_rect(framebuffer, x, y, x + w - 1, y + h - 1, POPUP_FALLBACK_FILL);
  }

  const int x1 = x + w - 1;
  const int y1 = y + h - 1;

  /* Layer 1: outer black. */
  fb_hline(framebuffer, y, x, x1, colors->outer);
  fb_hline(framebuffer, y1, x, x1, colors->outer);
  fb_vline(framebuffer, x, y, y1, colors->outer);
  fb_vline(framebuffer, x1, y, y1, colors->outer);

  if (w >= 3 && h >= 3) {
    /* Layer 2: mid brown, inset 1. */
    const int mx0 = x + 1;
    const int my0 = y + 1;
    const int mx1 = x1 - 1;
    const int my1 = y1 - 1;
    fb_hline(framebuffer, my0, mx0, mx1, colors->mid);
    fb_hline(framebuffer, my1, mx0, mx1, colors->mid);
    fb_vline(framebuffer, mx0, my0, my1, colors->mid);
    fb_vline(framebuffer, mx1, my0, my1, colors->mid);
  }

  if (w >= 5 && h >= 5) {
    /*
     * Layer 3: bevel inset 2 — light top/right, dark bottom/left.
     *
     * DOS `FUN_6f74_2278` (6f74:2378..23c4) emits the four runs in a fixed
     * order, and the order decides the two shared corners: left vline
     * (`DS:0x1f48` = @COLORS border2) at x+2, then right vline (`DS:0x1f46`
     * = border1) at x+w-3, both y+2..y+h-3; then the top hline (border1) and
     * the bottom hline (border2), both spanning x+2..x+w-3. The horizontals
     * land last, so the TOP-LEFT corner pixel is LIGHT and the bottom-right
     * is DARK. Drawing top-then-left instead (the port's old order) flipped
     * that top-left pixel to dark.
     */
    const int bx0 = x + 2;
    const int by0 = y + 2;
    const int bx1 = x1 - 2;
    const int by1 = y1 - 2;
    fb_vline(framebuffer, bx0, by0, by1, colors->dark); /* left */
    fb_vline(framebuffer, bx1, by0, by1, colors->light); /* right */
    fb_hline(framebuffer, by0, bx0, bx1, colors->light); /* top */
    fb_hline(framebuffer, by1, bx0, bx1, colors->dark); /* bottom */
  }

  if (w > POPUP_FRAME_INSET * 2 && h > POPUP_FRAME_INSET * 2) {
    if (out_inner_x) {
      *out_inner_x = x + POPUP_FRAME_INSET;
    }
    if (out_inner_y) {
      *out_inner_y = y + POPUP_FRAME_INSET;
    }
    if (out_inner_w) {
      *out_inner_w = w - POPUP_FRAME_INSET * 2;
    }
    if (out_inner_h) {
      *out_inner_h = h - POPUP_FRAME_INSET * 2;
    }
  }
}

void popup_draw_text_shadowed(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  if (!font || !framebuffer || !text || !text[0]) {
    return;
  }
  font_draw_text_unbold(font, framebuffer, x + 1, y + 1, text, 0);
  font_draw_text_unbold(font, framebuffer, x, y, text, color);
}

/* Strips {}. LINE/CENTER marks are NOT stripped here — callers measuring
 * marked-up bodies must strip those first (ai_popup does); passing a raw
 * marked line over-measures (audit 2026-09-09 #105). */
int popup_markup_text_width(const ColonizeFont* font, const char* text) {
  if (!text) {
    return 0;
  }
  char plain[512];
  size_t t = 0;
  for (const char* p = text; *p && t + 1 < sizeof(plain); ++p) {
    if (*p == '{' || *p == '}') {
      continue;
    }
    plain[t++] = *p;
  }
  plain[t] = '\0';
  return font_text_width(font, plain);
}

int popup_draw_text_markup(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t base_color,
  uint8_t hilite_color,
  bool unbold,
  bool shadow,
  bool* inout_hilite
) {
  if (!font || !text) {
    return x;
  }
  bool hi = inout_hilite ? *inout_hilite : false;
  int cx = x;
  const char* p = text;
  while (*p) {
    if (*p == '{') {
      hi = true;
      ++p;
      continue;
    }
    if (*p == '}') {
      hi = false;
      ++p;
      continue;
    }
    const char* start = p;
    while (*p && *p != '{' && *p != '}') {
      ++p;
    }
    const uint8_t ink = hi ? hilite_color : base_color;
    size_t left = (size_t)(p - start);
    while (left > 0) { /* slice runs longer than the scratch buffer */
      char run[256];
      size_t n = left < sizeof(run) - 1 ? left : sizeof(run) - 1;
      memcpy(run, start, n);
      run[n] = '\0';
      if (framebuffer) {
        if (unbold) {
          if (shadow) {
            font_draw_text_unbold(font, framebuffer, cx + 1, y + 1, run, 0);
          }
          font_draw_text_unbold(font, framebuffer, cx, y, run, ink);
        } else {
          if (shadow) {
            font_draw_text(font, framebuffer, cx + 1, y + 1, run, 0);
          }
          font_draw_text(font, framebuffer, cx, y, run, ink);
        }
      }
      cx += font_text_width(font, run);
      start += n;
      left -= n;
    }
  }
  if (inout_hilite) {
    *inout_hilite = hi;
  }
  return cx;
}


/* ---- Shared wood list-dialog pipeline (audit theme K) ---- */

int popup_row_at_y(int list_y0, int line_h, int count, int mouse_y) {
  if (line_h <= 0 || count <= 0) {
    return -1;
  }
  const int rel = mouse_y - list_y0;
  if (rel < 0) {
    return -1;
  }
  const int idx = rel / line_h;
  if (idx < 0 || idx >= count) {
    return -1;
  }
  return idx;
}

void popup_center_frame(
  ColonizeFramebuffer8* framebuffer,
  int w,
  int h,
  const ColonizeSpriteSheet* tile,
  const ColonizePopupColors* colors,
  PopupFrameGeom* out
) {
  PopupFrameGeom geom;
  memset(&geom, 0, sizeof(geom));
  if (!framebuffer || !framebuffer->pixels) {
    if (out) {
      *out = geom;
    }
    return;
  }
  ColonizePopupColors local;
  if (!colors) {
    popup_colors_from_ui(&local);
    colors = &local;
  }
  /*
   * Every user is a DOS 6f74 compositor dialog without @X/@Y (save/load FUN_7562_0052 via
   * FUN_291f_0182, the @GAMEOPTIONS / @COLONYOPTIONS / @SOUNDOPTIONS @CHECKBOX lists, @PICKMUSIC,
   * the cheat / goto lists of FUN_647e_01c6 & co, and the "Units Present" stack picker
   * FUN_2b5a_1b5a via FUN_291f_023c), so FUN_6f74_14c6 centres them: popup_place_dos, with no
   * menu-bar floor (port_bugs.md #1188; the old floor had no DOS counterpart).
   */
  const PopupDosPlace pl = popup_place_dos(framebuffer->width, framebuffer->height, w, h, NULL);
  geom.x = pl.x;
  geom.y = pl.y;
  geom.w = w;
  geom.h = h;
  popup_draw(
    framebuffer,
    geom.x,
    geom.y,
    w,
    h,
    tile,
    colors,
    &geom.inner_x,
    &geom.inner_y,
    &geom.inner_w,
    &geom.inner_h
  );
  if (out) {
    *out = geom;
  }
}

void popup_list_metrics_dos6f74(const ColonizeFont* font, PopupListMetrics* out) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  /*
   * FUN_6f74_0f16 row height: the 6-px font counts as 5. Body/prompt lines
   * advance by glyph_h + 1 (FUN_6f74_1198 @ OVL24 0x1234/0x124c); the +0x54
   * option rows advance by glyph_h + box[+0x46], and box[+0x46] is 3 for every
   * framed dialog (FUN_6f74_06d0 @ 0x078a-0x0799). The selection bar is
   * glyph_h + 2 tall (FUN_6f74_1b7c @ 0x1c5b-0x1c9b). Height and row tops:
   * popup_list_render (port_bugs.md #1189, #1190).
   */
  const int glyph_h = popup_dos_glyph_h(font);
  out->line_h = glyph_h + 1;
  out->option_h = glyph_h + 3;
  out->pad_x = 2; /* box[+0x48]: prompt at frame + 5 */
  out->pad_y = 3; /* prompt top frame + 6 */
  out->label_dx = 4; /* box[+0x22], FUN_6f74_06d0 default; FUN_6f74_0a00 widens by it too */
  out->sel_h = glyph_h + 2;
  /* A FONTTINY dialog draws no drop shadow (original_screenshots/europe/train.png, #1187). */
  out->shadow_text = glyph_h != 5;
}

static void popup_list_draw_row_text(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  const PopupListMetrics* m,
  int x,
  int y,
  const char* text,
  uint8_t text_color,
  uint8_t hilite_color
) {
  if (!font || !text) {
    return;
  }
  if (m->markup_text) {
    popup_draw_text_markup(
      font, framebuffer, x, y, text, text_color, hilite_color, true, true, NULL
    );
  } else if (m->shadow_text) {
    popup_draw_text_shadowed(font, framebuffer, x, y, text, text_color);
  } else {
    font_draw_text(font, framebuffer, x, y, text, text_color);
  }
}

void popup_list_render(
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  const PopupListMetrics* metrics,
  int width,
  const char* prompt,
  int count,
  int selection,
  PopupListLabelFn label_fn,
  void* user,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color,
  PopupListGeom* out
) {
  PopupListGeom geom;
  memset(&geom, 0, sizeof(geom));
  if (!framebuffer || !framebuffer->pixels || !metrics) {
    if (out) {
      *out = geom;
    }
    return;
  }
  const PopupListMetrics* m = metrics;
  const bool has_prompt = prompt && prompt[0];
  if (count < 0) {
    count = 0;
  }

  /* One prompt line (FUN_6f74_1198); every caller's prompt fits the @width. */
  const int prompt_h = has_prompt ? m->line_h : 0;
  int dialog_h = popup_dos_list_height(popup_dos_glyph_h(font), prompt_h, count, false, false);
  if (dialog_h > framebuffer->height) {
    dialog_h = framebuffer->height;
  }

  /* FUN_6f74_14c6: content = max(box[+0x28] (@width, default 0x50), widest row), the
   * row width being FUN_6f74_0a00's text + 2 * box[+0x48] + box[+0x22]. */
  int content_w = width > 0x50 ? width : 0x50;
  if (font) {
    if (has_prompt) {
      const int w = font_text_width(font, prompt) + 2 * m->pad_x;
      if (w > content_w) {
        content_w = w;
      }
    }
    for (int i = 0; i < count; ++i) {
      const char* label = label_fn ? label_fn(user, i) : NULL;
      if (!label) {
        continue;
      }
      const int w = font_text_width(font, label) + 2 * m->pad_x + m->label_dx;
      if (w > content_w) {
        content_w = w;
      }
    }
  }
  int dialog_w = content_w + 6; /* 3 px of wood per side */
  if (dialog_w > framebuffer->width) {
    dialog_w = framebuffer->width;
  }

  popup_center_frame(framebuffer, dialog_w, dialog_h, wood_tile, colors, &geom.frame);
  geom.line_h = m->option_h;

  const int text_y = geom.frame.inner_y + m->pad_y;
  if (has_prompt && font) {
    if (m->shadow_text) {
      popup_draw_text_shadowed(
        font, framebuffer, geom.frame.inner_x + m->pad_x, text_y, prompt, text_color
      );
    } else {
      font_draw_text(
        font, framebuffer, geom.frame.inner_x + m->pad_x, text_y, prompt, text_color
      );
    }
  }
  /* list_y0 is row 0's bar top and the text sits one px lower (FUN_6f74_1b7c). */
  geom.list_y0 = popup_dos_list_y0(geom.frame.y, prompt_h);

  for (int i = 0; i < count; ++i) {
    const int row_y = geom.list_y0 + i * m->option_h + 1;
    if (i == selection) {
      /* Bar: frame + 4 .. frame + w - 5, from the bar top. */
      popup_fill_rect(
        framebuffer,
        geom.frame.inner_x + 1,
        row_y - 1,
        geom.frame.inner_x + geom.frame.inner_w - 2,
        row_y - 1 + m->sel_h - 1,
        select_color
      );
    }
    const char* label = label_fn ? label_fn(user, i) : NULL;
    popup_list_draw_row_text(
      font, framebuffer, m, geom.frame.inner_x + m->pad_x + m->label_dx, row_y, label,
      text_color, hilite_color
    );
  }

  if (out) {
    *out = geom;
  }
}

int popup_dos_glyph_h(const ColonizeFont* font) {
  const int h = font ? (int)font->max_height : 6;
  return h == 6 ? 5 : h;
}

/* DOS-LITERAL FUN_6f74_14c6 height, raw 115900-115994 (6f74:1540-16c8); see popup.h. */
int popup_dos_list_height(int glyph_h, int prose_h, int rows, bool help_footer, bool tiny_font) {
  enum { MARGIN_V = 0, SPACING = 3, FRAME = 6 };
  int h = 2 * MARGIN_V + SPACING + FRAME;
  const int lists = prose_h + (glyph_h + SPACING) * rows;
  h += lists + (lists != 0 ? SPACING : 0);
  if (help_footer) {
    h += tiny_font ? 6 : 3;
  }
  return h;
}

/* DOS-LITERAL FUN_6f74_14c6 box[+0x26] (raw 115927-115932, 115962-115966, 116137); see popup.h. */
int popup_dos_list_y0(int frame_y, int prose_h) {
  return frame_y + 6 + (prose_h ? 3 + prose_h : 0);
}

/* DOS-LITERAL FUN_6f74_1a78 (raw 116166-116191); see popup.h. */
void popup_dos_draw_help_footer(
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* tiny_font,
  int frame_x,
  int frame_y,
  int w,
  int h,
  bool tiny_dialog,
  const char* word,
  uint8_t color
) {
  if (!tiny_font || !word || !word[0]) {
    return;
  }
  const int x = frame_x + w - 2 - font_text_width(tiny_font, word);
  font_draw_text_unbold(
    tiny_font, framebuffer, x, frame_y + h - (tiny_dialog ? 9 : 7), word, color
  );
}

/*
 * DOS-LITERAL FUN_6f74_14c6 placement (raw 115995-116012 and 116068-116132;
 * 6f74:16c8-1735, 18ae-1a0c), shared by every compositor dialog the port places itself.
 *
 * Plain arm: x = 160 - (w >> 1), y = 100 - (h >> 1) (SAR / SUB / NEG), then the
 * right edge is pulled back to 0x140 and the bottom to 200. There is no menu-bar
 * or top floor in DOS; a negative origin only calls the FUN_281f_0772 error
 * handler (6f74:1746, args 0xffaf/2/0x29), which the port does not reproduce
 * (it draws clipped).
 *
 * Figure arm (MSS/MYR sheet latched in DS:0x1f5e/0x1f60): the frame and the figure
 * are placed as one assembly. Total height t = sprite_h - min(sprite_h, oy) + h;
 * t >= 200 drops the figure and the frame keeps its plain spot, else the figure sits
 * at y 100 - (t >> 1) and the frame at fig_y - oy + sprite_h (oy raw, not clamped).
 * Mode 0 puts the figure left (overlap ox; total width clamped to 320 by widening
 * ox), mode 1 centres it over the unmoved frame, mode 2 puts it right.
 *
 * Port-only: 160 / 100 / 320 / 200 are fb_w / 2, fb_h / 2, fb_w, fb_h, so the
 * overland map view in a window larger than 320x200 centres in its own frame;
 * on the 320x200 frame (every other screen, screens-on-map) they are the DOS values.
 */
PopupDosPlace popup_place_dos(
  int fb_w, int fb_h, int w, int h, const ColonizeSpriteSheet* figure
) {
  PopupDosPlace p;
  memset(&p, 0, sizeof(p));
  p.x = fb_w / 2 - (w >> 1);
  p.y = fb_h / 2 - (h >> 1);
  if (p.x + w > fb_w) {
    p.x -= p.x + w - fb_w;
  }
  if (p.y + h > fb_h) {
    p.y += fb_h - (p.y + h);
  }
  if (!figure || figure->sprite_count <= 0 || !figure->sprites[0].pixels) {
    return p;
  }
  const int sw = figure->sprites[0].width;
  const int sh = figure->sprites[0].height;
  const int oy = figure->place_offset_y;
  const int total_h = sh - (oy < sh ? oy : sh) + h;
  if (total_h >= fb_h) {
    return p;
  }
  p.figure = true;
  p.fig_y = fb_h / 2 - (total_h >> 1);
  p.y = p.fig_y - oy + sh;
  int ox = figure->place_offset_x;
  int total_w = w - ox + sw;
  if (figure->place_mode == 1) {
    p.fig_x = fb_w / 2 - (sw >> 1);
    return p;
  }
  if (total_w > fb_w) {
    ox += total_w - fb_w;
    total_w = fb_w;
  }
  if (figure->place_mode == 2) {
    p.x = fb_w / 2 - (total_w >> 1);
    p.fig_x = p.x + w - ox;
  } else {
    p.fig_x = fb_w / 2 - (total_w >> 1);
    p.x = p.fig_x - ox + sw;
  }
  return p;
}

void popup_field_frame(
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  int width,
  const char* prompt,
  const char* label,
  int max_len,
  uint8_t text_color,
  const ColonizeSpriteSheet* decoration,
  PopupFieldGeom* out
) {
  PopupFieldGeom geom;
  memset(&geom, 0, sizeof(geom));
  if (!framebuffer || !framebuffer->pixels) {
    if (out) {
      *out = geom;
    }
    return;
  }
  /* FUN_6f74_0f16: font[0], a 6-row font counts as 5. */
  int gh = font ? (int)font->max_height : 6;
  if (gh == 6) {
    gh = 5;
  }
  geom.glyph_h = gh;
  geom.text_h = gh + 1;
  /* FUN_6f74_0d44: label + " " (DS:0x1f95), field text = width("W") * max_len. */
  char label_sp[POPUP_MSG_CHOICE_LEN + 2];
  snprintf(label_sp, sizeof(label_sp), "%s ", label ? label : "");
  const int label_w = font ? font_text_width(font, label_sp) : 0;
  const int text_w = (font ? font_text_width(font, "W") : 0) * max_len;
  /* FUN_6f74_14c6 prologue: content width = max(0x50, @width, label_w + text_w + 10). */
  int content_w = 0x50;
  if (width > content_w) {
    content_w = width;
  }
  if (label_w + text_w + 10 > content_w) {
    content_w = label_w + text_w + 10;
  }
  if (content_w + 6 > framebuffer->width) {
    content_w = framebuffer->width - 6;
  }
  /* FUN_6f74_1198: the prompt flows inside content - 4. */
  enum { MAX_ROWS = 8 };
  char lines[MAX_ROWS][128];
  bool centred[MAX_ROWS];
  memset(centred, 0, sizeof(centred));
  const int n = font ? popup_wrap_text(
                         font, prompt, &lines[0][0], sizeof(lines[0]), centred, MAX_ROWS,
                         content_w - 4
                       )
                     : 0;
  const int body_h = n * (gh + 1);
  const int w = content_w + 6;
  const int h = body_h + gh + 20;
  /* FUN_6f74_14c6 placement (plain and DS:0x1f5e figure arms): popup_place_dos. */
  const PopupDosPlace pl = popup_place_dos(framebuffer->width, framebuffer->height, w, h, decoration);
  const int x = pl.x;
  const int y = pl.y;
  const ColonizeSpriteSheet* figure = pl.figure ? decoration : NULL;
  const int fig_x = pl.fig_x;
  const int fig_y = pl.fig_y;
  ColonizePopupColors local_colors;
  if (!colors) {
    popup_colors_from_ui(&local_colors);
    colors = &local_colors;
  }
  geom.frame.x = x;
  geom.frame.y = y;
  geom.frame.w = w;
  geom.frame.h = h;
  popup_draw(
    framebuffer, x, y, w, h, wood_tile, colors, &geom.frame.inner_x, &geom.frame.inner_y,
    &geom.frame.inner_w, &geom.frame.inner_h
  );
  const int y0 = y + 9 + body_h;
  geom.box_x = x + 5 + label_w;
  geom.box_y = y0;
  geom.box_w = text_w + 6;
  geom.box_h = gh + 5;
  geom.text_x = geom.box_x + 3;
  geom.text_y = y0 + 3;
  if (font) {
    bool emph = false;
    for (int i = 0; i < n; ++i) {
      int lx = x + 5;
      if (centred[i]) {
        const int lw = popup_markup_text_width(font, lines[i]);
        if (lw < content_w - 4) {
          lx += (content_w - 4 - lw) / 2;
        }
      }
      popup_draw_text_markup(
        font, framebuffer, lx, y + 6 + i * (gh + 1), lines[i], text_color, COLONIZE_COL_HILITE,
        true, true, &emph /* unbold: same FONTINTR body style as every popup (port_bugs.md #1180) */
      );
    }
    if (label && label[0]) {
      popup_draw_text_markup(
        font, framebuffer, x + 5, y0 + 3, label, text_color, COLONIZE_COL_HILITE, true, true, &emph
      );
    }
  }
  fb_rect_outline(framebuffer, geom.box_x, geom.box_y, geom.box_w, geom.box_h, text_color, text_color);
  if (figure) {
    /* FUN_6f74_248e: the figure goes over the finished frame and body. */
    ss_blit_sprite(figure, 0, framebuffer, fig_x, fig_y);
  }
  if (out) {
    *out = geom;
  }
}

/* ---- Shared word wrap (promoted from ai_popup_wrap_body; audit GL-8) ---- */

int popup_wrap_text(
  const ColonizeFont* font,
  const char* text,
  char* out,
  size_t out_stride,
  bool* out_center,
  int max_out,
  int max_w
) {
  int count = 0;
  if (!text || !text[0] || !out || out_stride < 2 || max_out <= 0) {
    return 0;
  }
  char accum[512];
  accum[0] = '\0';

  const char* p = text;
  while (*p && count < max_out) {
    while (*p == ' ') {
      p++;
    }
    if (!*p) {
      break;
    }
    if (*p == '\n') {
      if (accum[0]) {
        snprintf(out + (size_t)count * out_stride, out_stride, "%s", accum);
        if (out_center) {
          out_center[count] = false;
        }
        count++;
        accum[0] = '\0';
        if (count >= max_out) {
          return count;
        }
      } else {
        out[(size_t)count * out_stride] = '\0';
        if (out_center) {
          out_center[count] = false;
        }
        count++;
      }
      p++;
      continue;
    }
    if (*p == POPUP_MSG_LINE_MARK || *p == POPUP_MSG_CENTER_MARK) {
      /* Caret row: flush the paragraph, then take the rest of the source line
       * whole — no wrapping, no re-flowing into what follows. */
      if (accum[0]) {
        snprintf(out + (size_t)count * out_stride, out_stride, "%s", accum);
        if (out_center) {
          out_center[count] = false;
        }
        count++;
        accum[0] = '\0';
        if (count >= max_out) {
          return count;
        }
      }
      const bool centered = (*p == POPUP_MSG_CENTER_MARK);
      p++;
      const char* row = p;
      while (*p && *p != '\n') {
        p++;
      }
      size_t rn = (size_t)(p - row);
      if (rn >= out_stride) {
        rn = out_stride - 1;
      }
      char* dst = out + (size_t)count * out_stride;
      memcpy(dst, row, rn);
      dst[rn] = '\0';
      if (out_center) {
        out_center[count] = centered;
      }
      count++;
      if (*p == '\n') {
        p++; /* the trailing break is the row's own terminator, not a blank */
      }
      continue;
    }

    const char* start = p;
    while (*p && *p != ' ' && *p != '\n') {
      p++;
    }
    char word[512];
    size_t n = (size_t)(p - start);
    if (n >= sizeof(word)) {
      n = sizeof(word) - 1;
    }
    memcpy(word, start, n);
    word[n] = '\0';

    const int word_w = popup_markup_text_width(font, word);
    if (accum[0]) {
      const int space_w = font_text_width(font, " ");
      if (popup_markup_text_width(font, accum) + space_w + word_w > max_w) {
        snprintf(out + (size_t)count * out_stride, out_stride, "%s", accum);
        if (out_center) {
          out_center[count] = false;
        }
        count++;
        accum[0] = '\0';
        if (count >= max_out) {
          return count;
        }
      }
    }
    size_t len = strlen(accum);
    if (accum[0] && len + 1 < sizeof(accum)) {
      accum[len++] = ' ';
      accum[len] = '\0';
    }
    for (const char* w = word; *w && len + 1 < sizeof(accum); ++w) {
      accum[len++] = *w;
    }
    accum[len] = '\0';
  }
  if (accum[0] && count < max_out) {
    snprintf(out + (size_t)count * out_stride, out_stride, "%s", accum);
    if (out_center) {
      out_center[count] = false;
    }
    count++;
  }
  return count;
}
