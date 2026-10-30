/*
 * unit_chrome rendering half — the framebuffer painters split out of
 * unit_chrome.c 2026-09-16 so the simulation side keeps only the colour /
 * flag / crown-nation lookups. Pure move: bodies are byte-identical.
 *
 * This file is UI. turn_draw_owner_indicator lives here for the same reason
 * (it was the only painter left in turn.c).
 */
#include "core/unit_chrome.h"
#include "core/unit_chrome_draw.h"

#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/fb.h"
#include "core/font.h"
#include "core/screen_geom.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/turn.h"

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
  fb_fill_rect(fb, x, y, w, h, 0);
  fb_fill_rect(fb, x + 1, y + 1, w - 2, h - 2, fill);
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
   * for anything slimmer than a ship (port_bugs.md: "selection rectangles now too
   * narrow ... should be square").
   */
  *out_x = x - 1;
  *out_y = y - 1;
  *out_w = 18;
  *out_h = 18;
}

/*
 * DOS FUN_112b_01ba layout width `local_24 = FUN_1c2a_0006(letter) + 3`, the same as the
 * drawn box width. Golden proof: naval.png ship column (x = 2 from FUN_3f41_220c, 13-wide
 * ship sprites: top-right box at x + 9, top-left silhouette at x + 1).
 */
static int unit_chrome_lay_w(const ColonizeFont* font, int nation_id, int orders_index) {
  char letter_buf[2] = {unit_chrome_order_letter(orders_index, nation_id), '\0'};
  return font_text_width(font, letter_buf) + 3;
}

/* DOS 112b:0568..0602: x of the black silhouette (local_6), the colour sprite is 2 right of it.
 * Only the top-left arm (local_14 == 3) moves it: `x + local_24`, minus the overflow
 * `local_24 + sw - 14` when the pair is wider than 14. */
static int unit_chrome_shadow_dx(
  const ColonizeFont* font, int display_type_index, bool damaged, int nation_id, int orders_index,
  int sprite_w
) {
  if (unit_chrome_corner_for_type(display_type_index, damaged) != UNIT_CHROME_CORNER_TOP_LEFT) {
    return 0;
  }
  const int lay_w = unit_chrome_lay_w(font, nation_id, orders_index);
  return lay_w + sprite_w + 2 > 16 ? 14 - sprite_w : lay_w;
}

/*
 * Shared implementation behind the *_colored()
 * override variants. fill_override/letter_override < 0 means "use the
 * normal nation-color computation" (the plain public behavior,
 * unchanged); >= 0 substitutes a caller-supplied raw palette index for
 * that specific active output palette instead.
 *
 * Why an override exists at all: unit_chrome_nation_color()'s raw
 * DS:0x848 indices (12/9/14/13, letter index - 8) are right only under
 * DOS's VICEROY.PAL DAC. A screen the port presents through another
 * palette (a picture palette with EGA magenta / pink at 5/12/13) must
 * nearest-match VICEROY's RGB instead (k_nation_fill_rgb_native). */
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
  bool damaged,
  int fill_override,
  int letter_override,
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  int sprite_x
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
  const int lay_w = unit_chrome_lay_w(font, nation_id, orders_index);
  const int span = lay_w + sprite_w;
  if (span > 16) {
    right_x -= (span - 16);
  }

  const UnitChromeCorner corner = unit_chrome_corner_for_type(display_type_index, damaged);
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
    case UNIT_CHROME_CORNER_TOP_CENTER_DAMAGED: {
      const int mid = icon_x - (lay_w >> 1);
      box_x = mid + 9;
      box_y = icon_y;
      if (corner == UNIT_CHROME_CORNER_TOP_CENTER_DAMAGED) {
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
       * exactly the commonest units (port_bugs.md).
       */
      stack_x = box_x - 2;
      stack_y = box_y - 2;
      break;
  }

  if (show_stack) {
    unit_chrome_draw_box(fb, stack_x, stack_y, box_w, box_h, fill);
  }
  unit_chrome_draw_box(fb, box_x, box_y, box_w, box_h, fill);

  /* DOS 112b:0602..: boxes, then the colour sprite (FUN_112b_015c(2)), then the letter on top. */
  if (sheet) {
    ss_blit_sprite(sheet, sprite_index, fb, sprite_x, icon_y);
  }

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
  const int ink_h = ink_y1 - ink_y0 + 1;
  /* DOS 112b:0749..0758: FUN_1c11_000c pen x = box x + 2 (local_4 + 2 INC INC), cell origin, not ink-centred. */
  const int tx = box_x + 2;
  const int ty = box_y + (box_h - ink_h) / 2 - ink_y0;
  font_draw_text(font, fb, tx, ty, letter_buf, ink);
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
  bool damaged,
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
  const int shadow_x =
    x + unit_chrome_shadow_dx(font, display_type_index, damaged, nation_id, orders_index, iw);
  const int sx = shadow_x + UNIT_CHROME_SPRITE_DX;
  ss_blit_sprite_color(sheet, sprite_index, fb, shadow_x, y, (uint8_t)shadow_color);
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
    damaged,
    fill_override,
    letter_override,
    sheet,
    sprite_index,
    sx
  );
}

static void unit_chrome_blit_unit_colored(
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
  bool damaged,
  int fill_override,
  int letter_override
) {
  unit_chrome_blit_unit_colored_shadow(
    fb, font, sheet, sprite_index, x, y, display_type_index, nation_id, orders_index, show_stack,
    damaged, fill_override, letter_override, 0
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
  bool damaged,
  const ColonizePalette* active_palette
) {
  int fill_override = -1;
  int letter_override = -1;
  if (active_palette && nation_id >= 0 && nation_id < 4) {
    /* port_bugs.md: REF renders WHITE — this palette wrapper bypassed the
     * crown override in unit_chrome_nation_color, so the main map still
     * showed the borrowed peer's blue. */
    static const uint8_t k_white_rgb[3] = {255, 255, 255};
    const uint8_t* fill_rgb =
      nation_id == g_chrome_crown_nation ? k_white_rgb : k_nation_fill_rgb_native[nation_id];
    fill_override =
      assets_palette_nearest_rgb(active_palette, fill_rgb[0], fill_rgb[1], fill_rgb[2]);
    /*
     * DOS 112b:1996..19b8: only Sentry (1) and Fortified (6) letters take
     * the nation shade — every other order (No Orders, Go To, Fortify in
     * progress, …) is ink 0, black. This wrapper used to force the nation
     * shade for every order, which erased exactly those distinctions on
     * palette-adapted screens (port_bugs.md: no-orders wrong colour, fortify vs
     * fortified indistinguishable). Black is index 0 in every palette, so
     * no remap is needed for the black case.
     */
    if (unit_chrome_is_repair_badge(orders_index)) {
      /* DOS local_1a ink: 12 (light red) for nation 2, else 15 (white). */
      letter_override = nation_id == 2
                          ? assets_palette_nearest_rgb(active_palette, 255, 85, 85)
                          : assets_palette_nearest_rgb(active_palette, 255, 255, 255);
    } else if (
      unit_chrome_real_orders(orders_index) == 1 /* Sentry */ ||
      unit_chrome_real_orders(orders_index) == 6 /* Fortified */
    ) {
      static const uint8_t k_grey_rgb[3] = {180, 180, 180};
      const uint8_t* letter_rgb =
        nation_id == g_chrome_crown_nation ? k_grey_rgb : k_nation_letter_rgb_native[nation_id];
      letter_override =
        assets_palette_nearest_rgb(active_palette, letter_rgb[0], letter_rgb[1], letter_rgb[2]);
    } else {
      letter_override = 0;
    }
  }
  unit_chrome_blit_unit_colored(
    fb, font, sheet, sprite_index, x, y, display_type_index, nation_id, orders_index, show_stack,
    damaged, fill_override, letter_override
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
  bool damaged,
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
        damaged, fill_override, letter_override, shadow_color
      );
      return;
  }
}

/* Moved out of turn.c 2026-09-16 (sim/UI split): the only framebuffer
 * painter the EOT processor owned. Pure move. */
void turn_draw_owner_indicator(ColonizeFramebuffer8* framebuffer, int nation_id) {
  const uint8_t color = unit_chrome_nation_color(nation_id);
  /* DOS constants are bottom-right of a 320x200 screen; the port's sidebar is
   * flush right and grows downwards, so keep the same inset from the logical
   * screen's bottom-right corner. */
  const int x0 = screen_geom_w() - (SCREEN_BASE_W - TURN_OWNER_INDICATOR_X);
  const int y0 = screen_geom_h() - (SCREEN_BASE_H - TURN_OWNER_INDICATOR_Y);
  fb_fill_rect(framebuffer, x0, y0, TURN_OWNER_INDICATOR_W, TURN_OWNER_INDICATOR_H, color);
}
