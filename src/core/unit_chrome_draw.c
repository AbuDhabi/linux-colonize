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
   * for anything slimmer than a ship (bugs.md: "selection rectangles now too
   * narrow ... should be square").
   */
  *out_x = x - 1;
  *out_y = y - 1;
  *out_w = 18;
  *out_h = 18;
}

/*
 * Shared implementation behind the *_colored()
 * override variants. fill_override/letter_override < 0 means "use the
 * normal nation-color computation" (the plain public behavior,
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
  bool damaged,
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
      const int mid = icon_x - (box_w >> 1);
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
    damaged,
    fill_override,
    letter_override
  );
  ss_blit_sprite(sheet, sprite_index, fb, sx, y);
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
    /* bugs.md: REF renders WHITE — this palette wrapper bypassed the
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
     * palette-adapted screens (bugs.md: no-orders wrong colour, fortify vs
     * fortified indistinguishable). Black is index 0 in every palette, so
     * no remap is needed for the black case.
     */
    if (orders_index == 1 /* Sentry */ || orders_index == 6 /* Fortified */) {
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
  if (!framebuffer || !framebuffer->pixels || framebuffer->width <= 0 || framebuffer->height <= 0) {
    return;
  }
  const uint8_t color = unit_chrome_nation_color(nation_id);
  const int x0 = TURN_OWNER_INDICATOR_X;
  const int y0 = TURN_OWNER_INDICATOR_Y;
  for (int y = y0; y < y0 + TURN_OWNER_INDICATOR_H; ++y) {
    if (y < 0 || y >= framebuffer->height) {
      continue;
    }
    for (int x = x0; x < x0 + TURN_OWNER_INDICATOR_W; ++x) {
      if (x < 0 || x >= framebuffer->width) {
        continue;
      }
      framebuffer->pixels[y * framebuffer->width + x] = color;
    }
  }
}
