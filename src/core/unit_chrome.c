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

/*
 * k_european_fill/k_european_names above are tuned against ICONS.SS's own
 * *native* palette (confirmed via a direct native-palette dump: index 13
 * really is a saturated Dutch orange (255,113,0), index 5 — the
 * Sentry/Fortified letter color, k_european_names[nation]-8 — a matching
 * darker (170,73,0)); every England/France/Spain index (112/9/14, and
 * their -8 letter equivalents 4/1/6) happens to already be identical
 * between ICONS-native and every other screen's own background palette,
 * so only Dutch (13/5) ever actually differs. But *every* other screen's
 * background palette (TERRAIN.SS for the map, WOODPANL.PIK for the colony
 * screen, EUROPE.PIK, every REPORT*.PIK) repurposes indices 5/13 back to
 * plain EGA magenta (or, for COLONY.PIK specifically, leaves them
 * unpopulated/black) — a raw-index box fill only looks right when the
 * active output palette happens to still be ICONS.SS-native-compatible
 * there, which none of them are. This table is that native RGB, so a
 * caller who knows the *actual* active output palette can look up the
 * nearest available match in it instead of trusting the raw index blindly.
 * See unit_chrome_blit_unit_for_palette.
 */
static const uint8_t k_nation_fill_rgb_native[4][3] = {
  {243, 0, 0}, {85, 85, 255}, {255, 255, 85}, {255, 113, 0}
};
static const uint8_t k_nation_letter_rgb_native[4][3] = {
  {170, 0, 0}, {0, 0, 170}, {170, 85, 0}, {170, 73, 0}
};

static int unit_chrome_nearest_palette_index(const ColonizePalette* pal, const uint8_t rgb[3]) {
  int best = 0;
  int best_d = 1 << 30;
  for (int i = 0; i < 256; ++i) {
    const int dr = (int)pal->rgb[i][0] - rgb[0];
    const int dg = (int)pal->rgb[i][1] - rgb[1];
    const int db = (int)pal->rgb[i][2] - rgb[2];
    const int d = dr * dr + dg * dg + db * db;
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

/*
 * ICONS.SS #0-3 (colony settlement fortification markers) each carry an
 * identical 14-pixel two-shade flag (a light body + a darker pole/shadow
 * edge — native RGB (65,89,166)/(52,73,158), a plain highlight/shadow pair
 * of the same blue hue), always at the same (dx,dy) offsets regardless of
 * fortification tier — dumped directly from ICONS.SS, see
 * colony_map_icon_flag_pixels. DOS recolors this flag to the owning
 * nation (confirmed indirectly: FUN_112b_0c64, the colony-map-chrome
 * decompile, reads the same @COUNTRY/DS:0x848 nation-color table
 * unit_chrome's own k_european_names does) — currently every nation's
 * colony shows the same stored blue. Derive a light/dark pair for a given
 * nation the same way (same k_nation_fill_rgb_native "light" color as the
 * chrome box; "dark" scaled by the same ~0.82 per-channel ratio the
 * original blue's own two shades already have — no DOS-confirmed source
 * for the exact per-nation dark shade, this is the closest reproducible
 * approximation), nearest-matched into whatever palette is actually
 * active (same reasoning as unit_chrome_blit_unit_for_palette).
 */
void unit_chrome_nation_flag_shades_for_palette(
  int nation_id, const ColonizePalette* active_palette, int* out_light, int* out_dark
) {
  if (out_light) {
    *out_light = -1;
  }
  if (out_dark) {
    *out_dark = -1;
  }
  if (!active_palette || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const uint8_t* light_rgb = k_nation_fill_rgb_native[nation_id];
  const uint8_t dark_rgb[3] = {
    (uint8_t)((int)light_rgb[0] * 82 / 100),
    (uint8_t)((int)light_rgb[1] * 82 / 100),
    (uint8_t)((int)light_rgb[2] * 82 / 100)
  };
  if (out_light) {
    *out_light = unit_chrome_nearest_palette_index(active_palette, light_rgb);
  }
  if (out_dark) {
    *out_dark = unit_chrome_nearest_palette_index(active_palette, dark_rgb);
  }
}

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

/* bugs.md: after the Declaration the Royal Expeditionary Force renders
 * WHITE — it is the Crown's army, not the peer nation whose slot it
 * borrows (an English player's REF was showing French blue). */
static int g_chrome_crown_nation = -1;
void unit_chrome_set_crown_nation(int nation_id) {
  g_chrome_crown_nation = nation_id;
}

uint8_t unit_chrome_nation_color(int nation_id) {
  if (nation_id >= 0 && nation_id < 4 && nation_id == g_chrome_crown_nation) {
    return 15; /* white */
  }
  if (nation_id >= 0 && nation_id < 4) {
    return k_european_fill[nation_id];
  }
  if (nation_id >= 4 && nation_id <= 11) {
    return k_tribe_colors[nation_id - 4];
  }
  return k_european_fill[0];
}

static uint8_t unit_chrome_names_color(int nation_id) {
  if (nation_id >= 0 && nation_id < 4 && nation_id == g_chrome_crown_nation) {
    return 15; /* white — REF (see unit_chrome_set_crown_nation) */
  }
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
  (void)sprite_w;
  (void)sprite_h;
  /*
   * Fixed 18x18 cell box, independent of the sprite's own width. DOS draws
   * the selection around the 16x16 icon *cell*, not the sprite: the military
   * view's box at 2f2b:1f7f..1faa spans x-1..x+16 by y-1..y+16 (18x18
   * inclusive) whatever unit sits in the slot, and the Europe screenshot
   * (original_screenshots/europe/main_with_caravel_and_two_colonists.png)
   * shows the same 18x18 frame around a dock colonist whose colour sprite is
   * only ~8px wide. The previous sprite-hugging box came out visibly narrow
   * for anything slimmer than a ship (bugs.md: "selection rectangles now too
   * narrow ... should be square").
   */
  *out_x = x - 1;
  *out_y = y - 1;
  *out_w = 18;
  *out_h = 18;
}

/*
 * Shared implementation behind unit_chrome_draw() and the *_colored()
 * override variants. fill_override/letter_override < 0 means "use the
 * normal nation-color computation" (unit_chrome_draw's public behavior,
 * unchanged); >= 0 substitutes a caller-supplied raw palette index for
 * that specific active output palette instead.
 *
 * Why an override exists at all: unit_chrome_nation_color()'s indices
 * (k_european_fill/k_european_names) are tuned against ICONS.SS's own
 * *native* palette (confirmed: index 13 there is a true (255,113,0)
 * Dutch orange, index 5 a matching (170,73,0) darker shade for the
 * Fortify/Fortified letter) — exactly right for screens whose active
 * output palette matches that native one closely enough. Report screens
 * (REPORT*.PIK) don't: their own embedded palettes repurpose slots 5/13
 * back to plain EGA magenta, so a raw index-13/5 fill drawn straight to
 * a report framebuffer comes out magenta instead of orange (found via
 * colony_p1.png: Dutch garrison badges are solid (255,113,0) with a
 * (170,73,0) letter in the golden, matching ICONS.SS-native exactly, but
 * REPORT6.PIK's own palette has no close match to either — best available
 * is a muted (211,101,32)-ish index). See reports.c's Colony report
 * (F6) garrison row for the call site that supplies the override. */
static void unit_chrome_draw_impl(
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
  bool aboard,
  int fill_override,
  int letter_override
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
  const uint8_t fill =
    fill_override >= 0 ? (uint8_t)fill_override : unit_chrome_nation_color(nation_id);

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
      /*
       * DOS FUN_112b_01ba, the fall-through corner (112b:0568..057c): the
       * stack tab peeks UP-LEFT of the box — `AX = DX; DEC; DEC` for x and
       * `AX = y - box_h + sprite_h; DEC; DEC` for y — the same −2/+2-family
       * offset the other three corners use, not down-right. The port had it
       * at +2/+2, which is the corner every plain Colonists/Soldiers unit
       * takes, so the "more units here" rect showed on the wrong side for
       * exactly the commonest units (bugs.md).
       */
      stack_x = box_x - 2;
      stack_y = box_y - 2;
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
  const uint8_t ink =
    letter_override >= 0 ? (uint8_t)letter_override : unit_chrome_letter_color(nation_id, orders_index);
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
  unit_chrome_draw_impl(
    fb, font, icon_x, icon_y, icon_w, icon_h, display_type_index, nation_id, orders_index,
    show_stack, aboard, -1, -1
  );
}

/* Shared impl behind unit_chrome_blit_unit_colored and unit_chrome_blit's
 * ORDERS mode — the only difference is the shadow tint (every existing
 * caller wants plain black; unit_chrome_blit lets a caller ask for
 * something else, unused today but plumbed through for consistency with
 * the SHADOW mode's own shadow_color param). */
static void unit_chrome_blit_unit_colored_shadow(
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
  bool aboard,
  int fill_override,
  int letter_override,
  int shadow_color
) {
  if (!fb || !sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &sheet->sprites[sprite_index];
  const int iw = sp->width > 0 ? sp->width : 16;
  const int ih = sp->height > 0 ? sp->height : 16;

  /* Orders box at tile origin; sprite (+ silhouette) shifted right. */
  const int sx = x + UNIT_CHROME_SPRITE_DX;
  ss_blit_sprite_color(sheet, sprite_index, fb, sx + UNIT_CHROME_SHADOW_DX, y, (uint8_t)shadow_color);
  unit_chrome_draw_impl(
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
    aboard,
    fill_override,
    letter_override
  );
  ss_blit_sprite(sheet, sprite_index, fb, sx, y);
}

void unit_chrome_blit_unit_colored(
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
  bool aboard,
  int fill_override,
  int letter_override
) {
  unit_chrome_blit_unit_colored_shadow(
    fb, font, sheet, sprite_index, x, y, display_type_index, nation_id, orders_index, show_stack,
    aboard, fill_override, letter_override, 0
  );
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
  unit_chrome_blit_unit_colored(
    fb, font, sheet, sprite_index, x, y, display_type_index, nation_id, orders_index, show_stack,
    aboard, -1, -1
  );
}

void unit_chrome_blit_unit_for_palette(
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
  bool aboard,
  const ColonizePalette* active_palette
) {
  int fill_override = -1;
  int letter_override = -1;
  if (active_palette && nation_id >= 0 && nation_id < 4) {
    fill_override = unit_chrome_nearest_palette_index(active_palette, k_nation_fill_rgb_native[nation_id]);
    /*
     * DOS 112b:1996..19b8: only Sentry (1) and Fortified (6) letters take
     * the nation shade — every other order (No Orders, Go To, Fortify in
     * progress, …) is ink 0, black. This wrapper used to force the nation
     * shade for every order, which erased exactly those distinctions on
     * palette-adapted screens (bugs.md: no-orders wrong colour, fortify vs
     * fortified indistinguishable). Black is index 0 in every palette, so
     * no remap is needed for the black case.
     */
    if (orders_index == 1 /* Sentry */ || orders_index == 6 /* Fortified */) {
      letter_override =
        unit_chrome_nearest_palette_index(active_palette, k_nation_letter_rgb_native[nation_id]);
    } else {
      letter_override = 0;
    }
  }
  unit_chrome_blit_unit_colored(
    fb, font, sheet, sprite_index, x, y, display_type_index, nation_id, orders_index, show_stack,
    aboard, fill_override, letter_override
  );
}

void unit_chrome_blit(
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  int x,
  int y,
  UnitChromeDrawMode mode,
  int shadow_color,
  int display_type_index,
  int nation_id,
  int orders_index,
  bool show_stack,
  bool aboard,
  int fill_override,
  int letter_override
) {
  if (!fb || !sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  switch (mode) {
    case UNIT_CHROME_PLAIN_SPRITE:
      ss_blit_sprite(sheet, sprite_index, fb, x, y);
      return;
    case UNIT_CHROME_SPRITE_WITH_SHADOW:
      ss_blit_sprite_color(sheet, sprite_index, fb, x + UNIT_CHROME_SHADOW_DX, y, (uint8_t)shadow_color);
      ss_blit_sprite(sheet, sprite_index, fb, x, y);
      return;
    case UNIT_CHROME_SPRITE_ORDERS:
    default:
      unit_chrome_blit_unit_colored_shadow(
        fb, font, sheet, sprite_index, x, y, display_type_index, nation_id, orders_index, show_stack,
        aboard, fill_override, letter_override, shadow_color
      );
      return;
  }
}
