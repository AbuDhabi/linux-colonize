#include "core/unit_chrome.h"

#include <stdio.h>
#include <string.h>

/*
 * NAMES.TXT @COUNTRY indices (DS:0x848). Used for letter-color math (color-8).
 * Fill uses k_european_fill — England 112 is saturated red matching original
 * screenshots; VGA slot 12 is pink (255,85,85).
 */
static const uint8_t k_european_names[4] = {12, 9, 14, 13};
static const uint8_t k_european_fill[4] = {112, 9, 14, 13};
static const uint8_t k_tribe_colors[8] = {97, 149, 54, 87, 67, 111, 118, 71};

/* Fallback @ORDERS letters if NAMES.TXT is unavailable. */
static const char k_default_order_letters[UNIT_CHROME_ORDERS_MAX] = {
  '-', 'S', 'T', 'G', 'L', 'F', 'F', 'B', 'P', 'R', '-', '-', '-', '-', '-', '-'
};

static char g_order_letters[UNIT_CHROME_ORDERS_MAX];
static bool g_orders_loaded = false;

static void unit_chrome_init_defaults(void) {
  memcpy(g_order_letters, k_default_order_letters, sizeof(g_order_letters));
  g_orders_loaded = true;
}

static void unit_chrome_trim(char* s) {
  if (!s) {
    return;
  }
  char* start = s;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
    s[--n] = '\0';
  }
}

void unit_chrome_load_orders(const ColonizeMsgCatalog* names) {
  unit_chrome_init_defaults();
  if (!names) {
    return;
  }
  const ColonizeMsgSection* section = assets_msg_find(names, "ORDERS");
  if (!section) {
    return;
  }
  int n = 0;
  for (int i = 0; i < section->line_count && n < UNIT_CHROME_ORDERS_MAX; ++i) {
    char line[COLONIZE_MSG_LINE_LEN];
    snprintf(line, sizeof(line), "%s", section->lines[i]);
    if (line[0] == ';' || line[0] == '\0') {
      continue;
    }
    char* semi = strchr(line, ';');
    if (semi) {
      *semi = '\0';
    }
    char* comma = strchr(line, ',');
    if (!comma) {
      continue;
    }
    unit_chrome_trim(comma + 1);
    const char* letter = comma + 1;
    g_order_letters[n++] = letter[0] ? letter[0] : '-';
  }
}

uint8_t unit_chrome_nation_color(int nation_id) {
  if (nation_id >= 0 && nation_id < 4) {
    return k_european_fill[nation_id];
  }
  if (nation_id >= 4 && nation_id <= 11) {
    return k_tribe_colors[nation_id - 4];
  }
  return k_european_fill[0];
}

static uint8_t unit_chrome_names_color(int nation_id) {
  if (nation_id >= 0 && nation_id < 4) {
    return k_european_names[nation_id];
  }
  if (nation_id >= 4 && nation_id <= 11) {
    return k_tribe_colors[nation_id - 4];
  }
  return k_european_names[0];
}

UnitChromeCorner unit_chrome_corner_for_type(int display_type_index, bool aboard) {
  const int t = display_type_index;
  if (t >= 0x0d && t <= 0x12) {
    if (t == 0x0f || t == 0x10 || t == 0x11 || t == 0x12) {
      return UNIT_CHROME_CORNER_TOP_RIGHT;
    }
    return UNIT_CHROME_CORNER_TOP_LEFT;
  }
  if (t == 4 || t == 5 || t == 7 || t == 8 || t == 0x15 || t == 0x16) {
    return UNIT_CHROME_CORNER_TOP_LEFT;
  }
  if (t == 10 || t == 11 || t == 12) {
    if (t == 11 && aboard) {
      return UNIT_CHROME_CORNER_TOP_CENTER_ABOARD;
    }
    return UNIT_CHROME_CORNER_TOP_CENTER;
  }
  return UNIT_CHROME_CORNER_BOTTOM_RIGHT;
}

char unit_chrome_order_letter(int orders_index, int nation_id) {
  if (!g_orders_loaded) {
    unit_chrome_init_defaults();
  }
  if (nation_id > 3) {
    orders_index = 0;
  }
  if (orders_index < 0 || orders_index >= UNIT_CHROME_ORDERS_MAX) {
    orders_index = 0;
  }
  const char ch = g_order_letters[orders_index];
  return ch ? ch : '-';
}

uint8_t unit_chrome_letter_color(int nation_id, int orders_index) {
  if (nation_id > 3) {
    orders_index = 0;
  }
  /* Sentry (1) and Fortified (6): euro → NAMES color-8, native → 8. */
  if (orders_index == 1 || orders_index == 6) {
    if (nation_id >= 0 && nation_id < 4) {
      return (uint8_t)(unit_chrome_names_color(nation_id) - 8);
    }
    return 8;
  }
  return 0;
}

static void unit_chrome_put(ColonizeFramebuffer8* fb, int x, int y, uint8_t c) {
  if (!fb || !fb->pixels || x < 0 || y < 0 || x >= fb->width || y >= fb->height) {
    return;
  }
  fb->pixels[y * fb->width + x] = c;
}

static void unit_chrome_fill_rect(
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  int h,
  uint8_t color
) {
  if (w <= 0 || h <= 0) {
    return;
  }
  for (int dy = 0; dy < h; ++dy) {
    for (int dx = 0; dx < w; ++dx) {
      unit_chrome_put(fb, x + dx, y + dy, color);
    }
  }
}

static void unit_chrome_draw_box(
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  int h,
  uint8_t fill
) {
  if (w < 2 || h < 2) {
    return;
  }
  unit_chrome_fill_rect(fb, x, y, w, h, 0);
  unit_chrome_fill_rect(fb, x + 1, y + 1, w - 2, h - 2, fill);
}

void unit_chrome_selection_frame(
  int x,
  int y,
  int sprite_w,
  int sprite_h,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
) {
  if (!out_x || !out_y || !out_w || !out_h) {
    return;
  }
  const int sw = sprite_w > 0 ? sprite_w : 16;
  const int sh = sprite_h > 0 ? sprite_h : 16;
  /*
   * Shadow at x+SPRITE_DX+SHADOW_DX, sprite at x+SPRITE_DX, orders at x.
   * Stack under-rect may extend ±STACK_PAD from the badge.
   */
  const int art_left = x + UNIT_CHROME_SPRITE_DX + UNIT_CHROME_SHADOW_DX;
  const int art_right = x + UNIT_CHROME_SPRITE_DX + sw;
  const int left = (art_left < x ? art_left : x) - UNIT_CHROME_STACK_PAD;
  const int right = art_right + UNIT_CHROME_STACK_PAD;
  const int top = y - UNIT_CHROME_STACK_PAD;
  const int bottom = y + sh + UNIT_CHROME_STACK_PAD;
  *out_x = left - 1;
  *out_y = top - 1;
  *out_w = (right - left) + 2;
  *out_h = (bottom - top) + 2;
}

void unit_chrome_draw(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  int icon_x,
  int icon_y,
  int icon_w,
  int icon_h,
  int display_type_index,
  int nation_id,
  int orders_index,
  bool show_stack,
  bool aboard
) {
  if (!fb || !fb->pixels) {
    return;
  }
  if (!g_orders_loaded) {
    unit_chrome_init_defaults();
  }

  char letter_buf[2];
  letter_buf[0] = unit_chrome_order_letter(orders_index, nation_id);
  letter_buf[1] = '\0';

  const int text_w = font_text_width(font, letter_buf);
  const int font_h = (font && font->max_height > 0) ? (int)font->max_height : 5;
  const int box_w = text_w + 3;
  const int box_h = font_h + 3;
  if (box_w < 2 || box_h < 2) {
    return;
  }

  const int sprite_w = icon_w > 0 ? icon_w : 16;
  const int sprite_h = icon_h > 0 ? icon_h : 16;
  const uint8_t fill = unit_chrome_nation_color(nation_id);

  int right_x = icon_x + sprite_w;
  const int span = box_w + sprite_w;
  if (span > 16) {
    right_x -= (span - 16);
  }

  const UnitChromeCorner corner = unit_chrome_corner_for_type(display_type_index, aboard);
  int box_x = right_x;
  int box_y = icon_y;
  int stack_x = right_x - 2;
  int stack_y = icon_y + 2;

  switch (corner) {
    case UNIT_CHROME_CORNER_TOP_RIGHT:
      box_x = right_x;
      box_y = icon_y;
      stack_x = box_x - 2;
      stack_y = icon_y + 2;
      break;
    case UNIT_CHROME_CORNER_TOP_LEFT:
      box_x = icon_x;
      box_y = icon_y;
      stack_x = icon_x + 2;
      stack_y = icon_y + 2;
      break;
    case UNIT_CHROME_CORNER_TOP_CENTER:
    case UNIT_CHROME_CORNER_TOP_CENTER_ABOARD: {
      const int mid = icon_x - (box_w >> 1);
      box_x = mid + 9;
      box_y = icon_y;
      if (corner == UNIT_CHROME_CORNER_TOP_CENTER_ABOARD) {
        box_y = icon_y + 2;
      }
      stack_x = mid + 7;
      stack_y = box_y + 2;
      break;
    }
    case UNIT_CHROME_CORNER_BOTTOM_RIGHT:
    default:
      box_x = right_x;
      box_y = (icon_y - box_h) + sprite_h;
      /* Peek bottom-right (away from the figure) so the tab stays visible. */
      stack_x = box_x + 2;
      stack_y = box_y + 2;
      break;
  }

  if (show_stack) {
    unit_chrome_draw_box(fb, stack_x, stack_y, box_w, box_h, fill);
  }
  unit_chrome_draw_box(fb, box_x, box_y, box_w, box_h, fill);

  /*
   * Center the glyph's ink (not the full advance cell) in the outline rect.
   * FONTTINY '-' is a 3×1 dash in a 4×6 cell; cell-centering looked off.
   */
  const uint8_t ink = unit_chrome_letter_color(nation_id, orders_index);
  int ink_x0 = 0;
  int ink_y0 = 0;
  int ink_x1 = text_w - 1;
  int ink_y1 = font_h - 1;
  if (!font_glyph_ink_bounds(
        font, (unsigned char)letter_buf[0], &ink_x0, &ink_y0, &ink_x1, &ink_y1
      )) {
    ink_x0 = 0;
    ink_y0 = 0;
    ink_x1 = text_w > 0 ? text_w - 1 : 0;
    ink_y1 = font_h > 0 ? font_h - 1 : 0;
  }
  const int ink_w = ink_x1 - ink_x0 + 1;
  const int ink_h = ink_y1 - ink_y0 + 1;
  const int tx = box_x + (box_w - ink_w) / 2 - ink_x0;
  const int ty = box_y + (box_h - ink_h) / 2 - ink_y0;
  font_draw_text(font, fb, tx, ty, letter_buf, ink);
}

void unit_chrome_blit_unit(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  int x,
  int y,
  int display_type_index,
  int nation_id,
  int orders_index,
  bool show_stack,
  bool aboard
) {
  if (!fb || !sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &sheet->sprites[sprite_index];
  const int iw = sp->width > 0 ? sp->width : 16;
  const int ih = sp->height > 0 ? sp->height : 16;

  /* Orders box at tile origin; sprite (+ silhouette) shifted right. */
  const int sx = x + UNIT_CHROME_SPRITE_DX;
  ss_blit_sprite_color(sheet, sprite_index, fb, sx + UNIT_CHROME_SHADOW_DX, y, 0);
  unit_chrome_draw(
    fb,
    font,
    x,
    y,
    iw,
    ih,
    display_type_index,
    nation_id,
    orders_index,
    show_stack,
    aboard
  );
  ss_blit_sprite(sheet, sprite_index, fb, sx, y);
}
