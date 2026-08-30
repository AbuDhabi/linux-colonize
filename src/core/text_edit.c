#include "core/text_edit.h"

#include <string.h>

#include "core/ui_colors.h"

#define TEXT_EDIT_CLIP_MAX 128

static char g_local_clip[TEXT_EDIT_CLIP_MAX];
static TextEditClipboard g_clipboard;

static bool local_clip_get(char* out, size_t cap) {
  if (!out || cap == 0) {
    return false;
  }
  size_t n = strlen(g_local_clip);
  if (n + 1 > cap) {
    n = cap - 1;
  }
  memcpy(out, g_local_clip, n);
  out[n] = '\0';
  return n > 0;
}

static void local_clip_set(const char* text) {
  if (!text) {
    g_local_clip[0] = '\0';
    return;
  }
  size_t n = strlen(text);
  if (n + 1 > sizeof(g_local_clip)) {
    n = sizeof(g_local_clip) - 1;
  }
  memcpy(g_local_clip, text, n);
  g_local_clip[n] = '\0';
}

void text_edit_set_clipboard(const TextEditClipboard* clipboard) {
  if (clipboard && clipboard->get && clipboard->set) {
    g_clipboard = *clipboard;
  } else {
    memset(&g_clipboard, 0, sizeof(g_clipboard));
  }
}

static bool clip_get(char* out, size_t cap) {
  if (g_clipboard.get) {
    return g_clipboard.get(out, cap);
  }
  return local_clip_get(out, cap);
}

static void clip_set(const char* text) {
  if (g_clipboard.set) {
    g_clipboard.set(text);
  } else {
    local_clip_set(text);
  }
}

void text_edit_default_colors(TextEditColors* out) {
  if (!out) {
    return;
  }
  out->text = COLONIZE_COL_BASIC;
  out->shadow = COLONIZE_COL_ENHANCE;
  out->selection = COLONIZE_COL_SELECT;
}

static int buf_len(const char* buf) {
  return buf ? (int)strlen(buf) : 0;
}

static int clampi(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

void text_edit_reset(TextEditState* st, const char* buf, bool select_all) {
  if (!st) {
    return;
  }
  const int n = buf_len(buf);
  st->cursor = n;
  st->anchor = (select_all && n > 0) ? 0 : n;
}

void text_edit_clamp(TextEditState* st, const char* buf) {
  if (!st) {
    return;
  }
  const int n = buf_len(buf);
  st->cursor = clampi(st->cursor, 0, n);
  st->anchor = clampi(st->anchor, 0, n);
}

bool text_edit_has_selection(const TextEditState* st) {
  return st && st->cursor != st->anchor;
}

int text_edit_sel_lo(const TextEditState* st) {
  if (!st) {
    return 0;
  }
  return st->cursor < st->anchor ? st->cursor : st->anchor;
}

int text_edit_sel_hi(const TextEditState* st) {
  if (!st) {
    return 0;
  }
  return st->cursor > st->anchor ? st->cursor : st->anchor;
}

/* Remove [lo,hi) from buf; returns true if anything went. */
static bool erase_range(char* buf, int lo, int hi) {
  const int n = buf_len(buf);
  lo = clampi(lo, 0, n);
  hi = clampi(hi, 0, n);
  if (hi <= lo) {
    return false;
  }
  memmove(buf + lo, buf + hi, (size_t)(n - hi) + 1);
  return true;
}

static bool delete_selection(TextEditState* st, char* buf) {
  if (!text_edit_has_selection(st)) {
    return false;
  }
  const int lo = text_edit_sel_lo(st);
  const int hi = text_edit_sel_hi(st);
  erase_range(buf, lo, hi);
  st->cursor = lo;
  st->anchor = lo;
  return true;
}

static bool insert_text(TextEditState* st, char* buf, size_t cap, const char* text, int text_len) {
  if (text_len <= 0) {
    return false;
  }
  delete_selection(st, buf);
  const int n = buf_len(buf);
  const int room = (int)cap - 1 - n;
  if (room <= 0) {
    return false;
  }
  if (text_len > room) {
    text_len = room;
  }
  const int at = clampi(st->cursor, 0, n);
  memmove(buf + at + text_len, buf + at, (size_t)(n - at) + 1);
  memcpy(buf + at, text, (size_t)text_len);
  st->cursor = at + text_len;
  st->anchor = st->cursor;
  return true;
}

static bool is_word_char(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '\'';
}

/* Start of the word at or before pos (Ctrl+Left). */
static int word_left(const char* buf, int pos) {
  while (pos > 0 && !is_word_char(buf[pos - 1])) {
    --pos;
  }
  while (pos > 0 && is_word_char(buf[pos - 1])) {
    --pos;
  }
  return pos;
}

/* End of the word at or after pos (Ctrl+Right). */
static int word_right(const char* buf, int pos) {
  const int n = buf_len(buf);
  while (pos < n && !is_word_char(buf[pos])) {
    ++pos;
  }
  while (pos < n && is_word_char(buf[pos])) {
    ++pos;
  }
  return pos;
}

/* Move the caret; shift keeps the anchor (extends), plain drops it. */
static void move_caret(TextEditState* st, int to, bool extend) {
  st->cursor = to;
  if (!extend) {
    st->anchor = to;
  }
}

static void copy_selection(const TextEditState* st, const char* buf) {
  if (!text_edit_has_selection(st)) {
    return;
  }
  const int lo = text_edit_sel_lo(st);
  int len = text_edit_sel_hi(st) - lo;
  char tmp[TEXT_EDIT_CLIP_MAX];
  if (len > (int)sizeof(tmp) - 1) {
    len = (int)sizeof(tmp) - 1;
  }
  memcpy(tmp, buf + lo, (size_t)len);
  tmp[len] = '\0';
  clip_set(tmp);
}

TextEditAction text_edit_handle_input(
  TextEditState* st,
  char* buf,
  size_t cap,
  const ColonizeInputState* input
) {
  if (!st || !buf || cap < 2 || !input) {
    return TEXT_EDIT_ACTION_NONE;
  }
  text_edit_clamp(st, buf);
  const int n = buf_len(buf);
  const bool shift = input->shift_held;
  const bool ctrl = input->ctrl_held;

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    return TEXT_EDIT_ACTION_CANCEL;
  }
  if (input->last_key == COLONIZE_KEY_ENTER) {
    return TEXT_EDIT_ACTION_CONFIRM;
  }

  if (ctrl) {
    switch (input->last_key) {
      case COLONIZE_KEY_A:
        st->anchor = 0;
        st->cursor = n;
        return TEXT_EDIT_ACTION_EDIT;
      case COLONIZE_KEY_C:
        copy_selection(st, buf);
        return TEXT_EDIT_ACTION_EDIT;
      case COLONIZE_KEY_X:
        copy_selection(st, buf);
        delete_selection(st, buf);
        return TEXT_EDIT_ACTION_EDIT;
      case COLONIZE_KEY_V: {
        char tmp[TEXT_EDIT_CLIP_MAX];
        if (!clip_get(tmp, sizeof(tmp))) {
          return TEXT_EDIT_ACTION_EDIT;
        }
        /* Single-line field: stop at the first control char. */
        char clean[TEXT_EDIT_CLIP_MAX];
        int cn = 0;
        for (int i = 0; tmp[i]; ++i) {
          const unsigned char ch = (unsigned char)tmp[i];
          if (ch < 32 || ch >= 127) {
            break;
          }
          clean[cn++] = (char)ch;
        }
        clean[cn] = '\0';
        insert_text(st, buf, cap, clean, cn);
        return TEXT_EDIT_ACTION_EDIT;
      }
      default:
        break;
    }
  }

  switch (input->last_key) {
    case COLONIZE_KEY_LEFT:
      if (!shift && text_edit_has_selection(st)) {
        move_caret(st, text_edit_sel_lo(st), false);
      } else {
        move_caret(st, ctrl ? word_left(buf, st->cursor) : clampi(st->cursor - 1, 0, n), shift);
      }
      return TEXT_EDIT_ACTION_EDIT;
    case COLONIZE_KEY_RIGHT:
      if (!shift && text_edit_has_selection(st)) {
        move_caret(st, text_edit_sel_hi(st), false);
      } else {
        move_caret(st, ctrl ? word_right(buf, st->cursor) : clampi(st->cursor + 1, 0, n), shift);
      }
      return TEXT_EDIT_ACTION_EDIT;
    case COLONIZE_KEY_HOME:
      move_caret(st, 0, shift);
      return TEXT_EDIT_ACTION_EDIT;
    case COLONIZE_KEY_END:
      move_caret(st, n, shift);
      return TEXT_EDIT_ACTION_EDIT;
    case COLONIZE_KEY_BACKSPACE:
      if (!delete_selection(st, buf) && st->cursor > 0) {
        const int to = ctrl ? word_left(buf, st->cursor) : st->cursor - 1;
        erase_range(buf, to, st->cursor);
        st->cursor = to;
        st->anchor = to;
      }
      return TEXT_EDIT_ACTION_EDIT;
    case COLONIZE_KEY_DELETE:
      if (!delete_selection(st, buf) && st->cursor < n) {
        const int to = ctrl ? word_right(buf, st->cursor) : st->cursor + 1;
        erase_range(buf, st->cursor, to);
      }
      return TEXT_EDIT_ACTION_EDIT;
    default:
      break;
  }

  if (input->text_input_len > 0) {
    char clean[COLONIZE_TEXT_INPUT_MAX];
    int cn = 0;
    for (int i = 0; i < input->text_input_len; ++i) {
      const unsigned char ch = (unsigned char)input->text_input[i];
      if (ch >= 32 && ch < 127) {
        clean[cn++] = (char)ch;
      }
    }
    if (cn > 0) {
      insert_text(st, buf, cap, clean, cn);
      return TEXT_EDIT_ACTION_EDIT;
    }
  }
  return TEXT_EDIT_ACTION_NONE;
}

bool text_edit_handle_mouse(
  TextEditState* st,
  const char* buf,
  const ColonizeFont* font,
  const ColonizeInputState* input,
  int field_x,
  int field_y,
  int field_h
) {
  if (!st || !buf || !input) {
    return false;
  }
  if (input->mouse_left_released) {
    const bool was = st->dragging;
    st->dragging = false;
    if (was) {
      return true;
    }
  }
  if (input->mouse_left_clicked) {
    /* The field's hit box is generous vertically but unbounded to the right so
     * a click past the last glyph parks the caret at the end. */
    if (input->mouse_y >= field_y - 1 && input->mouse_y < field_y + field_h &&
        input->mouse_x >= field_x - 2) {
      const int at = text_edit_index_at_x(font, buf, input->mouse_x - field_x);
      st->cursor = at;
      st->anchor = at;
      st->dragging = true;
      return true;
    }
    return false;
  }
  if (st->dragging) {
    if (input->mouse_left_down) {
      st->cursor = text_edit_index_at_x(font, buf, input->mouse_x - field_x);
      return true;
    }
    st->dragging = false;
  }
  return false;
}

static int char_width(const ColonizeFont* font, unsigned char ch) {
  if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
    return font->char_widths[ch];
  }
  return 6;
}

int text_edit_prefix_width(const ColonizeFont* font, const char* buf, int n) {
  if (!buf) {
    return 0;
  }
  int w = 0;
  for (int i = 0; i < n && buf[i]; ++i) {
    w += char_width(font, (unsigned char)buf[i]);
  }
  return w;
}

int text_edit_index_at_x(const ColonizeFont* font, const char* buf, int dx) {
  const int n = buf_len(buf);
  int w = 0;
  for (int i = 0; i < n; ++i) {
    const int cw = char_width(font, (unsigned char)buf[i]);
    if (dx < w + cw / 2) {
      return i;
    }
    w += cw;
  }
  return n;
}

int text_edit_field_width(const ColonizeFont* font, const char* buf) {
  return text_edit_prefix_width(font, buf, buf_len(buf)) + char_width(font, '_');
}

static void fill_rect(ColonizeFramebuffer8* fb, int x, int y, int w, int h, uint8_t c) {
  if (!fb || !fb->pixels || w <= 0 || h <= 0) {
    return;
  }
  for (int yy = y; yy < y + h; ++yy) {
    if (yy < 0 || yy >= fb->height) {
      continue;
    }
    for (int xx = x; xx < x + w; ++xx) {
      if (xx < 0 || xx >= fb->width) {
        continue;
      }
      fb->pixels[yy * fb->width + xx] = c;
    }
  }
}

/*
 * Ink rows the font actually uses, over the whole printable range so the frame
 * does not jump as the text changes. FONTINTR fills its 9-row cell (0..8).
 */
static void font_ink_rows(const ColonizeFont* font, int* out_top, int* out_h) {
  int lo = 1 << 30;
  int hi = -1;
  if (font && font->section_data) {
    for (int c = 32; c < 127; ++c) {
      int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
      if (font_glyph_ink_bounds(font, (unsigned char)c, &min_x, &min_y, &max_x, &max_y)) {
        if (min_y < lo) {
          lo = min_y;
        }
        if (max_y > hi) {
          hi = max_y;
        }
      }
    }
  }
  if (hi < 0) {
    /* No FF glyphs — font.c falls back to the builtin 5x7 cell. */
    *out_top = 0;
    *out_h = font && font->max_height > 0 ? font->max_height : 7;
    return;
  }
  *out_top = lo;
  *out_h = hi - lo + 1;
}

void text_edit_frame_rect(
  const ColonizeFont* font,
  int x,
  int y,
  int box_w,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
) {
  int top = 0;
  int h = 0;
  font_ink_rows(font, &top, &h);
  if (out_x) {
    *out_x = x - TEXT_EDIT_FRAME_PAD;
  }
  if (out_y) {
    *out_y = y + top - TEXT_EDIT_FRAME_PAD;
  }
  if (out_w) {
    *out_w = box_w + TEXT_EDIT_FRAME_PAD * 2;
  }
  if (out_h) {
    /* +1 for the drop shadow, which sits one row below the ink. */
    *out_h = h + 1 + TEXT_EDIT_FRAME_PAD * 2;
  }
}

void text_edit_draw_frame(ColonizeFramebuffer8* fb, int x, int y, int w, int h, uint8_t color) {
  if (!fb || !fb->pixels || w <= 0 || h <= 0) {
    return;
  }
  fill_rect(fb, x, y, w, 1, color);
  fill_rect(fb, x, y + h - 1, w, 1, color);
  fill_rect(fb, x, y, 1, h, color);
  fill_rect(fb, x + w - 1, y, 1, h, color);
}

void text_edit_render(
  const TextEditState* st,
  const char* buf,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int line_h,
  const TextEditColors* colors
) {
  if (!fb || !fb->pixels || !buf) {
    return;
  }
  TextEditColors local;
  if (!colors) {
    text_edit_default_colors(&local);
    colors = &local;
  }
  const int n = buf_len(buf);
  if (st && text_edit_has_selection(st)) {
    const int lo = text_edit_sel_lo(st);
    const int hi = text_edit_sel_hi(st);
    const int sx = x + text_edit_prefix_width(font, buf, lo);
    const int sw = text_edit_prefix_width(font, buf + lo, hi - lo);
    fill_rect(fb, sx - 1, y - 1, sw + 2, line_h, colors->selection);
  }
  /* Unbold FONTINTR (shade-1 ink only) over a 1px offset drop shadow, same as
   * the wizard's captions but with the field's warm shadow colour. */
  font_draw_text_unbold(font, fb, x + 1, y + 1, buf, colors->shadow);
  font_draw_text_unbold(font, fb, x, y, buf, colors->text);

  /*
   * Caret. At the end of the text it is DOS's trailing underscore; once the
   * caret can sit inside the text an underline is too easy to lose against the
   * wood, so mid-text it becomes a plain insertion bar.
   */
  const int caret = st ? clampi(st->cursor, 0, n) : n;
  const int cx = x + text_edit_prefix_width(font, buf, caret);
  if (caret >= n) {
    font_draw_text_unbold(font, fb, cx + 1, y + 1, "_", colors->shadow);
    font_draw_text_unbold(font, fb, cx, y, "_", colors->text);
  } else {
    fill_rect(fb, cx, y, 1, line_h - 1, colors->text);
  }
}
