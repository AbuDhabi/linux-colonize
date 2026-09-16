#ifndef COLONIZE_POPUP_H
#define COLONIZE_POPUP_H

#include <stddef.h>
#include <stdint.h>

#include "core/font.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Reusable wood popup chrome (DOS dialog / confirm / prompt frame).
 *
 * Outside → in for any [x,y,w,h):
 *   1. Tile fill (caller sheet sprite 0; solid 4 if sheet missing)
 *   2. 1px black outer (all sides)
 *   3. 1px mid wood-brown (all sides) — @COLORS border0
 *   4. 1px bevel: light top+right (border1), dark bottom+left (border2)
 *
 * Content belongs in the inner rect (inset by POPUP_FRAME_INSET).
 * Title menu uses OPENTILE.SS; in-game popups typically use WOODTILE.SS.
 */
#define POPUP_FRAME_INSET 3
#define POPUP_FALLBACK_FILL 4

typedef struct ColonizePopupColors {
  uint8_t outer; /* black */
  uint8_t mid; /* wood brown, all sides */
  uint8_t light; /* top + right bevel */
  uint8_t dark; /* bottom + left bevel */
} ColonizePopupColors;

/* Fill from NAMES.TXT @COLORS border0/1/2 (+ outer 0) on WOODPANL / in-game indices. */
void popup_colors_from_ui(ColonizePopupColors* out);

/*
 * Remap mid/light/dark onto target_palette by nearest RGB, using source_palette
 * as the reference for the @COLORS indices. Outer stays 0.
 * If either palette is NULL, leaves colors unchanged (aside from ensuring outer=0).
 */
void popup_colors_remap(
  ColonizePopupColors* colors,
  const ColonizePalette* source_palette,
  const ColonizePalette* target_palette
);

/*
 * Draw fill + three outline layers. Optional out_inner_* receives the content
 * rect inset by POPUP_FRAME_INSET (clamped; may be empty if w/h too small).
 * tile may be NULL (solid POPUP_FALLBACK_FILL). colors may be NULL → from_ui.
 */
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
);

/* Nation-wizard / ai_popup style: FONTINTR unbold + black (0) drop-shadow. */
void popup_draw_text_shadowed(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t color
);

/*
 * Pixel width of text as popup_draw_text_markup would draw it.
 *
 * Marker contract for the three width loops in this port (audit SC-36):
 *   - '{' / '}'  emphasis switches: zero width, skipped HERE (this function).
 *   - '~' / '#'  hotkey markers: zero width, skipped by font_text_width, which
 *                this function calls on the de-braced text, so they are free
 *                here too. Use font_text_width directly when the string has no
 *                brace markup.
 *   - '^' / '^^' (POPUP_MSG_LINE_MARK / POPUP_MSG_CENTER_MARK) layout marks and
 *                '_' are NOT stripped: they belong to the line splitter, and a
 *                caller measuring a raw marked-up body over-measures
 *                (audit 2026-09-09 #105). Strip them first — popup_wrap_text
 *                already consumes them.
 */
int popup_markup_text_width(const ColonizeFont* font, const char* text);

/*
 * Draw text honoring the DOS dialog writer's {} emphasis markup
 * (FUN_6f74_0538): '{' switches to hilite ink, '}' back to base, both
 * zero-width. shadow draws a 1px black drop shadow first. unbold selects
 * font_draw_text_unbold (FONTINTR captions). inout_hilite carries the
 * emphasis state across lines the way DS:0x1f62 does (NULL = start off,
 * discard). fb may be NULL (measure only). Returns the end x.
 */
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
);


/* ---- Shared wood list-dialog pipeline (audit theme K) ---- */

/* Outer and inner rect of a centred wood frame. */
typedef struct PopupFrameGeom {
  int x;
  int y;
  int w;
  int h;
  int inner_x;
  int inner_y;
  int inner_w;
  int inner_h;
} PopupFrameGeom;

/*
 * Centre a w x h dialog on the framebuffer, floor its top at the map menu bar
 * (MAP_MENU_BAR_H + 2), draw the wood frame, hand back both rects. w/h are
 * used as given, so clamp them first. colors may be NULL (popup_colors_from_ui).
 * This is the prologue cheat_list / pick_music / options / save_load /
 * unit_stack each copied.
 */
void popup_center_frame(
  ColonizeFramebuffer8* framebuffer,
  int w,
  int h,
  const ColonizeSpriteSheet* tile,
  const ColonizePopupColors* colors,
  PopupFrameGeom* out
);

/* Row under mouse_y, or -1. Carries the count <= 0 guard the option_at_y
 * copies in options_dialog and ai_popup dropped. */
int popup_row_at_y(int list_y0, int line_h, int count, int mouse_y);

/* The port's classic wood-dialog row pitch (font->max_height + 2; 8 unfonted). */
int popup_dialog_line_h(const ColonizeFont* font);

/*
 * Metrics for popup_list_render. Two documented sets exist:
 *   popup_list_metrics_classic  — the port's INSET-based wood list
 *                                 (cheat_list, pick_music, options_dialog).
 *   popup_list_metrics_dos6f74  — the DOS FUN_6f74_14c6 compositor metrics
 *                                 (save_load_dialog; ai_popup_render's own).
 * Fill one of those, then override the few fields your dialog differs in.
 */
typedef struct PopupListMetrics {
  int line_h; /* prompt row pitch */
  int option_h; /* list row pitch — this is the hit-test pitch */
  int pad_x; /* text inset from the inner rect's left edge */
  int pad_y; /* first text row's inset from the inner rect's top */
  int bottom_pad; /* space kept under the last row inside the inner rect */
  int prompt_gap; /* extra px after the prompt row */
  int label_dx; /* extra x for row labels (options_dialog's checkbox column) */
  int min_h; /* minimum outer height; 0 = none */
  int frame_pad; /* outer width = content width + this */
  int screen_margin_w; /* outer width clamped to framebuffer width minus this */
  int screen_margin_h; /* outer height clamped to framebuffer height minus this */
  int sel_h; /* selection bar height */
  bool widen_to_rows; /* FUN_6f74_14c6: grow the box to the widest emitted row */
  bool shadow_text; /* popup_draw_text_shadowed instead of font_draw_text */
  bool markup_text; /* row labels via popup_draw_text_markup (unbold + shadow) */
} PopupListMetrics;

void popup_list_metrics_classic(const ColonizeFont* font, PopupListMetrics* out);
void popup_list_metrics_dos6f74(const ColonizeFont* font, PopupListMetrics* out);

typedef struct PopupListGeom {
  PopupFrameGeom frame;
  int line_h; /* = metrics->option_h, the pitch to store for hit-testing */
  int list_y0;
} PopupListGeom;

/* Label for row index; NULL draws nothing for that row. */
typedef const char* (*PopupListLabelFn)(void* user, int index);
/*
 * Optional per-row decoration, run after the selection bar and before the
 * label. row_x is the label's own x, so a checkbox column sits at
 * row_x - metrics->label_dx.
 */
typedef void (*PopupListRowFn)(
  void* user,
  int index,
  ColonizeFramebuffer8* framebuffer,
  int row_x,
  int row_y,
  int line_h
);

/*
 * The whole wood list dialog: height from the prompt + rows, height/width
 * clamps, centring with the menu-bar floor, popup_colors_from_ui fallback,
 * popup_draw, prompt row, selection bar, row labels. Geometry comes back in
 * `out` for the caller to stash (dialog_x/y/w/h, line_h, list_y0).
 * `width` is the dialog's declared content width (GAME.TXT @width).
 * hilite_color is only read when metrics->markup_text.
 */
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
  PopupListRowFn row_fn,
  void* user,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color,
  PopupListGeom* out
);

typedef struct PopupPromptGeom {
  PopupFrameGeom frame;
  int line_h;
  int text_y; /* y just past the last prompt row slot */
} PopupPromptGeom;

/*
 * Fixed-width prompt dialog (name_entry / howmuch): centre a w x h frame with
 * the menu-bar floor, then lay the prompt out as up to `rows` shadowed lines
 * of at most `cols` characters each, skipping the spaces at every break.
 * That chunker is a character budget, not a word wrap — it is what both
 * dialogs have always drawn; popup_wrap_text is the real wrapper.
 * Draws nothing past the frame when font is NULL.
 */
void popup_prompt_frame(
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  int w,
  int h,
  int pad,
  const char* prompt,
  int rows,
  int cols,
  uint8_t text_color,
  PopupPromptGeom* out
);

/*
 * Flow-wrap text to max_w pixels — the DOS FUN_6f74_1198 loop ai_popup has
 * always used, promoted here (audit GL-8 / SC-35 / IN-7).
 *
 * Honors embedded '\n' and the GAME.TXT caret rows popup_msg encodes as
 * POPUP_MSG_LINE_MARK / POPUP_MSG_CENTER_MARK: a caret row is flushed onto a
 * line of its own, taken verbatim to the end of the source line, and never
 * re-flowed. out is a caller array of max_out rows of out_stride bytes each.
 * out_center, when given, receives one flag per emitted row and is true only
 * for a '^^' row, which DOS centres. Widths are measured with
 * popup_markup_text_width, so {} emphasis costs nothing.
 * Returns the number of rows written.
 */
int popup_wrap_text(
  const ColonizeFont* font,
  const char* text,
  char* out,
  size_t out_stride,
  bool* out_center,
  int max_out,
  int max_w
);

#endif
