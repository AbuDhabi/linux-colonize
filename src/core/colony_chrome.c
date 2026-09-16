/*
 * Colony map chrome — the settlement icon blit (flag recolour included) and
 * the overland colony marker/name pass, split out of colony.c 2026-09-16 so
 * the colony simulation does not reach ss_/font_. Pure move: bodies are
 * byte-identical to their colony.c originals.
 *
 * This file is UI.
 */
#include "core/colony.h"

#include <stdio.h>
#include <string.h>

#include "core/font.h"
#include "core/ss.h"
#include "core/unit_chrome.h"

/* ICONS.SS #0-3: European colonies by fortification (none / stockade / fort /
 * fortress) - mirrors the colony.c block; colonies_settlement_icon returns
 * these values. */
#define COLONY_MAP_ICON_NONE 3

/*
 * ICONS.SS #0-3's stored flag: 15 fixed (dx,dy) pixels, identical across
 * all four fortification tiers (dumped directly from ICONS.SS — see
 * unit_chrome_nation_flag_shades_for_palette's comment for why this needs
 * recoloring at all). is_dark selects which of the two nation shades that
 * pixel gets.
 */
typedef struct ColonyIconFlagPixel {
  int8_t dx;
  int8_t dy;
  bool is_dark;
} ColonyIconFlagPixel;

static const ColonyIconFlagPixel k_colony_icon_flag_pixels[15] = {
  {6, 0, true}, {7, 0, false}, {8, 0, false},
  {6, 1, true}, {7, 1, false}, {8, 1, false}, {9, 1, false},
  {6, 2, true}, {7, 2, false}, {8, 2, false}, {9, 2, false}, {10, 2, false},
  {8, 3, true}, {9, 3, false}, {10, 3, false}
};

void colonies_blit_settlement_icon(
  const ColonizeSpriteSheet* icons,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int px,
  int py,
  int nation_id,
  const ColonizePalette* active_palette
) {
  if (!icons || sprite < 0 || sprite >= icons->sprite_count || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const ColonizeSprite* sp = &icons->sprites[sprite];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  ss_blit_sprite(icons, sprite, framebuffer, px, py);

  int light = -1;
  int dark = -1;
  unit_chrome_nation_flag_shades_for_palette(nation_id, active_palette, &light, &dark);
  if (light < 0 && dark < 0) {
    return;
  }
  /* bugs.md: the rebel nation flies an actual striped American flag — navy
   * hoist edge, alternating red/white stripe rows — not a plain white flag
   * ("we are not surrendering just yet"). */
  int us_navy = -1;
  int us_red = -1;
  int us_white = -1;
  const bool rebel = nation_id >= 0 && nation_id == unit_chrome_rebel_nation();
  if (rebel) {
    unit_chrome_rebel_flag_colors_for_palette(active_palette, &us_navy, &us_red, &us_white);
  }
  for (int i = 0; i < 15; ++i) {
    const ColonyIconFlagPixel* fp = &k_colony_icon_flag_pixels[i];
    int color = fp->is_dark ? dark : light;
    if (rebel && us_navy >= 0) {
      color = fp->is_dark ? us_navy : ((fp->dy & 1) ? us_white : us_red);
    }
    if (color < 0) {
      continue;
    }
    const int fx = px + fp->dx;
    const int fy = py + fp->dy;
    if (fx < 0 || fy < 0 || fx >= framebuffer->width || fy >= framebuffer->height) {
      continue;
    }
    framebuffer->pixels[fy * framebuffer->width + fx] = (uint8_t)color;
  }
}

static void colony_blit_map_icon(
  const ColonizeSpriteSheet* icons,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int tile_px,
  int tile_py,
  int tile_w,
  int tile_h,
  int nation_id,
  const ColonizePalette* active_palette
) {
  if (!icons || sprite < 0 || sprite >= icons->sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &icons->sprites[sprite];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  /* 21×16 markers: center on the 16×16 tile. */
  const int px = tile_px + (tile_w - sp->width) / 2;
  const int py = tile_py + (tile_h - sp->height) / 2;
  colonies_blit_settlement_icon(icons, sprite, framebuffer, px, py, nation_id, active_palette);
}

/* Draw ICONS.SS colony settlement (#0–3 by fortification) with name below the tile. */
void colonies_render_on_map(
  const ColonizeColonyPool* pool,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  const ColonizeFont* pop_font,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  const ColonizeWorldMap* fog_map,
  int fog_nation,
  const ColonizePalette* active_palette
) {
  if (!pool || !framebuffer) {
    return;
  }

  const bool have_icon = icons && icons->sprite_count > COLONY_MAP_ICON_NONE;

  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (!c->active) {
      continue;
    }
    if (fog_map && !map_tile_seen_by(fog_map, c->x, c->y, fog_nation)) {
      continue;
    }
    const int sx = c->x - view_x;
    const int sy = c->y - view_y;
    if (sx < 0 || sy < 0 || sx >= view_cols || sy >= view_rows) {
      continue;
    }

    const int px = origin_x + sx * tile_w;
    const int py = origin_y + sy * tile_h;

    if (have_icon) {
      colony_blit_map_icon(
        icons, colonies_settlement_icon(pool, c), framebuffer, px, py, tile_w, tile_h, c->nation_id,
        active_palette
      );
    } else {
      /* Icons missing: tiny cyan marker so colonies stay findable. */
      for (int row = py; row < py + 2 && row < framebuffer->height; ++row) {
        if (row < 0) {
          continue;
        }
        for (int col = px; col < px + 2 && col < framebuffer->width; ++col) {
          if (col < 0) {
            continue;
          }
          framebuffer->pixels[row * framebuffer->width + col] = 11;
        }
      }
    }

    /*
     * Population badge + name label — DOS FUN_112b_0c64, and only in the
     * full-size (16px) tile set: the smaller zoom levels draw the icon
     * alone. Geometry read off CODE_5:112b:0dd b..0e79 (Ghidra drops the
     * register arguments of FUN_1c11_000c, so the coordinates come from
     * the instruction stream): digits at tile+(7,7), name at tile+(2,16).
     */
    if (font && tile_w >= 16 && tile_h >= 16) {
      /*
       * What is counted: your own colonies show their live population; a
       * foreign one shows what you last saw (colony +0xba per viewer,
       * floored at 1), so the badge never leaks a rival's growth. DOS
       * writes that floor back into the record; the reveal writer
       * (colonies_note_seen_by) owns it here, so the draw stays const.
       */
      int shown = c->population;
      if (fog_nation >= 0 && fog_nation < 4 && c->nation_id != fog_nation) {
        shown = c->pop_on_map[fog_nation];
        if (shown <= 0) {
          shown = 1;
        }
      }
      /* Colour by Sons of Liberty latch: white, bright green at ≥50%,
       * bright cyan once 100% is latched on top of it (+0x1c bits 4/2). */
      uint8_t ink = 15;
      if ((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0) {
        ink = 10;
        if ((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0) {
          ink = 11;
        }
      }
      char pop_text[8];
      snprintf(pop_text, sizeof(pop_text), "%d", shown);
      const uint8_t pop_shade[4] = {0, ink, ink, ink};
      /* FONTTINY (DS:0x89e), not the name label's FONTINTR (DS:0x268a). */
      font_draw_text_shaded(
        pop_font ? pop_font : font, framebuffer, px + 7, py + 7, pop_text, pop_shade
      );

      /* Colony name below the tile: white ink, black shadow. FONTINTR
       * already bakes a soft AA shadow into shade 2/3 of every glyph
       * (font_draw_text's color==15 path, FF_COLOR_MAP) — that shadow just
       * renders grey/brown, not black. Recolor it in place via
       * font_draw_text_shaded rather than layering a second, separate
       * manual shadow on top (player-caught: an earlier pass added one,
       * doubling up). */
      static const uint8_t kShade[4] = {0, 15, 0, 0};
      font_draw_text_shaded(font, framebuffer, px + 2, py + 16, c->name, kShade);
    }
  }
}
