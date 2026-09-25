#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Split out of game_loop.c (2026-09-23) — code moved verbatim.
 *
 * Sections:
 *  - Map sprite blitting & zoom helpers
 *  - Europe screen rendering
 *  - Colony screen render, asset/palette loading, game_create/destroy
 *
 * Cross-file seams are declared in core/game_loop_internal.h.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif

#include "core/assets.h"
#include "core/ai.h"
#include "core/ai_euro.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_goals.h"
#include "core/ai_king.h"
#include "core/ai_popup.h"
#include "core/cheat_list_dialog.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/colony_screen.h"
#include "core/colony_yield.h"
#include "core/combat_strength.h"
#include "core/debug_atlas.h"
#include "core/closing.h"
#include "core/opening.h"
#include "core/declaration.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/fb.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/howmuch_dialog.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/name_entry_dialog.h"
#include "core/new_game.h"
#include "core/options_dialog.h"
#include "core/pedia.h"
#include "core/pik.h"
#include "core/pick_music.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/save_load_dialog.h"
#include "core/savegame.h"
#include "core/settings.h"
#include "core/sound.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/trade_screen.h"
#include "core/turn.h"
#include "core/ui_button.h"
#include "core/ui_colors.h"
#include "core/ui_drag.h"
#include "core/unit_chrome.h"
#include "core/unit_stack.h"
#include "core/units.h"
#include "core/woodcut.h"
#include "core/combat_analysis.h"
#include "core/version.h"
#include "platform/diagnostics.h"

#include "core/game_dialogs.h"
#include "core/game_loop_internal.h"

/* ===================== Map sprite blitting & zoom helpers (blit_map_sprite_offset .. game_pick_rng_seed) ===================== */


void blit_map_sprite_offset(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  int pixel_ox,
  int pixel_oy
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[sprite_index];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int ox = origin_x + screen_tile_x * tile_w + pixel_ox;
  const int oy = origin_y + screen_tile_y * tile_h + pixel_oy;
  ss_blit_sprite(sheet, sprite_index, framebuffer, ox, oy);
}

void blit_map_sprite_where_dest(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  uint8_t match_color
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[sprite_index];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int cox = (tile_w - tile->width) / 2;
  const int coy = (tile_h - tile->height) / 2;
  const int ox = origin_x + screen_tile_x * tile_w + cox;
  const int oy = origin_y + screen_tile_y * tile_h + coy;
  ss_blit_sprite_where_dest(sheet, sprite_index, framebuffer, ox, oy, match_color);
}

/*
 * Fill terrain into a fog dither mask's colour-0 holes ONLY — i.e. the pixels
 * the mask sprite itself painted 0, not every colour-0 pixel already in the
 * tile. A plain where-dest==0 fill also hits black pixels inside overlays
 * drawn earlier (resource sprites at the fog edge got neighbour-terrain
 * pixels splattered into their dark outlines — "corrupted palette").
 */
void blit_map_sprite_fill_mask_holes(
  const ColonizeSpriteSheet* fill_sheet,
  int fill_index,
  const ColonizeSpriteSheet* mask_sheet,
  int mask_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y
) {
  if (!fill_sheet || !mask_sheet || !framebuffer || !framebuffer->pixels ||
      fill_index < 0 || fill_index >= fill_sheet->sprite_count ||
      mask_index < 0 || mask_index >= mask_sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* fill = &fill_sheet->sprites[fill_index];
  const ColonizeSprite* mask = &mask_sheet->sprites[mask_index];
  if (!fill->pixels || !mask->pixels || fill->width <= 0 || mask->width <= 0) {
    return;
  }
  const int tx0 = origin_x + screen_tile_x * tile_w;
  const int ty0 = origin_y + screen_tile_y * tile_h;
  const int mox = (tile_w - mask->width) / 2;
  const int moy = (tile_h - mask->height) / 2;
  const int fox = (tile_w - fill->width) / 2;
  const int foy = (tile_h - fill->height) / 2;
  for (int my = 0; my < mask->height; ++my) {
    const int py = ty0 + moy + my;
    if (py < 0 || py >= framebuffer->height) {
      continue;
    }
    for (int mx = 0; mx < mask->width; ++mx) {
      const int px = tx0 + mox + mx;
      if (px < 0 || px >= framebuffer->width) {
        continue;
      }
      if (mask->pixels[my * mask->width + mx] != 0) {
        continue; /* only the mask's colour-0 holes take fill */
      }
      const int fx = px - (tx0 + fox);
      const int fy = py - (ty0 + foy);
      if (fx < 0 || fy < 0 || fx >= fill->width || fy >= fill->height) {
        continue;
      }
      const uint8_t c = fill->pixels[fy * fill->width + fx];
      if (c == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      framebuffer->pixels[py * framebuffer->width + px] = c;
    }
  }
}

void blit_map_sprite(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int screen_tile_x,
  int screen_tile_y,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* tile = &sheet->sprites[sprite_index];
  if (!tile->pixels || tile->width <= 0 || tile->height <= 0) {
    return;
  }
  const int cox = (tile_w - tile->width) / 2;
  const int coy = (tile_h - tile->height) / 2;
  blit_map_sprite_offset(
    sheet,
    sprite_index,
    framebuffer,
    screen_tile_x,
    screen_tile_y,
    tile_w,
    tile_h,
    origin_x,
    origin_y,
    cox,
    coy
  );
}

/*
 * VIEW Zoom In/Out/Level N (FUN_2b5a_0f92: DS:0x184 clamped 0..3). DOS redraws
 * the viewport at 16>>zoom px/tile so 15<<zoom × 12<<zoom tiles fit the same
 * 240×192 area (FUN_6ba1_000c: view_w=0xf<<zoom, view_h=0xc<<zoom, tile_px=
 * 0x10>>zoom). The port composites the wider tile grid at native 16px/tile
 * into an offscreen buffer, then nearest-neighbor-decimates it into the fixed
 * on-screen viewport — same visual result, different graphics pipeline.
 */

/*
 * VIEW ~Hidden Terrain (H): brief pause between each of DOS's three peel
 * passes before auto-advancing to the next. Equivalent-information UI
 * convenience, not a timed DOS value — see docs.
 */

int game_map_zoom_clamp(int zoom) {
  if (zoom < 0) {
    return 0;
  }
  if (zoom > MAP_ZOOM_MAX) {
    return MAP_ZOOM_MAX;
  }
  return zoom;
}

/* Nominal viewport size in tiles at this zoom tier; DOS 0xf<<zoom / 0xc<<zoom. */
void game_map_zoom_view_size(int zoom, int* out_cols, int* out_rows) {
  zoom = game_map_zoom_clamp(zoom);
  if (out_cols) {
    *out_cols = MAP_VIEW_TILE_COLS << zoom;
  }
  if (out_rows) {
    *out_rows = MAP_VIEW_TILE_ROWS << zoom;
  }
}

/* On-screen pixels per tile at this zoom tier; DOS 0x10>>zoom. */
int game_map_zoom_tile_px(int zoom) {
  return MAP_ZOOM_NATIVE_TILE >> game_map_zoom_clamp(zoom);
}

void game_map_zoom_set(ColonizeGameState* game, int zoom) {
  if (!game) {
    return;
  }
  game->map_zoom = game_map_zoom_clamp(zoom);
}


/* CLI --seed / settings.json seed: fixed campaign RNG (DOS DS:0x83a6 timer
 * word; VR_SEED = 100). 0 is valid when rng_seed_set. */
uint32_t game_pick_rng_seed(const ColonizeGameState* game, uint32_t fallback) {
  if (game && game->config.rng_seed_set) {
    return game->config.rng_seed;
  }
  return fallback ? fallback : 1u;
}
/* ===================== Europe screen rendering (europe_draw_box_border .. render_europe_screen) ===================== */



static void europe_draw_box_border(
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  int h,
  uint8_t color
) {
  /* Same edge order fb_rect_outline uses (top, bottom, left, right), so the
   * corner pixels still belong to the vertical edges. */
  fb_rect_outline(fb, x, y, w, h, color, color);
}

/* Two-line header + ship icons inside an Expected/Bound/Loading water box. */
/*
 * @UNIT type for unit_chrome's orders-box corner (bugs.md #419). Must use the
 * SAME by-name fallback the sprite lookup above uses: europe_purchase() files
 * a freshly bought hull with type_index = -1 ("resolved by name in game_loop /
 * caller"), and feeding that -1 straight to unit_chrome_corner_for_type made
 * it miss the 0x0d..0x12 ship range and fall through to BOTTOM_RIGHT — DOS
 * FUN_112b_01ba puts Caravel/Merchantman (0x0d/0x0e) TOP_LEFT and
 * Galleon..Man-O-War (0x0f..0x12) TOP_RIGHT. A Merchantman bought in Europe
 * is the commonest way to see it (the starting Caravel sails in from the map
 * with its type already resolved), which is why the box sat off the stern on
 * that hull only.
 */
static int europe_ship_display_type(const ColonizeUnitPool* units, const EuropeHarborShip* ship) {
  if (!units || !ship) {
    return -1;
  }
  if (ship->type_index >= 0) {
    return ship->type_index;
  }
  return units_find_type(units, ship->name);
}

/* The icon sprite of that same resolved type — audit GL-4: this used to be a
 * second copy of the type-index-or-find-by-name resolution above (and GL-3: a
 * verbatim twin of europe.c's static europe_ship_icon_sprite). */
int europe_ship_icon_sprite(const ColonizeUnitPool* units, const EuropeHarborShip* ship) {
  const ColonizeUnitType* ut = units_type(units, europe_ship_display_type(units, ship));
  return ut ? ut->icon_sprite : -1;
}

static void europe_render_transit_box(
  const ColonizeGameState* game,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int box_x,
  int box_y,
  int box_w,
  int box_h,
  const char* header,
  const EuropeHarborShip* ships,
  int count,
  int selected_index
) {
  if (!framebuffer) {
    return;
  }
  const int line_h = font ? (font->max_height > 0 ? (int)font->max_height + 2 : 8) : 8;
  font_draw_text(font, framebuffer, box_x + 2, box_y + 2, header, EUROPE_TEXT_GREEN);

  if (!game || !game->unit_icons_ok || !game->units_ok || count <= 0 || !ships) {
    return;
  }
  EuropeIconFlow f;
  if (!europe_icon_flow_begin(&f, box_x, box_y, box_w, box_h, line_h)) {
    return;
  }
  const ColonizePalette* pal =
    (game->europe_ok && game->europe.background.has_palette) ? &game->europe.background.palette
                                                              : NULL;

  for (int i = 0; i < count; ++i) {
    const int sprite = europe_ship_icon_sprite(&game->units, &ships[i]);
    if (sprite < 0 || sprite >= game->unit_icons.sprite_count) {
      continue;
    }
    int sw = 0;
    int sh = 0;
    europe_icon_flow_size(&game->unit_icons, sprite, &sw, &sh);
    if (!europe_icon_flow_place(&f, sw, sh)) {
      break;
    }
    unit_chrome_blit_unit_for_palette(
      framebuffer,
      font,
      &game->unit_icons,
      sprite,
      f.x,
      f.y,
      europe_ship_display_type(&game->units, &ships[i]),
      game->human_nation,
      UNITS_ORDER_NONE,
      ships[i].cargo_count > 0,
      false,
      pal
    );
    if (i == selected_index) {
      /* bugs.md: same fixed 18x18 cell frame as every other unit selection. */
      int fx = 0;
      int fy = 0;
      int fw = 0;
      int fh = 0;
      unit_chrome_selection_frame(f.x, f.y, sw, sh, &fx, &fy, &fw, &fh);
      europe_draw_box_border(framebuffer, fx, fy, fw, fh, 14);
    }
    europe_icon_flow_advance(&f, sw);

    /*
     * Everyone riding along shows next to their ship — bugs.md: "Colonists
     * embarked on ships arriving from or traveling to the new world on the
     * European Status need to be visible alongside ships." They are still
     * cargo, so they get no selection frame of their own — the ship is what
     * the player clicks — but they do carry the Sentry letter: a passenger is
     * a sentried unit (DOS FUN_38fd_0718 stamps orders 1 on every dock unit,
     * and boarding preserves it), so "no orders" was wrong (bugs.md).
     */
    for (int c = 0; c < ships[i].cargo_count && c < EUROPE_SHIP_CARGO_MAX; ++c) {
      const int pax_type = europe_pax_type_index(&game->units, ships[i].cargo_types[c]);
      const int pax_sprite =
        europe_passenger_icon_sprite(&game->units, pax_type, ships[i].cargo_professions[c]);
      if (pax_sprite < 0 || pax_sprite >= game->unit_icons.sprite_count) {
        continue;
      }
      int pw = 0;
      int ph = 0;
      europe_icon_flow_size(&game->unit_icons, pax_sprite, &pw, &ph);
      if (!europe_icon_flow_place(&f, pw, ph)) {
        break;
      }
      unit_chrome_blit_unit_for_palette(
        framebuffer,
        font,
        &game->unit_icons,
        pax_sprite,
        f.x,
        f.y,
        pax_type,
        game->human_nation,
        UNITS_ORDER_SENTRY,
        false,
        false,
        pal
      );
      europe_icon_flow_advance(&f, pw);
    }
  }
}

/*
 * Confirm the highlighted row of whichever Europe menu is open. The dock menu
 * goes through europe_dock_menu_apply_selection with the unit pool so an
 * @ARMOPTIONS row that re-types the immigrant (armed, equipped, blessed) also
 * moves its (236,236) mirror unit; everything else is the plain confirm.
 */
bool game_europe_menu_confirm(ColonizeGameState* game) {
  if (!game) {
    return false;
  }
  EuropeScreen* eu = &game->europe;
  if (eu->menu == EUROPE_MENU_DOCK) {
    const bool ok = europe_dock_menu_apply_selection_ex_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(game->units_ok ? &game->units : NULL), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL), .europe=(EuropeScreen*)(eu)}, game->human_nation);
    if (ok) {
      europe_menu_close(eu);
    }
    return ok;
  }
  /* FUN_38fd_41ce raw 64399-64403: an unaffordable Train row is greyed
   * (FUN_291f_01b6(list, row, 1)) and inert — the dialog stays up and no
   * "need gold" answer exists in DOS. bugs.md #566. */
  if (eu->menu == EUROPE_MENU_TRAIN && eu->menu_selection > 0 &&
      !europe_train_affordable(eu, eu->menu_selection - 1)) {
    return false;
  }
  /* FUN_38fd_4b50 raw 64858-64862: an unaffordable Purchase row is greyed
   * and inert the same way — no "need gold" answer exists in DOS.
   * bugs.md #754. */
  if (eu->menu == EUROPE_MENU_PURCHASE && !eu->purchase_confirming && eu->menu_selection > 0 &&
      !europe_purchase_affordable(eu, eu->menu_selection - 1)) {
    return false;
  }
  /* The RECRUIT row's pool refill is a `46d4` roll DOS takes off the shared
   * game stream — hand the real rng down (smell audit 2026-09-10 G5). */
  return europe_menu_confirm_ex(eu, &game->move_rng);
}

/*
 * Geometry of the RECRUIT/TRAIN/PURCHASE/DOCK wood popup. Shared by the
 * renderer and the click hit-test so a row the player sees and the row a
 * click lands on cannot drift apart. bugs.md: those three popups were
 * keyboard-only because the Europe input path swallowed every click while a
 * menu was open.
 */
/*
 * DOS builds all four of these through the one generic list dialog
 * (FUN_291f_0182 -> FUN_6f74_32a4), which takes its font from the far pointer
 * at DS:0x1f9e/0x1fa0. That global's standing value is DS:0x268a — FONTINTR;
 * startup sets it there (75c2:2306) and the one place that swaps it to
 * FONTTINY (DS:0x89e) puts FONTINTR back straight after. So Recruit, Train,
 * Purchase and the dock-orders menu are FONTINTR, not the FONTSMAL this port
 * was drawing them in (bugs.md). Same getter feeds the layout and the
 * renderer so the row pitch the hit-test uses is the row pitch drawn.
 */
static const ColonizeFont* europe_menu_font(const ColonizeGameState* game) {
  if (!game) {
    return NULL;
  }
  /* GAME.TXT @KINGRECRUIT (the Train dialog) carries @smallfont — DOS swaps
   * the list dialog to FONTTINY for it (bugs.md: "improper fonts"); the
   * other three stay on FONTINTR, the generic dialog font. */
  if (game->europe.menu == EUROPE_MENU_TRAIN && game->colony_font_ok) {
    return &game->colony_font;
  }
  if (game->intro_font_ok) {
    return &game->intro_font;
  }
  return game->menu_font_ok ? &game->menu_font : NULL;
}

/*
 * GAME.TXT section that carries this menu's prose — and, on its @width line,
 * the frame width DOS lays the list dialog out at.
 */
static const char* europe_menu_section(int menu) {
  switch (menu) {
    case EUROPE_MENU_RECRUIT:
      return "RECRUIT";
    case EUROPE_MENU_TRAIN:
      return "KINGRECRUIT";
    case EUROPE_MENU_PURCHASE:
      return "PURCHASE";
    case EUROPE_MENU_DOCK:
      return "EUROPEARM";
    default:
      return NULL;
  }
}

/*
 * Title prose for the open menu, GAME.TXT verbatim with the {highlight}
 * braces kept (drawn in the highlight ink, as DOS does). %NUMBER0 in
 * @RECRUIT is the passage price.
 */
static void europe_menu_title_prose(const ColonizeGameState* game, char* buf, size_t n) {
  const EuropeScreen* eu = &game->europe;
  /* @REALLYBUY confirm (bugs.md #753): "Purchase %STRING0 for %NUMBER0$?" */
  if (eu->menu == EUROPE_MENU_PURCHASE && eu->purchase_confirming) {
    const int pi = eu->purchase_confirm_index;
    const char* name = (pi >= 0 && pi < eu->purchase_count) ? eu->purchase[pi].name : "";
    PopupMsgTokens tok = {0};
    tok.string0 = name;
    tok.number0 = eu->purchase_confirm_cost;
    tok.has_number0 = true;
    popup_msg_fill(
      &game->messages, "REALLYBUY", &tok, "", buf, n
    );
    return;
  }
  const char* section = europe_menu_section(eu->menu);
  const char* fallback = "";
  switch (eu->menu) {
    case EUROPE_MENU_RECRUIT:
    case EUROPE_MENU_TRAIN:
    case EUROPE_MENU_PURCHASE:
    case EUROPE_MENU_DOCK:
      fallback = "";
      break;
    default:
      buf[0] = '\0';
      return;
  }
  /* Join the section's prose lines into one flowing paragraph. */
  char prose[512];
  prose[0] = '\0';
  size_t used = 0;
  const ColonizeMsgSection* sec = assets_msg_find(&game->messages, section);
  for (int i = 0; sec && i < sec->line_count; ++i) {
    const char* ln = sec->lines[i];
    if (popup_msg_is_directive(ln) || !ln[0]) {
      continue;
    }
    if (used > 0 && used + 1 < sizeof(prose)) {
      prose[used++] = ' ';
      prose[used] = '\0';
    }
    const size_t len = strlen(ln);
    if (used + len < sizeof(prose)) {
      memcpy(prose + used, ln, len + 1);
      used += len;
    }
  }
  const char* src = prose[0] ? prose : fallback;
  /* Substitute %NUMBER0 (passage) but KEEP the braces for the ink toggle. */
  size_t o = 0;
  for (size_t i = 0; src[i] && o + 1 < n;) {
    if (strncmp(src + i, "%NUMBER0", 8) == 0) {
      char num[16];
      const int len = snprintf(num, sizeof(num), "%d", eu->recruit_passage);
      if (o + (size_t)len < n) {
        memcpy(buf + o, num, (size_t)len);
        o += (size_t)len;
      }
      i += 8;
      continue;
    }
    buf[o++] = src[i++];
  }
  buf[o] = '\0';
}

/* DOS list-dialog text carries a 1px black drop shadow (recruit/train/
 * purchase screenshots — bugs.md: "shading, mostly"). */
static void europe_menu_draw_shadowed(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  /* bugs.md #280: thin letters like every other dialog caption. */
  font_draw_text_unbold(font, fb, x + 1, y + 1, text, 0);
  font_draw_text_unbold(font, fb, x, y, text, color);
}

/* Pixel width of a string with the {} markup skipped. */

#define EUROPE_PROSE_MAX_LINES 24
#define EUROPE_PROSE_LINE_LEN 256

/*
 * Word-wrap and (when fb != NULL) draw prose at (x,y) within w, returning the
 * height used. {…} spans take the highlight ink and the state carries across
 * words and lines, like DOS's dialog text writer (FUN_6f74_0538); the 1px
 * black drop shadow and the unbold face are the same ones every other
 * FONTINTR caption uses (bugs.md #280).
 *
 * Audit GL-8: the wrap engine and the brace-run drawer were private copies of
 * popup_wrap_text / popup_draw_text_markup. They are now those two — verified
 * pixel- and height-identical over the four GAME.TXT strings this screen shows
 * (@RECRUIT, @KINGRECRUIT, @PURCHASE, the dock fallback), the empty string,
 * all four shipped fonts and every width from 60 to 220.
 */
static int europe_draw_prose(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  const char* text,
  uint8_t base_color,
  uint8_t hi_color
) {
  if (!font || !text || w <= 0) {
    return 0;
  }
  const int line_h = font->max_height + 1;
  char lines[EUROPE_PROSE_MAX_LINES][EUROPE_PROSE_LINE_LEN];
  const int n =
    popup_wrap_text(font, text, lines[0], sizeof(lines[0]), NULL, EUROPE_PROSE_MAX_LINES, w);
  bool hi = false;
  for (int i = 0; i < n; ++i) {
    popup_draw_text_markup(font, fb, x, y + i * line_h, lines[i], base_color, hi_color, true,
                           true, &hi);
  }
  /* An all-blank body still reserves one line, the way the old word loop did. */
  return (n > 0 ? n : 1) * line_h;
}

/*
 * MSS popup decoration for the open Europe menu (DOS DS:0x1f5e latch).
 *
 * DOS builds Recruit and Purchase through the same generic list dialog as
 * every GAME.TXT popup (FUN_291f_0182 -> FUN_6f74_32a4, shown by
 * FUN_291f_016a -> FUN_6f74_2580), and latches the decoration index right
 * before it — asm PUSH/LEA pairs, since Ghidra drops these args:
 *
 *   38fd:4948  MOV word [0x1f5e],0x2 / LEA BX,[0x87c] / LEA AX,[0x1109]
 *              -> FUN_38fd_4884(0,0), tag @RECRUIT       -> MSS2 courtier
 *   38fd:4b5d  MOV word [0x1f5e],0x2 / LEA BX,[0x87c] / LEA AX,[0x1111]
 *              -> FUN_38fd_4b50,      tag @PURCHASE      -> MSS2 courtier
 *
 * The two other 4884 arms are already wired through ai_popup (units.c):
 * 38fd:4910 latches 3 with tag 0x10f1 = @LOSTCITY0 (Fountain of Youth) and
 * 38fd:4924 latches 4 with tag 0x10fb = @RECRUITCHOOSE (Brewster). The Train
 * dialog (@KINGRECRUIT) and the dock @ARMOPTIONS menu latch nothing, and DOS
 * accordingly shows them undecorated (original_screenshots/europe/train.png).
 * FUN_6f74_2580 clears all three latches on exit (LAB_6f74_3018), so the
 * decoration never leaks to the next dialog.
 */
static int europe_menu_mss_index(const ColonizeGameState* game) {
  if (!game) {
    return -1;
  }
  switch (game->europe.menu) {
    case EUROPE_MENU_RECRUIT:
    case EUROPE_MENU_PURCHASE:
      return 2;
    default:
      return -1;
  }
}

const ColonizeSpriteSheet* europe_menu_decoration(const ColonizeGameState* game) {
  const int mss = europe_menu_mss_index(game);
  if (mss < 0) {
    return NULL;
  }
  const ColonizeSpriteSheet* sheet = ai_popup_decoration_sheet(mss, -1);
  if (!sheet || sheet->sprite_count <= 0 || !sheet->sprites[0].pixels) {
    return NULL;
  }
  return sheet;
}

typedef struct EuropeMenuLayout {
  int rows;
  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int inner_x;
  int inner_y;
  int inner_w;
  int title_h;
  int list_y0;
  int line_h;
  int pad;
  /* MSS decoration, NULL when this menu has none or it does not fit. */
  const ColonizeSpriteSheet* graphic;
  int graphic_x;
  int graphic_y;
} EuropeMenuLayout;

static bool europe_menu_layout(
  const ColonizeGameState* game,
  int fb_w,
  int fb_h,
  EuropeMenuLayout* out
) {
  if (!game || !out || fb_w <= 0 || fb_h <= 0) {
    return false;
  }
  const EuropeScreen* eu = &game->europe;
  const ColonizeFont* font = europe_menu_font(game);
  out->line_h = font ? (font->max_height + 2) : 9;
  out->pad = 4;
  switch (eu->menu) {
    case EUROPE_MENU_RECRUIT:
      out->rows = 1 + EUROPE_POOL_SIZE;
      break;
    case EUROPE_MENU_TRAIN:
      out->rows = 1 + eu->train_count;
      break;
    case EUROPE_MENU_PURCHASE:
      /* @REALLYBUY confirm swaps the list for its own 2-row Yes/No
       * (bugs.md #753). */
      out->rows = eu->purchase_confirming ? 2 : 1 + eu->purchase_count;
      break;
    case EUROPE_MENU_DOCK:
      /* @ARMOPTIONS rows this immigrant actually gets, no "None" header —
       * DOS's own last row is "No changes." (bugs.md). */
      out->rows = eu->dock_menu_count;
      break;
    default:
      return false;
  }
  /*
   * Width comes from the menu section's own GAME.TXT @width directive, the
   * same directive every other popup honours (popup_msg_section_width):
   * @RECRUIT / @KINGRECRUIT / @PURCHASE / @EUROPEARM all read @width=190.
   * The port's hardcoded 220 was 30px too wide (that is @RECRUITCHOOSE's
   * width, the Brewster pick dialog, not these four).
   */
  const ColonizeMsgSection* width_sec =
    assets_msg_find(&game->messages, europe_menu_section(eu->menu));
  const int msg_w = popup_msg_section_width(width_sec);
  int dialog_w = msg_w > 0 ? msg_w : 220;
  if (dialog_w > fb_w - 8) {
    dialog_w = fb_w - 8;
  }
  out->inner_w = dialog_w - POPUP_FRAME_INSET * 2;
  /* Multi-line GAME.TXT prose title; measure with the menu's own font so
   * the row hit-test can never drift from the drawn rows. */
  char prose[512];
  europe_menu_title_prose(game, prose, sizeof(prose));
  out->title_h = font ? europe_draw_prose(
                          font, NULL, 0, 0, out->inner_w - out->pad * 2, prose, 10, 14
                        )
                      : out->line_h;
  /* +line_h at the bottom: the DOS "(F1 for Help)" footer line. */
  int dialog_h = POPUP_FRAME_INSET * 2 + out->pad + out->title_h + 2 +
                 out->rows * out->line_h + out->line_h + out->pad;
  if (dialog_h > fb_h - 8) {
    dialog_h = fb_h - 8;
  }
  out->dialog_w = dialog_w;
  out->dialog_h = dialog_h;
  out->dialog_x = (fb_w - dialog_w) / 2;
  /*
   * DOS auto-places the list dialog like every other one (FUN_6f74_14c6 @
   * OVL24 0x16cb-0x16f6: 160 - w/2, 100 - h/2) — the port's fixed y=16 was a
   * stand-in. Both reference shots agree: recruit.png's 102px-tall dialog
   * lands at y=88 with the MSS2 figure above it, train.png's 180px-tall one
   * (no figure) at y=10 = (200-180)/2.
   */
  out->dialog_y = (fb_h - dialog_h) / 2;

  /*
   * Decoration placement, same data-driven rule as ai_popup_render: the
   * sprite stands ABOVE the dialog with its bottom overlapping the dialog top
   * by the sheet's place_offset_y; place_mode 0 = at the dialog's left edge
   * (horizontal overlap place_offset_x), 1 = centred over it, 2 = at the
   * right edge. The pair is centred vertically as a unit, and DOS drops the
   * sprite outright when the two together reach the screen height.
   */
  out->graphic = europe_menu_decoration(game);
  out->graphic_x = 0;
  out->graphic_y = 0;
  if (out->graphic) {
    const int gw = out->graphic->sprites[0].width;
    const int gh = out->graphic->sprites[0].height;
    int ov_y = out->graphic->place_offset_y;
    if (ov_y > gh) {
      ov_y = gh;
    }
    const int total_h = gh - ov_y + dialog_h;
    if (total_h >= fb_h) {
      out->graphic = NULL; /* DOS: too tall together → no decoration. */
    } else {
      int top = (fb_h - total_h) / 2;
      if (top < 0) {
        top = 0;
      }
      out->graphic_y = top;
      out->dialog_y = top + gh - ov_y;
      if (out->graphic->place_mode == 1) {
        out->graphic_x = (fb_w - gw) / 2;
      } else {
        int ov_x = out->graphic->place_offset_x;
        int total_w = dialog_w + gw - ov_x;
        if (total_w > fb_w) {
          ov_x += total_w - fb_w; /* DOS widens the overlap instead. */
          total_w = fb_w;
        }
        const int x0 = (fb_w - total_w) / 2;
        if (out->graphic->place_mode == 2) {
          out->dialog_x = x0;
          out->graphic_x = x0 + dialog_w - ov_x;
        } else {
          out->graphic_x = x0;
          out->dialog_x = x0 + gw - ov_x;
        }
      }
    }
  }
  if (out->dialog_y < 0) {
    out->dialog_y = 0;
  }
  out->inner_x = out->dialog_x + POPUP_FRAME_INSET;
  out->inner_y = out->dialog_y + POPUP_FRAME_INSET;
  out->list_y0 = out->inner_y + out->pad + out->title_h + 2;
  return out->inner_w > 0;
}

/* Row under (mx,my) in the open Europe menu, or -1 when the click misses it. */
int europe_menu_row_at(const ColonizeGameState* game, int mx, int my) {
  EuropeMenuLayout lay;
  if (!europe_menu_layout(game, 320, 200, &lay)) {
    return -1;
  }
  if (mx < lay.inner_x || mx >= lay.inner_x + lay.inner_w || my < lay.list_y0) {
    return -1;
  }
  const int row = (my - lay.list_y0) / lay.line_h;
  return (row >= 0 && row < lay.rows) ? row : -1;
}

/*
 * DOS-LITERAL FUN_38fd_41ce raw 64391-64396 — the Train/Purchase row's cost
 * suffix. DOS composes the row as @JOB column 1 + ' ' + DS:0x107f ("|   ",
 * the column separator) + DS:0x2dd4 + ' ' + the number + DS:0x2dd6. The two
 * 0x2dxx slots are entries 13 and 14 of the @MISC pointer table loaded at
 * DS:0x2dba (raw 121284-121287 fills 0xdd slots from the "MISC" tag), i.e.
 * LABELS.TXT @MISC lines 13 "(Cost:" and 14 ")". The port draws the suffix in
 * its own column, so only the parenthesised part is composed here.
 * bugs.md #602.
 */
static void europe_menu_cost_suffix(char* out, size_t cap, int amount) {
  snprintf(out, cap, "%s %d%s",
           reports_misc_display_word(13, ""), amount, reports_misc_display_word(14, ""));
}

/* RECRUIT/TRAIN/PURCHASE/DOCK wood popup — chrome via popup_draw. */
static void europe_render_menu_popup(
  const ColonizeGameState* game,
  ColonizeFramebuffer8* framebuffer
) {
  const EuropeScreen* eu = &game->europe;
  if (eu->menu == EUROPE_MENU_NONE) {
    return;
  }
  const ColonizeFont* font = europe_menu_font(game);
  EuropeMenuLayout lay;
  if (!europe_menu_layout(game, framebuffer->width, framebuffer->height, &lay)) {
    return;
  }
  const int line_h = lay.line_h;
  const int pad = lay.pad;
  const int rows = lay.rows;

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  popup_draw(
    framebuffer,
    lay.dialog_x,
    lay.dialog_y,
    lay.dialog_w,
    lay.dialog_h,
    (eu->wood_tile_ok) ? &eu->wood_tile : NULL,
    &colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  if (inner_w <= 0 || inner_h <= 0) {
    return;
  }

  /*
   * DOS list-dialog styling (original_screenshots/europe/recruit|train|
   * purchase.png): the GAME.TXT prose in green with {highlight} words in
   * the highlight ink, tan rows with the cost right-aligned as
   * "(Cost: N)", a grey row when the treasury cannot cover it, and the
   * "(F1 for Help)" footer bottom-right in green.
   */
  char prose[512];
  europe_menu_title_prose(game, prose, sizeof(prose));
  europe_draw_prose(
    font, framebuffer, inner_x + pad, inner_y + pad, inner_w - pad * 2, prose, 10, 14
  );

  for (int i = 0; i < rows; ++i) {
    const int row_y = lay.list_y0 + i * line_h;
    if (row_y + line_h > framebuffer->height) {
      break;
    }
    if (i == eu->menu_selection) {
      fb_fill_rect(framebuffer, inner_x + 1, row_y - 1, inner_w - 2, line_h, 138);
    }
    char label[72];
    char cost[24];
    cost[0] = '\0';
    uint8_t color = 14;
    if (eu->menu == EUROPE_MENU_DOCK) {
      if (i < 0 || i >= eu->dock_menu_count) {
        continue;
      }
      snprintf(label, sizeof(label), "%s", eu->dock_menu_label[i]);
      /* DOS greys a row the treasury cannot cover, and omits the ones it
       * disabled outright (europe_build_dock_menu already dropped those). */
      color = eu->dock_menu_greyed[i] ? 8 : 15;
    } else if (eu->menu == EUROPE_MENU_PURCHASE && eu->purchase_confirming) {
      /* @REALLYBUY Yes/No, GAME.TXT order verbatim (bugs.md #753): row 0
       * "Yes", row 1 "No" (raw 64874 `iVar6 == 1` = row 0). */
      char choice_buf[2][POPUP_MSG_CHOICE_LEN];
      const char* choice_labels[2];
      popup_msg_section_labels(
        &game->messages, "REALLYBUY", NULL, "", "", choice_buf, choice_labels
      );
      snprintf(label, sizeof(label), "%s", choice_labels[i]);
    } else if (i == 0) {
      const char* none = assets_msg_line_or(
        game->labels_ok ? &game->labels : NULL, "MISC", 3, ""
      );
      if (eu->menu == EUROPE_MENU_RECRUIT) {
        snprintf(label, sizeof(label), "(%s)", none);
      } else {
        snprintf(label, sizeof(label), "%s", none);
      }
    } else if (eu->menu == EUROPE_MENU_RECRUIT) {
      /* Same choice source as the Brewster / Fountain of Youth pick popups
       * (DOS 4884 draws one list for all three). */
      snprintf(label, sizeof(label), "%s", europe_pool_label(eu, i - 1));
      if (eu->gold < eu->recruit_passage) {
        color = 8; /* cannot pay the passage */
      }
    } else if (eu->menu == EUROPE_MENU_TRAIN) {
      const EuropeTrainOption* t = &eu->train[i - 1];
      snprintf(label, sizeof(label), "%s", t->expert_name);
      europe_menu_cost_suffix(cost, sizeof(cost), t->cost);
      color = (eu->gold >= t->cost) ? 14 : 8;
    } else if (eu->menu == EUROPE_MENU_PURCHASE) {
      const EuropePurchaseOption* p = &eu->purchase[i - 1];
      /* Artillery escalates +100 per prior purchase (FUN_38fd_4b50). */
      const int row_cost = europe_purchase_cost(eu, i - 1);
      snprintf(label, sizeof(label), "%s", p->name);
      europe_menu_cost_suffix(cost, sizeof(cost), row_cost);
      color = (eu->gold >= row_cost) ? 14 : 8;
    }
    /* ARMOPTIONS rows carry {} emphasis ("Arm with {Muskets}…"); a greyed
     * row stays all grey like DOS's disabled ink. */
    popup_draw_text_markup(
      font, framebuffer, inner_x + pad + 6, row_y, label, color,
      color == 8 ? (uint8_t)8 : (uint8_t)14, true, true, NULL
    );
    if (cost[0]) {
      const int cw = font_text_width(font, cost);
      europe_menu_draw_shadowed(
        font, framebuffer, inner_x + inner_w - pad - cw, row_y, cost, color
      );
    }
  }

  {
    const char* f1 = reports_misc_display_word(189, "");
    const int fw = font_text_width(font, f1);
    europe_menu_draw_shadowed(
      font, framebuffer, inner_x + inner_w - pad - fw,
      lay.list_y0 + rows * line_h, f1, 10
    );
  }

  /*
   * The MSS figure goes LAST. DOS FUN_6f74_248e blits the decoration sprite
   * before the wood frame only when the portrait latch DS:0x1f5c is set;
   * with 0x1f5c < 0 and 0x1f5e set — which is exactly these two menus — the
   * blit runs after everything, so the courtier's hands overlap the frame
   * (original_screenshots/europe/recruit.png, purchase.png).
   */
  if (lay.graphic) {
    ss_blit_sprite(lay.graphic, 0, framebuffer, lay.graphic_x, lay.graphic_y);
  }
}


static int europe_harbor_open_holds(const ColonizeUnitPool* units, const EuropeHarborShip* ship) {
  if (!units || !ship) {
    return 0;
  }
  int ti = ship->type_index;
  if (ti < 0) {
    ti = units_find_type(units, ship->name);
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  if (!ut || ut->cargo <= 0) {
    return 0;
  }
  return ut->cargo > EUROPE_HOLD_MAX ? EUROPE_HOLD_MAX : ut->cargo;
}

/*
 * The howmuch + name-entry modal overlay pair, drawn on top of whatever screen
 * is underneath (audit GL-13: this block was written out three times and the
 * map-branch copy used a different font fallback, so the same dialog came up
 * in two different fonts depending on the screen below it).
 *
 * DOS draws both through the generic dialog writer in FONTINTR — the same font
 * the map popups use, which is why the map branch's comment calls out "not
 * FONTTINY HUD". intro_font IS FONTINTR, so it is the first choice everywhere;
 * the rest of the chain only matters when FONTINTR failed to load, and the
 * menu font is the closer stand-in than a screen's own FONTTINY. last_resort
 * is the caller's screen font, used when neither loaded.
 *
 * howmuch and name_entry are never open at the same time (each opener closes
 * the other modal), so the draw order between them is not observable.
 */
COLONIZE_INTERNAL void game_render_modal_overlays(
  const ColonizeGameState* game,
  const ColonizeFont* last_resort,
  ColonizeFramebuffer8* framebuffer
) {
  if (!game->howmuch.open && !game->name_entry.open) {
    return;
  }
  const ColonizeSpriteSheet* wood =
    (game->map_panel_ok && game->map_panel.wood_ok) ? &game->map_panel.wood_tile : NULL;
  ColonizePopupColors popup_cols;
  popup_colors_from_ui(&popup_cols);
  const ColonizeFont* popup_font = game->intro_font_ok
    ? &game->intro_font
    : (game->menu_font_ok ? &game->menu_font : last_resort);
  if (game->howmuch.open) {
    howmuch_render(
      (HowmuchDialog*)&game->howmuch, popup_font, wood, &popup_cols, COLONIZE_COL_BASIC,
      COLONIZE_COL_SELECT, framebuffer
    );
  }
  if (game->name_entry.open) {
    name_entry_render(
      (NameEntryDialog*)&game->name_entry, popup_font, wood, &popup_cols, COLONIZE_COL_BASIC,
      COLONIZE_COL_SELECT, framebuffer
    );
  }
}

/*
 * render_europe_screen stages, painted in DOS order: chrome, then the
 * ship holds and dock, then the market row and the three menu buttons.
 */

/* EUROPE.PIK background, the wood top bar with the year/season line, and the
 * three transit boxes (outbound / in-port / inbound). */
static void render_europe_chrome(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, const ColonizeFont* font,
  const EuropeScreen* eu
) {
  if (game->europe_ok && eu->background_ok) {
    pik_blit(&eu->background, framebuffer, 0, 0);
  }

  /* Colony-style wood top bar + black separator; info centered. */
  if (eu->wood_tile_ok) {
    map_panel_tile_rect(&eu->wood_tile, 0, 0, framebuffer->width, EUROPE_TOP_BAR_H, framebuffer);
  } else {
    fb_fill_rect(framebuffer, 0, 0, framebuffer->width, EUROPE_TOP_BAR_H, 6);
  }
  fb_fill_rect(framebuffer, 0, EUROPE_TOP_SEPARATOR_Y, framebuffer->width, 1, 0);

  char line[192];
  if (game->game_year > 0) {
    char date[32];
    turn_format_date(game->game_year, game->game_autumn, date, sizeof(date));
    snprintf(
      line,
      sizeof(line),
      "%s, %s.  %s.  Tax: %d%%  Gold: %d$",
      eu->port_city,
      eu->nation_name,
      date,
      eu->tax_percent,
      eu->gold
    );
  } else {
    snprintf(
      line,
      sizeof(line),
      "%s, %s.  Tax: %d%%  Gold: %d$",
      eu->port_city,
      eu->nation_name,
      eu->tax_percent,
      eu->gold
    );
  }
  {
    /*
     * DOS status line owns this strip while one is armed: FUN_38fd_23c4
     * composes the sale into DS:0x2d54 and FUN_1009_02cc paints it INSTEAD of
     * the strip's normal content, in the arm kind's ink (bugs.md #376).
     */
    const char* bar = ai_popup_bar_message(&game->ai_popups);
    const char* text = bar ? bar : line;
    const uint8_t ink =
      bar ? ai_popup_bar_message_color(&game->ai_popups) : (uint8_t)EUROPE_TEXT_GREEN;
    const int tw = font_text_width(font, text);
    const int th = font ? (font->max_height > 0 ? (int)font->max_height : 6) : 7;
    const int tx = (framebuffer->width - tw) / 2;
    const int ty = (EUROPE_TOP_BAR_H - th) / 2;
    font_draw_text(font, framebuffer, tx > 0 ? tx : 0, ty > 0 ? ty : 1, text, ink);
  }

  /* Transit boxes: two-line headers + ship icons (not text lists). */
  char expected_header[32];
  snprintf(expected_header, sizeof(expected_header), "%s", reports_misc_display_word(9, ""));
  {
    char* sp = strchr(expected_header, ' ');
    if (sp) {
      *sp = '\n';
    }
  }
  europe_render_transit_box(
    game,
    font,
    framebuffer,
    EUROPE_EXPECTED_X,
    EUROPE_EXPECTED_Y,
    EUROPE_EXPECTED_W,
    EUROPE_EXPECTED_H,
    expected_header,
    eu->expected,
    eu->expected_ships,
    -1
  );

  snprintf(
    line,
    sizeof(line),
    "%s\n%s",
    reports_misc_display_word(10, ""),
    eu->colony_region[0] ? eu->colony_region : ""
  );
  europe_render_transit_box(
    game,
    font,
    framebuffer,
    EUROPE_BOUND_X,
    EUROPE_BOUND_Y,
    EUROPE_BOUND_W,
    EUROPE_BOUND_H,
    line,
    eu->bound,
    eu->bound_ships,
    -1
  );

  if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
    snprintf(line, sizeof(line), "Loading:\n%s", eu->harbor[eu->selected_harbor].name);
  } else {
    /* No selection: bare header, whether or not ships sit in the harbor. */
    snprintf(line, sizeof(line), "%s", "Loading:\n");
  }
  europe_render_transit_box(
    game,
    font,
    framebuffer,
    EUROPE_LOADING_X,
    EUROPE_LOADING_Y,
    EUROPE_LOADING_W,
    EUROPE_LOADING_H,
    line,
    eu->harbor,
    eu->harbor_ships,
    eu->selected_harbor
  );

}

/* The selected ship's commodity holds and the two dock-colonist quay rows. */
static void render_europe_holds_and_dock(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, const ColonizeFont* font,
  const EuropeScreen* eu
) {
  /* Commodity holds — same closed/open cover behavior as colony transport pane. */
  {
    const EuropeHarborShip* ship = NULL;
    int open_holds = 0;
    if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
      ship = &eu->harbor[eu->selected_harbor];
      open_holds = game->units_ok ? europe_harbor_open_holds(&game->units, ship) : 0;
    }
    if (ship && open_holds > 0) {
      for (int i = 0; i < open_holds; ++i) {
        const int x = EUROPE_HOLD_X + i * EUROPE_HOLD_PITCH;
        const int hy = EUROPE_HOLD_Y;
        const int amt = ship->hold_goods_amount[i];
        const int gtype = ship->hold_goods_type[i];
        if (amt > 0 && amt < 255 && gtype >= 0 && gtype < COLONIZE_CARGO_COUNT &&
            game->unit_icons_ok) {
          const int sprite =
            (amt >= EUROPE_CARGO_FULL ? EUROPE_CARGO_ICON_BASE : EUROPE_CARGO_GREY_BASE) + gtype;
          if (sprite < game->unit_icons.sprite_count) {
            const ColonizeSprite* sp = &game->unit_icons.sprites[sprite];
            const int ix = x + (EUROPE_HOLD_W - sp->width) / 2;
            ss_blit_sprite(&game->unit_icons, sprite, framebuffer, ix, hy);
          }
        }
      }
      for (int i = 0; i < ship->cargo_count; ++i) {
        const int slot = open_holds + i;
        if (slot >= EUROPE_HOLD_MAX) {
          break;
        }
        const int x = EUROPE_HOLD_X + slot * EUROPE_HOLD_PITCH;
        const int hy = EUROPE_HOLD_Y;
        const int pax_type = ship->cargo_types[i];
        if (game->unit_icons_ok && pax_type >= 0) {
          const ColonizeUnitType* ut = units_type(&game->units, pax_type);
          if (ut && ut->icon_sprite >= 0 && ut->icon_sprite < game->unit_icons.sprite_count) {
            ss_blit_sprite(&game->unit_icons, ut->icon_sprite, framebuffer, x, hy);
          }
        }
      }
    }
    if (game->unit_icons_ok && EUROPE_ICON_EMPTY_HOLD < game->unit_icons.sprite_count) {
      for (int i = open_holds; i < EUROPE_HOLD_MAX; ++i) {
        /* Cover #122 is 10×12; sit over the 9×12 interior plus its left border. */
        const int x = EUROPE_HOLD_X - 1 + i * EUROPE_HOLD_PITCH;
        const int y = EUROPE_HOLD_Y;
        ss_blit_sprite(&game->unit_icons, EUROPE_ICON_EMPTY_HOLD, framebuffer, x, y);
      }
    }
  }

  /* Dock colonists as unit sprites: two quay rows from (233,138)/(233,161). */
  if (game->units_ok && game->unit_icons_ok) {
    for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
      int dx = 0;
      int dy = 0;
      /* Two quay rows (DOS FUN_38fd_146c); slots past tier 1 are not drawn. */
      if (!europe_dock_slot_pos(i, &dx, &dy)) {
        break;
      }
      const int sprite = europe_dock_icon_sprite(&game->units, &eu->dock[i]);
      if (sprite >= 0 && sprite < game->unit_icons.sprite_count) {
        const ColonizeSprite* sp = &game->unit_icons.sprites[sprite];
        const int iw = sp->width > 0 ? sp->width : 16;
        const int ih = sp->height > 0 ? sp->height : 16;
        const int orders =
          eu->dock[i].sentry ? UNITS_ORDER_SENTRY : UNITS_ORDER_NONE;
        /*
         * The dock row lays every immigrant out side by side, so no one is
         * ever hidden behind anyone else: the "more units here" tab belongs
         * to a map stack and is always wrong here (bugs.md). Singular chrome.
         */
        const bool stacked = false;
        /* Box corner from the @UNIT display type, same rule the map runs
         * (bugs.md: the name-first lookup put every armed immigrant's box in
         * the Colonists corner). */
        int dtype = europe_dock_display_type_index(&game->units, &eu->dock[i]);
        if (dtype < 0) {
          dtype = 0;
        }
        unit_chrome_blit_unit_for_palette(
          framebuffer,
          font,
          &game->unit_icons,
          sprite,
          dx,
          dy,
          dtype,
          game->human_nation,
          orders,
          stacked,
          false,
          (game->europe_ok && game->europe.background.has_palette) ? &game->europe.background.palette : NULL
        );
        if (eu->menu == EUROPE_MENU_DOCK && eu->menu_dock_index == i) {
          int fx = 0;
          int fy = 0;
          int fw = 0;
          int fh = 0;
          unit_chrome_selection_frame(dx, dy, iw, ih, &fx, &fy, &fw, &fh);
          europe_draw_box_border(framebuffer, fx, fy, fw, fh, 14);
        }
      }
    }
  }
}

/* The 20x20 market cells and the RECRUIT / PURCHASE / TRAIN buttons. */
static void render_europe_market_and_buttons(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, const ColonizeFont* font,
  const EuropeScreen* eu
) {
  char line[192];
  /* Market: 20x20 cells sharing borders; selection lights the cell. */
  for (int i = 0; i < eu->cargo_count && i < EUROPE_CARGO_MAX; ++i) {
    const int mx = EUROPE_MARKET_X + i * EUROPE_MARKET_PITCH;
    if (mx + EUROPE_MARKET_CELL > framebuffer->width) {
      break;
    }
    const bool sel = (i == eu->selected_market);
    const int sprite = EUROPE_CARGO_ICON_BASE + i;
    if (game->unit_icons_ok && sprite < game->unit_icons.sprite_count) {
      const ColonizeSprite* sp = &game->unit_icons.sprites[sprite];
      const int ix = mx + (EUROPE_MARKET_CELL - sp->width) / 2;
      const int iy = EUROPE_MARKET_Y + 1;
      ss_blit_sprite(&game->unit_icons, sprite, framebuffer, ix, iy);
    }
    /* Boycotted cargo (ai_king.c tea-party / ai_diplo.c embargo): price text
     * in red — trading is blocked until the boycott lifts (europe_buy_cargo /
     * europe_sell_hold / europe_sell_unit_hold refuse it; see
     * europe_cargo_boycotted). Source: fandom Boycott (Col). */
    const bool boycotted = europe_cargo_boycotted_ex(
      eu, game->col1_ok ? &game->col1 : NULL, game->human_nation, i
    );
    /* DOS shows sell/buy = (euro_price − 1)/(euro_price + burden). */
    snprintf(line, sizeof(line), "%d/%d", europe_sell_price(eu, i), europe_buy_price(eu, i));
    {
      const int tw = font_text_width(font, line);
      const int th = font ? (font->max_height > 0 ? (int)font->max_height : 6) : 7;
      const int tx = mx + (EUROPE_MARKET_CELL - tw) / 2;
      const int ty = EUROPE_MARKET_Y + EUROPE_MARKET_CELL - th - 1;
      font_draw_text(font, framebuffer, tx, ty, line, boycotted ? 12 : 0);
    }
    if (sel) {
      europe_draw_box_border(
        framebuffer, mx, EUROPE_MARKET_Y, EUROPE_MARKET_CELL, EUROPE_MARKET_CELL + 1, 14
      );
    }
  }

  /* RECRUIT / PURCHASE / TRAIN — transparent beveled ALL-CAPS buttons. */
  {
    /* LABELS.TXT @EUROLABEL rows 0..2; '~' is the port's hotkey marker. */
    char k_btn_labels[3][24];
    for (int i = 0; i < 3; ++i) {
      snprintf(
        k_btn_labels[i], sizeof(k_btn_labels[i]), "~%s",
        assets_msg_line_or(game->labels_ok ? &game->labels : NULL, "EUROLABEL", i, "")
      );
    }
    UiButtonColors bc;
    bc.dark = EUROPE_BTN_DARK;
    bc.light = EUROPE_BTN_LIGHT;
    bc.text = 15;
    bc.hotkey = 14;
    for (int i = 0; i < 3; ++i) {
      int bw = EUROPE_BTN_W;
      int bh = EUROPE_BTN_H;
      ui_button_measure(font, k_btn_labels[i], &bw, &bh);
      if (bw < EUROPE_BTN_W) {
        bw = EUROPE_BTN_W;
      }
      ui_button_draw(
        font,
        framebuffer,
        EUROPE_BTN_X,
        EUROPE_BTN_Y + i * EUROPE_BTN_PITCH,
        bw,
        bh,
        k_btn_labels[i],
        &bc
      );
    }
  }
}

void render_europe_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer) {
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
  /* Main Europe chrome uses FONTTINY; the list popups use FONTINTR, DOS's own
   * dialog default (see europe_menu_font). */
  const ColonizeFont* font = game->colony_font_ok ? &game->colony_font
    : (game->menu_font_ok ? &game->menu_font : NULL);
  EuropeScreen* eu_mut = game->europe_ok ? (EuropeScreen*)&game->europe : NULL;
  const EuropeScreen* eu = &game->europe;
  if (eu_mut) {
    europe_refresh_harbor_selection(eu_mut);
    /* Render-only mirror for chrome (market colour, @ARMOPTIONS rows): the
     * trade gates read the nation word itself through
     * europe_cargo_boycotted_ex (audit G6). ai_king.c tea-party / ai_diplo.c
     * embargo write game->col1.nation[human].boycott_bitmap directly; europe.c
     * has no col1 pointer, so refresh the UI-side copy every render (screen is
     * always rendered at least once before the player can act on it). */
    if (game->col1_ok && game->human_nation >= 0 &&
        game->human_nation < (int)COLONIZE_COL1_NATION_COUNT) {
      /* Keep the import's 0xFFFF heal (col1_bridge_apply): a nation word that
       * still carries the removed all-cargo-embargo fingerprint must not be
       * mirrored back over the healed UI copy on the first frame. */
      const uint16_t nat_boycott = game->col1.nation[game->human_nation].boycott_bitmap;
      eu_mut->boycott_bitmap = (nat_boycott == 0xFFFFu) ? 0u : nat_boycott;
      /* bugs.md: Brewster bans criminals/servants from the pool the moment
       * he is owned — reroll stale slots so Recruit and dock agree with the
       * Brewster pick dialog. */
      europe_apply_brewster(
        eu_mut,
        founding_fathers_nation_has(&game->col1, game->human_nation, FF_WILLIAM_BREWSTER)
      );
    }
  }

  render_europe_chrome(game, framebuffer, font, eu);
  render_europe_holds_and_dock(game, framebuffer, font, eu);
  render_europe_market_and_buttons(game, framebuffer, font, eu);

  europe_render_menu_popup(game, framebuffer);

  game_render_modal_overlays(game, font, framebuffer);

  if (!game->europe_ok) {
    font_draw_text(font, framebuffer, 4, 100, "EUROPE.PIK / NAMES.TXT failed to load", 12);
  }
}
/* ===================== Colony screen render, asset/palette loading, game_create/destroy (render_colony_screen .. activate_menu_selection) ===================== */


void render_colony_screen(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer) {
  const ColonizeColony* colony = colonies_get(&game->colonies, game->colony_view_id);
  const ColonizeFont* font = game->colony_font_ok
    ? &game->colony_font
    : (game->menu_font_ok ? &game->menu_font : NULL);
  /* View is mutated for UI scratch (deltas / highlight); game pointer stays const. */
  colony_screen_render_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(game->units_ok ? &game->units : NULL), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(game->world_map_ok ? &game->world_map : NULL), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL)}, game->colony_screen_ok ? (ColonyScreenView*)&game->colony_screen : NULL, colony, game->terrain_ok ? &game->terrain : NULL, game->phys0_ok ? &game->phys0 : NULL, game->game_year, game->game_autumn, game->europe.gold, font, game->debug_building_rects, game->labels_ok ? &game->labels : NULL, framebuffer);
  game_render_modal_overlays(game, font, framebuffer);
}

static void fill_fallback_palette(ColonizePalette* palette) {
  for (int i = 0; i < 256; ++i) {
    palette->rgb[i][0] = (uint8_t)i;
    palette->rgb[i][1] = (uint8_t)((i * 3) & 0xff);
    palette->rgb[i][2] = (uint8_t)(255 - i);
  }
}

void game_hof_load(ColonizeGameState* game);
void game_hof_save(const ColonizeGameState* game);

/*
 * CYCLE.DAT: uint16 count + up to 8 records of [length, phase, start, rate]
 * loaded raw to DS:0x929e by DOS FUN_7a9d_0004; FUN_1a0a_0004 zeroes the
 * phase bytes on map-screen entry. rate is in ticks of the 60.877 Hz
 * DS:0x92e8 counter (IRQ0 at PIT divisor 0x7a8 = 608.77 Hz, ÷2 ÷5 in the
 * ISR); one step rotates the `length` palette triples at
 * `start` one index upward (rgb[start] takes rgb[start+length-1]'s place at
 * the top, everything else shifts up by one).
 */
static void game_load_cycle_dat(ColonizeGameState* game) {
  game->water_cycle_count = 0;
  char path[512];
  if (!dos_compat_normalize_asset_path(game->resolved_data_dir, "CYCLE.DAT", path, sizeof(path))) {
    return;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    return;
  }
  uint8_t raw[34];
  const size_t n = fread(raw, 1, sizeof(raw), f);
  fclose(f);
  if (n < 2) {
    return;
  }
  int count = raw[0] | (raw[1] << 8);
  if (count > 8) {
    count = 8;
  }
  for (int i = 0; i < count; ++i) {
    const size_t off = 2 + (size_t)i * 4;
    if (off + 4 > n) {
      break;
    }
    const uint8_t length = raw[off + 0];
    const uint8_t start = raw[off + 2];
    const uint8_t rate = raw[off + 3];
    if (length < 2 || (int)start + (int)length > 256) {
      continue;
    }
    const int slot = game->water_cycle_count++;
    game->water_cycle[slot].start = start;
    game->water_cycle[slot].length = length;
    /*
     * ticks → ms. The IRQ0 fires at 1193182/0x7a8 = 608.77 Hz, but the
     * DS:0x92e8 counter FUN_1c0c_0006 reads (via the DS:0x267a far ptr) only
     * increments on every 2nd IRQ × every 5th pass of the DS:0x376 countdown
     * (ISR at 1a29:0004, bytes 0x15/0x65/0x7b/0xb1) — a 60.877 Hz tick.
     * Retail rate 0x23 → ~575 ms/step.
     */
    game->water_cycle[slot].step_ms = ((uint32_t)rate * 164270u) / 10000u;
    game->water_cycle[slot].last_ms = 0;
  }
  if (game->water_cycle_count > 0) {
    diag_info(
      "Loaded CYCLE.DAT: %d cycle(s), first start=%u len=%u step=%ums",
      game->water_cycle_count,
      game->water_cycle[0].start,
      game->water_cycle[0].length,
      game->water_cycle[0].step_ms
    );
  }
}

/*
 * DOS FUN_1a0a_007a runs from the IRQ0 path whenever DS:0x372 is set —
 * i.e. while the map screen owns the display and 0x5383 bit0 (Water Color
 * Cycling off) is clear — including under popups and during AI turns. Here:
 * rotate the live map palette in place; game_render copies it out per frame,
 * which is this port's DAC write.
 */
/*
 * Asset-load idiom (audit GL-27): normalise the DOS path, load, then log the
 * same "Loaded <desc> …" / "Failed to load <FILE>: <err>" pair every one of
 * these blocks wrote out by hand. desc carries each call's own wording so the
 * log lines are unchanged. Returns true when the asset loaded; the caller sets
 * its own _ok flag and does whatever else that asset needs.
 */
static bool game_load_ss_asset(
  ColonizeGameState* game,
  const char* file,
  ColonizeSpriteSheet* out,
  const char* desc
) {
  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(game->resolved_data_dir, file, path, sizeof(path))) {
    return false;
  }
  if (!ss_load(path, out, err, sizeof(err))) {
    diag_warn("Failed to load %s: %s", file, err);
    return false;
  }
  diag_info("Loaded %s with %d sprites", desc, out->sprite_count);
  return true;
}

static bool game_load_ff_asset(
  ColonizeGameState* game,
  const char* file,
  ColonizeFont* out,
  const char* desc
) {
  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(game->resolved_data_dir, file, path, sizeof(path))) {
    return false;
  }
  if (!ff_load(path, out, err, sizeof(err))) {
    diag_warn("Failed to load %s: %s", file, err);
    return false;
  }
  diag_info("Loaded %s %s (%ux%u)", desc, file, out->max_width, out->max_height);
  return true;
}

void game_water_cycle_tick(ColonizeGameState* game) {
  if (game->water_cycle_count == 0 || !game->map_palette_ok ||
      game_screen_owns_display(game) || game->woodcut.open || game->declaration.open ||
      game->opening.open || game->closing.open) {
    return;
  }
  if (game->col1.head.game_options.water_color_cycling != 0) { /* bit set = off */
    return;
  }
  for (int i = 0; i < game->water_cycle_count; ++i) {
    if (game->elapsed_ms - game->water_cycle[i].last_ms < game->water_cycle[i].step_ms) {
      continue;
    }
    game->water_cycle[i].last_ms = game->elapsed_ms;
    const int start = game->water_cycle[i].start;
    const int len = game->water_cycle[i].length;
    uint8_t (*rgb)[3] = game->map_palette.rgb;
    uint8_t top[3];
    memcpy(top, rgb[start + len - 1], 3);
    memmove(rgb[start + 1], rgb[start], (size_t)(len - 1) * 3);
    memcpy(rgb[start], top, 3);
  }
}

/*
 * game_create stages, in load order. Each takes the freshly calloc'd state
 * and fills in one family of fields or assets; failures are recorded in the
 * matching *_ok flags rather than aborting (game_create never fails past the
 * allocation).
 */

/* Field defaults and the subsystem init calls (dialogs, pools, atlas). */
static void game_create_reset_fields(ColonizeGameState* game, const ColonizeGameConfig* config) {
  game->config = *config;
  game->ff_pedia_after_report = -1;
  game->found_open_colony_id = -1;
  game->tired_ok_unit = -1;
  game->foreign_trade_unit = -1;
  game->view_pan_hold_unit = -1;
  game->turn_number = 1;
  game->map_seed = 73;
  game->colony_view_id = -1;
  game->map_cursor_x = 29;
  game->map_cursor_y = 36;
  game->map_view_x = 29;
  game->map_view_y = 36;
  game->in_menu = true;
  game->difficulty = 0;
  game->leader_name[0] = '\0';
  game->game_year = 1492;
  game->game_autumn = 0;
  game->human_nation = 0;
  game->active_turn_nation = 0;
  {
    const uint32_t seed = game_pick_rng_seed(game, 1u);
    dos_rng_seed(&game->move_rng, seed);
    game->ai_rng_seed = seed;
  }
  game->pedia_category = PEDIA_CAT_TERRAIN;
  game->pedia_index = 0;
  game->pedia_hover_entry = -1;
  game->pedia_view = PEDIA_VIEW_LIST;
  game->pedia_return_to_list = false;
  game->debug_show_mouse_coords =
    (config && config->show_mouse_coords_set) ? config->show_mouse_coords : false;
  game->debug_building_rects =
    (config && config->show_building_rects_set) ? config->show_building_rects : false;
  game->debug_logs =
    (config && config->debug_logs_set) ? config->debug_logs : false;
  diag_set_info_enabled(game->debug_logs);
  game->cheat_create_pending_nation = -1;
  game->cheat_unlock_step = 0;
  game->fog_view = -2;
  col1_save_init(&game->col1);
  game->col1_ok = false;

  assets_msg_init(&game->messages);
  assets_msg_init(&game->map_menu_txt);
  assets_msg_init(&game->labels);
  assets_msg_init(&game->debug_txt);
  assets_msg_init(&game->pedia);
  assets_msg_init(&game->names);
  map_menu_init(&game->map_menu);
  pick_music_init(&game->pick_music);
  cheat_list_init(&game->cheat_list);
  howmuch_init(&game->howmuch);
  game->howmuch_move_dst_unit_id = -1;
  name_entry_init(&game->name_entry);
  options_dialog_init(&game->options_dlg);
  combat_analysis_close(&game->combat_analysis);
  game->platform = NULL;
  game_bind_combat_analysis(game);
  game->map_confirm = GAME_MAP_CONFIRM_NONE;
  game->map_confirm_payload = -1;
  game->trade_select_mode = 0;
  trade_screen_init(&game->trade_screen, NULL); /* labels rebound after asset load */
  game->trade_dest_stop = -1;
  game->trade_cargo_stop = -1;
  game->trade_cargo_is_load = false;
  game->trade_last_edited = 0;
  game->trade_create_stage = 0;
  game->trade_create_dest1 = -1;
  game->trade_create_sea = 0;
  game->trade_create_name[0] = '\0';
  game->trade_begin_route_pending = -1;
  game->goto_port_pending_unit = -1;
  ai_popup_init(&game->ai_popups);
  ai_diplo_talk_reset();
  save_load_init(&game->save_load);
  new_game_init(&game->new_game);
  units_reset(&game->units);
  colonies_init(&game->colonies);
  debug_atlas_init(&game->debug_atlas);
}

/* Data-dir resolution, required-file validation, VICEROY.PAL and GAME.TXT. */
static void game_create_resolve_data_dir(ColonizeGameState* game, const ColonizeGameConfig* config) {
  if (!assets_resolve_data_dir(config->data_dir, game->resolved_data_dir, sizeof(game->resolved_data_dir))) {
    /* Keep resolved path even if missing so errors remain actionable. */
  }
  game->config.data_dir = game->resolved_data_dir;

  assets_log_inventory(game->resolved_data_dir);

  char err[256];
  game->assets_ok = assets_validate_required_files(game->resolved_data_dir, err, sizeof(err));
  if (!game->assets_ok) {
    str_copy_trunc(game->assets_error, sizeof(game->assets_error), err);
    set_status(game, "Asset error", err);
    diag_error("Asset validation failed: %s", err);
  } else {
    snprintf(game->status, sizeof(game->status), "Colonization Linux Port");
    diag_info("Asset validation succeeded for data_dir=%s", game->resolved_data_dir);
  }
  game_hof_load(game);

  game->palette_ok = assets_load_palette(game->resolved_data_dir, &game->palette);
  if (!game->palette_ok) {
    fill_fallback_palette(&game->palette);
    diag_warn("Using fallback generated palette.");
  }
  ai_popup_set_portrait_source(game->resolved_data_dir);

  char game_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "GAME.TXT", game_txt, sizeof(game_txt))) {
    if (!assets_msg_load_file(&game->messages, game_txt)) {
      diag_warn("Failed to parse GAME.TXT");
    }
  }
  load_begin_menu(game);
}

/* MENU.TXT / LABELS.TXT / DEBUG.TXT / NAMES.TXT / COLONY.TXT / PEDIA.TXT and
 * the map sidebar panel. */
static void game_create_load_text_assets(ColonizeGameState* game, const ColonizeGameConfig* config) {
  char menu_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "MENU.TXT", menu_txt, sizeof(menu_txt))) {
    if (assets_msg_load_file(&game->map_menu_txt, menu_txt)) {
      map_menu_load(
        &game->map_menu,
        &game->map_menu_txt,
        (config && config->debug_menu_set) ? config->debug_menu : false
      );
    } else {
      diag_warn("Failed to parse MENU.TXT");
    }
  }

  char labels_txt[512];
  game->labels_ok = false;
  if (dos_compat_normalize_asset_path(
        game->resolved_data_dir, "LABELS.TXT", labels_txt, sizeof(labels_txt)
      )) {
    if (assets_msg_load_file(&game->labels, labels_txt)) {
      game->labels_ok = true;
      diag_info("Loaded LABELS.TXT");
      /* @MISC 34/35 = "Continue turn." / "Zoom to colony." (DS:0x2dfe/0x2e00),
       * the FUN_364b_0000 colony-event choice pair. */
      const ColonizeMsgSection* misc = assets_msg_find(&game->labels, "MISC");
      if (misc && misc->line_count > 35) {
        ai_popup_set_colony_event_labels(misc->lines[34], misc->lines[35]);
      }
      /* EDIT TRADE ROUTE screen strings (@ROUTE + @MISC 46 "OK"). */
      trade_screen_init(&game->trade_screen, &game->labels);
    } else {
      diag_warn("Failed to parse LABELS.TXT");
    }
  }

  /* DEBUG.TXT: cheat dialog strings (@SETVIEW, etc.). Optional — fallbacks exist. */
  game->debug_txt_ok = false;
  char debug_txt_path[512];
  if (dos_compat_normalize_asset_path(
        game->resolved_data_dir, "DEBUG.TXT", debug_txt_path, sizeof(debug_txt_path)
      )) {
    if (assets_msg_load_file(&game->debug_txt, debug_txt_path)) {
      game->debug_txt_ok = true;
      diag_info("Loaded DEBUG.TXT");
    } else {
      diag_warn("Failed to parse DEBUG.TXT");
    }
  }

  game->map_panel_ok = map_panel_load(
    &game->map_panel,
    game->resolved_data_dir,
    game->labels_ok ? &game->labels : NULL
  );
  if (game->map_panel_ok) {
    diag_info("Loaded map right panel (WOODTILE/NAMEPLAT)");
  } else {
    diag_warn("Map right panel assets incomplete");
  }

  /* Colonizopedia list chrome (header, "(Exit)", category names) reads
   * LABELS.TXT / MENU.TXT through this bind — see pedia_set_chrome_catalogs. */
  pedia_set_chrome_catalogs(
    game->labels_ok ? &game->labels : NULL, &game->map_menu_txt
  );

  char names_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "NAMES.TXT", names_txt, sizeof(names_txt))) {
    if (assets_msg_load_file(&game->names, names_txt)) {
      game->names_ok = true;
      unit_chrome_load_orders(&game->names);
      game->units_ok = units_load_types(&game->units, &game->names);
      if (!colonies_load_buildings(&game->colonies, &game->names)) {
        diag_warn("Failed to load @BUILDING from NAMES.TXT");
      }
    } else {
      diag_warn("Failed to parse NAMES.TXT");
    }
  }

  char colony_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "COLONY.TXT", colony_txt, sizeof(colony_txt))) {
    game->colonies_ok = colonies_load_names(&game->colonies, colony_txt);
  }

  char pedia_txt[512];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "PEDIA.TXT", pedia_txt, sizeof(pedia_txt))) {
    if (assets_msg_load_file(&game->pedia, pedia_txt)) {
      game->pedia_ok = true;
      diag_info("Loaded Colonizopedia text from PEDIA.TXT");
    } else {
      diag_warn("Failed to parse PEDIA.TXT");
    }
  }
}

/* OPENMENU / WOODPANL PIKs and the RGB-matched @COLORS remap for the title
 * menu (OPENMENU.PIK embeds a different palette than WOODPANL). */
static void game_create_load_menu_art(ColonizeGameState* game) {
  game->menu_bg_ok = false;
  game->pedia_wood_ok = false;
  char pik_path[512];
  char pik_err[256];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "OPENMENU.PIK", pik_path, sizeof(pik_path))) {
    if (pik_load(pik_path, &game->menu_bg, pik_err, sizeof(pik_err))) {
      game->menu_bg_ok = true;
      if (game->menu_bg.has_palette) {
        game->palette = game->menu_bg.palette;
        game->palette_ok = true;
        diag_info("Using palette embedded in OPENMENU.PIK for menu.");
      }
    } else {
      diag_warn("Failed to load menu background OPENMENU.PIK: %s", pik_err);
    }
  }

  game->menu_opentile_ok = false;
  {
    char ss_path_ot[512];
    char ss_err_ot[256];
    if (dos_compat_normalize_asset_path(
          game->resolved_data_dir, "OPENTILE.SS", ss_path_ot, sizeof(ss_path_ot)
        )) {
      if (ss_load(ss_path_ot, &game->menu_opentile, ss_err_ot, sizeof(ss_err_ot))) {
        game->menu_opentile_ok = true;
        diag_info(
          "Loaded title dialog wood OPENTILE.SS (%d sprites)",
          game->menu_opentile.sprite_count
        );
      } else {
        diag_warn("Failed to load OPENTILE.SS: %s", ss_err_ot);
      }
    }
  }
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "WOODPANL.PIK", pik_path, sizeof(pik_path))) {
    if (pik_load(pik_path, &game->pedia_wood, pik_err, sizeof(pik_err))) {
      game->pedia_wood_ok = true;
      diag_info(
        "Loaded Colonizopedia wood panel WOODPANL.PIK (%dx%d)",
        game->pedia_wood.width,
        game->pedia_wood.height
      );
    } else {
      diag_warn("Failed to load WOODPANL.PIK for Colonizopedia: %s", pik_err);
    }
  }

  /* OPENMENU.PIK embeds a different palette than WOODPANL; map @COLORS by RGB so
   * title-menu text matches Colonizopedia link green (basic=68 → often openmenu 254). */
  game->menu_col_basic = COLONIZE_COL_BASIC;
  game->menu_col_hilite = COLONIZE_COL_HILITE;
  game->menu_col_select = COLONIZE_COL_SELECT;
  popup_colors_from_ui(&game->menu_popup_colors);
  if (game->menu_bg_ok && game->menu_bg.has_palette && game->pedia_wood_ok &&
      game->pedia_wood.has_palette) {
    game->menu_col_basic = assets_palette_nearest_rgb(
      &game->menu_bg.palette,
      game->pedia_wood.palette.rgb[COLONIZE_COL_BASIC][0],
      game->pedia_wood.palette.rgb[COLONIZE_COL_BASIC][1],
      game->pedia_wood.palette.rgb[COLONIZE_COL_BASIC][2]
    );
    game->menu_col_hilite = assets_palette_nearest_rgb(
      &game->menu_bg.palette,
      game->pedia_wood.palette.rgb[COLONIZE_COL_HILITE][0],
      game->pedia_wood.palette.rgb[COLONIZE_COL_HILITE][1],
      game->pedia_wood.palette.rgb[COLONIZE_COL_HILITE][2]
    );
    game->menu_col_select = assets_palette_nearest_rgb(
      &game->menu_bg.palette,
      game->pedia_wood.palette.rgb[COLONIZE_COL_SELECT][0],
      game->pedia_wood.palette.rgb[COLONIZE_COL_SELECT][1],
      game->pedia_wood.palette.rgb[COLONIZE_COL_SELECT][2]
    );
    popup_colors_remap(
      &game->menu_popup_colors, &game->pedia_wood.palette, &game->menu_bg.palette
    );
    diag_info(
      "Title menu @COLORS remap: basic %u→%u hilite %u→%u select %u→%u "
      "popup mid/light/dark %u/%u/%u",
      (unsigned)COLONIZE_COL_BASIC,
      (unsigned)game->menu_col_basic,
      (unsigned)COLONIZE_COL_HILITE,
      (unsigned)game->menu_col_hilite,
      (unsigned)COLONIZE_COL_SELECT,
      (unsigned)game->menu_col_select,
      (unsigned)game->menu_popup_colors.mid,
      (unsigned)game->menu_popup_colors.light,
      (unsigned)game->menu_popup_colors.dark
    );
  }
}

/* MADSPACK samples, the .SS sprite sheets and the .FF fonts. */
static void game_create_load_sheets(ColonizeGameState* game) {
  /* Log MADSPACK samples for bring-up. */
  static const char* packed_samples[] = {"WOODPANL.PIK", "COLONY.PIK", "BUILDING.SS"};
  for (size_t i = 0; i < sizeof(packed_samples) / sizeof(packed_samples[0]); ++i) {
    char path[512];
    char info[128];
    if (dos_compat_normalize_asset_path(game->resolved_data_dir, packed_samples[i], path, sizeof(path)) &&
        assets_detect_madspack(path, info, sizeof(info))) {
      diag_info("Packed asset %s: %s", packed_samples[i], info);
    }
  }

  if (game_load_ss_asset(game, "TERRAIN.SS", &game->terrain, "terrain sheet")) {
    game->terrain_ok = true;
    if (game->terrain.has_palette) {
      game->map_palette = game->terrain.palette;
      game->map_palette_ok = true;
    }
  }

  game_load_cycle_dat(game);
  if (game_load_ss_asset(game, "PHYS0.SS", &game->phys0, "PHYS0 overlay sheet")) {
    game->phys0_ok = true;
  }
  if (game_load_ss_asset(game, "CURSOR.SS", &game->cursor, "cursor sheet")) {
    game->cursor_ok = true;
  }
  if (game_load_ss_asset(game, "ICONS.SS", &game->unit_icons, "unit icon sheet")) {
    game->unit_icons_ok = true;
    /*
     * bugs.md #364 (Dutch mission cross pink): the map screen draws
     * TERRAIN.SS tiles and ICONS.SS pieces through ONE DAC. The two sheets'
     * own palettes agree everywhere that matters except slots 5 and 13 —
     * TERRAIN leaves them as plain EGA magenta and never uses them (0 pixels
     * in any TERRAIN/PHYS0/BDARK/CURSOR sprite), while ICONS paints real
     * pixels with them and defines them as the Dutch pair (255,113,0) /
     * (170,73,0). So DOS's map DAC has to be carrying ICONS' values there —
     * merge them in rather than leaving the nation chrome to nearest-match a
     * washed-out substitute.
     */
    if (game->map_palette_ok && game->unit_icons.has_palette) {
      memcpy(game->map_palette.rgb[5], game->unit_icons.palette.rgb[5], 3);
      memcpy(game->map_palette.rgb[13], game->unit_icons.palette.rgb[13], 3);
    }
  }

  if (game_load_ff_asset(game, "FONTSMAL.FF", &game->menu_font, "menu font")) {
    game->menu_font_ok = true;
  }
  if (game_load_ff_asset(game, "FONTINTR.FF", &game->intro_font, "intro font")) {
    game->intro_font_ok = true;
  }
  if (game_load_ff_asset(game, "FONTTINY.FF", &game->colony_font, "colony font")) {
    game->colony_font_ok = true;
  }
}

/* AMER2.MP plus the Europe / colony / reports screen assets. */
static void game_create_load_screens(ColonizeGameState* game) {
  char mp_path[512];
  char mp_err[256];
  if (dos_compat_normalize_asset_path(game->resolved_data_dir, "AMER2.MP", mp_path, sizeof(mp_path))) {
    if (map_load_mp(mp_path, &game->world_map, mp_err, sizeof(mp_err))) {
      game->world_map_ok = true;
      game->map_cursor_x = game->world_map.width / 2;
      game->map_cursor_y = game->world_map.height / 2;
      game->map_view_x = game->map_cursor_x;
      game->map_view_y = game->map_cursor_y;
      diag_info(
        "Loaded world map AMER2.MP (%ux%u), cursor at %d,%d",
        game->world_map.width,
        game->world_map.height,
        game->map_cursor_x,
        game->map_cursor_y
      );
    } else {
      diag_warn("Failed to load AMER2.MP: %s", mp_err);
    }
  }

  char europe_err[256];
  if (europe_load(&game->europe, game->resolved_data_dir, europe_err, sizeof(europe_err))) {
    /* One treasury (audit G3): writers that hold only a save — colony-capture
     * plunder, combat ransom/loot — reach the human's live purse through this
     * registration (europe_set_live_screen). */
    europe_set_live_screen(&game->europe);
    europe_set_live_save(game->col1_ok ? &game->col1 : NULL);
    europe_set_popup_queue(&game->ai_popups);
    /* Bound AFTER europe_load: its memset wipes both handles (bugs.md #545).
     * @CMESSAGE wording for the Europe sale status line (bugs.md #376);
     * GAME.TXT for statuses europe.c composes itself (@KISSSORRY etc). */
    europe_set_labels(&game->europe, game->labels_ok ? &game->labels : NULL);
    europe_set_messages(&game->europe, &game->messages);
    game->europe_ok = true;
  } else {
    game->europe_ok = false;
    diag_warn("Failed to load Europe screen: %s", europe_err);
  }

  char colony_screen_err[256];
  if (colony_screen_load(&game->colony_screen, game->resolved_data_dir, colony_screen_err, sizeof(colony_screen_err))) {
    game->colony_screen_ok = true;
  } else {
    game->colony_screen_ok = false;
    diag_warn("Failed to load colony screen: %s", colony_screen_err);
  }

  char reports_err[256];
  reports_init(&game->reports);
  if (reports_load(&game->reports, game->resolved_data_dir, reports_err, sizeof(reports_err))) {
    game->reports_ok = true;
  } else {
    game->reports_ok = false;
    diag_warn("Failed to load report screens: %s", reports_err);
  }

  {
    char ss_path[512];
    char ss_err[128];
    if (dos_compat_normalize_asset_path(
          game->resolved_data_dir, "BUILDING.SS", ss_path, sizeof(ss_path)
        ) &&
        ss_load(ss_path, &game->pedia_buildings, ss_err, sizeof(ss_err))) {
      game->pedia_buildings_ok = true;
      diag_info(
        "Loaded BUILDING.SS for Colonizopedia (%d sprites)",
        game->pedia_buildings.sprite_count
      );
    } else {
      game->pedia_buildings_ok = false;
    }
  }
}

ColonizeGameState* game_create(const ColonizeGameConfig* config) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game || !config) {
    free(game);
    return NULL;
  }
  game_create_reset_fields(game, config);
  game_create_resolve_data_dir(game, config);
  game_create_load_text_assets(game, config);
  game_create_load_menu_art(game);
  game_create_load_sheets(game);
  game_create_load_screens(game);

  diag_info("Game config save_dir=%s", config->save_dir ? config->save_dir : "(null)");
  dos_compat_init();
  return game;
}

void game_set_platform(ColonizeGameState* game, ColonizePlatform* platform) {
  if (!game) {
    return;
  }
  game->platform = platform;
  units_set_combat_human_nation(game->human_nation);
}

void game_destroy(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  units_set_move_watch(NULL, NULL);
  units_set_combat_watch(NULL, NULL);
  units_set_combat_dissolve(NULL, NULL);
  units_set_combat_popup_pump(NULL, NULL);
  ai_set_native_score_plot(NULL, NULL);
  combat_analysis_set_presenter(NULL, NULL);
  combat_analysis_close(&game->combat_analysis);
  declaration_close(&game->declaration);
  opening_close(&game->opening);
  closing_close(&game->closing);
  woodcut_close(&game->woodcut);
  woodcut_clear_pending();
  pik_free(&game->menu_bg);
  pik_free(&game->pedia_wood);
  /* Unregister before the screen dies (audit G3) — same register-once idiom
   * as the units_set_* watches above. */
  europe_set_live_screen(NULL);
  europe_set_live_save(NULL);
  europe_set_popup_queue(NULL);
  europe_free(&game->europe);
  colony_screen_free(&game->colony_screen);
  reports_free(&game->reports);
  ss_free(&game->exploits_sheet);
  ss_free(&game->menu_opentile);
  ss_free(&game->terrain);
  ss_free(&game->phys0);
  ss_free(&game->cursor);
  ss_free(&game->unit_icons);
  ss_free(&game->pedia_buildings);
  ff_free(&game->menu_font);
  ff_free(&game->intro_font);
  ff_free(&game->colony_font);
  map_free(&game->world_map);
  col1_save_free(&game->col1);
  assets_msg_free(&game->messages);
  assets_msg_free(&game->map_menu_txt);
  assets_msg_free(&game->labels);
  assets_msg_free(&game->debug_txt);
  map_menu_free(&game->map_menu);
  map_panel_free(&game->map_panel);
  pick_music_close(&game->pick_music);
  cheat_list_close(&game->cheat_list);
  new_game_free(&game->new_game);
  assets_msg_free(&game->pedia);
  assets_msg_free(&game->names);
  debug_atlas_free(&game->debug_atlas);
  dos_compat_shutdown();
  free(game);
}

void game_commit_new_campaign(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  NewGameWizard* ng = &game->new_game;

  game->difficulty = ng->difficulty;
  game->human_nation = ng->nation;
  game->active_turn_nation = ng->nation;
  game->fog_view = -2;
  snprintf(game->leader_name, sizeof(game->leader_name), "%s", ng->leader_name);

  char err[256];
  map_free(&game->world_map);
  game->world_map_ok = false;
  units_set_occupancy_map(NULL);
  colonies_set_occupancy_map(NULL);

  char map_label[NEW_GAME_MAP_NAME_MAX];
  map_label[0] = '\0';

  ColonizeDosRng campaign_rng;
  memset(&campaign_rng, 0, sizeof(campaign_rng));
  bool share_campaign_rng = false;

  if (ng->generate_map || ng->path == NEW_GAME_PATH_NEW_WORLD ||
      ng->path == NEW_GAME_PATH_CUSTOMIZE) {
    uint32_t seed = ng->gen_params.seed;
    if (seed == 0) {
      seed = game_pick_rng_seed(game, game->elapsed_ms);
    }
    ng->gen_params.seed = seed;
    /*
     * DOS NEW WORLD: seed LCG → draw customize axes → reseed with same tick
     * before FUN_684c_08c0 (plan hypothesis 1). CUSTOMIZE keeps user axes;
     * only seed the LCG once for generate + tribe stream.
     */
    dos_rng_seed(&campaign_rng, seed);
    ng->gen_params.rng = &campaign_rng;
    ng->gen_params.focus_nation = game->human_nation;
    share_campaign_rng = true;
    if (ng->path == NEW_GAME_PATH_NEW_WORLD) {
      /* Hypothesis 1: draw customize axes, then reseed before FUN_684c_08c0. */
      map_gen_params_random(&ng->gen_params, seed);
      dos_rng_seed(&campaign_rng, seed);
      ng->gen_params.rng = &campaign_rng;
    }
    game->world_map_ok = map_generate(&game->world_map, &ng->gen_params, err, sizeof(err));
    if (!game->world_map_ok) {
      diag_error("new world map_generate failed: %s", err);
    }
    snprintf(
      map_label,
      sizeof(map_label),
      "%s",
      ng->path == NEW_GAME_PATH_CUSTOMIZE ? "CUSTOMIZE" : "NEW WORLD"
    );
  } else {
    char map_path[512];
    if (!dos_compat_normalize_asset_path(game->resolved_data_dir, ng->map_file, map_path, sizeof(map_path))) {
      str_path_join(map_path, sizeof(map_path), game->resolved_data_dir, ng->map_file);
    }
    game->world_map_ok = map_load_mp(map_path, &game->world_map, err, sizeof(err));
    if (!game->world_map_ok) {
      diag_error("new game map load failed (%s): %s", ng->map_file, err);
      if (dos_compat_normalize_asset_path(game->resolved_data_dir, "AMER2.MP", map_path, sizeof(map_path))) {
        game->world_map_ok = map_load_mp(map_path, &game->world_map, err, sizeof(err));
      }
    }
    /*
     * bugs.md #2: map_load_mp marks scenario .MP maps fully explored
     * (no fog plane in the file) — fine for the pre-game-start default load,
     * but a real Original Americas/AMER2 campaign start must begin fogged
     * like NEW WORLD/CUSTOMIZE (map_generate's map_alloc zero-inits `seen`).
     * Clear it here; the landfall reveal below uncovers the starting area.
     */
    if (game->world_map_ok && game->world_map.seen) {
      memset(game->world_map.seen, 0, game->world_map.tile_count);
    }
    /* bugs.md: DOS runs the FUN_684c_08c0 tail on a loaded map too (HS rings,
     * arctic rows, hill/mountain tiles lose their forest class) — AMER2.MP
     * ships 146 forested hill/mountain tiles that DOS never draws as forest. */
    if (game->world_map_ok && game->world_map.terrain) {
      map_gen_finalize_border_and_forest(
        game->world_map.terrain, (int)game->world_map.width, (int)game->world_map.height
      );
    }
    snprintf(map_label, sizeof(map_label), "%s", ng->map_file[0] ? ng->map_file : "AMER2.MP");
  }

  col1_save_free(&game->col1);
  game->col1_ok = false;
  game->game_year = 1492;
  game->game_autumn = 0;
  game->turn_number = 0;
  europe_reset_campaign_nation(&game->europe, game->human_nation);
  if (game->names_ok) {
    europe_set_nation(&game->europe, game->human_nation, &game->names);
  }
  /* DOS FUN_38fd_6024 seeds the recruit pool from the chosen difficulty;
   * europe_reset_campaign_nation had to guess Discoverer because it runs
   * before any col1 exists. */
  europe_seed_pool(&game->europe, game->difficulty, true);
  /* Wipe live colonies but restore @BUILDING / COLONY.TXT so founding can grant starters. */
  colonies_init(&game->colonies);
  game->colonies_ok = false;
  if (game->names_ok) {
    if (!colonies_load_buildings(&game->colonies, &game->names)) {
      diag_warn("Failed to reload @BUILDING after new campaign init");
    }
  }
  {
    char colony_txt[512];
    if (dos_compat_normalize_asset_path(
          game->resolved_data_dir, "COLONY.TXT", colony_txt, sizeof(colony_txt)
        )) {
      game->colonies_ok = colonies_load_names(&game->colonies, colony_txt);
    }
  }
  if (!game->colonies_ok) {
    game->colonies_ok = game->colonies.building_type_count > 0;
  }

  int sx = 39, sy = 10;
  if (ng->generate_map || ng->path == NEW_GAME_PATH_NEW_WORLD ||
      ng->path == NEW_GAME_PATH_CUSTOMIZE) {
    /* FUN_684c LAB_684c_1b4c HS-rim landfall for human nation. */
    if (!map_gen_euro_landfall(&game->world_map, game->human_nation, &sx, &sy)) {
      if (!map_gen_pick_start(
            &game->world_map, game->human_nation, -1, -1, 0, &sx, &sy
          )) {
        sx = game->world_map.width / 2;
        sy = game->world_map.height / 2;
      }
    }
  } else {
    char stem[64];
    snprintf(stem, sizeof(stem), "%s", ng->map_file);
    char* dot = strrchr(stem, '.');
    if (dot) {
      *dot = '\0';
    }
    new_game_scenario_start(
      game->names_ok ? &game->names : NULL, stem, game->human_nation, &sx, &sy
    );
  }

  if (game->world_map_ok && game->units_ok) {
    units_set_occupancy_map(&game->world_map);
    colonies_set_occupancy_map(&game->world_map);
    units_new_world_start(
      &game->units, &game->world_map, sx, sy, game->human_nation, game->difficulty
    );
    if (game->units.selected_id >= 0) {
      const ColonizeUnit* u = units_get_const(&game->units, game->units.selected_id);
      if (u) {
        game->map_cursor_x = u->x;
        game->map_cursor_y = u->y;
        game->map_view_x = u->x;
        game->map_view_y = u->y;
      }
    } else {
      game->map_cursor_x = sx;
      game->map_cursor_y = sy;
      game->map_view_x = sx;
      game->map_view_y = sy;
    }

    char stem[64];
    stem[0] = '\0';
    if (ng->map_file[0] && ng->path == NEW_GAME_PATH_AMERICA) {
      snprintf(stem, sizeof(stem), "%s", ng->map_file);
      char* dot = strrchr(stem, '.');
      if (dot) {
        *dot = '\0';
      }
    }
    AiNewGameParams ai;
    memset(&ai, 0, sizeof(ai));
    ai.col1 = &game->col1;
    ai.col1_ok = &game->col1_ok;
    ai.map = &game->world_map;
    ai.units = &game->units;
    ai.europe = &game->europe;
    ai.names = game->names_ok ? &game->names : NULL;
    ai.data_dir = game->resolved_data_dir;
    ai.human_nation = game->human_nation;
    ai.difficulty = game->difficulty;
    ai.leader_name = game->leader_name;
    ai.use_tribe_txt = (ng->path == NEW_GAME_PATH_AMERICA);
    ai.map_stem = stem[0] ? stem : NULL;
    ai.human_start_x = sx;
    ai.human_start_y = sy;
    ai.rng_seed = game_pick_rng_seed(game, game->elapsed_ms);
    ai.rng_seed_set = game->config.rng_seed_set || (ai.rng_seed != 0);
    if (ng->path == NEW_GAME_PATH_NEW_WORLD || ng->path == NEW_GAME_PATH_CUSTOMIZE) {
      ai.rng_seed = ng->gen_params.seed;
      ai.rng_seed_set = true;
    }
    if (share_campaign_rng) {
      ai.rng = &campaign_rng;
    }
    char ai_err[256];
    if (!ai_init_new_game(&ai, ai_err, sizeof(ai_err))) {
      diag_warn("ai_init_new_game failed: %s", ai_err[0] ? ai_err : "unknown");
    }
    /*
     * ai_init_new_game seeds the DOS new-game words; a loaded settings.json
     * then overrides them with the player's remembered options. Gated on
     * settings_is_loaded so harnesses that never call settings_init (tests,
     * goldens) keep the untouched DOS defaults.
     */
    if (game->col1_ok && settings_is_loaded()) {
      settings_apply_to_head(settings_get(), &game->col1.head);
      sound_set_options(settings_sound_options(settings_get()));
    }
    if (share_campaign_rng) {
      game->move_rng = campaign_rng;
    } else {
      dos_rng_seed(&game->move_rng, ai.rng_seed);
    }
    game->ai_rng_seed = ai.rng_seed;
    /* FUN_38fd_6024 opening-price roll — new campaign only, 16 draws from
     * the campaign stream (smell audit #55). Load path keeps euro_price. */
    europe_seed_campaign_prices(&game->europe, &game->move_rng);
    /*
     * Reveal around owned units and colonies. Was NEW WORLD/CUSTOMIZE-only in
     * practice — scenario .MP starts (Original Americas/AMER2) used to load
     * fully explored, so this loop was a no-op there (bugs.md #2, fixed
     * by clearing `seen` after map_load_mp above).
     */
    if (game->world_map_ok) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &game->units.units[i];
        if (!u->active || u->nation_id != game->human_nation || !units_is_on_map(u)) {
          continue;
        }
        (void)units_reveal_sight_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL)}, u);
      }
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &game->colonies.colonies[i];
        if (!c->active || c->nation_id != game->human_nation) {
          continue;
        }
        map_reveal_radius(&game->world_map, c->x, c->y, game->human_nation, 2);
      }
      if (game->col1_ok) {
        /* Live play prompts @LANDHO; do not silent-mark on campaign start. */
        game_try_prompt_landho(game);
      }
    }
    if (game->units.selected_id >= 0) {
      const ColonizeUnit* u = units_get_const(&game->units, game->units.selected_id);
      if (u) {
        game->map_cursor_x = u->x;
        game->map_cursor_y = u->y;
        game->map_view_x = u->x;
        game->map_view_y = u->y;
      }
    }
  }

  sound_set_bgm(1);
  sound_play(0x39); /* FUN_75c2_235c new-game init: Hornpipe first (75c2:2474) */
  new_game_cancel(ng);
  game->in_menu = false;
  game->view_pieces_mode = false;
  snprintf(
    game->status,
    sizeof(game->status),
    "%s of %s (%s)",
    game->leader_name,
    new_game_nation_name(game->human_nation),
    map_label
  );
  diag_info(
    "NEWGAME %s of %s, difficulty %d, map %s, year %u",
    game->leader_name,
    new_game_nation_name(game->human_nation),
    game->difficulty,
    map_label,
    (unsigned)game->game_year
  );
}

/*
 * GAME.TXT @BEGINMENU option rows, in DOS's own file order. Dispatch is by
 * ROW, never by label: the port compiles none of the game's wording, and a
 * translated or re-worded menu must still work. A sixth row, if a data dir
 * supplies one, is the quit row (DOS has none; Esc quits there).
 */
enum {
  TITLE_MENU_ROW_NEW_WORLD = 0,
  TITLE_MENU_ROW_AMERICA = 1,
  TITLE_MENU_ROW_CUSTOMIZE = 2,
  TITLE_MENU_ROW_LOAD = 3,
  TITLE_MENU_ROW_HALL_OF_FAME = 4,
  TITLE_MENU_ROW_EXIT = 5
};

void activate_menu_selection(ColonizeGameState* game) {
  if (game->menu_selection < 0 || game->menu_selection >= game->menu_option_count) {
    return;
  }
  diag_info("Menu selected: row %d (%s)", game->menu_selection, game->menu_options[game->menu_selection]);

  switch (game->menu_selection) {
    case TITLE_MENU_ROW_EXIT: {
    game_enqueue_yes_no(
      game, GAME_MAP_CONFIRM_TITLE_EXIT, -1, "DOS", "", NULL
    );
    return;
    }
    case TITLE_MENU_ROW_LOAD: {
    game_open_save_load(game, SAVE_LOAD_MODE_LOAD);
    return;
    }
    case TITLE_MENU_ROW_HALL_OF_FAME: {
    game->in_menu = false;
    game->in_hall_of_fame = true;
    set_status(game, "Menu", "hall");
    return;
    }
    case TITLE_MENU_ROW_CUSTOMIZE: {
    game->in_menu = false;
    new_game_begin(
      &game->new_game,
      NEW_GAME_PATH_CUSTOMIZE,
      game->resolved_data_dir,
      &game->messages,
      game->names_ok ? &game->names : NULL
    );
    set_status(game, "New game", "customize");
    return;
    }
    case TITLE_MENU_ROW_AMERICA: {
    game->in_menu = false;
    new_game_begin(
      &game->new_game,
      NEW_GAME_PATH_AMERICA,
      game->resolved_data_dir,
      &game->messages,
      game->names_ok ? &game->names : NULL
    );
    set_status(game, "New game", "scenario map");
    return;
    }
    case TITLE_MENU_ROW_NEW_WORLD: {
    game->in_menu = false;
    new_game_begin(
      &game->new_game,
      NEW_GAME_PATH_NEW_WORLD,
      game->resolved_data_dir,
      &game->messages,
      game->names_ok ? &game->names : NULL
    );
    set_status(game, "New game", "generated map");
    return;
    }
    default:
      break;
  }
  set_status(game, "Menu", "unknown option");
}
