#include "core/popup.h"

#include <ctype.h>
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
  out->enhance = COLONIZE_COL_ENHANCE;
  out->shadow = COLONIZE_COL_SHADOW;
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
  const uint8_t enhance = colors->enhance;
  const uint8_t shadow = colors->shadow;
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
  colors->enhance = assets_palette_nearest_rgb(
    target_palette,
    source_palette->rgb[enhance][0],
    source_palette->rgb[enhance][1],
    source_palette->rgb[enhance][2]
  );
  colors->shadow = assets_palette_nearest_rgb(
    target_palette,
    source_palette->rgb[shadow][0],
    source_palette->rgb[shadow][1],
    source_palette->rgb[shadow][2]
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

/* FUN_6f74_0486 (the 0a00 / 1b7c row measure) deletes every char of DS:0x1f8c "~{}|"
 * before measuring, escaped ones too, so a '~' and the char after it cost the char only.
 * LINE/CENTER marks are NOT stripped here — callers measuring marked-up bodies must strip
 * those first (ai_popup does); passing a raw marked line over-measures (audit 2026-09-09 #105). */
int popup_markup_text_width(const ColonizeFont* font, const char* text) {
  if (!text) {
    return 0;
  }
  char plain[512];
  size_t t = 0;
  for (const char* p = text; *p && t + 1 < sizeof(plain); ++p) {
    if (!strchr("~{}|", *p)) {
      plain[t++] = *p;
    }
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
  const ColonizePopupColors* colors,
  bool* inout_hilite
) {
  if (!font || !text) {
    return x;
  }
  ColonizePopupColors ui;
  if (!colors) {
    popup_colors_from_ui(&ui);
    colors = &ui;
  }
  bool hi = inout_hilite ? *inout_hilite : false;
  int cx = x;
  const char* p = text;
  while (*p && *p != '|') { /* 0538: '|' ends the text */
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
    if (*p == '~') {
      /* 6f74:05be: reload the map with DX = (DS:0x1f62 == 0), i.e. the other ink, draw the
       * next byte as a plain glyph (so '~' also escapes '{' '}' '|' '~'), restore with
       * DX = 0x1f62 (6f74:060a). DOS reads past a trailing '~'; stop at the NUL. */
      if (!*++p) {
        break;
      }
      const uint8_t esc[4] = {0, hi ? base_color : hilite_color, colors->enhance, colors->shadow};
      /* A char the face lacks draws nothing and advances 0 (font.c, FUN_1c11_000c): FONTTINY
       * has no '=', so the jobs rows' DS:0xca8 " = " shows as two spaces (port_bugs.md #1225). */
      cx += font_draw_glyph_shaded(font, framebuffer, cx, y, (unsigned char)*p++, esc);
      continue;
    }
    /* FUN_6f74_04f6 -> FUN_1c28_000a: DS:0x269e = {skip, ink, style+8, style+0xa}. */
    const uint8_t shades[4] = {0, hi ? hilite_color : base_color, colors->enhance, colors->shadow};
    char run[256];
    size_t n = 0;
    while (*p && !strchr("{}~|", *p)) {
      run[n++] = *p++;
      /* flush at the slice end, or when the scratch buffer is full */
      if (n == sizeof(run) - 1 || strchr("{}~|", *p)) {
        run[n] = '\0';
        if (framebuffer) {
          font_draw_text_shaded(font, framebuffer, cx, y, run, shades);
        }
        cx += font_text_width(font, run);
        n = 0;
      }
    }
  }
  if (inout_hilite) {
    *inout_hilite = hi;
  }
  return cx;
}

/* DOS-LITERAL FUN_6f74_0642 raw 115049 (strrchr FUN_1d1d_10ea, strchr FUN_1d1d_1010,
 * ctype DS:0x27ed & 4 = digit, & 2 = lower). */
int popup_hotkey_of(const char* label) {
  const char* last = label ? strrchr(label, '~') : NULL;
  if (!last) {
    return 0;
  }
  const char* first = strchr(label, '~');
  const unsigned char k = (unsigned char)last[1];
  if (first != last && first[1] == 'F' && isdigit(k)) {
    return k + 0x10a;
  }
  return islower(k) ? k - 0x20 : k;
}

int popup_input_dos_key(const ColonizeInputState* input) {
  if (!input || input->alt_held || input->ctrl_held) {
    return 0;
  }
  if (input->last_key >= COLONIZE_KEY_F1 && input->last_key <= COLONIZE_KEY_F9) {
    return 0x13b + (int)(input->last_key - COLONIZE_KEY_F1);
  }
  if (input->text_input_len <= 0) {
    return 0;
  }
  const unsigned char c = (unsigned char)input->text_input[0];
  return islower(c) ? c - 0x20 : c;
}

bool popup_list_key_step(ColonizeKey key, int* selection, int count, uint32_t disabled_mask) {
  const int step = colonize_key_up(key) ? -1 : colonize_key_down(key) ? 1 : 0;
  if (!step || !selection || count <= 0) {
    return false;
  }
  /* DOS-LITERAL FUN_6f74_2580 raw 116905-116940: do { cur = cur->prev / next; if null,
   * cur = last / first, wraps++ } while (cur disabled && wraps < 2). */
  int sel = *selection;
  int wraps = 0;
  do {
    if (sel >= 0 && sel < count) {
      sel += step;
    }
    if (sel < 0 || sel >= count) {
      sel = step < 0 ? count - 1 : 0;
      wraps++;
    }
  } while (sel < 32 && (disabled_mask >> sel & 1u) && wraps < 2);
  *selection = sel;
  return true;
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

static void popup_frame_placed(
  ColonizeFramebuffer8* framebuffer,
  int w,
  int h,
  const ColonizeSpriteSheet* tile,
  const ColonizePopupColors* colors,
  const ColonizeSpriteSheet* figure,
  PopupDosPlace* place,
  PopupFrameGeom* out
);

void popup_center_frame(
  ColonizeFramebuffer8* framebuffer,
  int w,
  int h,
  const ColonizeSpriteSheet* tile,
  const ColonizePopupColors* colors,
  PopupFrameGeom* out
) {
  popup_frame_placed(framebuffer, w, h, tile, colors, NULL, NULL, out);
}

static void popup_frame_placed(
  ColonizeFramebuffer8* framebuffer,
  int w,
  int h,
  const ColonizeSpriteSheet* tile,
  const ColonizePopupColors* colors,
  const ColonizeSpriteSheet* figure,
  PopupDosPlace* place,
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
  const PopupDosPlace pl = popup_place_dos(framebuffer->width, framebuffer->height, w, h, figure);
  if (place) {
    *place = pl;
  }
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

  /* FUN_6f74_14c6: content = max(box[+0x28] (@width as given, 0x50 without one: FUN_6f74_06d0
   * / 0c22), widest row), the row width being FUN_6f74_0a00's text less ~{}| (FUN_6f74_0486)
   * + 2 * box[+0x48] + box[+0x22]. The prompt does not widen it: FUN_6f74_1198 wraps it to
   * content - 2 * box[+0x48]. */
  int content_w = width > 0 ? width : 0x50;
  if (font) {
    for (int i = 0; i < count; ++i) {
      const char* label = label_fn ? label_fn(user, i) : NULL;
      if (!label) {
        continue;
      }
      const int w = popup_markup_text_width(font, label) + 2 * m->pad_x + m->label_dx;
      if (w > content_w) {
        content_w = w;
      }
    }
  }
  if (content_w + 6 > framebuffer->width) {
    content_w = framebuffer->width - 6;
  }
  enum { PROMPT_ROWS = 8 };
  char prompt_lines[PROMPT_ROWS][128];
  bool prompt_centred[PROMPT_ROWS];
  memset(prompt_centred, 0, sizeof(prompt_centred));
  const int prompt_n = (has_prompt && font)
                         ? popup_wrap_text(font, prompt, prompt_lines[0], sizeof(prompt_lines[0]),
                                           prompt_centred, PROMPT_ROWS, content_w - 2 * m->pad_x)
                         : 0;
  const int prompt_h = prompt_n * m->line_h;
  int dialog_h = popup_dos_list_height(popup_dos_glyph_h(font), prompt_h, count, false, false);
  if (dialog_h > framebuffer->height) {
    dialog_h = framebuffer->height;
  }
  int dialog_w = content_w + 6; /* 3 px of wood per side */
  if (dialog_w > framebuffer->width) {
    dialog_w = framebuffer->width;
  }

  PopupDosPlace place = {0};
  popup_frame_placed(
    framebuffer, dialog_w, dialog_h, wood_tile, colors, m->figure, &place, &geom.frame
  );
  geom.line_h = m->option_h;

  const int text_y = geom.frame.inner_y + m->pad_y;
  /* Prompt (FUN_6f74_116c) and rows (FUN_6f74_1b7c) both go through FUN_6f74_0538, the
   * DS:0x1f62 {} state carrying from line to line. */
  bool emph = false;
  for (int i = 0; i < prompt_n; ++i) {
    const int dx =
      prompt_centred[i] ? popup_dos_center_dx(font, prompt_lines[i], content_w - 2 * m->pad_x) : 0;
    popup_draw_text_markup(
      font, framebuffer, geom.frame.inner_x + m->pad_x + dx, text_y + i * m->line_h,
      prompt_lines[i], text_color, hilite_color, colors, &emph
    );
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
    const bool disabled = i < 32 && (m->disabled_mask & (1u << i));
    popup_draw_text_markup(
      font, framebuffer, geom.frame.inner_x + m->pad_x + m->label_dx, row_y, label,
      disabled ? COLONIZE_COL_GREY : text_color, disabled ? COLONIZE_COL_GREY : hilite_color,
      colors, &emph
    );
  }
  if (place.figure) {
    /* FUN_6f74_248e: the MSS figure goes on last (DS:0x1f5c < 0). */
    ss_blit_sprite(m->figure, 0, framebuffer, place.fig_x, place.fig_y);
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

/*
 * `frameless` = box flag 0x10 (DS:0x1f56, set by FUN_281f_040a -> FUN_6f74_37f6 before the
 * box is built): FUN_6f74_06d0 zeroes box[+0x46] (row spacing) and box[+0x48] (margin)
 * and FUN_6f74_14c6 box[+0x2a] (frame), and FUN_6f74_248e draws no wood.
 */
static void popup_field_frame_impl(
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
  bool frameless,
  PopupFieldGeom* out
) {
  const int fr = frameless ? 0 : 3; /* box[+0x2a] */
  const int sp = frameless ? 0 : 3; /* box[+0x46] */
  const int margin = frameless ? 0 : 2; /* box[+0x48] */
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
  /* FUN_6f74_14c6 prologue: content width = max(box[+0x28], label_w + text_w + 10), box[+0x28]
   * being @width as given (FUN_6f74_0c22) or the FUN_6f74_06d0 default 0x50 (width 0). */
  int content_w = width > 0 ? width : 0x50;
  if (label_w + text_w + 10 > content_w) {
    content_w = label_w + text_w + 10;
  }
  if (content_w + 2 * fr > framebuffer->width) {
    content_w = framebuffer->width - 2 * fr;
  }
  /* FUN_6f74_1198: the prompt flows inside content - 2 * margin. */
  enum { MAX_ROWS = 8 };
  char lines[MAX_ROWS][128];
  bool centred[MAX_ROWS];
  memset(centred, 0, sizeof(centred));
  const int n = font ? popup_wrap_text(
                         font, prompt, &lines[0][0], sizeof(lines[0]), centred, MAX_ROWS,
                         content_w - 2 * margin
                       )
                     : 0;
  const int body_h = n * (gh + 1);
  const int w = content_w + 2 * fr;
  /* 14c6: 2 * fr + sp + (prose + (gh + sp + 5) + sp); framed = body_h + gh + 20. */
  const int h = 2 * fr + 3 * sp + body_h + gh + 5;
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
  if (frameless) {
    geom.frame.inner_x = x;
    geom.frame.inner_y = y;
    geom.frame.inner_w = w;
    geom.frame.inner_h = h;
  } else {
    popup_draw(
      framebuffer, x, y, w, h, wood_tile, colors, &geom.frame.inner_x, &geom.frame.inner_y,
      &geom.frame.inner_w, &geom.frame.inner_h
    );
  }
  const int tx = x + fr + margin;
  const int y0 = y + fr + 2 * sp + body_h;
  geom.box_x = tx + label_w;
  geom.box_y = y0;
  geom.box_w = text_w + 6;
  geom.box_h = gh + 5;
  geom.text_x = geom.box_x + 3;
  geom.text_y = y0 + 3;
  if (font) {
    bool emph = false;
    for (int i = 0; i < n; ++i) {
      /* FUN_6f74_1198 '^^' row (6f74:1263-128a): x += (wrap_w >> 1) - (width >> 1). */
      const int lx = tx + (centred[i] ? popup_dos_center_dx(font, lines[i], content_w - 2 * margin)
                                      : 0);
      popup_draw_text_markup(
        font, framebuffer, lx, y + fr + sp + i * (gh + 1), lines[i], text_color,
        COLONIZE_COL_HILITE, colors, &emph
      );
    }
    if (label && label[0]) {
      popup_draw_text_markup(
        font, framebuffer, tx, y0 + 3, label, text_color, COLONIZE_COL_HILITE, colors, &emph
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
  popup_field_frame_impl(
    framebuffer, font, wood_tile, colors, width, prompt, label, max_len, text_color, decoration,
    false, out
  );
}

void popup_field_frame_frameless(
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  const ColonizePopupColors* colors,
  int width,
  const char* prompt,
  const char* label,
  int max_len,
  uint8_t text_color,
  PopupFieldGeom* out
) {
  popup_field_frame_impl(
    framebuffer, font, NULL, colors, width, prompt, label, max_len, text_color, NULL, true, out
  );
}

int popup_dos_center_dx(const ColonizeFont* font, const char* text, int wrap_w) {
  return (wrap_w >> 1) - (popup_markup_text_width(font, text) >> 1);
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
  /*
   * FUN_6f74_1198 (6f74:132e-1435) keeps a running width local_8: each word is measured
   * as " word" (the space only when the line already has text) and the line breaks
   * when local_8 + that > the wrap width. On a break the space is stripped from the
   * text but local_8 restarts at the measured " word", so a continuation line's first
   * word is charged one space it does not draw. DOSBox-X capture of @NATION0A
   * (port_bugs.md #1210): "...had neither the" (300 px at @width 300) breaks before
   * "the" in DOS. Every 6f74 dialog wraps through here (port_bugs.md #1216).
   */
  int line_w = 0;

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
      const int piece_w = font_text_width(font, " ") + word_w;
      if (line_w + piece_w > max_w) {
        snprintf(out + (size_t)count * out_stride, out_stride, "%s", accum);
        if (out_center) {
          out_center[count] = false;
        }
        count++;
        accum[0] = '\0';
        line_w = 0; /* 6f74:13fd: the " word" below is still charged */
        if (count >= max_out) {
          return count;
        }
      }
      line_w += piece_w;
    } else {
      line_w = word_w;
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
