#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Sections (this file):
 *  - Screen tracking & move/combat watch presentation (~line 148)
 *  - Begin menu, full-screen render & public accessor API (~line 955)
 *
 * The other eleven sections were split out verbatim (2026-09-23) into:
 *  - game_loop_menus.c     cheat menu, trade-route wizard, cheat-list &
 *                          save/load popups
 *  - game_loop_saveload.c  save/load slot IO, reports/score/pedia menus
 *  - game_loop_render.c    map sprite blitting & zoom helpers, Europe screen
 *                          render, colony screen render, asset/palette
 *                          loading, game_create/destroy
 *  - game_loop_orders.c    unit selection & movement dispatch, foreign-colony
 *                          trade
 *  - game_loop_colony.c    colony UI (enter, drag & drop, job/building
 *                          assignment), Europe voyages, colony roles, dock
 *                          orders
 *  - game_loop_eot.c       end-of-turn flow, trade-route servicing, map menu
 *                          actions
 *  - game_loop_update.c    woodcuts/intro/popup queue servicing and the main
 *                          per-frame game_update
 * The names shared across those files are declared in game_loop_internal.h
 * ("Cross-file seams"); everything else stayed `static` in its own file.
 *
 * The dialog-wiring family (game_request_* / game_open_* / game_apply_*_result
 * and the confirm / name-entry / how-much helpers) lives in game_dialogs.c;
 * ColonizeGameState and the helpers the two files share are in
 * game_dialogs.h.
 *
 * game_update is a dispatcher: its per-screen / per-phase steps are the
 * game_update_* statics just above it, each returning a GameUpdateStep the
 * dispatcher honours (CONTINUE = fall through to the next step).
 *
 * Per the big-function rule, the other long routines are split the same way,
 * each stage static sitting directly above its dispatcher:
 *  - render_europe_screen   -> render_europe_chrome / _holds_and_dock /
 *                              _market_and_buttons
 *  - game_create            -> game_create_reset_fields / _resolve_data_dir /
 *                              _load_text_assets / _load_menu_art /
 *                              _load_sheets / _load_screens
 *  - game_try_unit_move     -> game_move_* stages (GameMoveStep)
 *  - game_apply_map_menu_action -> game_menu_action_* case clusters
 *                              (GameMenuActionStatus)
 *  - game_update_unit_pacer -> game_pacer_goto_step
 *  - game_update_colony_screen -> game_colony_screen_keys / _cargo_keys
 *                              (alongside the older _key_enter / _mouse_click)
 *  - game_update_europe_screen -> game_europe_screen_menu_keys / _keys / _mouse
 *  - game_render            -> game_render_fullscreen_takeover /
 *                              _select_palette / _screen / _map (which in turn
 *                              calls _map_composite / _overlays / _panel /
 *                              _dialogs)
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

/*
 * Every blocking modal, in the order game_screen_name reports them.
 *
 * Audit GL-28: the same 14-modal list was enumerated three times in three
 * orders (game_modal_open, game_screen_name, game_handle_modal_input) and the
 * documented invariant "every new modal must join game_modal_open" (memory:
 * popup-blocking-invariant) was enforced by nothing. game_modal_open and
 * game_screen_name now both walk this list, so a modal added here cannot be
 * missed by either. game_handle_modal_input still dispatches by hand — each
 * arm has its own keys, result handling and rollback, and there is no
 * behaviour-preserving way to table-drive that.
 *
 * A NULL name means the modal blocks but does not rename the screen:
 * ai_popups draws over whatever screen raised it and that screen keeps the
 * log context.
 */
#define GAME_MODAL_LIST(X)                                                                        \
  X(opening, "intro")                                                                             \
  X(closing, "closing")                                                                           \
  X(declaration, "declaration")                                                                   \
  X(woodcut, "woodcut")                                                                           \
  X(combat_analysis, "combat panel")                                             \
  X(trade_screen, "trade route editor")                                                           \
  X(save_load, "save/load")                                                                       \
  X(options_dlg, "options")                                                                       \
  X(pick_music, "music picker")                                                         \
  X(name_entry, "name entry")                                                                     \
  X(howmuch, "how much")                                                                          \
  X(cheat_list, "cheat list")                                                                     \
  X(unit_stack, "unit stack")                                                                     \
  X(ai_popups, NULL)
/* ===================== Screen tracking & move/combat watch presentation (game_screen_name .. game_key_letter) ===================== */


/*
 * debug.logs: the screen the player is looking at, stamped onto every log
 * line (diag_set_context) and logged whenever it changes, so a popup, an
 * order or a production line says where it happened. Modal chrome wins over
 * the screen under it, matching the input gate in game_update.
 */
/* LABELS.TXT @MISC row, or the built-in English when no catalog is loaded. */
const char* game_labels_misc_or(int row, const char* fallback) {
  const char* live = reports_labels_field("MISC", row);
  return live ? live : fallback;
}

static void game_screen_name(const ColonizeGameState* game, char* out, size_t out_size) {
  if (!game || !out || out_size == 0) {
    return;
  }
#define GAME_MODAL_NAME_ARM(field, label)                                                          \
  if (game->field.open && (label) != NULL) {                                                       \
    snprintf(out, out_size, "%s", (const char*)(label));                                           \
    return;                                                                                        \
  }
  GAME_MODAL_LIST(GAME_MODAL_NAME_ARM)
#undef GAME_MODAL_NAME_ARM
  /* The wizard owns the display the same way the title menu does (it is
   * entered from it and returns to it), so it must name itself here or a
   * log line raised while it is up claims to have happened on the map. */
  if (new_game_active(&game->new_game)) {
    snprintf(out, out_size, "new game");
    return;
  }
  if (game->in_menu) {
    snprintf(out, out_size, "title menu");
    return;
  }
  if (game->in_hall_of_fame) {
    snprintf(out, out_size, "hall of fame");
    return;
  }
  if (game->in_exploits) {
    snprintf(out, out_size, "exploits");
    return;
  }
  if (game->in_debug_atlas) {
    snprintf(out, out_size, "sprite atlas");
    return;
  }
  if (game->in_pedia) {
    snprintf(
      out, out_size, "pedia:%s",
      game->pedia_view == PEDIA_VIEW_ARTICLE ? "article" : "list"
    );
    return;
  }
  if (game->in_report) {
    snprintf(out, out_size, "report:%s", reports_title(game->report_id));
    return;
  }
  if (game->in_europe) {
    snprintf(out, out_size, "europe");
    return;
  }
  if (game->in_colony) {
    const ColonizeColony* c = colonies_get(&game->colonies, game->colony_view_id);
    snprintf(
      out, out_size, "colony:%s", (c && c->name[0]) ? c->name : "?"
    );
    return;
  }
  snprintf(out, out_size, "map");
}

/*
 * True when one of the eight full-screen views owns the display, i.e. the map
 * and its sidebar are not on screen.
 *
 * Audit GL-30: this conjunction was hand-written at eight sites, each with a
 * different subset of the flags, and the note above game_service_popup_queue
 * records the invisible-popup bug that drift caused (a queued dialog opened on
 * a branch that draws no popup and silently ate the player's keys). Callers
 * that need more than the screens — the new-game wizard, an open modal, the
 * cinematics — add those terms themselves; this is only the screen set.
 */
bool game_screen_owns_display(const ColonizeGameState* game) {
  return game->in_menu || game->in_colony || game->in_europe || game->in_report ||
    game->in_pedia || game->in_debug_atlas || game->in_hall_of_fame || game->in_exploits;
}

void game_track_screen(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  char now[64];
  now[0] = '\0';
  game_screen_name(game, now, sizeof(now));
  if (strcmp(now, game->log_screen) == 0) {
    return;
  }
  if (diag_info_enabled()) {
    diag_set_context(NULL);
    diag_info(
      "VIEW %s -> %s", game->log_screen[0] ? game->log_screen : "(none)", now
    );
  }
  str_copy_trunc(game->log_screen, sizeof(game->log_screen), now);
  diag_set_context(game->log_screen);
}

static void game_combat_analysis_present(const ColonizeCombatEngagement* eng, void* user) {
  ColonizeGameState* game = (ColonizeGameState*)user;
  if (!game || !eng || !game->units_ok) {
    return;
  }
  if (!combat_analysis_open(&game->combat_analysis, &game->units, eng)) {
    return;
  }
  /* Headless / no platform: auto-dismiss (tests never set presenter). */
  if (!game->platform) {
    combat_analysis_close(&game->combat_analysis);
    return;
  }
  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  ColonizePalette pal;
  memset(pixels, 0, sizeof(pixels));
  while (game->combat_analysis.open) {
    ColonizeInputState input = {0};
    if (!platform_poll_input(game->platform, &input) || input.quit_requested) {
      combat_analysis_close(&game->combat_analysis);
      break;
    }
    (void)combat_analysis_handle_input(&game->combat_analysis, &input);
    game_render(game, &fb, &pal);
    if (!platform_present(game->platform, &fb, &pal)) {
      combat_analysis_close(&game->combat_analysis);
      break;
    }
    platform_sleep_ms(16);
  }
}

void game_set_view_center(ColonizeGameState* game, int x, int y);

/*
 * bugs.md: "Show Foreign/Indian Moves" gate — DOS FUN_465b (raw ~75660)
 * animates a rival step only when show_entire_map (DS:0x53a2) is set or the
 * mover's vis-mask carries the human bit after FUN_281f_0826(x,y): the
 * destination lies inside the human's colony claim (layer3 owner nibble) or
 * one of its 8 neighbours holds a human unit/colony (FUN_1427_0bfe). There is
 * no "tile explored" test; the port used map_tile_seen_by here, so every move
 * on once-seen ground was watched with nobody nearby.
 */
COLONIZE_INTERNAL bool game_move_is_near_human(
  const ColonizeGameState* game,
  const ColonizeUnit* mover,
  const ColonizeWorldMap* map,
  int x,
  int y
) {
  if (!game || !mover || !map || !game->col1_ok) {
    return false;
  }
  if (game->col1.head.show_entire_map) {
    return true;
  }
  if (game->human_nation < 0 || game->human_nation > 3) {
    return false;
  }
  const uint8_t mask = units_vis_mask_for_tile(map, x, y, mover->nation_id);
  return (mask & (1u << game->human_nation)) != 0;
}

void game_map_zoom_view_size(int zoom, int* out_cols, int* out_rows);

/*
 * HUD / unit-chrome font (FONTTINY, DS:0x89e) — the one the stationary map
 * draw hands unit_chrome. bugs.md: the move slide and the combat bump passed
 * NULL here, so font_text_width fell back to its 6px cell (a wider box than
 * the tiny font's own '-' or 'S') and font_draw_text drew no letter at all —
 * a moving piece wore visibly different chrome from the same piece standing
 * still, which is what read as a second chrome implementation.
 */
static const ColonizeFont* game_chrome_font(const ColonizeGameState* game) {
  if (!game) {
    return NULL;
  }
  if (game->colony_font_ok) {
    return &game->colony_font;
  }
  return game->menu_font_ok ? &game->menu_font : NULL;
}

/*
 * bugs.md: the animated piece (move slide / combat lunge) is blitted onto the
 * finished 320x200 frame, while the standing piece is drawn into the offscreen
 * map composite that `game_render` then copies into the 240x192 viewport. The
 * standing draw is therefore clipped at the viewport edge and the animated one
 * was not: a wide sprite (ICONS.SS units run 13-21px) moving in the rightmost
 * columns painted itself and its orders box across the right info panel for
 * the length of the animation, and the combat lunge — which travels a whole
 * tile outward — put the attacker entirely on the panel when it struck east
 * from the last column. Draw the animated copy through a viewport-sized
 * scratch frame so it clips exactly where the static one does.
 */
static void game_blit_unit_in_viewport(
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
  static uint8_t s_viewport[MAP_VIEW_W * MAP_VIEW_H];
  if (!fb || !fb->pixels || fb->width < MAP_VIEW_W || fb->height < MAP_MENU_BAR_H + MAP_VIEW_H) {
    return;
  }
  ColonizeFramebuffer8 view = {.width = MAP_VIEW_W, .height = MAP_VIEW_H, .pixels = s_viewport};
  for (int row = 0; row < MAP_VIEW_H; ++row) {
    memcpy(
      &s_viewport[(size_t)row * MAP_VIEW_W],
      &fb->pixels[(size_t)(MAP_MENU_BAR_H + row) * (size_t)fb->width],
      MAP_VIEW_W
    );
  }
  unit_chrome_blit_unit_for_palette(
    &view, font, sheet, sprite_index, x, y - MAP_MENU_BAR_H, display_type_index, nation_id,
    orders_index, show_stack, damaged, active_palette
  );
  for (int row = 0; row < MAP_VIEW_H; ++row) {
    memcpy(
      &fb->pixels[(size_t)(MAP_MENU_BAR_H + row) * (size_t)fb->width],
      &s_viewport[(size_t)row * MAP_VIEW_W],
      MAP_VIEW_W
    );
  }
}

COLONIZE_INTERNAL void game_move_watch_w(
  const ColonizeWorld* w,
  void* user,
  int unit_id,
  int from_x,
  int from_y,
  int to_x,
  int to_y
) {
  const ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  ColonizeGameState* game = (ColonizeGameState*)user;
  const ColonizeUnit* unit = pool ? units_get_const(pool, unit_id) : NULL;
  if (!game || !unit || !map || !colonies || !game->platform || !game->col1_ok) {
    return;
  }
  const bool own = unit->nation_id == game->human_nation;
  if (!own) {
    const bool show =
      (unit->nation_id >= 0 && unit->nation_id < 4)
        ? game->col1.head.game_options.show_foreign_moves != 0
        : game->col1.head.game_options.show_indian_moves != 0;
    if (!show || !game_move_is_near_human(game, unit, map, to_x, to_y)) {
      return;
    }
    game_set_view_center(game, to_x, to_y);
  } else {
    /* bugs.md: scripted own-nation moves — the allied intervention force's
     * disembark slides — can originate entirely off-screen, where the slide
     * plays invisibly (a player-driven unit is by definition on screen, so
     * this recentre never fires for ordinary moves). */
    int cols = 0;
    int rows = 0;
    game_map_zoom_view_size(game->map_zoom, &cols, &rows);
    int vx = 0;
    int vy = 0;
    map_panel_clamp_view_origin(
      (int)game->world_map.width, (int)game->world_map.height, game->map_view_x,
      game->map_view_y, cols, rows, &vx, &vy
    );
    if (from_x < vx || from_y < vy || from_x >= vx + cols || from_y >= vy + rows ||
        to_x < vx || to_y < vy || to_x >= vx + cols || to_y >= vy + rows) {
      game_set_view_center(game, to_x, to_y);
    }
  }

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  ColonizePalette pal;
  const bool fast = game->col1.head.game_options.fast_piece_slide != 0;

  /*
   * bugs.md: the sprite must visibly travel from the old tile to the new one
   * — for the player's own units too (a jump-cut reads as "no animation").
   * Zoom 0 only (16px tiles); the unit is hidden from the base frame and a
   * chrome'd copy is blitted along the interpolated pixel path at an
   * eyeball-visible ~25 fps.
   */
  bool slid = false;
  if (game->map_zoom == 0 && game->unit_icons_ok) {
    int cols = 0;
    int rows = 0;
    game_map_zoom_view_size(game->map_zoom, &cols, &rows);
    int vx = 0;
    int vy = 0;
    map_panel_clamp_view_origin(
      (int)game->world_map.width, (int)game->world_map.height, game->map_view_x,
      game->map_view_y, cols, rows, &vx, &vy
    );
    const int fxl = from_x - vx;
    const int fyl = from_y - vy;
    const int txl = to_x - vx;
    const int tyl = to_y - vy;
    const int sprite = units_map_sprite(pool, unit_id);
    if (sprite >= 0 && sprite < game->unit_icons.sprite_count && fxl >= 0 && fyl >= 0 &&
        fxl < cols && fyl < rows && txl >= 0 && tyl >= 0 && txl < cols && tyl < rows) {
      ColonizeUnit* mu = units_get(&game->units, unit_id);
      if (mu) {
        /*
         * bugs.md #365: the sliding piece wears the SAME chrome it wears
         * standing still. DOS animates the move by calling FUN_112b_01ba once
         * per pixel step with no extra arguments — every decision inside it
         * (the stack tab from the unit's own chain, the damaged-Artillery
         * corner) is read
         * off the unit record, so it cannot change just because the piece is
         * in motion. Forcing both to false here made a loaded wagon train or
         * ship drop its "more units here" tab for the length of the move and
         * snap it back on arrival. Counted after the move, so the unit itself
         * is already on the destination tile — the same expression
         * units_render_on_map uses.
         */
        const bool slide_stacked = units_map_stack_chrome(pool, unit_id);
        /* unit_chrome's 4th badge arm is Artillery + the damaged bit
         * (+0x3148 bit7), not "aboard a ship". */
        const bool slide_damaged = (mu->col1_flags15 & 0x80u) != 0;
        /*
         * bugs.md: and it must be read BEFORE `mu->active` is cleared below.
         * `units_display_type_index` goes through `units_get_const`, which
         * only matches active units, so calling it inside the loop returned
         * -1 and `unit_chrome_corner_for_type` fell through to its default
         * BOTTOM_RIGHT for every piece in motion. Units that already sit in
         * that corner (Colonists, Soldiers, Pioneers, Missionaries, Braves)
         * looked right, which is why the symptom read as "only transports,
         * dragoons and artillery"; everything with a top corner —
         * Dragoons/Scouts/Cavalry (TOP_LEFT), Artillery/Wagon Train/Treasure
         * (TOP_CENTER), the six ship types (TOP_LEFT/TOP_RIGHT) — had its
         * orders box jump to the middle-right of a 13-14px sprite for the
         * length of the animation, i.e. behind the art.
         */
        const int slide_dtype = units_display_type_index(pool, unit_id);
        const int x0 = fxl * 16;
        const int y0 = MAP_MENU_BAR_H + fyl * 16;
        const int x1 = txl * 16;
        const int y1 = MAP_MENU_BAR_H + tyl * 16;
        /* bugs.md: doubled once more ("can barely register they moved"). */
        const int steps = fast ? 2 : 4;
        const uint32_t delay_ms = fast ? 60u : 80u;
        const bool was_active = mu->active;
        mu->active = false; /* keep the base frame from drawing it at `to` */
        for (int f = 1; f <= steps; ++f) {
          game_render(game, &fb, &pal);
          const int px = x0 + (x1 - x0) * f / steps;
          const int py = y0 + (y1 - y0) * f / steps;
          game_blit_unit_in_viewport(
            &fb,
            game_chrome_font(game),
            &game->unit_icons,
            sprite,
            px,
            py,
            slide_dtype,
            unit->nation_id,
            /* Same chrome the unit wears standing still: real order letter,
             * not a forced '-' (bugs.md). */
            unit->orders,
            slide_stacked,
            slide_damaged,
            /* bugs.md: NULL here fell back to the raw ICONS.SS fill index,
             * which the game palette repurposes (Dutch orange → pink while
             * moving). Use the frame's real palette like the static draw. */
            &pal
          );
          if (!platform_present(game->platform, &fb, &pal)) {
            break;
          }
          platform_sleep_ms(delay_ms);
        }
        mu->active = was_active;
        slid = true;
      }
    }
  }

  if (!slid && !own) {
    /* Off-screen or zoomed out: keep the old one-frame beat at the arrival. */
    game_render(game, &fb, &pal);
    if (platform_present(game->platform, &fb, &pal)) {
      platform_sleep_ms(fast ? 80u : 100u);
    }
  }
}

/* Compat shim: pre-ColonizeWorld signature (see src/core/world.h). Kept:
 * registered by name as the ColonizeUnitsMoveWatchFn callback
 * (units_set_move_watch), so its signature is fixed by that typedef. */
COLONIZE_INTERNAL void game_move_watch(
  void* user,
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int unit_id,
  int from_x,
  int from_y,
  int to_x,
  int to_y
) {
  ColonizeWorld w_ = world_make(pool, colonies, map, NULL, false, NULL, NULL);
  game_move_watch_w(&w_, user, unit_id, from_x, from_y, to_x, to_y);
}



/*
 * bugs.md: combat "bump" — the attacker's sprite travels toward the defender
 * before the engagement resolves, the way DOS animates an attack, so
 * back-to-back events stay perceptible. Zoom 0 only (other tiers decimate
 * sprites); needs a live platform (headless callers present nothing).
 *
 * DOS FUN_112b_0eb6 (the tile-to-tile slide behind FUN_281f_02d0, which the
 * visiting-Brave path FUN_5bfb_022e reuses) animates the OUTBOUND leg only:
 * `0x10 >> zoom` one-pixel steps carry the piece a whole tile toward the
 * target, and the tail then redraws it at its own tile (param_4/param_5) in
 * a single 01ba call — the way home is never animated. The port's k_bump
 * table stepped out to +5/+9 and then back to +4, so the lunge read as a
 * there-and-back wobble that stopped short of the defender (bugs.md).
 */
void game_map_zoom_view_size(int zoom, int* out_cols, int* out_rows);

void game_combat_watch(
  void* user, const ColonizeUnitPool* pool, int attacker_id, int def_x, int def_y
) {
  ColonizeGameState* game = (ColonizeGameState*)user;
  const ColonizeUnit* atk = pool ? units_get_const(pool, attacker_id) : NULL;
  if (!game || !atk || !game->platform || !game->world_map_ok || game->map_zoom != 0 ||
      !game->unit_icons_ok) {
    return;
  }
  /* Only when the human can see the fight (live sight of either tile). */
  const int hn = game->human_nation;
  if (!map_tile_seen_by(&game->world_map, def_x, def_y, hn)) {
    return;
  }
  game_set_view_center(game, def_x, def_y);
  int cols = 0;
  int rows = 0;
  game_map_zoom_view_size(game->map_zoom, &cols, &rows);
  int vx = 0;
  int vy = 0;
  map_panel_clamp_view_origin(
    (int)game->world_map.width, (int)game->world_map.height, game->map_view_x,
    game->map_view_y, cols, rows, &vx, &vy
  );
  const int sprite = units_map_sprite(pool, attacker_id);
  if (sprite < 0 || sprite >= game->unit_icons.sprite_count) {
    return;
  }
  const int sxp = (atk->x - vx) * 16;
  const int syp = MAP_MENU_BAR_H + (atk->y - vy) * 16;
  if (sxp < 0 || syp < MAP_MENU_BAR_H || atk->x - vx >= cols || atk->y - vy >= rows) {
    return;
  }
  const int ddx = (def_x > atk->x) - (def_x < atk->x);
  const int ddy = (def_y > atk->y) - (def_y < atk->y);
  /* Outbound only, ending one full tile out (16px at zoom 0) as DOS's
   * pixel-step loop does; the snap home below is a single frame. */
  static const int k_step[3] = {6, 11, 16};
  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  ColonizePalette pal;
  /* bugs.md #241: the lunge must draw ON TOP of everything and not fight its
   * own static sprite — hide the attacker from the base frame (as the move
   * slide does) and blit the chrome'd copy after game_render. */
  ColonizeUnit* mu = units_get((ColonizeUnitPool*)pool, attacker_id);
  const bool was_active = mu ? mu->active : false;
  /* bugs.md #365: same chrome standing or lunging — see game_move_watch. */
  const bool bump_stacked = units_map_stack_chrome(pool, attacker_id);
  const bool bump_damaged = (atk->col1_flags15 & 0x80u) != 0;
  /* Read the chrome corner before the piece is hidden — see game_move_watch:
   * units_display_type_index resolves through units_get_const, which skips
   * inactive units, so an in-loop call gives -1 and the default corner. */
  const int bump_dtype = units_display_type_index(pool, attacker_id);
  if (mu) {
    mu->active = false;
  }
  for (int f = 0; f < 3; ++f) {
    game_render(game, &fb, &pal);
    game_blit_unit_in_viewport(
      &fb,
      game_chrome_font(game),
      &game->unit_icons,
      sprite,
      sxp + ddx * k_step[f],
      syp + ddy * k_step[f],
      bump_dtype,
      atk->nation_id,
      atk->orders, /* as above: the lunging piece keeps its own letter */
      bump_stacked,
      bump_damaged,
      /* bugs.md: same palette rule as the move slide — raw index went pink. */
      &pal
    );
    if (!platform_present(game->platform, &fb, &pal)) {
      break;
    }
    /* bugs.md: 3x slower with fast piece slide on, 6x slower without. */
    platform_sleep_ms(
      game->col1_ok && game->col1.head.game_options.fast_piece_slide ? 90u : 270u
    );
  }
  if (mu) {
    mu->active = was_active;
  }
  /*
   * DOS's tail: the piece is back on its own tile the instant the outbound
   * leg ends, drawn in one frame with no travel of its own. Present it here
   * so it never sits overlapping the defender while the combat popup opens.
   */
  game_render(game, &fb, &pal);
  platform_present(game->platform, &fb, &pal);
}

/*
 * DOS "fizzle" present — FUN_12d6_0000 behind thunk FUN_281f_03ea. The DOS
 * combat tail (FUN_5fef_1b0e), the tile-wipe (FUN_5fef_36fe) and the LCR
 * "vanished without a trace" despawn (FUN_65dd_0004 case 5) all present their
 * outcome redraw by copying the whole 320×200 offscreen buffer to the VGA in
 * 16-bit LFSR order (asm at file 0xF5E6: si=1; shr si; on carry si^=0xB400;
 * copy pixel si-1 while si≤0xFA00 — every one of the 64000 pixels exactly
 * once), paced over duration 8. Only pixels that CHANGED are visible, so a
 * destroyed unit on an open tile "pixelates" away while a fight inside a
 * colony (sprite unchanged) shows nothing — which is exactly the DOS look.
 *
 * Phase 0 (before the outcome mutates the pool) renders and stores the
 * "before" frame; phase 1 renders the "after" frame and animates between the
 * two. Identical frames (fight out of sight / hidden by a colony) skip the
 * animation entirely, which also keeps unwatched AI-vs-AI fights instant.
 */
void game_combat_dissolve(void* user, int phase) {
  ColonizeGameState* game = (ColonizeGameState*)user;
  static uint8_t s_before[320 * 200];
  static uint8_t s_work[320 * 200];
  static bool s_before_ok = false;
  if (!game || !game->platform) {
    return;
  }
  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  ColonizePalette pal;
  /*
   * bugs.md #533: freeze the map blink (DS:0x929c) across BOTH frames. The
   * outcome enqueues popups between phase 0 and phase 1, which flips
   * game_map_blink_running() and with it the active unit's blink phase — the
   * selected unit owns its tile outright (units_top_on_map_tile), so the
   * attacker was absent from one frame and present in the other and the
   * LFSR dissolve played on the WINNER's sprite. DOS never toggles 0x929c
   * inside the 1b0e tail: the map's input loop is the only thing that does.
   */
  game->combat_dissolve_freeze = true;
  if (phase == 0) {
    game_render(game, &fb, &pal);
    memcpy(s_before, pixels, sizeof(s_before));
    s_before_ok = true;
    return;
  }
  if (!s_before_ok) {
    game->combat_dissolve_freeze = false;
    return;
  }
  s_before_ok = false;
  game_render(game, &fb, &pal);
  if (memcmp(s_before, pixels, sizeof(pixels)) == 0) {
    game->combat_dissolve_freeze = false;
    return;
  }
  memcpy(s_work, s_before, sizeof(s_work));
  ColonizeFramebuffer8 wfb = {.width = 320, .height = 200, .pixels = s_work};
  /* DOS duration 8 spreads the 64000 copies over roughly half a second;
   * 16 presented batches × 28ms reads the same at 60Hz. */
  const int k_batches = 16;
  uint16_t lfsr = 1;
  bool cycled = false;
  for (int f = 0; f < k_batches && !cycled; ++f) {
    int budget = 64000 / k_batches;
    while (budget > 0) {
      const unsigned carry = lfsr & 1u;
      lfsr >>= 1;
      if (carry) {
        lfsr ^= 0xB400u;
      }
      if (lfsr == 1u) { /* full LFSR cycle: every pixel visited */
        cycled = true;
        break;
      }
      if (lfsr <= 0xFA00u) {
        const unsigned idx = (unsigned)lfsr - 1u;
        s_work[idx] = pixels[idx];
        --budget;
      }
    }
    if (!platform_present(game->platform, &wfb, &pal)) {
      break;
    }
    platform_sleep_ms(28);
  }
  /* Final frame exact (the batch split leaves a 64000%16 remainder). */
  game_render(game, &fb, &pal);
  platform_present(game->platform, &fb, &pal);
  game->combat_dissolve_freeze = false;
}

/*
 * bugs.md: every combat concludes in isolation. After a combat resolves,
 * present and answer its queued popups RIGHT NOW in a nested modal loop —
 * the way DOS's blocking dialogs pace the King's turn — instead of hoarding
 * "X defeats Y" until every combat has ceased. Combat Analysis already runs
 * nested (game_combat_analysis_present); this is its outcome-popup twin.
 */
void game_apply_ai_popup_result(ColonizeGameState* game);

void game_combat_popup_pump(void* user) {
  ColonizeGameState* game = (ColonizeGameState*)user;
  static bool pumping = false;
  if (!game || !game->platform || pumping) {
    return;
  }
  pumping = true;
  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  ColonizePalette pal;
  while (ai_popup_busy(&game->ai_popups)) {
    if (!game->ai_popups.open && game->ai_popups.has_result) {
      /* A result is mid-application up-stack (its handler called into combat
       * before consuming). Nothing can present or be answered until that
       * frame returns — spinning here was the village-attack freeze. */
      break;
    }
    if (!game->ai_popups.open && !game->ai_popups.has_result) {
      /*
       * bugs.md: this loop must always be able to make progress, or it is a
       * livelock — nothing on screen, the same frame redrawn forever, and the
       * OS window-close button (input.quit_requested) the only way out. That
       * is what the REF landing froze on, and what made "several popups in
       * the WoI" look the same: ai_popup_try_present_next legitimately
       * refuses while a colony zoom is elected, because the hold serves only
       * that colony's own message batch (DOS FUN_364b_0688 is per-colony
       * blocking) — and the queue at that moment holds a King/REF arrival or
       * a combat outcome instead.
       *
       * A popup raised from inside a nested blocking pump is already outside
       * that per-colony ordering (in DOS it IS the blocking call), so let it
       * through the hold — but only NON-colony-event popups (bugs.md #398:
       * the old blanket hold-drop presented another colony's zoom CHOICE
       * mid-batch). Only if that still cannot present is there nothing to do
       * but leave.
       */
      if (!ai_popup_try_present_next(&game->ai_popups) &&
          !ai_popup_try_present_next_urgent(&game->ai_popups)) {
        break;
      }
    }
    ColonizeInputState input = {0};
    if (!platform_poll_input(game->platform, &input) || input.quit_requested) {
      break;
    }
    if (game->ai_popups.open) {
      ai_popup_handle_input(&game->ai_popups, &input);
      game_apply_ai_popup_result(game);
    }
    game_render(game, &fb, &pal);
    if (!platform_present(game->platform, &fb, &pal)) {
      break;
    }
    platform_sleep_ms(16);
  }
  pumping = false;
}

void game_bind_combat_analysis(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  combat_analysis_set_presenter(game_combat_analysis_present, game);
  units_set_combat_human_nation(game->human_nation);
  units_set_combat_music_hooks(sound_play, sound_active_song_id);
  units_set_bgm_hook(sound_set_bgm);
  ai_diplo_set_sound_hook(sound_play);
  europe_set_sound_hook(sound_play);
  europe_set_bgm_hook(sound_set_bgm);
  woodcut_set_sound_hooks(sound_play, sound_set_bgm);
  closing_set_sound_hooks(sound_play, sound_set_bgm, sound_stop_sfx);
  opening_set_sound_hooks(sound_play, sound_set_bgm);
}

void game_refresh_orders_menu(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  MapMenuOrdersContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = game->units_ok ? &game->units : NULL;
  ctx.map = game->world_map_ok ? &game->world_map : NULL;
  ctx.colonies = game->colonies_ok ? &game->colonies : NULL;
  ctx.selected_id = game->units_ok ? game->units.selected_id : -1;
  ctx.cursor_x = game->map_cursor_x;
  ctx.cursor_y = game->map_cursor_y;
  ctx.human_nation = game->human_nation;
  ctx.europe_ok = game->europe_ok;
  map_menu_refresh_orders(&game->map_menu, &ctx);
}

char game_key_letter(ColonizeKey key) {
  switch (key) {
    case COLONIZE_KEY_A: return 'A';
    case COLONIZE_KEY_B: return 'B';
    case COLONIZE_KEY_C: return 'C';
    case COLONIZE_KEY_D: return 'D';
    case COLONIZE_KEY_E: return 'E';
    case COLONIZE_KEY_F: return 'F';
    case COLONIZE_KEY_G: return 'G';
    case COLONIZE_KEY_H: return 'H';
    case COLONIZE_KEY_I: return 'I';
    case COLONIZE_KEY_L: return 'L';
    case COLONIZE_KEY_M: return 'M';
    case COLONIZE_KEY_N: return 'N';
    case COLONIZE_KEY_O: return 'O';
    case COLONIZE_KEY_P: return 'P';
    case COLONIZE_KEY_Q: return 'Q';
    case COLONIZE_KEY_R: return 'R';
    case COLONIZE_KEY_S: return 'S';
    case COLONIZE_KEY_T: return 'T';
    case COLONIZE_KEY_U: return 'U';
    case COLONIZE_KEY_V: return 'V';
    case COLONIZE_KEY_W: return 'W';
    case COLONIZE_KEY_X: return 'X';
    case COLONIZE_KEY_Z: return 'Z';
    default: return 0;
  }
}

void set_status(ColonizeGameState* game, const char* prefix, const char* detail);
bool game_do_found_colony_at_unit(ColonizeGameState* game, int uid, bool land_resolved);
void game_request_noport_found_confirm(ColonizeGameState* game, int uid);
bool game_try_found_colony_at_cursor(ColonizeGameState* game);
void game_fill_turn_context(ColonizeGameState* game, ColonizeTurnContext* ctx);
void game_apply_ai_popup_result(ColonizeGameState* game);
void game_open_report(ColonizeGameState* game, ColonizeReportId id);
/* Defined further down in this file; the dialog wiring that used to sit
 * here moved to game_dialogs.c. */
bool game_try_unit_move(ColonizeGameState* game, int dest_x, int dest_y);
void game_after_unit_action(ColonizeGameState* game);
bool game_select_next_unit_awaiting_orders(ColonizeGameState* game);
void activate_menu_selection(ColonizeGameState* game);
void game_wait_next_unit(ColonizeGameState* game);
bool game_turn_flow_allowed(const ColonizeGameState* game);
bool game_defer_turn_flow(ColonizeGameState* game);
void game_open_retire_score(ColonizeGameState* game);
void game_begin_win_closing_or_score(ColonizeGameState* game);
void game_trade_open_editor(ColonizeGameState* game, int route);
void game_trade_open_dest_picker(ColonizeGameState* game, int stop_i);
void game_trade_open_cargo_picker(ColonizeGameState* game, int stop_i, bool is_load);
void game_trade_open_stop_picker(ColonizeGameState* game, int route, int preselect);
void game_trade_wizard_open_dest(ColonizeGameState* game, int number);
void game_trade_wizard_open_name(ColonizeGameState* game);
void game_apply_trade_dest(ColonizeGameState* game, int id);
void game_apply_goto_port(ColonizeGameState* game, int id);
void game_trade_default_name(ColonizeGameState* game, int dest1, char* out, size_t out_size);
const char* game_trade_europe_label(const ColonizeGameState* game);
int game_trade_route_aim_stop(ColonizeGameState* game, ColonizeUnit* u, int stop_i);
void game_europe_service_trade_harbor(ColonizeGameState* game);
bool game_commit_sea_lane_step(ColonizeGameState* game, int sid, int dest_x, int dest_y);
bool game_ship_sail_to_europe(ColonizeGameState* game, int sid);
void game_set_view_center(ColonizeGameState* game, int x, int y);
void game_enter_colony(ColonizeGameState* game, int cid);
void game_apply_save_load_result(ColonizeGameState* game);
void game_apply_cheat_list_result(ColonizeGameState* game);
void game_select_unit(ColonizeGameState* game, int unit_id);
void game_click_activate_unit(ColonizeGameState* game, int unit_id);
void game_reveal_sight_for_unit(ColonizeGameState* game, const ColonizeUnit* u);
bool game_load_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size);
bool game_save_col1_slot(ColonizeGameState* game, int slot, char* err, size_t err_size);
void game_europe_sail_harbor(ColonizeGameState* game, int hidx);
void game_europe_open_buy_prompt(ColonizeGameState* game);
void game_europe_open_buy_prompt_for(ColonizeGameState* game, int hidx, int cargo);
int begin_menu_option_at_xy(const BeginMenuLayout* layout, int mx, int my);
void game_colony_finish_eject(ColonizeGameState* game, int colonist_index, int role);

/* ===================== Begin menu, full-screen render & public accessor API (begin_menu_font .. game_unit_info) ===================== */


/* Title-menu helpers: brace markup + selection fill (chrome via popup_draw). */




static const ColonizeFont* begin_menu_font(const ColonizeGameState* game) {
  if (!game) {
    return NULL;
  }
  /* @smallfont → FONTTINY (colony_font). Default dialog slot is FONTINTR. */
  if (game->menu_smallfont && game->colony_font_ok) {
    return &game->colony_font;
  }
  if (game->intro_font_ok) {
    return &game->intro_font;
  }
  if (game->colony_font_ok) {
    return &game->colony_font;
  }
  if (game->menu_font_ok) {
    return &game->menu_font;
  }
  return NULL;
}

/* Same geometry as render — used for mouse hit-testing. */
bool begin_menu_compute_layout(
  const ColonizeGameState* game,
  int fb_w,
  int fb_h,
  BeginMenuLayout* out
) {
  if (!game || !out || fb_w <= 0 || fb_h <= 0) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  const ColonizeFont* font = begin_menu_font(game);
  out->line_h = font ? (font->max_height + 2) : 8;
  out->title_h = font ? font->max_height : 6;
  out->title_pad_top = 3;
  out->title_pad_x = 2;
  out->option_pad_x = 8;
  out->gap_after_title = 4;
  out->option_count = game->menu_option_count;
  const int options_h = out->option_count * out->line_h;
  /*
   * Outer height from the previous pad_y=6 / line_h layout, then -12 from the
   * bottom (dialog_y unchanged). Inner title spacing is tighter (3 / title_h / 4).
   */
  int dialog_h = POPUP_FRAME_INSET * 2 + 6 +
                 (game->menu_version_line[0] ? out->line_h + 4 : 0) + options_h + 6 - 12;
  if (dialog_h < 24) {
    dialog_h = 24;
  }

  int dialog_w = game->menu_dialog_width > 0 ? game->menu_dialog_width : 160;
  if (dialog_w > fb_w) {
    dialog_w = fb_w;
  }
  int dialog_x = (fb_w - dialog_w) / 2;
  int dialog_y = game->menu_dialog_y;
  if (dialog_y < 0) {
    dialog_y = 0;
  }
  if (dialog_y + dialog_h > fb_h) {
    dialog_y = fb_h - dialog_h;
    if (dialog_y < 0) {
      dialog_y = 0;
      dialog_h = fb_h;
    }
  }

  out->dialog_x = dialog_x;
  out->dialog_y = dialog_y;
  out->dialog_w = dialog_w;
  out->dialog_h = dialog_h;
  out->inner_x = dialog_x + POPUP_FRAME_INSET;
  out->inner_y = dialog_y + POPUP_FRAME_INSET;
  out->inner_w = dialog_w - POPUP_FRAME_INSET * 2;
  out->inner_h = dialog_h - POPUP_FRAME_INSET * 2;
  if (out->inner_w <= 0 || out->inner_h <= 0) {
    return false;
  }

  int text_y = out->inner_y + out->title_pad_top;
  if (game->menu_version_line[0]) {
    text_y += out->title_h + out->gap_after_title;
  }
  out->list_y0 = text_y;
  return true;
}

int begin_menu_option_at_xy(const BeginMenuLayout* layout, int mx, int my) {
  if (!layout || layout->option_count <= 0 || layout->line_h <= 0) {
    return -1;
  }
  if (mx < layout->inner_x || mx >= layout->inner_x + layout->inner_w) {
    return -1;
  }
  if (my < layout->list_y0) {
    return -1;
  }
  const int rel = my - layout->list_y0;
  const int idx = rel / layout->line_h;
  if (idx < 0 || idx >= layout->option_count) {
    return -1;
  }
  return idx;
}

COLONIZE_INTERNAL void game_render_begin_menu(
  const ColonizeGameState* game,
  ColonizeFramebuffer8* framebuffer
) {
  if (!game || !framebuffer || !framebuffer->pixels) {
    return;
  }

  BeginMenuLayout L;
  if (!begin_menu_compute_layout(game, framebuffer->width, framebuffer->height, &L)) {
    return;
  }

  const ColonizeFont* font = begin_menu_font(game);
  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  popup_draw(
    framebuffer,
    L.dialog_x,
    L.dialog_y,
    L.dialog_w,
    L.dialog_h,
    game->menu_opentile_ok ? &game->menu_opentile : NULL,
    &game->menu_popup_colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  if (inner_w <= 0 || inner_h <= 0) {
    return;
  }

  int text_y = inner_y + L.title_pad_top;
  if (game->menu_version_line[0]) {
    const int tw = font_text_width_skip(font, game->menu_version_line, FONT_SKIP_BRACES);
    int tx = inner_x + (inner_w - tw) / 2;
    if (tx < inner_x + L.title_pad_x) {
      tx = inner_x + L.title_pad_x;
    }
    /* audit GL-8: was a private char-by-char copy of the {} emphasis drawer.
     * The line is load_begin_menu's fixed "{COLONIZATION} Linux Port <ver>",
     * so the one place the two differ — popup_draw_text_markup drops the
     * ~ / # hotkey markers that the old loop drew as glyphs — cannot arise. */
    (void)popup_draw_text_markup(
      font, framebuffer, tx, text_y, game->menu_version_line, game->menu_col_basic,
      game->menu_col_hilite, false, false, NULL
    );
    text_y += L.title_h + L.gap_after_title;
  }

  for (int i = 0; i < game->menu_option_count; ++i) {
    const int row_y = text_y + i * L.line_h;
    const bool selected = (i == game->menu_selection);
    if (selected) {
      /* 1px inset from each side of the inner content edge. */
      fb_fill_rect(
        framebuffer, inner_x + 1, row_y - 1, inner_w - 2, L.line_h, game->menu_col_select
      );
    }
    font_draw_text(
      font, framebuffer, inner_x + L.option_pad_x, row_y, game->menu_options[i], game->menu_col_basic
    );
  }
}

/*
 * DS:0x929c, the flag every map blink reads (active unit, tile cursor, the
 * "End of Turn" prompt), is toggled by FUN_281f_0dcc from ONE place: the
 * map's own wait-for-input loop (FUN_2b5a_3800-ish, the 0x14-tick timer
 * compare on DS:0x97ec/0x97ee). Nothing else in the game touches it, so the
 * moment control leaves that loop the blink stops dead and the map keeps
 * whatever phase it was last drawn in.
 *
 * That is exactly what the status line does: FUN_1009_00b4 blits the strip
 * and then spins on FUN_1c0c_0006 alone -- no redraw, no toggle. So while a
 * Custom House sale line (or any other DS:0x2d54 line) is dwelling, and
 * likewise while end-of-turn processing or a dialog owns the frame, DOS
 * shows a still map. The port drove all three blinks straight off
 * elapsed_ms, so units went on flashing under the sale lines (bugs.md).
 *
 * Frozen phase is "on" here: the unit/cursor stays drawn rather than
 * vanishing for the hold.
 */
static bool game_map_blink_running(const ColonizeGameState* game) {
  if (!game) {
    return false;
  }
  /* bugs.md #533: fizzle in progress — both dissolve frames must agree. */
  if (game->combat_dissolve_freeze) {
    return false;
  }
  /* Status line up: FUN_1009_00b4's blocking dwell. */
  if (ai_popup_bar_message(&game->ai_popups)) {
    return false;
  }
  /* End-of-turn pipeline, or anything modal over the map. */
  if (turn_processor_active(&game->turn_proc)) {
    return false;
  }
  if (game_modal_open(game) || ai_popup_busy(&game->ai_popups) || game->woodcut.open ||
      game->closing.open) {
    return false;
  }
  return true;
}

/*
 * game_render stages. The full-screen owners (opening / woodcut /
 * declaration / closing / throne audience) short-circuit everything;
 * game_render_screen paints one of the full-screen views and reports
 * whether it did, and game_render_map paints the overland map otherwise.
 */
COLONIZE_INTERNAL bool game_render_fullscreen_takeover(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette
) {
  /* OPENING.EXE owns the whole screen (OPENING.PIK palette). */
  if (game->opening.open) {
    opening_render(&game->opening, framebuffer, palette);
    return true;
  }

  /* A woodcut owns the whole screen (and its own WDCUTnn palette). */
  if (game->woodcut.open) {
    woodcut_render(&game->woodcut, framebuffer, palette);
    return true;
  }

  /* Signing cinematic owns the whole screen (and its own DECOIND palette). */
  if (game->declaration.open) {
    declaration_render(&game->declaration, framebuffer, palette);
    return true;
  }

  if (game->closing.open) {
    closing_render(&game->closing, framebuffer, palette);
    return true;
  }

  /* War-end throne audience owns the whole screen (KINGLSS palette): the
   * KING_THRONE popup renders as the DOS FUN_75c2_20e2 audience — KINGLOSE.SS
   * king when the player won (@KINGLOSE scroll at 232/31/68), KINGWIN.SS when
   * the King did (@KINGWIN at 202/125/90). Any key / click dismisses (OK). */
  if (game->ai_popups.open && game->ai_popups.current.tag == AI_POPUP_TAG_KING_THRONE) {
    const bool won = (game->ai_popups.current.payload == 1);
    new_game_render_throne_audience(
      (NewGameWizard*)&game->new_game,
      game->resolved_data_dir,
      game->human_nation,
      won ? "KINGLOSE.SS" : "KINGWIN.SS",
      game->ai_popups.current.body,
      won ? 232 : 202,
      won ? 31 : 125,
      won ? 68 : 90,
      framebuffer,
      palette
    );
    return true;
  }
  return false;
}

/* Screen palette cascade + the reserved-DAC-block merges the open popup /
 * Europe menu / exploits sheets need (merge, never remap). */
COLONIZE_INTERNAL void game_render_select_palette(
  const ColonizeGameState* game, const ColonizeFramebuffer8* framebuffer, ColonizePalette* palette,
  uint32_t render_log_counter
) {
  *palette = (game->in_menu && !game->in_debug_atlas && !game->in_pedia && !game->in_europe &&
              !game->in_colony && !game->in_report && !game->in_hall_of_fame && !game->in_exploits)
    ? game->palette
    : (game->in_debug_atlas && debug_atlas_palette(&game->debug_atlas))
      ? *debug_atlas_palette(&game->debug_atlas)
      : (game->in_pedia && game->pedia_wood_ok && game->pedia_wood.has_palette)
        ? game->pedia_wood.palette
        : (game->in_report && game->reports_ok &&
           game->report_id == COLONIZE_REPORT_CONGRESS && !game->congress_page2 &&
           game->reports.congress_page1_bg_ok && game->reports.congress_page1_bg.has_palette)
          ? game->reports.congress_page1_bg.palette
        : (game->in_report && game->reports_ok && game->reports.background_ok[game->report_id] &&
           game->reports.backgrounds[game->report_id].has_palette)
          ? game->reports.backgrounds[game->report_id].palette
          : (game->in_exploits && game->reports_ok && game->reports.exploits_bg_ok &&
             game->reports.exploits_bg.has_palette)
            ? game->reports.exploits_bg.palette
          : ((game->in_hall_of_fame || game->in_exploits) && game->reports_ok &&
             game->reports.background_ok[COLONIZE_REPORT_SCORE] &&
             game->reports.backgrounds[COLONIZE_REPORT_SCORE].has_palette)
            ? game->reports.backgrounds[COLONIZE_REPORT_SCORE].palette
            : (game->in_europe && game->europe_ok && game->europe.background.has_palette)
              ? game->europe.background.palette
              : (game->in_colony && game->colony_screen_ok && game->colony_screen.frame.has_palette)
                ? game->colony_screen.frame.palette
                : (game->map_palette_ok ? game->map_palette : game->palette);

  /*
   * An open popup's King / chief / MSS / MYR sheet carries its own entries for
   * the DAC block every screen palette above leaves black; lend them now so the
   * art blits with DOS's colours instead of a nearest-colour approximation
   * (bugs.md: the tax-audience King flair).
   */
  ai_popup_art_palette_merge((AiPopupState*)&game->ai_popups, palette);

  /* Same reserved-block rule for the Europe Recruit/Purchase menus, which
   * carry the MSS2 courtier but are drawn by europe_render_menu_popup rather
   * than ai_popup (EUROPE.PIK leaves 120..251 black; MSS2.SS fills them). */
  if (game->in_europe) {
    ai_popup_sheet_palette_merge(europe_menu_decoration(game), palette);
  }

  /* bugs.md #402: SCORE<nn>.SS carries the exploits painting's colours in the
   * DAC slots WOODPAN2.PIK leaves black — same reserved-block rule as the
   * popup art sheets (merge, never remap). */
  if (game->in_exploits && game->exploits_sheet_ok && game->exploits_sheet.has_palette) {
    for (int i = 1; i < 256; ++i) {
      if (palette->rgb[i][0] || palette->rgb[i][1] || palette->rgb[i][2]) {
        continue;
      }
      palette->rgb[i][0] = game->exploits_sheet.palette.rgb[i][0];
      palette->rgb[i][1] = game->exploits_sheet.palette.rgb[i][1];
      palette->rgb[i][2] = game->exploits_sheet.palette.rgb[i][2];
    }
  }

  if (render_log_counter == 0) {
    /* audit GL-29: the old private render_mode_name was a stale subset of this
     * cascade with its own spellings ("hall-of-fame" vs "hall of fame"). */
    char mode_name[48];
    game_screen_name(game, mode_name, sizeof(mode_name));
    diag_info(
      "Render mode=%s framebuffer=%dx%d palette=%s",
      mode_name,
      framebuffer->width,
      framebuffer->height,
      game->palette_ok ? "VICEROY.PAL" : "fallback"
    );
  }
}

/* One of the full-screen views (Europe, reports, exploits, hall of fame,
 * colony, pedia, debug atlas, new-game wizard, title menu). Returns true
 * when it painted one — the map path is then skipped. */
COLONIZE_INTERNAL bool game_render_screen(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette
) {
  if (game->in_europe) {
    render_europe_screen(game, framebuffer);
    /*
     * bugs.md: an open AI popup was never DRAWN over the European Status —
     * but it still swallowed every input (game_handle_modal_input gates on
     * ai_popups.open). Arrival events queue popups in the same end-turn
     * that auto-opens Europe on ship arrival, so the screen froze until
     * blind clicks happened to answer the invisible dialog. Same fix as
     * the colony branch.
     */
    if (game->ai_popups.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      const ColonizeFont* popup_font = game->intro_font_ok ? &game->intro_font
                                        : (game->menu_font_ok ? &game->menu_font
                                                               : (game->colony_font_ok
                                                                    ? &game->colony_font
                                                                    : NULL));
      const ColonizeSpriteSheet* wood =
        game->europe_ok && game->europe.wood_tile_ok ? &game->europe.wood_tile : NULL;
      ai_popup_render(
        (AiPopupState*)&game->ai_popups,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_HILITE,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    return true;
  }

  if (game->in_report) {
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    reports_render_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(game->units_ok ? &game->units : NULL), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(game->world_map_ok ? &game->world_map : NULL), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL), .europe=(EuropeScreen*)(game->europe_ok ? &game->europe : NULL)}, game->reports_ok ? &game->reports : NULL, game->report_id, game->congress_page2, game->labor_detail_job, game->economic_page, game->colony_page, game->naval_page, game->human_nation, game->map_cursor_x, game->map_cursor_y, game->turn_number, font, framebuffer);
    return true;
  }

  if (game->in_exploits) {
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    reports_render_exploits(
      game->reports_ok ? &game->reports : NULL, &game->exploits, font, framebuffer
    );
    return true;
  }

  if (game->in_hall_of_fame) {
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    ColonizeHofRow rows[COLONIZE_HOF_ROW_MAX];
    const int count = game->hof_count > COLONIZE_HOF_ROW_MAX ? COLONIZE_HOF_ROW_MAX : game->hof_count;
    for (int i = 0; i < count; ++i) {
      const ColonizeHofEntry* e = &game->hof_entries[i];
      snprintf(rows[i].leader, sizeof(rows[i].leader), "%s", e->leader);
      snprintf(rows[i].nation, sizeof(rows[i].nation), "%s", e->nation);
      rows[i].score = e->score;
      rows[i].year = e->year;
      rows[i].difficulty = e->difficulty;
      rows[i].rating = e->rating;
      rows[i].declared = e->declared;
      rows[i].achieved = e->achieved;
      rows[i].independent_name[0] = '\0';
      game_names_row(game, "INDEPENDENT", e->nation_id, rows[i].independent_name, sizeof(rows[i].independent_name));
    }
    reports_render_hall_of_fame(
      game->reports_ok ? &game->reports : NULL, rows, count, font, framebuffer
    );
    return true;
  }

  if (game->in_colony) {
    render_colony_screen(game, framebuffer);
    /* Player-reported: BUY (and any other ai_popups-routed colony-screen
     * confirm) silently did nothing — the popup really was enqueued and
     * open (and blocking input, per ai_popups.open gating in the input
     * handler), just never drawn: this branch jumped straight to
     * render_log_sample, skipping the map-rendering path's ai_popup_render
     * call entirely. */
    if (game->ai_popups.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      const ColonizeFont* popup_font = game->intro_font_ok ? &game->intro_font
                                        : (game->menu_font_ok ? &game->menu_font
                                                               : (game->colony_font_ok
                                                                    ? &game->colony_font
                                                                    : NULL));
      /* Player-reported: wrong colors — this framebuffer is about to be
       * expanded through the colony screen's own palette (render_colony
       * sets it below), not the map's. The map branch's wood tile
       * (menu_opentile / map_panel's) is a sprite sheet remapped for the
       * map palette; reusing it here painted the popup background with
       * indices meant for different RGB entries. Use the colony screen's
       * own wood tile (already remapped for its palette) instead. */
      const ColonizeSpriteSheet* wood =
        game->colony_screen_ok && game->colony_screen.wood_tile_ok ? &game->colony_screen.wood_tile
                                                                     : NULL;
      ai_popup_render(
        (AiPopupState*)&game->ai_popups,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_HILITE,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    return true;
  }

  if (game->in_pedia) {
    render_pedia_screen(game, framebuffer);
    return true;
  }

  if (game->in_debug_atlas) {
    const ColonizeFont* font = game->menu_font_ok ? &game->menu_font : NULL;
    debug_atlas_render(&game->debug_atlas, font, framebuffer);
    return true;
  }

  if (new_game_active(&game->new_game)) {
    ColonizePopupColors popup_cols;
    popup_colors_from_ui(&popup_cols);
    /* Hit-test layout is updated during render. */
    NewGameWizard* ng = (NewGameWizard*)&game->new_game;
    ng->ui_font = game->intro_font_ok ? &game->intro_font
      : (game->colony_font_ok ? &game->colony_font
                              : (game->menu_font_ok ? &game->menu_font : NULL));
    ng->tiny_font = game->colony_font_ok ? &game->colony_font
      : (game->menu_font_ok ? &game->menu_font : ng->ui_font);
    ng->lore_font = ng->ui_font;
    ng->wood_tile = game->menu_opentile_ok ? &game->menu_opentile : NULL;
    ng->woodpanl = game->pedia_wood_ok ? &game->pedia_wood : NULL;
    ng->labels_txt = game->labels_ok ? &game->labels : NULL;
    uint8_t basic = COLONIZE_COL_BASIC;
    uint8_t hilite = COLONIZE_COL_HILITE;
    uint8_t select = COLONIZE_COL_SELECT;
    /* Bright selection border on DIFFICUL/NATIONS/CUSTOMIZ own palettes. */
    if (ng->phase == NEW_GAME_PHASE_AMERICA_CHOICE || ng->phase == NEW_GAME_PHASE_MAP_PICK) {
      /*
       * Black screen + OPENTILE popup: same palette/colors as @BEGINMENU
       * (OPENMENU / remapped WOODPANL @COLORS). Do not use terrain palette.
       */
      if (game->menu_bg_ok && game->menu_bg.has_palette) {
        *palette = game->menu_bg.palette;
      } else if (game->menu_opentile_ok && game->menu_opentile.has_palette) {
        *palette = game->menu_opentile.palette;
      }
      popup_cols = game->menu_popup_colors;
      basic = game->menu_col_basic;
      hilite = game->menu_col_hilite;
      select = game->menu_col_select;
    } else if (ng->phase == NEW_GAME_PHASE_DIFFICULTY && ng->difficul_ok) {
      hilite = assets_palette_nearest_rgb(&ng->difficul_pik.palette, 0, 255, 0);
      basic = assets_palette_nearest_rgb(&ng->difficul_pik.palette, 0, 180, 0);
    } else if (ng->phase == NEW_GAME_PHASE_NATION && ng->nations_ok) {
      hilite = assets_palette_nearest_rgb(&ng->nations_pik.palette, 0, 255, 0);
      basic = assets_palette_nearest_rgb(&ng->nations_pik.palette, 0, 180, 0);
    } else if (ng->phase == NEW_GAME_PHASE_CUSTOMIZE && ng->customiz_ok) {
      /* Focused column yellow; other selection rects green (DOS 0xe / 0xa). */
      hilite = assets_palette_nearest_rgb(&ng->customiz_pik.palette, 255, 255, 0);
      basic = assets_palette_nearest_rgb(&ng->customiz_pik.palette, 0, 220, 0);
    } else if (
      (ng->phase == NEW_GAME_PHASE_LEADER_NAME || ng->phase == NEW_GAME_PHASE_NATION_LORE_A ||
       ng->phase == NEW_GAME_PHASE_NATION_LORE_B) &&
      game->pedia_wood_ok && game->pedia_wood.has_palette
    ) {
      basic = assets_palette_nearest_rgb(&game->pedia_wood.palette, 40, 140, 40);
      hilite = assets_palette_nearest_rgb(&game->pedia_wood.palette, 220, 220, 40);
    }
    new_game_render(
      ng,
      framebuffer,
      palette,
      &popup_cols,
      basic,
      hilite,
      select
    );
    return true;
  }

  if (game->in_menu) {
    if (game->menu_bg_ok) {
      memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
      pik_blit(&game->menu_bg, framebuffer, 0, 0);
    } else {
      memset(framebuffer->pixels, 1, (size_t)framebuffer->width * (size_t)framebuffer->height);
      for (int y = 20; y < 180; ++y) {
        for (int x = 40; x < 280; ++x) {
          framebuffer->pixels[y * framebuffer->width + x] = 0;
        }
      }
    }
    game_render_begin_menu(game, framebuffer);
    if (game->save_load.open || game->ai_popups.open) {
      const ColonizeFont* font = game->intro_font_ok ? &game->intro_font :
                                 (game->colony_font_ok ? &game->colony_font :
                                  (game->menu_font_ok ? &game->menu_font : NULL));
      const ColonizeSpriteSheet* wood =
        game->menu_opentile_ok ? &game->menu_opentile :
        ((game->map_panel_ok && game->map_panel.wood_ok) ? &game->map_panel.wood_tile : NULL);
      /* bugs.md: OPENMENU.PIK embeds its own palette — the WOODPANL @COLORS
       * indices land on light yellow / wrong greens here. Use the RGB-
       * remapped title-menu set (menu_popup_colors / menu_col_*), same as
       * the begin menu itself. */
      if (game->save_load.open) {
        save_load_render(
          (SaveLoadDialog*)&game->save_load,
          font,
          wood,
          &game->menu_popup_colors,
          game->menu_col_basic,
          game->menu_col_select,
          framebuffer
        );
      }
      if (game->ai_popups.open) {
        ai_popup_render(
          (AiPopupState*)&game->ai_popups,
          font,
          wood,
          &game->menu_popup_colors,
          game->menu_col_basic,
          game->menu_col_hilite,
          game->menu_col_select,
          framebuffer
        );
      }
    }
    return true;
  }
  return false;
}

/* Terrain / fog / colonies / tribes / units composited at native 16px per
 * tile into the offscreen zoom buffer, then decimated to the 240x192 view. */
COLONIZE_INTERNAL void game_render_map_composite(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, int map_zoom, int view_x,
  int view_y, int view_cols, int view_rows
) {
  const int tile_w = MAP_ZOOM_NATIVE_TILE; /* offscreen compositing stays native 16px/tile */
  const int tile_h = MAP_ZOOM_NATIVE_TILE;
  const int map_origin_x = 0;
  const int map_origin_y = 0;
  /*
   * DOS redraws the viewport at 16>>zoom px/tile per FUN_6ba1_000c. This port
   * instead reuses the zoom-0 tile compositor unchanged, drawing the wider
   * (15<<zoom × 12<<zoom) tile grid at native 16px/tile into an offscreen
   * buffer, then nearest-neighbor-decimates it into the fixed 240×192
   * on-screen viewport below — same visual result (more tiles, smaller),
   * different graphics pipeline, per the architectural gap noted in
   * docs/port_plan.md.
   */
  static uint8_t s_map_zoom_buf[MAP_ZOOM_MAX_VIEW_COLS * MAP_ZOOM_NATIVE_TILE *
                                 MAP_ZOOM_MAX_VIEW_ROWS * MAP_ZOOM_NATIVE_TILE];
  ColonizeFramebuffer8 zoom_fb = {
    .width = view_cols * tile_w, .height = view_rows * tile_h, .pixels = s_map_zoom_buf
  };
  memset(zoom_fb.pixels, 0, (size_t)zoom_fb.width * (size_t)zoom_fb.height);
  ColonizeFramebuffer8* const framebuffer_screen = framebuffer;
  framebuffer = &zoom_fb;

  if (game->terrain_ok && game->terrain.sprite_count > 0) {
    for (int sy = 0; sy < view_rows; ++sy) {
      for (int sx = 0; sx < view_cols; ++sx) {
        int base_sprite;
        ColonizeMapLayerCmd cmds[MAP_LAYER_CMDS_MAX];
        int ncmd = 0;
        if (game->world_map_ok) {
          const int mx = view_x + sx;
          const int my = view_y + sy;
          if (mx < 0 || my < 0 || mx >= game->world_map.width || my >= game->world_map.height) {
            continue;
          }
          if (!map_tile_seen_by(&game->world_map, mx, my, game_fog_nation(game))) {
            /*
             * bugs.md: DOS fog of war is a textured blue, not black. The
             * unseen branch of DOS FUN_6ba1_0938 (asm 6ba1:09a2) blits
             * sprite 0x95 (1-based) = PHYS0 #148 — a 16x16 noise fill of
             * palette 60..62, one shade darker than the ocean's 58..60.
             */
            if (game->phys0_ok && 148 < game->phys0.sprite_count) {
              blit_map_sprite(
                &game->phys0, 148, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
              );
              /* FUN_6ba1_0938 unseen tail: seen neighbours dither their
               * terrain onto the fog tile (mask 104+q + fill into holes). */
              const int fog_n2 = game_fog_nation(game);
              const int redges = map_fog_reveal_edge_count(&game->world_map, mx, my, fog_n2);
              for (int ei = 0; ei < redges; ++ei) {
                const int mask =
                  map_fog_reveal_edge_mask_sprite_at(&game->world_map, mx, my, fog_n2, ei);
                const int fill =
                  map_fog_reveal_edge_fill_sprite_at(&game->world_map, mx, my, fog_n2, ei);
                if (mask >= 0) {
                  blit_map_sprite(
                    &game->phys0, mask, framebuffer, sx, sy, tile_w, tile_h, map_origin_x,
                    map_origin_y
                  );
                }
                if (fill >= 0 && fill < game->terrain.sprite_count) {
                  blit_map_sprite_where_dest(
                    &game->terrain, fill, framebuffer, sx, sy, tile_w, tile_h, map_origin_x,
                    map_origin_y, 0
                  );
                }
              }
            }
            continue;
          }
          ncmd = map_tile_layer_cmds(
            &game->world_map, mx, my, game->terrain_peel_phase, cmds, MAP_LAYER_CMDS_MAX
          );
          base_sprite = ncmd > 0 ? cmds[0].sprite : -1;
        } else {
          base_sprite = (view_x + sx + view_y + sy + (int)game->map_seed) % game->terrain.sprite_count;
        }
        if (base_sprite < 0 || base_sprite >= game->terrain.sprite_count) {
          base_sprite = 0;
        }
        blit_map_sprite(
          &game->terrain, base_sprite, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
        );

        if (game->phys0_ok && game->world_map_ok) {
          const int mx = view_x + sx;
          const int my = view_y + sy;
          for (int ci = 1; ci < ncmd; ++ci) {
            const ColonizeMapLayerCmd* cmd = &cmds[ci];
            const ColonizeSpriteSheet* sheet =
              (cmd->sheet == MAP_LAYER_SHEET_TERRAIN) ? &game->terrain : &game->phys0;
            if (cmd->sprite < 0 || cmd->sprite >= sheet->sprite_count) {
              continue;
            }
            if (cmd->into_holes) {
              blit_map_sprite_where_dest(
                sheet, cmd->sprite, framebuffer, sx, sy, tile_w, tile_h, map_origin_x,
                map_origin_y, 0
              );
            } else if (cmd->offset) {
              blit_map_sprite_offset(
                sheet, cmd->sprite, framebuffer, sx, sy, tile_w, tile_h, map_origin_x,
                map_origin_y, cmd->ox, cmd->oy
              );
            } else {
              blit_map_sprite(
                sheet, cmd->sprite, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
              );
            }
          }
          /* Fog transitional edges toward unseen: PHYS0 104-107 mask, then —
           * as DOS FUN_6ba1_06e0 always does — the unseen neighbour's own
           * terrain filled into the mask's colour-0 holes, so the boundary
           * dithers instead of staying black (bugs.md). */
          {
            const int fog_n = game_fog_nation(game);
            const int edges = map_fog_edge_count(&game->world_map, mx, my, fog_n);
            for (int ei = 0; ei < edges; ++ei) {
              const int mask = map_fog_edge_mask_sprite_at(&game->world_map, mx, my, fog_n, ei);
              const int fill = map_fog_edge_fill_sprite_at(&game->world_map, mx, my, fog_n, ei);
              if (mask >= 0) {
                blit_map_sprite(
                  &game->phys0, mask, framebuffer, sx, sy, tile_w, tile_h, map_origin_x, map_origin_y
                );
              }
              /* Fill through the mask sprite's own colour-0 holes, never a
               * bare dest==0 match — earlier overlays (resource sprites at
               * the fog edge) legitimately contain colour 0 and must keep
               * their pixels. */
              if (mask >= 0 && fill >= 0 && fill < game->terrain.sprite_count) {
                blit_map_sprite_fill_mask_holes(
                  &game->terrain, fill, &game->phys0, mask, framebuffer, sx, sy, tile_w, tile_h,
                  map_origin_x, map_origin_y
                );
              }
            }
          }
        }
      }
    }
  } else {
    for (int y = map_origin_y; y < framebuffer->height; ++y) {
      for (int x = 0; x < framebuffer->width; ++x) {
        const int idx = y * framebuffer->width + x;
        uint8_t base = (uint8_t)(((x / 8) ^ (y / 8) ^ (int)game->turn_number) & 0x0f);
        framebuffer->pixels[idx] = (uint8_t)(16 + ((base + game->map_seed) & 0x0f));
      }
    }
  }

  /* Hidden Terrain phase 1+ (VIEW ~Hidden Terrain): units/settlements peeled first, stay off. */
  if (game->terrain_peel_phase == 0 && (game->colonies_ok || game->colonies.colony_count > 0)) {
    colonies_render_on_map(
      &game->colonies,
      game->unit_icons_ok ? &game->unit_icons : NULL,
      framebuffer,
      game->intro_font_ok ? &game->intro_font : NULL,   /* FONTINTR — name */
      game->colony_font_ok ? &game->colony_font : NULL, /* FONTTINY — pop badge */
      view_x,
      view_y,
      view_cols,
      view_rows,
      tile_w,
      tile_h,
      map_origin_x,
      map_origin_y,
      game->world_map_ok ? &game->world_map : NULL,
      game_fog_nation(game),
      game->map_palette_ok ? &game->map_palette : NULL
    );
  }

  if (game->terrain_peel_phase == 0 && game->col1_ok && game->unit_icons_ok) {
    map_panel_render_tribes_on_map_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(game->units_ok ? &game->units : NULL), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(game->world_map_ok ? &game->world_map : NULL), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true}, &game->unit_icons, framebuffer, view_x, view_y, view_cols, view_rows, tile_w, tile_h, map_origin_x, map_origin_y, game_fog_nation(game), game->map_palette_ok ? &game->map_palette : NULL);
  }

  if (game->terrain_peel_phase == 0 && game->units_ok && game->unit_icons_ok) {
    /* Half-period 500ms → full blink cycle 1s (was 250ms / 500ms cycle).
     * Frozen on while the map's input loop is not the thing running. */
    const bool blink_on =
      !game_map_blink_running(game) || ((game->elapsed_ms / 500u) % 2u) == 0u;
    const ColonizeFont* chrome_font = game_chrome_font(game);
    units_render_on_map(
      &game->units,
      &game->unit_icons,
      chrome_font,
      framebuffer,
      view_x,
      view_y,
      view_cols,
      view_rows,
      tile_w,
      tile_h,
      map_origin_x,
      map_origin_y,
      blink_on,
      game->world_map_ok ? &game->world_map : NULL,
      game_fog_nation(game),
      game->map_palette_ok ? &game->map_palette : NULL
    );
  }

  /* Decimate the native-res composite down to the fixed 240×192 on-screen
   * viewport (step = 1<<zoom is exact — nominal view width in screen px is
   * (15<<zoom)*(16>>zoom) = 240 at every tier, so this always lands in-bounds). */
  framebuffer = framebuffer_screen;
  {
    const int step = 1 << map_zoom;
    for (int sy = 0; sy < MAP_VIEW_H; ++sy) {
      const int src_y = sy * step;
      if (src_y >= zoom_fb.height) {
        continue;
      }
      const uint8_t* src_row = &zoom_fb.pixels[(size_t)src_y * (size_t)zoom_fb.width];
      uint8_t* dst_row = &framebuffer->pixels[(size_t)(MAP_MENU_BAR_H + sy) * (size_t)framebuffer->width];
      for (int sx = 0; sx < MAP_VIEW_W; ++sx) {
        const int src_x = sx * step;
        if (src_x < zoom_fb.width) {
          dst_row[sx] = src_row[src_x];
        }
      }
    }
  }
}

/* Post-decimation map overlays: the blinking tile cursor and the two CHEAT
 * debug layers (Show Strategy, Show Colony Sites). */
COLONIZE_INTERNAL void game_render_map_overlays(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, int view_x, int view_y,
  int view_cols, int view_rows, int screen_tile_px
) {
  /*
   * Map tile cursor: blinking white outline only in tile-select mode (no unit selected).
   * CURSOR.SS is the OS mouse pointer, not a tile overlay. Drawn post-decimation at
   * on-screen tile size (16>>zoom) so the outline stays crisp at every zoom tier.
   */
  if (game->units.selected_id < 0) {
    const int sx = game->map_cursor_x - view_x;
    const int sy = game->map_cursor_y - view_y;
    if (sx >= 0 && sy >= 0 && sx < view_cols && sy < view_rows) {
      const bool blink_on =
        !game_map_blink_running(game) || ((game->elapsed_ms / 250u) % 2u) == 0u;
      if (blink_on) {
        const int cx0 = sx * screen_tile_px;
        const int cy0 = MAP_MENU_BAR_H + sy * screen_tile_px;
        for (int y = cy0; y < cy0 + screen_tile_px; ++y) {
          for (int x = cx0; x < cx0 + screen_tile_px; ++x) {
            if (x < 0 || y < 0 || x >= framebuffer->width || y >= framebuffer->height) {
              continue;
            }
            if (x == cx0 || x == cx0 + screen_tile_px - 1 || y == cy0 ||
                y == cy0 + screen_tile_px - 1) {
              framebuffer->pixels[y * framebuffer->width + x] = 15;
            }
          }
        }
      }
    }
  }

  /*
   * CHEAT Show Strategy: each AI nation's top-priority goal slot
   * (ai_goals_primary — AI_GOAL_* code + target tile), labelled at that
   * tile. Surfaces the live AI planner state the DOS debug tool exposed.
   */
  if (game->debug_show_strategy) {
    const ColonizeFont* dbg_font =
      game->colony_font_ok ? &game->colony_font : (game->menu_font_ok ? &game->menu_font : NULL);
    if (dbg_font) {
      static const char k_nation_letter[4] = {'E', 'F', 'S', 'D'};
      for (int n = 0; n < 4; ++n) {
        if (n == game->human_nation) {
          continue;
        }
        const AiGoalSlot* g = ai_goals_primary(n, 0);
        if (!g || g->code == AI_GOAL_EMPTY) {
          continue;
        }
        const int tx = g->x - view_x;
        const int ty = g->y - view_y;
        if (tx < 0 || ty < 0 || tx >= view_cols || ty >= view_rows) {
          continue;
        }
        const char* code_name = "?";
        switch (g->code) {
          case AI_GOAL_CONTACT: code_name = "CONTACT"; break;
          case AI_GOAL_FOUND: code_name = "FOUND"; break;
          case AI_GOAL_LABOR: code_name = "LABOR"; break;
          case AI_GOAL_MILITARY: code_name = "MILIT"; break;
          case AI_GOAL_COLONY: code_name = "COLONY"; break;
          case AI_GOAL_MIL_EXPAND: code_name = "MILEXP"; break;
          case AI_GOAL_COLONY_ALT: code_name = "COLONY2"; break;
          default: break;
        }
        char label[16];
        snprintf(label, sizeof(label), "%c:%s", k_nation_letter[n], code_name);
        font_draw_text(
          dbg_font, framebuffer, tx * screen_tile_px, MAP_MENU_BAR_H + ty * screen_tile_px, label, 15
        );
      }
    }
  }

  /*
   * CHEAT Show Colony Sites: each AI nation's cached best founding tile
   * (ai_goals_best_found_tile), marked at that tile.
   */
  if (game->debug_show_colony_sites) {
    const ColonizeFont* dbg_font =
      game->colony_font_ok ? &game->colony_font : (game->menu_font_ok ? &game->menu_font : NULL);
    if (dbg_font) {
      static const char k_nation_letter[4] = {'E', 'F', 'S', 'D'};
      for (int n = 0; n < 4; ++n) {
        int bx = 0;
        int by = 0;
        if (!ai_goals_best_found_tile(n, &bx, &by)) {
          continue;
        }
        const int tx = bx - view_x;
        const int ty = by - view_y;
        if (tx < 0 || ty < 0 || tx >= view_cols || ty >= view_rows) {
          continue;
        }
        char label[4];
        snprintf(label, sizeof(label), "%c*", k_nation_letter[n]);
        font_draw_text(
          dbg_font, framebuffer, tx * screen_tile_px, MAP_MENU_BAR_H + ty * screen_tile_px, label, 14
        );
      }
    }
  }
}

/* Right-hand info panel (map_panel_render). */
COLONIZE_INTERNAL void game_render_map_panel(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, int view_x, int view_y,
  int view_cols, int view_rows
) {
  if (game->map_panel_ok) {
    const ColonizeFont* panel_font = game->colony_font_ok ? &game->colony_font :
                                     (game->menu_font_ok ? &game->menu_font : NULL);
    map_panel_render_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(game->units_ok ? &game->units : NULL), .colonies=(ColonizeColonyPool*)(game->colonies_ok || game->colonies.colony_count > 0 ? &game->colonies : NULL), .map=(ColonizeWorldMap*)(game->world_map_ok ? &game->world_map : NULL), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL)}, &game->map_panel, game->unit_icons_ok ? &game->unit_icons : NULL, panel_font, game->names_ok ? &game->names : NULL, game->labels_ok ? &game->labels : NULL, view_x, view_y, view_cols, view_rows, game->map_cursor_x, game->map_cursor_y, game->units.selected_id, game_fog_nation(game), game->game_year, game->game_autumn, game->europe.gold, game->europe.tax_percent, game->europe.nation_name, game->map_palette_ok ? &game->map_palette : NULL, game_end_turn_prompt_active(game), !game_map_blink_running(game) || (game->elapsed_ms / 250u) % 2u == 0u, framebuffer);
  }
}

/* Map menu bar and every dialog that floats over the overland map. */
COLONIZE_INTERNAL void game_render_map_dialogs(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette
) {
  const ColonizeFont* hud_font = game->colony_font_ok ? &game->colony_font :
                                 (game->menu_font_ok ? &game->menu_font : NULL);
  if (!game_screen_owns_display(game)) {
    const ColonizeSpriteSheet* wood =
      (game->map_panel_ok && game->map_panel.wood_ok) ? &game->map_panel.wood_tile : NULL;
    map_menu_render((MapMenuBar*)&game->map_menu, hud_font, wood, framebuffer);
    if (game->trade_screen.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      trade_screen_render(
        &game->trade_screen,
        game->col1_ok ? &game->col1 : NULL,
        game->colonies_ok ? &game->colonies : NULL,
        game_trade_europe_label(game),
        popup_font,
        wood,
        game->unit_icons_ok ? &game->unit_icons : NULL,
        &popup_cols,
        framebuffer
      );
    }
    if (game->pick_music.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      pick_music_render(
        (PickMusicDialog*)&game->pick_music,
        /* @smallfont → FONTTINY, else the generic dialog font (game_loop's
         * @BEGINMENU rule); the hit-test above picks the same font. */
        pick_music_font(
          &game->pick_music,
          game->colony_font_ok ? &game->colony_font : NULL,
          game->intro_font_ok ? &game->intro_font
                              : (game->menu_font_ok ? &game->menu_font : NULL)
        ),
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->save_load.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      /* DOS uses the FONTINTR dialog font here, not the FONTTINY HUD one. */
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      save_load_render(
        (SaveLoadDialog*)&game->save_load,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->cheat_list.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      cheat_list_render(
        (CheatListDialog*)&game->cheat_list,
        hud_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->options_dlg.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      options_dialog_render(
        (OptionsDialog*)&game->options_dlg,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_HILITE,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->combat_analysis.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      /*
       * DOS FUN_636c_0000 sets no font of its own — it draws with whatever
       * the map HUD left current, which is FONTTINY (DS:0x89e). Using the
       * FONTINTR dialog font here also blew up the header unit chrome: the
       * orders box sizes itself from the font metrics (unit_chrome.c
       * box_w/box_h), so the taller glyph cell made an oversized badge.
       */
      const ColonizeFont* popup_font = hud_font;
      combat_analysis_render(
        (CombatAnalysisDialog*)&game->combat_analysis,
        popup_font,
        wood,
        game->unit_icons_ok ? &game->unit_icons : NULL,
        /* Ambush/Terrain row icon: DOS 636c blits the engagement tile itself. */
        game->terrain_ok ? &game->terrain : NULL,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        game->map_palette_ok ? &game->map_palette : NULL,
        framebuffer
      );
    }
    game_render_modal_overlays(game, hud_font, framebuffer);
    if (game->ai_popups.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      /* Map dialogs use FONTINTR (nation-explanation size), not FONTTINY HUD. */
      const ColonizeFont* popup_font =
        game->intro_font_ok ? &game->intro_font
        : (game->menu_font_ok ? &game->menu_font : hud_font);
      ai_popup_render(
        (AiPopupState*)&game->ai_popups,
        popup_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_HILITE,
        COLONIZE_COL_SELECT,
        framebuffer
      );
    }
    if (game->unit_stack.open) {
      ColonizePopupColors popup_cols;
      popup_colors_from_ui(&popup_cols);
      unit_stack_render(
        (UnitStackPopup*)&game->unit_stack,
        &game->units,
        game->unit_icons_ok ? &game->unit_icons : NULL,
        &game->names,
        hud_font,
        wood,
        &popup_cols,
        COLONIZE_COL_BASIC,
        COLONIZE_COL_SELECT,
        palette,
        framebuffer
      );
    }
  }
}

/* Overland map view: geometry, then composite / overlays / panel / dialogs. */
COLONIZE_INTERNAL void game_render_map(
  const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette
) {
  /* Map view: scrollable world map (15<<zoom × 12<<zoom tiles) left of the right info panel. */
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);

  const int map_zoom = game_map_zoom_clamp(game->map_zoom);
  int view_cols = 0;
  int view_rows = 0;
  game_map_zoom_view_size(map_zoom, &view_cols, &view_rows);
  const int screen_tile_px = game_map_zoom_tile_px(map_zoom);

  int view_x = 0;
  int view_y = 0;
  if (game->world_map_ok) {
    map_panel_clamp_view_origin(
      (int)game->world_map.width,
      (int)game->world_map.height,
      game->map_view_x,
      game->map_view_y,
      view_cols,
      view_rows,
      &view_x,
      &view_y
    );
  }

  game_render_map_composite(game, framebuffer, map_zoom, view_x, view_y, view_cols, view_rows);
  game_render_map_overlays(
    game, framebuffer, view_x, view_y, view_cols, view_rows, screen_tile_px
  );
  game_render_map_panel(game, framebuffer, view_x, view_y, view_cols, view_rows);
  game_render_map_dialogs(game, framebuffer, palette);
}

void game_render(const ColonizeGameState* game, ColonizeFramebuffer8* framebuffer, ColonizePalette* palette) {
  static uint32_t render_log_counter = 0;
  if (!game || !framebuffer || !palette || !framebuffer->pixels) {
    return;
  }

  if (game_render_fullscreen_takeover(game, framebuffer, palette)) {
    return;
  }

  game_render_select_palette(game, framebuffer, palette, render_log_counter);

  if (!game_render_screen(game, framebuffer, palette)) {
    game_render_map(game, framebuffer, palette);
  }

  /* Whose-turn box lives on the map sidebar only — never over a full-screen
   * view (colony, Europe, reports, pedia, ...) that covers the sidebar. */
  if (!game_screen_owns_display(game) && turn_processor_show_indicator(&game->turn_proc)) {
    turn_draw_owner_indicator(framebuffer, game->active_turn_nation);
  }
  /* Debug measure: original-resolution pixel under the pointer (for layout). */
  if (game->debug_show_mouse_coords) {
    const ColonizeFont* font = game->colony_font_ok ? &game->colony_font
      : (game->menu_font_ok ? &game->menu_font : NULL);
    const int mx = game->debug_mouse_x;
    const int my = game->debug_mouse_y;
    if (font && mx >= 0 && my >= 0 && mx < framebuffer->width && my < framebuffer->height) {
      char label[24];
      snprintf(label, sizeof(label), "%d,%d", mx, my);
      int tx = mx + 12;
      int ty = my + 2;
      if (tx > framebuffer->width - 40) {
        tx = mx - 40;
      }
      if (ty > framebuffer->height - 8) {
        ty = framebuffer->height - 8;
      }
      if (tx < 0) {
        tx = 0;
      }
      if (ty < 0) {
        ty = 0;
      }
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          font_draw_text(font, framebuffer, tx + dx, ty + dy, label, 0);
        }
      }
      font_draw_text(font, framebuffer, tx, ty, label, 15);
    }
  }
  render_log_counter++;
  if (render_log_counter == 1 || render_log_counter == 60 || render_log_counter == 300) {
    const int cx = framebuffer->width / 2;
    const int cy = framebuffer->height / 2;
    const int idx_center = cy * framebuffer->width + cx;
    char mode_name[48];
    game_screen_name(game, mode_name, sizeof(mode_name));
    diag_info(
      "Framebuffer sample frame=%u mode=%s idx0=%u idx_center=%u turn=%u cursor=%d,%d",
      render_log_counter,
      mode_name,
      framebuffer->pixels[0],
      framebuffer->pixels[idx_center],
      game->turn_number,
      game->map_cursor_x,
      game->map_cursor_y
    );
  }
}

const char* game_status_text(const ColonizeGameState* game) {
  if (!game) {
    return "Colonization Linux Port";
  }
  /* Status feeds the window title — plain text, so eat any {} emphasis
   * markup a GAME.TXT-sourced line carried along. */
  static char plain[sizeof(((ColonizeGameState*)0)->status)];
  snprintf(plain, sizeof(plain), "%s", game->status);
  popup_msg_strip_markup(plain);
  return plain;
}

void game_apply_mouse_cursor(
  ColonizeGameState* game,
  ColonizePlatform* platform,
  int mouse_x,
  int mouse_y
) {
  if (!game || !platform) {
    return;
  }

  game->platform = platform;
  game->debug_mouse_x = mouse_x;
  game->debug_mouse_y = mouse_y;

  const ColonizePalette* pal = NULL;
  /* Map go-to uses CURSOR.SS #1; cargo/unit icons use ICONS.SS palette. */
  if (ui_drag_active(&game->ui_drag) && game->ui_drag.kind == UI_DRAG_MAP_GOTO) {
    if (game->cursor_ok && game->cursor.has_palette) {
      pal = &game->cursor.palette;
    }
  } else if (ui_drag_active(&game->ui_drag) && game->ui_drag.cursor_ok) {
    if (game->unit_icons_ok && game->unit_icons.has_palette) {
      pal = &game->unit_icons.palette;
    } else if (game->in_europe && game->europe_ok && game->europe.background.has_palette) {
      pal = &game->europe.background.palette;
    } else if (game->in_colony && game->colony_screen_ok && game->colony_screen.frame.has_palette) {
      pal = &game->colony_screen.frame.palette;
    }
  }
  if (!pal) {
    if (game->cursor_ok && game->cursor.has_palette) {
      pal = &game->cursor.palette;
    } else if (game->map_palette_ok) {
      pal = &game->map_palette;
    } else if (game->palette_ok) {
      pal = &game->palette;
    }
  }

  if (pal && game->cursor_ok) {
    ui_drag_apply_cursor(
      &game->ui_drag, platform, pal, &game->cursor, &game->mouse_cursor_built
    );
  }

  /* Game cursor over the full 320x200 frame on every screen (menu, map, reports…). */
  const bool on_game_frame =
    mouse_x >= 0 && mouse_x < 320 && mouse_y >= 0 && mouse_y < 200;

  if (on_game_frame && game->mouse_cursor_built) {
    platform_show_game_mouse_cursor(platform, true);
  } else if (on_game_frame && ui_drag_active(&game->ui_drag) && game->ui_drag.cursor_ok) {
    platform_show_game_mouse_cursor(platform, true);
  } else {
    platform_show_game_mouse_cursor(platform, false);
  }
}

bool game_assets_ok(const ColonizeGameState* game) {
  return game && game->assets_ok;
}

const char* game_assets_error(const ColonizeGameState* game) {
  return (game && game->assets_error[0]) ? game->assets_error : "";
}

bool game_in_menu(const ColonizeGameState* game) {
  return game && game->in_menu;
}

bool game_in_new_game(const ColonizeGameState* game) {
  return game && new_game_active(&game->new_game);
}

bool game_in_hall_of_fame(const ColonizeGameState* game) {
  return game && game->in_hall_of_fame;
}

int game_human_nation(const ColonizeGameState* game) {
  return game ? game->human_nation : 0;
}

int game_difficulty(const ColonizeGameState* game) {
  return game ? game->difficulty : 0;
}

const char* game_leader_name(const ColonizeGameState* game) {
  return game ? game->leader_name : "";
}

int game_hof_count(const ColonizeGameState* game) {
  if (!game) {
    return 0;
  }
  return game->hof_count > COLONIZE_HOF_MAX ? COLONIZE_HOF_MAX : game->hof_count;
}

bool game_hof_entry(const ColonizeGameState* game, int index, ColonizeHofEntryView* out) {
  if (!game || !out || index < 0 || index >= game_hof_count(game)) {
    return false;
  }
  const ColonizeHofEntry* e = &game->hof_entries[index];
  snprintf(out->leader, sizeof(out->leader), "%s", e->leader);
  snprintf(out->nation, sizeof(out->nation), "%s", e->nation);
  out->score = e->score;
  out->year = e->year;
  out->difficulty = e->difficulty;
  out->rating = e->rating;
  out->declared = e->declared;
  out->achieved = e->achieved;
  return true;
}

/* --- Headless play-smoke probes (see game_loop.h). --- */

bool game_in_colony_screen(const ColonizeGameState* game) {
  return game && game->in_colony;
}

bool game_in_europe_screen(const ColonizeGameState* game) {
  return game && game->in_europe;
}

bool game_modal_open(const ColonizeGameState* game) {
  if (!game) {
    return false;
  }
#define GAME_MODAL_OPEN_ARM(field, label)                                                          \
  if (game->field.open) {                                                                          \
    return true;                                                                                   \
  }
  GAME_MODAL_LIST(GAME_MODAL_OPEN_ARM)
#undef GAME_MODAL_OPEN_ARM
  return false;
}

bool game_ai_popup_pending(const ColonizeGameState* game) {
  return game && ai_popup_busy(&game->ai_popups) && !game->ai_popups.open;
}

int game_ai_popup_tag(const ColonizeGameState* game) {
  if (!game || !game->ai_popups.open) {
    return -1;
  }
  return (int)game->ai_popups.current.tag;
}

bool game_save_dialog_open(const ColonizeGameState* game) {
  return game && game->save_load.open;
}

bool game_turn_busy(const ColonizeGameState* game) {
  return game && turn_processor_active(&game->turn_proc);
}

uint32_t game_turn_number(const ColonizeGameState* game) {
  return game ? game->turn_number : 0;
}

int game_colony_count(const ColonizeGameState* game) {
  return (game && game->colonies_ok) ? game->colonies.colony_count : 0;
}

/* `index` counts LIVE colonies (the same population game_colony_count reports).
 * colonies_abandon zeroes its slot in place and recounts, so the pool holds
 * holes and a raw slot index would both skip dead slots and miss a live tail. */
bool game_colony_pos(const ColonizeGameState* game, int index, int* x, int* y) {
  if (!game || !game->colonies_ok || index < 0) {
    return false;
  }
  int seen = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* col = &game->colonies.colonies[i];
    if (!col->active) {
      continue;
    }
    if (seen++ != index) {
      continue;
    }
    if (x) {
      *x = col->x;
    }
    if (y) {
      *y = col->y;
    }
    return true;
  }
  return false;
}

int game_selected_unit(const ColonizeGameState* game) {
  return (game && game->units_ok) ? game->units.selected_id : -1;
}

bool game_unit_info(
  const ColonizeGameState* game, int unit_id, int* x, int* y, bool* is_sea, int* moves
) {
  if (!game || !game->units_ok) {
    return false;
  }
  const ColonizeUnit* u = units_get_const(&game->units, unit_id);
  if (!u || !u->active) {
    return false;
  }
  if (x) {
    *x = u->x;
  }
  if (y) {
    *y = u->y;
  }
  if (is_sea) {
    *is_sea = units_is_sea(&game->units, unit_id);
  }
  if (moves) {
    *moves = u->moves;
  }
  return true;
}
