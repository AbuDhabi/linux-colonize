#include "core/internal.h"
#include "core/game_loop.h"

/*
 * Autoplay (DS:0x826) and the self-running demo (DS:0x828), docs/demo.md.
 *
 * Sections:
 *  - Pacing: the 6d8e watch, frame present and tick pause
 *  - Per-frame input filter and turn driver
 *  - Demo start, year tail and cycle end
 *
 * Cross-file seams are declared in core/game_loop_internal.h.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/map_panel.h"
#include "core/new_game.h"
#include "core/opening.h"
#include "core/savegame.h"
#include "core/sound.h"
#include "core/turn.h"
#include "core/units.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"
#include "core/game_dialogs.h"
#include "core/game_loop_internal.h"

/* 6d8e raw 93299-93305: 0x1e ticks after a watched unit's last act. */
#define GAME_AUTOPLAY_UNIT_PAUSE_MS GAME_DEMO_TICKS_MS(0x1eu)
/* FUN_2f2b_6cd4 raw 56125-56157 / FUN_38fd_55b6 raw 68207-68236: the demo's colony and
 * Europe views stay up until 600 ticks have passed. */
#define GAME_DEMO_SCREEN_MS GAME_DEMO_TICKS_MS(600u)
/* 130d_0290 raw 6494-6496 (asm 5BC2-5BD0): the cycle ends once the clock word reads
 * 0x3840 since VICEROY started. */
#define GAME_DEMO_CYCLE_MS GAME_DEMO_TICKS_MS(0x3840u)
/* FUN_75c2_2778 raw 121921: a COLONY05.SAV older than 1700 resumes. */
#define GAME_DEMO_RESUME_YEAR_MAX 0x6a4
/* 130d_0290 raw 6488-6490: 1725 (or the WoI) ends the cycle. */
#define GAME_DEMO_LAST_YEAR 0x6bd
#define GAME_DEMO_SLOT 5

/* ===================== Pacing: the 6d8e watch, frame present and tick pause ===================== */
/* DS:0x5396: the fixed view (DS:0x53a4) when one is set, else DS:0x5398. */
static int game_autoplay_view_nation(const ColonizeGameState* game) {
  if (game->col1_ok && game->col1.head.fixed_nation_map_view < 4u) {
    return (int)game->col1.head.fixed_nation_map_view;
  }
  return game->human_nation;
}

static void game_autoplay_present(ColonizeGameState* game) {
  static uint8_t* s_buf;
  static size_t s_cap;
  const size_t need = (size_t)screen_geom_w() * (size_t)screen_geom_h();
  if (need > s_cap) {
    uint8_t* grown = (uint8_t*)realloc(s_buf, need);
    if (!grown) {
      return;
    }
    s_buf = grown;
    s_cap = need;
  }
  ColonizeFramebuffer8 fb = {.width = screen_geom_w(), .height = screen_geom_h(), .pixels = s_buf};
  ColonizePalette pal;
  game_render(game, &fb, &pal);
  (void)platform_present(game->platform, &fb, &pal);
}

/* A key or click while autoplay runs. In the demo it waits in the BIOS buffer for the
 * next test (6d8e pass top, a demo screen); otherwise only a click counts, at the end
 * of the day (130d_0290 raw 6427-6432). */
static void game_autoplay_note_input(ColonizeGameState* game, const ColonizeInputState* in) {
  if (in->quit_requested) {
    game->elapsed_ms = UINT32_MAX;
  }
  if (game->demo) {
    if (game->demo_pending_key == 0 && colonize_input_any_key_or_click(in)) {
      /* asm 5322C: a click reads as Esc (0x1b). */
      game->demo_pending_key = in->text_input_len > 0 ? (unsigned char)in->text_input[0] : 0x1b;
    }
  } else if (in->mouse_left_clicked || in->mouse_right_clicked) {
    game->autoplay_clicked = true;
  }
}

/* The pause DOS spins on the clock word, with the window kept alive. */
static void game_autoplay_pause(ColonizeGameState* game, uint32_t ms) {
  for (uint32_t spent = 0; spent < ms && game->elapsed_ms != UINT32_MAX; spent += 16u) {
    ColonizeInputState in = {0};
    if (!platform_poll_input(game->platform, &in)) {
      game->elapsed_ms = UINT32_MAX;
      break;
    }
    game_autoplay_note_input(game, &in);
    game_autoplay_present(game);
    platform_sleep_ms(16);
  }
}

/* asm 53215-53266: the demo's key test at the top of each 6d8e pass. */
static void game_demo_take_key(ColonizeGameState* game) {
  if (game->demo_pending_key == 0) {
    return;
  }
  if (toupper(game->demo_pending_key) == 'J') {
    game->demo_end = true; /* DS:0x82b */
  } else {
    game->demo_quit = true; /* FUN_281f_05b6(5), DS:0x53c2 = 0 */
  }
  game->demo_pending_key = 0;
}

static bool game_autoplay_tile_on_screen(const ColonizeGameState* game, int x, int y) {
  int cols = 0;
  int rows = 0;
  game_map_zoom_view_size(game->map_zoom, &cols, &rows);
  int vx = 0;
  int vy = 0;
  map_panel_clamp_view_origin(
    (int)game->world_map.width, (int)game->world_map.height, game->map_view_x, game->map_view_y,
    cols, rows, &vx, &vy
  );
  return x >= vx && y >= vy && x < vx + cols && y < vy + rows;
}

/*
 * FUN_521d_6d8e raw 93258-93311 under DS:0x826, for the view nation (DS:0x5396) or with
 * the whole map shown (DS:0x53a2): before the act the cursor goes to an off-screen unit
 * (FUN_281f_0302) and the map redraws (FUN_281f_055e); after the unit's last act the
 * loop spins 0x1e ticks.
 */
void game_autoplay_watch(
  void* user, int nation_id, int unit_id, AiEuroWatchPhase phase, bool unit_done
) {
  ColonizeGameState* game = (ColonizeGameState*)user;
  if (!game || !game->autoplay || !game->platform || game->elapsed_ms == UINT32_MAX) {
    return;
  }
  if (phase == AI_EURO_WATCH_PASS) {
    if (game->demo) {
      game_demo_take_key(game);
    }
    return;
  }
  const bool watched =
    nation_id == game_autoplay_view_nation(game) ||
    (game->col1_ok && game->col1.head.show_entire_map);
  if (!watched) {
    return;
  }
  if (phase == AI_EURO_WATCH_BEFORE_ACT) {
    const ColonizeUnit* u = units_get_const(&game->units, unit_id);
    if (u && units_is_on_map(u) && !game_autoplay_tile_on_screen(game, u->x, u->y)) {
      game->map_cursor_x = u->x;
      game->map_cursor_y = u->y;
      game_set_view_center(game, u->x, u->y);
    }
    game_autoplay_present(game);
    return;
  }
  if (unit_done) {
    game_autoplay_pause(game, GAME_AUTOPLAY_UNIT_PAUSE_MS);
  }
}

/* ===================== Per-frame input filter and turn driver ===================== */
static void game_demo_quit_now(ColonizeGameState* game) {
  char err[256];
  if (!game_save_col1_slot(game, GAME_DEMO_SLOT, err, sizeof(err))) {
    diag_warn("demo: slot %d save failed: %s", GAME_DEMO_SLOT, err);
  }
  game->elapsed_ms = UINT32_MAX;
}

/* FUN_130d_019e (raw 6238-6257): save slot 5, then chain "opening -o d" — OPENING runs
 * again and hands back to VICEROY's demo start. */
static void game_demo_cycle_end(ColonizeGameState* game) {
  char err[256];
  if (!game_save_col1_slot(game, GAME_DEMO_SLOT, err, sizeof(err))) {
    diag_warn("demo: slot %d save failed: %s", GAME_DEMO_SLOT, err);
  }
  game->demo_end = false;
  game->demo_end_due = false;
  game->autoplay = false;
  if (!opening_open(&game->opening, game->resolved_data_dir)) {
    game_demo_start(game);
    return;
  }
  game->opening_just_opened = true;
}

/*
 * Called at the top of game_update. While autoplay runs no human input reaches the game
 * (DOS never enters Move Pieces, raw 46879): keys and clicks are only noted, a dialog
 * or key wait is answered after 0x78 ticks, the demo's timed screens close on their
 * own, and the next day starts as soon as the map is free. Returns the input the rest
 * of the frame sees.
 */
const ColonizeInputState* game_autoplay_filter_input(
  ColonizeGameState* game, const ColonizeInputState* input, ColonizeInputState* scratch
) {
  if (!game->autoplay && !(game->demo && new_game_active(&game->new_game))) {
    return input;
  }
  memset(scratch, 0, sizeof(*scratch));
  scratch->mouse_x = input->mouse_x;
  scratch->mouse_y = input->mouse_y;
  scratch->quit_requested = input->quit_requested;
  if (game->demo && new_game_active(&game->new_game)) {
    /* FUN_1262_02fe raw 3085-3098: a key or click on the sail pages under DS:0x828
     * leaves through FUN_1d1d_030d(3), the program exit, with nothing saved. */
    if (colonize_input_any_key_or_click(input)) {
      game->elapsed_ms = UINT32_MAX;
    }
    return scratch;
  }
  if (game->demo_screen_until_ms != 0) {
    if (colonize_input_any_key_or_click(input)) {
      game->demo_screen_until_ms = 0;
      game_demo_quit_now(game);
      return scratch;
    }
    if (game->elapsed_ms >= game->demo_screen_until_ms) {
      game->demo_screen_until_ms = 0;
      if (game->in_colony || game->in_europe) {
        scratch->last_key = COLONIZE_KEY_ESCAPE;
      }
    }
    return scratch;
  }
  game_autoplay_note_input(game, input);
  if (game->demo_quit) {
    game->demo_quit = false;
    game_demo_quit_now(game);
    return scratch;
  }
  const bool blocked = game_modal_open(game) || game->ai_popups.open || game->woodcut.open ||
                       ai_popup_bar_message(&game->ai_popups) != NULL ||
                       game_screen_owns_display(game);
  if (blocked) {
    if (game->autoplay_wait_ms == 0) {
      game->autoplay_wait_ms = game->elapsed_ms + GAME_AUTOPLAY_DIALOG_MS;
    } else if (game->elapsed_ms >= game->autoplay_wait_ms) {
      game->autoplay_wait_ms = 0;
      scratch->last_key = game_screen_owns_display(game) && !game_modal_open(game) &&
                              !game->ai_popups.open && !game->woodcut.open
                            ? COLONIZE_KEY_ESCAPE
                            : COLONIZE_KEY_ENTER;
    }
    return scratch;
  }
  game->autoplay_wait_ms = 0;
  if (game->demo && (game->demo_end || game->demo_end_due) &&
      !turn_processor_active(&game->turn_proc) && !ai_popup_busy(&game->ai_popups)) {
    game_demo_cycle_end(game);
    return scratch;
  }
  if (!turn_processor_active(&game->turn_proc) && game_turn_flow_allowed(game) &&
      !woodcut_has_pending()) {
    game_do_end_turn(game);
  }
  return scratch;
}

/* ===================== Demo start, year tail and cycle end ===================== */
/*
 * FUN_75c2_2778's DS:0x828 arm (raw 121917-121929), reached from the title step after
 * OPENING: COLONY05.SAV resumes when it reads and is older than 1700, else a New World
 * starts (FUN_75c2_235c(0)) and every Euro control byte turns 1 once it is built.
 */
void game_demo_start(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  game->in_menu = false;
  game->autoplay = true;
  game->demo_end = false;
  game->demo_end_due = false;
  game->demo_quit = false;
  game->demo_pending_key = 0;
  game->demo_splash = 0; /* DS:0x150, a fresh VICEROY each cycle */
  game->demo_cycle_start_ms = game->elapsed_ms;
  game->demo_screen_until_ms = 0;
  game->autoplay_wait_ms = 0;
  ColonizeSaveSlotInfo info;
  char err[256];
  if (savegame_probe_col1_slot(game->config.save_dir, GAME_DEMO_SLOT, &info) && info.occupied &&
      info.year < GAME_DEMO_RESUME_YEAR_MAX &&
      game_load_col1_slot(game, GAME_DEMO_SLOT, err, sizeof(err))) {
    diag_info("demo: resumed slot %d, year %u", GAME_DEMO_SLOT, (unsigned)info.year);
    game->units.selected_id = -1;
    game->view_pieces_mode = true;
    return;
  }
  /* FUN_75c2_10ae raw 120589-120593: DS:0x5398 = FUN_281f_04d4(0, 3). */
  const int nation = dos_rng_range(&game->move_rng, 0, 3);
  if (!new_game_begin_demo(
        &game->new_game, game->resolved_data_dir, &game->messages,
        game->names_ok ? &game->names : NULL, nation
      )) {
    diag_warn("demo: new game failed to start");
    game->autoplay = false;
    game->in_menu = true;
  }
}

/* FUN_75c2_2778 raw 121926-121929, after a demo New World is built. */
void game_demo_after_new_campaign(ColonizeGameState* game) {
  if (!game || !game->demo || !game->col1_ok) {
    return;
  }
  char err[256];
  if (!game_set_all_ai(game, err, sizeof(err))) {
    diag_warn("demo: %s", err);
  }
}

/*
 * The year loop's tail after each day (130d_0290 raw 6427-6432, 6467-6500). Autoplay
 * outside the demo stops on a click. The demo shows something every fourth turn — a
 * splash (ids up to 25 through FUN_12fd_006c) every twelfth, a colony of DS:0x5398
 * nearest a random point when turn % 3 == 1, else that power's Europe — then ends the
 * cycle on the War of Independence, after 1725, or once the clock has run 0x3840 ticks.
 */
void game_autoplay_after_turn(ColonizeGameState* game) {
  if (!game || !game->autoplay) {
    return;
  }
  if (!game->demo) {
    if (game->autoplay_clicked) {
      game->autoplay = false;
      game->autoplay_clicked = false;
    }
    return;
  }
  const int fixed = game->col1_ok ? col1_fixed_human(&game->col1, -1) : -1;
  const int nation = fixed >= 0 ? fixed : game->human_nation;
  const uint32_t turn = game->turn_number;
  if (turn % 4u == 0u) {
    if (turn % 3u == 0u) {
      while (game->demo_splash < 25) {
        ++game->demo_splash;
        if (woodcut_fire(game->col1_ok ? &game->col1 : NULL, game->demo_splash)) {
          break;
        }
      }
    } else if (turn % 3u == 1u) {
      /* asm 5B5D-5B95: y is drawn first, then x, both FUN_19ef_0032(2, size - 2). */
      const int y = dos_rng_range(&game->move_rng, 2, (int)game->world_map.height - 2);
      const int x = dos_rng_range(&game->move_rng, 2, (int)game->world_map.width - 2);
      const int idx = ai_goals_nearest_colony_15eb_0142(
        &game->world_map, &game->colonies, x, y, nation, -1, NULL
      );
      const ColonizeColony* c = idx >= 0 ? &game->colonies.colonies[idx] : NULL;
      if (c && c->active) {
        game_enter_colony(game, c->id);
        if (game->in_colony) {
          game->demo_screen_until_ms = game->elapsed_ms + GAME_DEMO_SCREEN_MS;
        }
      }
    } else if (game_try_enter_europe(game)) {
      game->demo_screen_until_ms = game->elapsed_ms + GAME_DEMO_SCREEN_MS;
    }
  }
  if ((game->col1_ok && game->col1.head.game_options.woi) ||
      (int)game->game_year > GAME_DEMO_LAST_YEAR) {
    game->demo_end = true;
  }
  if (game->demo_end || game->elapsed_ms - game->demo_cycle_start_ms >= GAME_DEMO_CYCLE_MS) {
    game->demo_end_due = true;
  }
}
