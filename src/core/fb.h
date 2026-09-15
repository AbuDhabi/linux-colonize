#ifndef COLONIZE_FB_H
#define COLONIZE_FB_H

#include <stdint.h>

#include "platform/platform.h"

/*
 * Clipped 8-bit framebuffer primitives — the one home for the ~25 private
 * copies the screen modules grew (audit 2026-09-14 theme F, rows IN-8, IN-11,
 * IN-12, GL-6). Every function is a no-op on a NULL framebuffer or NULL
 * pixels, and clips silently to the framebuffer rectangle.
 *
 * TWO SIGNATURE FAMILIES existed and must not be mixed up while migrating.
 * fb_fill_rect and fb_rect_outline take x, y, WIDTH, HEIGHT (exclusive right
 * and bottom edges); fb_hline / fb_vline take INCLUSIVE end coordinates,
 * matching the popup/map_panel spelling. The existing copies split like this:
 *
 *   inclusive x0,y0,x1,y1 fills : popup.c popup_fill_rect, map_menu.c
 *                                 map_menu_fill_rect, colony_screen.c (both
 *                                 colony_screen_fill_rect at :24 and :3723),
 *                                 game_loop.c europe_fill_rect and
 *                                 begin_menu_fill_rect
 *     -> fb_fill_rect(fb, x0, y0, x1 - x0 + 1, y1 - y0 + 1, c)
 *
 *   x,y,w,h fills              : map_panel.c map_panel_fill, text_edit.c
 *                                 fill_rect, new_game.c new_game_fill_rect,
 *                                 reports.c reports_score_fill_rect,
 *                                 unit_chrome.c unit_chrome_fill_rect
 *     -> fb_fill_rect unchanged
 *
 *   inclusive line ends        : popup_hline/_vline, map_menu_hline/_vline,
 *                                 map_panel_hline/_vline, trade_screen.c
 *                                 trade_hline/_vline, reports_draw_hline/_vline
 *     -> fb_hline / fb_vline unchanged (they also normalise swapped ends)
 *
 *   put-pixel                  : font.c put_pixel, popup.c popup_put,
 *                                 map_panel.c map_panel_put, ui_button.c
 *                                 ui_button_put, unit_chrome.c unit_chrome_put
 *     -> fb_put
 *
 *   rect outline, w/h          : text_edit.c text_edit_draw_frame and
 *                                 new_game.c new_game_draw_rect_border (single
 *                                 colour), ui_button.c ui_button_draw_frame
 *                                 (bevelled two-colour)
 *     -> fb_rect_outline(fb, x, y, w, h, top_left, bottom_right); pass the
 *        same colour twice for the single-colour form
 *
 *   rect outline, inclusive    : pedia.c pedia_fill_rect_outline, reports.c
 *                                 reports_draw_rect_outline
 *     -> fb_rect_outline(fb, x0, y0, x1 - x0 + 1, y1 - y0 + 1, c, c)
 *
 * map_gen.c's paint_rect_outline is NOT in this family — it paints terrain
 * bytes into a bare map plane, not a framebuffer.
 */

void fb_put(ColonizeFramebuffer8* fb, int x, int y, uint8_t color);

/* Inclusive x0..x1 / y0..y1; swapped ends are normalised. */
void fb_hline(ColonizeFramebuffer8* fb, int y, int x0, int x1, uint8_t color);
void fb_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color);

/* w/h form: fills x..x+w-1, y..y+h-1. Non-positive w or h draws nothing. */
void fb_fill_rect(ColonizeFramebuffer8* fb, int x, int y, int w, int h, uint8_t color);

/*
 * One-pixel outline of the w/h rectangle. The top and left edges use
 * top_left_col, the bottom and right edges bottom_right_col (the bevel form);
 * pass the same colour twice for a plain single-colour border. Both corner
 * columns win over the horizontal edges, matching ui_button_draw_frame's own
 * paint order. Note ui_button_draw_frame additionally bails out for w < 2 or
 * h < 2 (where the two edges would coincide); fb_rect_outline draws the
 * degenerate 1px line instead, so keep that guard at the call site if it
 * matters there.
 */
void fb_rect_outline(
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  int h,
  uint8_t top_left_col,
  uint8_t bottom_right_col
);

#endif
