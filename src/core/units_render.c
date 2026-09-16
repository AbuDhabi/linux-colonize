/*
 * units map rendering, split out of units.c 2026-09-16 so the unit
 * simulation does not reach the chrome painters. Pure move: the body is
 * byte-identical to its units.c original.
 *
 * This file is UI.
 */
#include "core/units.h"

#include <string.h>

#include "core/font.h"
#include "core/ss.h"
#include "core/unit_chrome.h"

void units_render_on_map(
  const ColonizeUnitPool* pool,
  const ColonizeSpriteSheet* nation_sheet,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  bool selected_visible,
  const ColonizeWorldMap* fog_map,
  int fog_nation,
  const ColonizePalette* active_palette
) {
  if (!pool || !nation_sheet || !framebuffer) {
    return;
  }

  /* One sprite per tile (top unit); stack chrome when more share the tile. */
  bool visited[COLONIZE_UNITS_MAX];
  memset(visited, 0, sizeof(visited));

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* unit = &pool->units[i];
    if (!units_is_on_map(unit) || visited[i]) {
      continue;
    }
    if (fog_map && !map_tile_seen_by(fog_map, unit->x, unit->y, fog_nation)) {
      continue;
    }
    const int sx = unit->x - view_x;
    const int sy = unit->y - view_y;
    if (sx < 0 || sy < 0 || sx >= view_cols || sy >= view_rows) {
      continue;
    }

    /* Mark all on-map units on this tile visited. */
    for (int j = 0; j < COLONIZE_UNITS_MAX; ++j) {
      const ColonizeUnit* u = &pool->units[j];
      if (units_is_on_map(u) && u->x == unit->x && u->y == unit->y) {
        visited[j] = true;
      }
    }

    const int top_id = units_top_on_map_tile(pool, unit->x, unit->y, selected_visible, fog_map);
    if (top_id < 0) {
      continue;
    }
    const ColonizeUnit* top = units_get_const(pool, top_id);
    if (!top) {
      continue;
    }
    /*
     * DOS map draw (FUN_2f2b_6372): a unit shows only while the viewer's vis
     * bit is set on it — set when the viewer's sight reveals its tile, reset
     * to "who watches this tile" on every move. Explored-but-unwatched ground
     * shows terrain (fog_map check above) but not who's moving on it now.
     */
    if (top->nation_id != fog_nation && fog_nation >= 0 && fog_nation <= 3 &&
        (top->col1_vis_mask & (1u << fog_nation)) == 0) {
      continue;
    }

    const int sprite = units_map_sprite(pool, top->id);
    if (sprite < 0 || sprite >= nation_sheet->sprite_count) {
      continue;
    }

    const int px = origin_x + sx * tile_w;
    const int py = origin_y + sy * tile_h;
    const int dtype = units_display_type_index(pool, top->id);
    /*
     * "More units here" covers passengers as well. DOS decides it with
     * FUN_1427_0002/004a — "is there another unit after this one in the
     * chain" (112b:01de..01fa) — and that chain is the same next/prev pair
     * that links a ship to the units riding in it, not just the units
     * standing on the tile. A loaded transport carries the tab (bugs.md).
     */
    const bool stacked = units_map_stack_chrome(pool, top->id);
    /* Chrome's 4th badge arm is Artillery + the damaged bit (+0x3148 bit7),
     * not "aboard a ship" — unit_chrome_corner_for_type. */
    const bool damaged = (top->col1_flags15 & 0x80u) != 0;

    unit_chrome_blit_unit_for_palette(
      framebuffer,
      font,
      nation_sheet,
      sprite,
      px,
      py,
      dtype,
      top->nation_id,
      top->orders,
      stacked,
      damaged,
      active_palette
    );
  }
}
