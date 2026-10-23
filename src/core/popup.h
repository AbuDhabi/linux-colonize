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
  uint8_t enhance; /* dialog text glyph shade 2 (@COLORS enhance, DS:0x1f50) */
  uint8_t shadow; /* dialog text glyph shade 3 (@COLORS shadow, DS:0x1f52) */
} ColonizePopupColors;

/* Fill from NAMES.TXT @COLORS border0/1/2, enhance, shadow (+ outer 0) on WOODPANL /
 * in-game indices. */
void popup_colors_from_ui(ColonizePopupColors* out);

/*
 * Remap mid/light/dark/enhance/shadow onto target_palette by nearest RGB, using source_palette
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

/*
 * Pixel width of text the way FUN_6f74_0486 measures it: every '~' '{' '}' '|' is
 * deleted first (escaped ones too), then font_text_width (which also skips '#').
 *
 * Marker contract for the three width loops in this port (audit SC-36):
 *   - '{' / '}' / '~' / '|'  zero width, deleted HERE (this function).
 *   - '#'        zero width, skipped by font_text_width. Use font_text_width
 *                directly when the string has no dialog markup.
 *   - '^' / '^^' (POPUP_MSG_LINE_MARK / POPUP_MSG_CENTER_MARK) layout marks and
 *                '_' are NOT stripped: they belong to the line splitter, and a
 *                caller measuring a raw marked-up body over-measures
 *                (audit 2026-09-09 #105). Strip them first — popup_wrap_text
 *                already consumes them.
 */
int popup_markup_text_width(const ColonizeFont* font, const char* text);

/*
 * Draw text the way the DOS dialog writer does (FUN_6f74_0538): '{' switches to
 * hilite ink, '}' back to base, both zero-width; '~' draws the next byte as a plain
 * glyph in the other ink (hilite outside {}, base inside; a disabled row passes grey
 * as both), so it marks hotkey letters and escapes markup (port_bugs.md #1199); '|'
 * ends the text. Each glyph is blitted once (FUN_1c11_000c) through the 4-entry
 * shade map FUN_6f74_04f6 loads: shade 0
 * transparent, 1 the ink, 2 colors->enhance, 3 colors->shadow (NULL = popup_colors_from_ui).
 * There is no drop-shadow pass; FONTINTR / FONT-NP carry their own edge shades,
 * FONTTINY / FONTSMAL only shade 1 (port_bugs.md #1191). inout_hilite carries the
 * emphasis state across lines the way DS:0x1f62 does (NULL = start off, discard).
 * fb may be NULL (measure only). Returns the end x.
 */
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
);

/*
 * FUN_6f74_0642: a row's hotkey, stored at node+2 when FUN_6f74_0a00 adds the row
 * (6f74:0b72, on the raw label): the char after the LAST '~', lowercase folded to
 * upper; "~F~n" (first '~' != last, first[1] == 'F', digit after the last) gives the
 * F-key code n + 0x10a. No '~' = 0.
 */
int popup_hotkey_of(const char* label);

/* The key FUN_6f74_2580 compares with the rows' hotkeys (FUN_281f_03e0 at 6f74:28df,
 * lowercase < 0x100 folded to upper at 6f74:2a1a): this frame's first typed char, or
 * F1..F9 as 0x13b..; 0 = none (also with Alt / Ctrl held, which DOS reports as codes no
 * row carries). */
int popup_input_dos_key(const ColonizeInputState* input);

/*
 * FUN_6f74_2580 list arm Up / Down (raw 116905-116940): Up / KP8 (0x148) steps to the
 * previous row, Down / KP2 (0x150) to the next, wrapping to the last (box+0x70) / first
 * (box+0x54) and stepping past disabled rows (option bit 0, FUN_6f74_08fa; bit i of
 * `disabled_mask`, rows >= 32 never disabled), giving up after two wraps. A selection
 * outside [0, count) is DOS's empty box+0x4c: the first step lands on the wrap row.
 * Returns false (and leaves *selection) for any other key or an empty list.
 */
bool popup_list_key_step(ColonizeKey key, int* selection, int count, uint32_t disabled_mask);

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
 * Place a w x h dialog like the DOS compositor does (popup_place_dos: 160 - (w >> 1),
 * 100 - (h >> 1), edge clamps, no menu-bar floor), draw the wood frame, hand back both
 * rects. w/h are used as given, so clamp them first. colors may be NULL
 * (popup_colors_from_ui). This is the prologue cheat_list / pick_music / options /
 * save_load / unit_stack each copied; all five are DOS 6f74 dialogs (port_bugs.md #1188).
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

/*
 * Metrics for popup_list_render: the DOS FUN_6f74_14c6 compositor (save/load, options,
 * pick music, cheat / goto lists; ai_popup_render keeps its own copy). Fill with
 * popup_list_metrics_dos6f74, then set the builder's own box[+0x22] in label_dx.
 * Height popup_dos_list_height, row 0's bar top popup_dos_list_y0, text one px lower.
 */
typedef struct PopupListMetrics {
  int line_h; /* prompt row pitch */
  int option_h; /* list row pitch — this is the hit-test pitch */
  int pad_x; /* text inset from the inner rect's left edge */
  int pad_y; /* first text row's inset from the inner rect's top */
  int label_dx; /* box[+0x22]: row text indent (4, FUN_6f74_06d0 default) */
  int sel_h; /* selection bar height */
  uint32_t disabled_mask; /* bit i: row i is disabled (option bit 0, FUN_6f74_08fa), drawn in
                           * COLONIZE_COL_GREY as both inks (FUN_6f74_0538 BX&1, style+4) */
} PopupListMetrics;

void popup_list_metrics_dos6f74(const ColonizeFont* font, PopupListMetrics* out);

typedef struct PopupListGeom {
  PopupFrameGeom frame;
  int line_h; /* = metrics->option_h, the pitch to store for hit-testing */
  int list_y0;
} PopupListGeom;

/* Label for row index; NULL draws nothing for that row. */
typedef const char* (*PopupListLabelFn)(void* user, int index);

/*
 * The whole wood list dialog (FUN_6f74_14c6 sizes, popup_place_dos centring via
 * popup_center_frame, popup_colors_from_ui fallback): the prompt wrapped by popup_wrap_text
 * to content - 4 from (frame + 5, frame + 6), rows at popup_dos_list_y0 + k * option_h with
 * the text one px lower at frame + 5 + label_dx, the glyph_h + 2 bar at frame + 4 .. frame +
 * w - 5. Geometry comes back in `out` for the caller to stash (dialog_x/y/w/h, line_h,
 * list_y0). `width` is box[+0x28]: the GAME.TXT @width as given, 0 for the 0x50 default
 * (popup_msg_section_width), widened to the widest row (FUN_6f74_0a00: text + 2 * box[+0x48]
 * + box[+0x22]).
 * Prompt and rows go through popup_draw_text_markup ({} in hilite_color, shades from colors).
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
  void* user,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color,
  PopupListGeom* out
);

/*
 * FUN_6f74_14c6 placement of a w x h compositor frame on a fb_w x fb_h frame,
 * optionally around an MSS/MYR figure sheet (NULL = none). Shift-form centring
 * (160 - (w >> 1), 100 - (h >> 1)), DOS edge clamps, no menu-bar floor;
 * figure arm and port-only generalisation: popup.c popup_place_dos.
 * `figure` false = the sheet was dropped (assembly >= frame height) or absent.
 */
typedef struct PopupDosPlace {
  int x; /* frame origin */
  int y;
  bool figure;
  int fig_x; /* figure origin, valid when figure */
  int fig_y;
} PopupDosPlace;

PopupDosPlace popup_place_dos(
  int fb_w, int fb_h, int w, int h, const ColonizeSpriteSheet* figure
);

/*
 * FUN_6f74_0f16 glyph height of a dialog font (font[0]; the 6-row font counts as 5
 * unless DS:0x1f8a is set, which the port never models). NULL font = the 6-row default.
 */
int popup_dos_glyph_h(const ColonizeFont* font);

/*
 * FUN_6f74_14c6 height of a framed list dialog (prose + option rows), asm 6f74:1540-16c8:
 *   h = 2*box[+0x4a] + box[+0x46]   (0 + 3)
 *     + 6                            (frame, flags & 0x10 clear)
 *     + prose_h + (glyph_h + box[+0x46]) * rows + box[+0x46]   (the +0x58 / +0x54 lists)
 *     + footer                       (DS:0x1f66 "(F1 for Help)": 6 when the dialog font is
 *                                     FONTTINY = DS:0x89e, else 3; 0 when the flag is clear)
 * prose_h = prose lines * (glyph_h + 1) (FUN_6f74_1198). DOS shots: Recruit 103 (4 prose lines,
 * 4 rows, FONTINTR), Purchase 119 (2, 7), Train 180 (3, 18, FONTTINY). A dialog with neither
 * list is not covered (DOS skips the list block). The first row's selection bar top is
 * frame_y + 9 + prose_h (6 without prose), text one row lower; the footer text top is
 * frame_y + h - 7 (FONTTINY dialogs h - 9), FUN_6f74_1a78.
 */
int popup_dos_list_height(int glyph_h, int prose_h, int rows, bool help_footer, bool tiny_font);

/*
 * FUN_6f74_14c6 box[+0x26]: top of option row 0's selection bar, frame_y + 6, plus
 * box[+0x46] (3) + prose_h when the dialog has prose. FUN_6f74_1b7c steps it by
 * glyph_h + 3 per row and draws each row's text one px lower, at x frame_x + 5 +
 * box[+0x22] (FUN_6f74_06d0 default 4). Hit tests take rows from here too.
 */
int popup_dos_list_y0(int frame_y, int prose_h);

/*
 * The DS:0x1f66 "(F1 for Help)" footer, FUN_6f74_1a78: LABELS @MISC 189 (DS:0x2f34,
 * passed as `word`) right-aligned to frame_x + w - 2, text top frame_y + h - 7 (- 9 when
 * the dialog font is FONTTINY, DS:0x89e), in the FONTTINY face (every DOS shot), no drop
 * shadow. DOS raises the flag only in FUN_2f2b_348c (jobs / leave-as), FUN_2f2b_5bd2
 * (construction), FUN_38fd_41ce / 4884 / 4b50 (Train, Recruit / Fountain of Youth /
 * Brewster, Purchase) and FUN_4345_06d2 (Congress); the height it costs is
 * popup_dos_list_height's help_footer term.
 */
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
);

/*
 * DOS text-entry dialog (FUN_291f_0120 -> FUN_6f74_37fc: the generic compositor
 * with one edit-field row, port_bugs.md #1181). Layout read from the asm:
 *   FUN_6f74_0d44  label = GAME.TXT row after the blank + " ", label_w = its
 *                  width; field text width = width("W") * max_len; the dialog
 *                  content width grows to label_w + that + 10.
 *   FUN_6f74_14c6  content C = max(80, @width, the above); frame W = C + 6,
 *                  H = body_h + glyph_h + 20 (body_h = rows * (glyph_h + 1));
 *                  centred at (160 - W/2, 100 - H/2).
 *   FUN_6f74_1198  body rows wrapped to C - 4, drawn at (x + 5, y + 6 + k*(glyph_h+1)).
 *   FUN_6f74_1e14  Y0 = y + 9 + body_h. Label at (x + 5, Y0 + 3). Field box at
 *                  (x + 5 + label_w, Y0), (text_w + 6) x (glyph_h + 5), a 1px
 *                  outline in the normal text ink (struct +2) around a wood
 *                  fill that matches the frame; first glyph at (box + 3, box + 3);
 *                  pre-selected default text gets a select-ink bar
 *                  (box + 2, box + 2, text width + 2, glyph_h + 1).
 */
typedef struct PopupFieldGeom {
  PopupFrameGeom frame;
  int glyph_h;
  int box_x; /* field outline rect */
  int box_y;
  int box_w;
  int box_h;
  int text_x; /* first glyph of the field text */
  int text_y;
  int text_h; /* selection bar height / hit height: glyph_h + 1 */
} PopupFieldGeom;

/*
 * Draws the frame, the wrapped prompt, the label and the field outline of the
 * DOS text-entry dialog and returns where the field text goes. `width` is the
 * section's @width (0 = none), `label` the field caption ("" = none). A NULL
 * font draws the frame only. `decoration` is the MSS sheet DOS latches in
 * DS:0x1f5e before the prompt (NULL = bare): FUN_6f74_14c6 (0x18ae-0x1a0c)
 * re-places the frame around the sprite and FUN_6f74_248e blits it over the
 * frame, geom.frame reporting the moved frame (port_bugs.md #1183).
 */
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
);

/*
 * The same dialog with box flag 0x10 (DS:0x1f56 |= 0x18, FUN_281f_040a -> FUN_6f74_37f6,
 * read by FUN_6f74_06d0): no wood, no frame / spacing / margin, so W = content,
 * H = body_h + glyph_h + 5, prose at (x, y), field box at (x + label_w, y + body_h).
 * The new-game @LEADERNAME prompt (FUN_75c2_10ae, 75c2:119d) over WOODPANL.
 */
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
);

/*
 * FUN_6f74_1198 '^^' row offset (6f74:1263-128a): (wrap_w >> 1) - (width >> 1), the width
 * being FUN_6f74_0486's (popup_markup_text_width) and wrap_w box[+0x28] - 2 * box[+0x48].
 */
int popup_dos_center_dx(const ColonizeFont* font, const char* text, int wrap_w);

/*
 * Flow-wrap text to max_w pixels: FUN_6f74_1198's loop (6f74:132e-1435), the one wrap every
 * 6f74 dialog uses (port_bugs.md #1216). It keeps a running width: every word is measured
 * as " word" (the space only when the line already has text), the line breaks when that
 * would pass max_w, and a continuation line's first word keeps the space in the count
 * although it is not drawn (6f74:13fd), so a line within one space of max_w breaks a word
 * earlier. Pixel-verified on the DOSBox-X @NATION0A capture (port_bugs.md #1210).
 *
 * Honors embedded '\n' and the GAME.TXT caret rows popup_msg encodes as
 * POPUP_MSG_LINE_MARK / POPUP_MSG_CENTER_MARK: a caret row is flushed onto a
 * line of its own, taken verbatim to the end of the source line, and never
 * re-flowed. out is a caller array of max_out rows of out_stride bytes each.
 * out_center, when given, receives one flag per emitted row and is true only
 * for a '^^' row, which DOS centres (popup_dos_center_dx). Widths are measured with
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
