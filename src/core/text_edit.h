#ifndef COLONIZE_TEXT_EDIT_H
#define COLONIZE_TEXT_EDIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/font.h"
#include "platform/platform.h"

/*
 * Shared single-line text field (leader name, colony / New World name).
 *
 * DOS (FUN_6f74_2580's edit-field branch, record at dialog+0x30) only supports
 * append + Backspace, plus a "whole field selected" flag (bit 0x80 of the field
 * record) that the first keystroke clears by wiping the buffer. We keep the DOS
 * look — green @COLORS basic ink, brown/orange FONTINTR edge, dark-brown
 * selection fill, trailing underscore caret — but deliberately diverge on
 * behaviour: this is a modern caret + selection editor (arrows, shift-arrows,
 * word jumps, Home/End, Delete, Ctrl+A/C/X/V), which is what a keyboard-only
 * user expects of a text box today.
 */

typedef struct TextEditState {
  int cursor;    /* caret byte index, 0..len */
  int anchor;    /* selection anchor byte index; selection is [lo,hi) */
  bool dragging; /* left button went down in the field and is still held */
} TextEditState;

typedef enum TextEditAction {
  TEXT_EDIT_ACTION_NONE = 0, /* nothing this field cares about */
  TEXT_EDIT_ACTION_EDIT,     /* buffer and/or caret changed */
  TEXT_EDIT_ACTION_CONFIRM,  /* Enter */
  TEXT_EDIT_ACTION_CANCEL    /* Esc */
} TextEditAction;

/* Point the caret at the end of buf; select_all highlights the whole text. */
void text_edit_reset(TextEditState* st, const char* buf, bool select_all);

/* Re-clamp caret/anchor after the owner rewrote buf behind our back. */
void text_edit_clamp(TextEditState* st, const char* buf);

bool text_edit_has_selection(const TextEditState* st);
int text_edit_sel_lo(const TextEditState* st);
int text_edit_sel_hi(const TextEditState* st);

/*
 * Applies one frame of input to buf (NUL-terminated, capacity cap incl. the
 * terminator). Enter / Esc do not touch the buffer — the caller decides what
 * confirm and cancel mean.
 */
TextEditAction text_edit_handle_input(
  TextEditState* st,
  char* buf,
  size_t cap,
  const ColonizeInputState* input
);

/*
 * Click / drag caret placement inside the field rect (field_x is the x of the
 * first glyph, as passed to text_edit_render). Returns true when it claimed the
 * mouse, in which case the caller must not treat the click as confirm/cancel.
 */
bool text_edit_handle_mouse(
  TextEditState* st,
  const char* buf,
  const ColonizeFont* font,
  const ColonizeInputState* input,
  int field_x,
  int field_y,
  int field_h
);

/* Pixel width of the first n bytes of buf, as text_edit_render draws them. */
int text_edit_prefix_width(const ColonizeFont* font, const char* buf, int n);

/* Byte index whose caret slot is nearest dx pixels from the field origin. */
int text_edit_index_at_x(const ColonizeFont* font, const char* buf, int dx);

/* Total drawn width including the trailing caret cell. */
int text_edit_field_width(const ColonizeFont* font, const char* buf);

/*
 * NAMES.TXT @COLORS entries, under the WOODPANL / in-game palette:
 *   text = basic 68 (85,150,52) green
 *   shadow = enhance 128 (121,73,52) brown-orange — the field's drop shadow is
 *            warm, unlike the black one popup_draw_text_shadowed uses
 *   selection = select 138 (60,32,24) dark brown
 */
typedef struct TextEditColors {
  uint8_t text;
  uint8_t shadow;
  uint8_t selection;
} TextEditColors;

void text_edit_default_colors(TextEditColors* out);

/* Gap between the field frame and the text's ink, on every side. */
#define TEXT_EDIT_FRAME_PAD 3

/*
 * Frame rect for a field whose first glyph is drawn at (x, y) and whose text
 * area is box_w wide. Sized off the font's actual ink rows plus the 1px drop
 * shadow — not off the caller's line height, which carries interline slack that
 * would otherwise show up as dead space under the text.
 */
void text_edit_frame_rect(
  const ColonizeFont* font,
  int x,
  int y,
  int box_w,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
);

/* 1px green rectangle around the field. */
void text_edit_draw_frame(ColonizeFramebuffer8* fb, int x, int y, int w, int h, uint8_t color);

void text_edit_render(
  const TextEditState* st,
  const char* buf,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int line_h,
  const TextEditColors* colors
);

/*
 * Clipboard for Ctrl+C/X/V. Defaults to a process-local buffer; the SDL layer
 * installs hooks onto the system clipboard at startup.
 */
typedef struct TextEditClipboard {
  bool (*get)(char* out, size_t cap);
  void (*set)(const char* text);
} TextEditClipboard;

void text_edit_set_clipboard(const TextEditClipboard* clipboard);

#endif
