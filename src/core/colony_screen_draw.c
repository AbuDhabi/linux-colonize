#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony_craft.h"
#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_screen_internal.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/fb.h"
#include "core/ff.h"
#include "core/founding_fathers.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/ui_colors.h"
#include "core/turn.h"
#include "core/ui_button.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * Sections:
 *  - Background & chrome rendering primitives (colony_screen_fill_parch .. colony_screen_draw_outlined_number) (~line 38)
 *  - Icon strips & resource-count rendering (colony_screen_icon_strip_layout .. colony_screen_debug_building_rect) (~line 306)
 *  - Area overlays & minimap rendering (colony_screen_draw_area_overlays .. colony_screen_render_minimap) (~line 638)
 */

/* ===================== Background & chrome rendering primitives (colony_screen_fill_parch .. colony_screen_draw_outlined_number) ===================== */

void colony_screen_fill_parch(const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer) {
  if (!view || !view->parch_ok) {
    return;
  }
  map_panel_tile_rect(
    &view->parch,
    COLONY_VIEWPORT_X,
    COLONY_VIEWPORT_Y,
    COLONY_PARCH_FILL_W,
    COLONY_PARCH_FILL_H,
    framebuffer
  );
}

void colony_screen_fill_wood_tile(const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer) {
  if (!view || !view->wood_tile_ok) {
    return;
  }
  map_menu_tile_rect_screen_phase(
    &view->wood_tile,
    COLONY_MINIMAP_SECTION_X,
    COLONY_MINIMAP_SECTION_Y,
    COLONY_MINIMAP_SECTION_W,
    COLONY_MINIMAP_SECTION_H,
    framebuffer
  );
}

/* Golden: the top bar is the same full-screen WOODTILE grain, not
 * WOODPANL.PIK (whose art carries a dark 2px right edge the golden lacks). */
void colony_screen_fill_top_bar_wood(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !view->wood_tile_ok) {
    return;
  }
  map_menu_tile_rect_screen_phase(
    &view->wood_tile, 0, 0, COLONY_SCREEN_WIDTH, COLONY_TOP_BAR_H, framebuffer
  );
}

void colony_screen_draw_hline(ColonizeFramebuffer8* framebuffer, int y, int color) {
  if (!framebuffer || !framebuffer->pixels || y < 0 || y >= framebuffer->height) {
    return;
  }
  uint8_t c = (uint8_t)color;
  for (int x = 0; x < framebuffer->width; ++x) {
    framebuffer->pixels[y * framebuffer->width + x] = c;
  }
}

void colony_screen_draw_vline(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y0,
  int y1,
  int color
) {
  if (!framebuffer || !framebuffer->pixels || x < 0 || x >= framebuffer->width) {
    return;
  }
  if (y0 > y1) {
    const int t = y0;
    y0 = y1;
    y1 = t;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (y1 >= framebuffer->height) {
    y1 = framebuffer->height - 1;
  }
  uint8_t c = (uint8_t)color;
  for (int y = y0; y <= y1; ++y) {
    framebuffer->pixels[y * framebuffer->width + x] = c;
  }
}

void colony_screen_draw_top_bar(
  const ColonizeColony* colony,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!font || !framebuffer) {
    return;
  }
  /* DOS FUN_2f2b_0fce: one centered string "<Name>.  <Season>, <Year>.
   * Gold: <N>$" — not three separately-positioned fields (golden-measured:
   * single green (WOODPANL.PIK idx 68, exact RGB match) run, native width
   * ~158px centered at x~160, y=2; period/comma placement confirmed by
   * zooming the golden's punctuation glyphs — the mark after the colony
   * name and after the year is a plain baseline dot (period), the one after
   * the season has a trailing hooked descender (comma)). Built locally
   * rather than via turn_format_date() (shared by other screens that want
   * plain "Season Year" with no punctuation). */
  const char* name = (colony && colony->name[0]) ? colony->name : "";
  char date[32];
  turn_format_date(game_year, game_autumn, date, sizeof(date));
  char season[16] = "";
  unsigned year = 0;
  sscanf(date, "%15s %u", season, &year);
  char line[96];
  snprintf(line, sizeof(line), "%s.  %s, %u.  Gold: %d$", name, season, year, gold);
  const int w = font_text_width(font, line);
  const int x = (COLONY_SCREEN_WIDTH - w) / 2;
  /* bugs.md #1: golden (new_amsterdam_production.png) ink top edge
   * measures native y=1, not 2 — title sat 1px too low. */
  font_draw_text(font, framebuffer, x, 1, line, COLONIZE_COL_BASIC);
}

void colony_screen_draw_selection_box(
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  uint8_t color
) {
  if (!framebuffer || w <= 0 || h <= 0) {
    return;
  }
  fb_fill_rect(framebuffer, x, y, w, 1, color);
  fb_fill_rect(framebuffer, x, y + h - 1, w, 1, color);
  fb_fill_rect(framebuffer, x, y, 1, h, color);
  fb_fill_rect(framebuffer, x + w - 1, y, 1, h, color);
}

/* Tight green box around an ICONS.SS sprite at (x,y), 1px margin. */
void colony_screen_draw_icon_selection(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int sprite,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 || sprite >= view->icons.sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[sprite];
  if (!sp || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  colony_screen_draw_selection_box(framebuffer, x - 1, y - 1, sp->width + 2, sp->height + 2, 10);
}

/* Selection frame for unit_chrome_blit_unit art (shadow + orders + sprite). */
void colony_screen_draw_chrome_selection(
  const ColonyScreenView* view,
  ColonizeFramebuffer8* framebuffer,
  int sprite,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 || sprite >= view->icons.sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[sprite];
  if (!sp || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  int fx = 0;
  int fy = 0;
  int fw = 0;
  int fh = 0;
  unit_chrome_selection_frame(x, y, sp->width, sp->height, &fx, &fy, &fw, &fh);
  colony_screen_draw_selection_box(framebuffer, fx, fy, fw, fh, 10);
}

void colony_screen_blit_icon(
  const ColonyScreenView* view,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 || sprite >= view->icons.sprite_count) {
    return;
  }
  unit_chrome_blit(
    framebuffer, NULL, &view->icons, sprite, x, y, UNIT_CHROME_PLAIN_SPRITE, 0, 0, -1, 0, false, false,
    -1, -1
  );
}

/*
 * Colonist/on-tile-unit figures only (not buildings, cargo, or badge
 * icons): the same black 2px-left shadow silhouette unit_chrome uses for
 * units on the overland map (UNIT_CHROME_SHADOW_DX) — same amount of
 * shadow, just without the orders/allegiance box this screen doesn't draw
 * on its own figures. */
static void colony_screen_blit_icon_shadowed(
  const ColonyScreenView* view,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y
) {
  if (!view || !view->icons_ok || !framebuffer || sprite < 0 || sprite >= view->icons.sprite_count) {
    return;
  }
  unit_chrome_blit(
    framebuffer, NULL, &view->icons, sprite, x, y, UNIT_CHROME_SPRITE_WITH_SHADOW, 0, 0, -1, 0, false,
    false, -1, -1
  );
}

void colony_screen_blit_cargo(
  const ColonyScreenView* view,
  int cargo,
  bool grey,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y
) {
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  const int sprite = (grey ? COLONY_CARGO_GREY_BASE : COLONY_CARGO_ICON_BASE) + cargo;
  colony_screen_blit_icon(view, sprite, framebuffer, x, y);
}

/*
 * Note 1: one icon per unit of resource, evenly spaced in [x,y,w,h].
 * Number (left, black outline + number_color) appears when always_show_number,
 * or when start-to-start spacing between icons is <= 1px (nearly total overlap).
 */
void colony_screen_draw_outlined_number(
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  const char* text,
  uint8_t fg_color
) {
  if (!font || !framebuffer || !text) {
    return;
  }
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      font_draw_text(font, framebuffer, x + dx, y + dy, text, 0);
    }
  }
  font_draw_text(font, framebuffer, x, y, text, fg_color);
}

/*
 * Note 1 layout: icon x positions for a row centred in [x, x+w).
 * ref_iw is the reference sprite width (usually the first icon).
 * Returns start-to-start step (0 if fully stacked).
 *
 * The row is packed shoulder to shoulder — one sprite width per step — and
 * the whole block centred on the host rect, rather than spread across its
 * full width. bugs.md: "Colonists working buildings in colony UI stand too
 * far apart. If the building is wide enough, have them stand centered
 * horizontally on the building, right next to each other as per their sprite
 * widths. If the building is too narrow for that, squeeze them as needed."
 * The squeeze keeps the last sprite inside the rect, and the caller's
 * count badge still appears once the step collapses to <= 1px.
 */
/* ===================== Icon strips & resource-count rendering (colony_screen_icon_strip_layout .. colony_screen_debug_building_rect) ===================== */

int colony_screen_icon_strip_layout(int x, int w, int count, int ref_iw, int* out_x) {
  if (count <= 0 || !out_x || ref_iw <= 0) {
    return 0;
  }
  if (w <= ref_iw) {
    /* Narrower than one sprite: stack them all, centred as best we can. */
    const int only = x + (w - ref_iw) / 2;
    for (int i = 0; i < count; ++i) {
      out_x[i] = only;
    }
    return count == 1 ? ref_iw : 0;
  }
  int step = ref_iw;
  if (count > 1 && ref_iw * count > w) {
    step = (w - ref_iw) / (count - 1); /* overlap just enough to fit */
    if (step < 0) {
      step = 0;
    }
  }
  const int total = step * (count - 1) + ref_iw;
  const int start = x + (w - total) / 2;
  for (int i = 0; i < count; ++i) {
    out_x[i] = start + i * step;
  }
  return count == 1 ? ref_iw : step;
}

/*
 * One icon per entry, evenly spaced (Note 1). Heterogeneous sprites allowed.
 * selected_index >= 0 draws a selection box on that item (colonists / units);
 * pass -1 for non-selectable resource strips.
 */
void colony_screen_draw_icon_strip(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  const int* icons,
  int count,
  int selected_index,
  uint8_t number_color,
  bool always_show_number
) {
  if (!view || !view->icons_ok || !framebuffer || !icons || count <= 0 || w <= 0 || h <= 0) {
    return;
  }
  if (count > COLONY_OUTSIDE_MAX) {
    count = COLONY_OUTSIDE_MAX;
  }
  int ref_iw = 12;
  {
    const int first = icons[0];
    if (first >= 0 && first < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[first];
      if (sp && sp->width > 0) {
        ref_iw = sp->width;
      }
    }
  }
  int xs[COLONY_OUTSIDE_MAX];
  const int start_step = colony_screen_icon_strip_layout(x, w, count, ref_iw, xs);
  for (int i = 0; i < count; ++i) {
    const int icon = icons[i];
    if (icon < 0 || icon >= view->icons.sprite_count) {
      continue;
    }
    const ColonizeSprite* sp = &view->icons.sprites[icon];
    if (!sp || !sp->pixels || sp->width <= 0 || sp->height <= 0) {
      continue;
    }
    const int iy = y + (h - sp->height) / 2;
    colony_screen_blit_icon_shadowed(view, icon, framebuffer, xs[i], iy);
    if (selected_index == i) {
      colony_screen_draw_icon_selection(view, framebuffer, icon, xs[i], iy);
    }
  }
  if ((always_show_number || start_step <= 1) && font) {
    char num[12];
    snprintf(num, sizeof(num), "%d", count);
    colony_screen_draw_outlined_number(
      font, framebuffer, x + 1, y + (h > 6 ? 1 : 0), num, number_color
    );
  }
}

/*
 * One icon per unit, evenly spaced. When amount0+amount1 > 0, sprites for
 * amount0 use icon0 first, then amount1 use icon1 (e.g. fish then grain).
 * Not selectable (resources).
 */
void colony_screen_draw_resource_count_pair(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  int icon0,
  int amount0,
  int icon1,
  int amount1,
  uint8_t number_color,
  bool always_show_number
) {
  if (!view || !view->icons_ok || !framebuffer || w <= 0 || h <= 0) {
    return;
  }
  if (amount0 < 0) {
    amount0 = 0;
  }
  if (amount1 < 0) {
    amount1 = 0;
  }
  const int amount = amount0 + amount1;
  if (amount <= 0) {
    return;
  }
  /* One icon per unit of resource, spread across [x,x+w) (Note 1) — golden-
   * confirmed via the newer "numberless" reference captures (`new_amsterdam
   * _production_numberless.png` / `recife_..._numberless.png`): DOS really
   * does repeat the icon `amount` times, not draw one static icon. What a
   * prior pass read off the (number-mode) goldens as a deliberately painted
   * "content-sized black background pill" was actually just this: dozens of
   * black-bordered icon copies overlapping almost completely, so only the
   * last (topmost) one's art is visible and everything else fuses into a
   * black smear — real, and it *scales with amount* (a bigger stock badge
   * genuinely smears wider), which a fixed single-icon-plus-box never did.
   * That box is gone; `amount` copies are blit for real, exactly like
   * `colony_screen_draw_icon_strip` does for worker/unit strips, except this
   * function's number is unconditional — confirmed against *both* the
   * numbered and numberless goldens that resource-count badges (settlement/
   * Production-tab/People-band/area-view) always show their number
   * regardless of amount; the "always show numbers" toggle only changes the
   * area-view field-tile badges (a different code path), not these. See
   * docs/colony_screen.md. */
  const int first_icon = amount0 > 0 ? icon0 : icon1;
  if (first_icon < 0 || first_icon >= view->icons.sprite_count) {
    return;
  }
  if (amount1 > 0 && (icon1 < 0 || icon1 >= view->icons.sprite_count)) {
    return;
  }
  if (amount0 > 0 && (icon0 < 0 || icon0 >= view->icons.sprite_count)) {
    return;
  }
  const ColonizeSprite* sp = &view->icons.sprites[first_icon];
  if (!sp || !sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  const int iw = sp->width;
  const int ih = sp->height;
  const int iy = y + (h - ih) / 2;
  /* Where the icon run begins — the number anchors to it (bugs.md: a lone
   * centred icon left its number stranded in the cell's corner). */
  int cluster_x = x;
  if (amount == 1) {
    const int ix = x + (w - iw) / 2;
    cluster_x = ix;
    ss_blit_sprite(&view->icons, first_icon, framebuffer, ix, iy);
  } else if (w <= iw) {
    for (int i = 0; i < amount; ++i) {
      const int icon = (i < amount0) ? icon0 : icon1;
      ss_blit_sprite(&view->icons, icon, framebuffer, x, iy);
    }
  } else {
    const int span = w - iw;
    for (int i = 0; i < amount; ++i) {
      const int icon = (i < amount0) ? icon0 : icon1;
      const int ix = x + (i * span) / (amount - 1);
      ss_blit_sprite(&view->icons, icon, framebuffer, ix, iy);
    }
  }
  /*
   * DOS FUN_1097_0174 number rule (bugs.md): the count label appears when
   * the game-wide numbers toggle (DS:0x336, `show_production_numbers`) is
   * on, OR when the icons packed into an uncountable smear — DOS's
   * `step == 1 && count > 1` override, the "subitizable" cut: 5 corn icons
   * at a readable pitch stay bare with numbers off, 14 fused ones get the
   * label anyway (both states golden-confirmed against new_amsterdam_
   * production(.numberless).png). `always_show_number` marks the badges
   * DOS numbers unconditionally (People band, warehouse-style labels).
   */
  bool show_number = always_show_number || view->show_production_numbers;
  if (!show_number && amount > 1) {
    if (w <= iw) {
      show_number = true; /* everything stacked on one spot */
    } else {
      const int step = (w - iw) / (amount - 1);
      if (step <= 1) {
        show_number = true;
      }
    }
  }
  if (show_number && font) {
    char num[12];
    snprintf(num, sizeof(num), "%d", amount);
    /* On the first icon's own position — both axes (bugs.md: it sat at the
     * cell's top edge while the icons centred themselves lower). */
    colony_screen_draw_outlined_number(
      font, framebuffer, cluster_x + 1, iy, num, number_color
    );
  }
}

void colony_screen_draw_resource_count(
  const ColonyScreenView* view,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int x,
  int y,
  int w,
  int h,
  int icon_sprite,
  int amount,
  uint8_t number_color,
  bool always_show_number
) {
  colony_screen_draw_resource_count_pair(
    view,
    font,
    framebuffer,
    x,
    y,
    w,
    h,
    icon_sprite,
    amount,
    icon_sprite,
    0,
    number_color,
    always_show_number
  );
}

/*
 * Nearest-neighbour 1.5× blit (16→24 for standard terrain cells).
 * Audit CO-19 / IN-14: this and the "only where the destination already
 * holds match_color" variant were the same 35 lines twice. match_color < 0
 * means "write every pixel", which is what the plain blit did.
 */
static void colony_screen_blit_scaled_15_where_dest(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y,
  int match_color
) {
  if (!sheet || !framebuffer || !framebuffer->pixels || sprite_index < 0 ||
      sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sprite = &sheet->sprites[sprite_index];
  if (!sprite->pixels || sprite->width <= 0 || sprite->height <= 0) {
    return;
  }
  const int dw = (sprite->width * 3) / 2;
  const int dh = (sprite->height * 3) / 2;
  for (int dy = 0; dy < dh; ++dy) {
    const int sy = dy * sprite->height / dh;
    const int fy = dst_y + dy;
    if (fy < 0 || fy >= framebuffer->height) {
      continue;
    }
    for (int dx = 0; dx < dw; ++dx) {
      const int sx = dx * sprite->width / dw;
      const int fx = dst_x + dx;
      if (fx < 0 || fx >= framebuffer->width) {
        continue;
      }
      const int di = fy * framebuffer->width + fx;
      if (match_color >= 0 && framebuffer->pixels[di] != (uint8_t)match_color) {
        continue;
      }
      const uint8_t color = sprite->pixels[sy * sprite->width + sx];
      if (color == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      framebuffer->pixels[di] = color;
    }
  }
}

void colony_screen_debug_building_rect(
  const ColonyScreenView* view, ColonizeFramebuffer8* framebuffer, int sprite, int x, int y
);

/*
 * DOS settlement-view size-class boxes: DS:0x230 (width) and DS:0x236
 * (height), indexed by NAMES.TXT @BUILDING column 4. Class 3 (73x18) is the
 * fence/stockade corner and class 4 (75x48) the dock corner — both already
 * hardcoded above as COLONY_FENCE_W/H and COLONY_COAST_W/H, which is what
 * pins this table to the right rows. FUN_2f2b_14d4 centres its level badge
 * in this box, so the port needs the same numbers.
 */
const int colony_screen_class_box[5][2] = {{23, 27}, {44, 22}, {53, 37}, {73, 18}, {75, 48}};


/* ===================== Area overlays & minimap rendering (colony_screen_draw_area_overlays .. colony_screen_render_minimap) ===================== */

void colony_screen_draw_area_overlays(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !colony || !map || !framebuffer) {
    return;
  }
  int origin_x = 0;
  int origin_y = 0;
  colony_screen_minimap_origin(&origin_x, &origin_y);
  const int half = COLONY_MINIMAP_GRID / 2;
  const int tile = COLONY_MINIMAP_TILE;

  /* Field-yield SoL bonus/latch bits — same inputs colony_preview.c folds in
   * (turn_produce_one_colony's real formula), needed so these on-map badges
   * agree with the golden and with the Production multifunction pane
   * instead of showing the unboosted base rate. */
  const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);

  /* Center settlement icon + auto-yield rows (Note 1), on the center tile. */
  {
    const int tile_x = origin_x + half * tile;
    const int tile_y = origin_y + half * tile;
    const int icon = colonies_settlement_icon(pool, colony);
    if (view->icons_ok && icon >= 0 && icon < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[icon];
      const int px = tile_x + (tile - sp->width) / 2;
      const int py = tile_y + (tile - sp->height) / 2;
      ss_blit_sprite(&view->icons, icon, framebuffer, px, py);
    }
    ColonizeTownCommonsYield tc;
    colony_yield_town_commons(
      map, colony->x, colony->y, colony->colony_flags,
      col1 ? (int)col1->head.difficulty : 4, &tc
    );
    int row = 0;
    if (tc.food > 0) {
      colony_screen_draw_resource_count(
        view,
        font,
        framebuffer,
        tile_x,
        tile_y + row * 10,
        tile,
        10,
        COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_FOOD,
        tc.food,
        15,
        false
      );
      row++;
    }
    if (tc.secondary_amount > 0 && tc.secondary_cargo >= 0) {
      colony_screen_draw_resource_count(
        view,
        font,
        framebuffer,
        tile_x,
        tile_y + row * 10,
        tile,
        10,
        COLONY_CARGO_ICON_BASE + tc.secondary_cargo,
        tc.secondary_amount,
        15,
        false
      );
    }
  }

  /* Docks (or an upgrade: Drydock/Shipyard) gates Fisherman yield to 0 —
   * FUN_15eb_18ec:11967-11969. Shared answer, so it cannot drift from
   * turn.c's check the way the AI scorer's copy did (audit E#2). */
  const bool has_docks = colony_yield_colony_has_docks(pool, colony);

  /* Same one-badge-per-colonist dedupe turn_produce_one_colony (turn.c) and
   * colony_preview_compute apply to the production loops: a colonist index
   * appearing in two tiles[] slots is worked (and drawn) once, at the first
   * slot. Without it a stale second slot drew a second badge + figure for a
   * colonist the tick itself never paid twice (smell audit #65). */
  ColonizeWorkedTileIter wit;
  ColonizeWorkedTile w;
  colony_yield_worked_tiles_begin(&wit, colony);
  while (colony_yield_worked_tiles_next(&wit, &w)) {
    const ColonizeColonist* c = w.colonist;
    const int tile_x = origin_x + (w.dx + half) * tile;
    const int tile_y = origin_y + (w.dy + half) * tile;
    const int cargo = colony_yield_job_cargo(c->field_job);
    /* Henry Hudson's Fur Trapper doubling lives inside the pipeline now
     * (colony_yield.c, DOS FUN_15eb_18ec 11970-11973; smell audit #60) —
     * matches turn.c/colony_preview.c. */
    int yld = colony_yield_for_worker(
      map,
      w.x,
      w.y,
      c->field_job,
      c->profession,
      has_docks,
      sol_b_field,
      colony->colony_flags,
      col1 && founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)
    );
    if (cargo >= 0 && yld > 0) {
      /* FUN_2f2b_* raw 47750-47751: the per-plot badge swaps the Food cargo
       * icon for ICONS.SS #0x3a (COLONY_ICON_FISH) when the plot's job is the
       * Fisherman row — the produce icon is the JOB's, not the cargo's. */
      const int icon = (c->field_job == COLONIZE_JOB_FISHERMAN)
                         ? COLONY_ICON_FISH
                         : (COLONY_CARGO_ICON_BASE + cargo);
      colony_screen_draw_resource_count(
        view,
        font,
        framebuffer,
        tile_x,
        tile_y,
        tile,
        12,
        icon,
        yld,
        15,
        false
      );
    } else if (cargo >= 0) {
      /* bugs.md: a worker whose job produces nothing here (farmer on sea /
       * mountain, dockless fisherman, …) shows the job's normal produce
       * icon with the red slashed-circle (ICONS.SS #64) superimposed —
       * not an empty tile. Same raw 47750-47751 fish-icon swap as above. */
      const int icon = (c->field_job == COLONIZE_JOB_FISHERMAN)
                         ? COLONY_ICON_FISH
                         : (COLONY_CARGO_ICON_BASE + cargo);
      if (view->icons_ok && icon >= 0 && icon < view->icons.sprite_count) {
        const ColonizeSprite* sp = &view->icons.sprites[icon];
        const int px = tile_x + (tile - sp->width) / 2;
        const int py = tile_y + (12 - sp->height) / 2;
        ss_blit_sprite(&view->icons, icon, framebuffer, px, py);
        if (64 < view->icons.sprite_count) {
          const ColonizeSprite* slash = &view->icons.sprites[64];
          ss_blit_sprite(
            &view->icons, 64, framebuffer,
            px + (sp->width - slash->width) / 2,
            py + (sp->height - slash->height) / 2
          );
        }
      }
    }
    if (units) {
      const int sprite =
        units_working_colonist_sprite(units, c->unit_type_index, c->profession);
      if (sprite >= 0) {
        const ColonizeSprite* sp =
          (view->icons_ok && sprite < view->icons.sprite_count) ? &view->icons.sprites[sprite]
                                                               : NULL;
        const int iw = sp ? sp->width : 12;
        const int ih = sp ? sp->height : 12;
        const int ix = tile_x + (tile - iw) / 2;
        const int iy = tile_y + tile - ih - 1;
        colony_screen_blit_icon_shadowed(view, sprite, framebuffer, ix, iy);
        if (view->selected_colonist == w.colonist_index) {
          colony_screen_draw_selection_box(framebuffer, tile_x, tile_y, tile, tile, 10);
        }
      }
    }
  }

  /*
   * bugs.md #284 / DOS FUN_15eb_26e4: unworked tiles still claimed as Indian
   * land (MET tribe radius, land unbought, no Peter Minuit) carry a totem
   * pole marker — ICONS.SS #108 (8x16 red totem pole), bottom-centred with
   * the standard 2px shadow like other on-tile figures.
   */
  if (col1) {
    for (int dy = -half; dy <= half; ++dy) {
      for (int dx = -half; dx <= half; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int ti = colonies_field_tile_index(dx, dy);
        if (ti >= 0) {
          const int who = (int)colony->tiles[ti];
          if (who >= 0 && who < colony->colonist_count &&
              colony->colonists[who].active && colony->colonists[who].field_job >= 0) {
            continue; /* worked tile — DOS clears the claim slot */
          }
        }
        if (colonies_indian_claim_tribe_from_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(pool), .map=(ColonizeWorldMap*)(map), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL)}, colony->nation_id, colony->x, colony->y, colony->x + dx, colony->y + dy) < 0) {
          continue;
        }
        const int tx = origin_x + (dx + half) * tile;
        const int ty = origin_y + (dy + half) * tile;
        if (view->icons_ok && COLONY_ICON_TOTEM < view->icons.sprite_count) {
          const ColonizeSprite* sp = &view->icons.sprites[COLONY_ICON_TOTEM];
          const int ix = tx + (tile - sp->width) / 2;
          const int iy = ty + tile - sp->height - 1;
          colony_screen_blit_icon_shadowed(view, COLONY_ICON_TOTEM, framebuffer, ix, iy);
        }
      }
    }
  }
}

void colony_screen_render_minimap(
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  int colony_x,
  int colony_y,
  ColonizeFramebuffer8* framebuffer
) {
  if (!map || !terrain || !framebuffer) {
    return;
  }

  int origin_x = 0;
  int origin_y = 0;
  colony_screen_minimap_origin(&origin_x, &origin_y);
  const int half = COLONY_MINIMAP_GRID / 2;
  const int tile = COLONY_MINIMAP_TILE;

  for (int dy = -half; dy <= half; ++dy) {
    for (int dx = -half; dx <= half; ++dx) {
      const int mx = colony_x + dx;
      const int my = colony_y + dy;
      const int tile_x = origin_x + (dx + half) * tile;
      const int tile_y = origin_y + (dy + half) * tile;
      ColonizeMapLayerCmd cmds[MAP_LAYER_CMDS_MAX];
      const int ncmd = map_tile_layer_cmds(map, mx, my, 0, cmds, MAP_LAYER_CMDS_MAX);
      for (int ci = 0; ci < ncmd; ++ci) {
        const ColonizeMapLayerCmd* cmd = &cmds[ci];
        const ColonizeSpriteSheet* sheet =
          (cmd->sheet == MAP_LAYER_SHEET_TERRAIN) ? terrain : phys0;
        if (!sheet || cmd->sprite < 0 || cmd->sprite >= sheet->sprite_count) {
          continue;
        }
        colony_screen_blit_scaled_15_where_dest(
          sheet, cmd->sprite, framebuffer, tile_x + (cmd->ox * 3) / 2,
          tile_y + (cmd->oy * 3) / 2, cmd->into_holes ? 0 : -1
        );
      }
    }
  }

  /* Black 1px frame around the whole 3x3 tile grid — golden-measured
   * (new_amsterdam_production.png: a 73x73 native square, exactly the
   * grid's own COLONY_MINIMAP_GRID*COLONY_MINIMAP_TILE bounding box) —
   * was missing entirely. Drawn after the tiles so it sits on the grid's
   * edge; the cursor's green selection box and unit/badge overlays are
   * drawn after this call (colony_screen_draw_area_overlays) and correctly
   * layer on top. */
  colony_screen_draw_selection_box(
    framebuffer, origin_x, origin_y, COLONY_MINIMAP_GRID * tile, COLONY_MINIMAP_GRID * tile, 0
  );
}
