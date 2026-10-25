/*
 * Headless play smoke (port_plan P1.2): real COLONIZE assets, new game →
 * sail west → landfall → found colony → colony screen → Europe screen → save.
 * Every screen is rendered once so draw-path crashes surface here, not in
 * the user's session. Runs from the source dir (WORKING_DIRECTORY).
 */
#include <stdbool.h>
#include "../common/test_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include "core/ai_popup.h"
#include "core/game_loop.h"
#include "core/game_dialogs.h"
#include "core/savegame.h"

static ColonizeGameState* g_game;
static uint8_t g_pixels[320 * 200];

static bool frame(ColonizeKey key) {
  ColonizeInputState in = {0};
  in.last_key = key;
  return game_update(g_game, &in, 16);
}

static bool click(int x, int y) {
  ColonizeInputState in = {0};
  in.mouse_left_clicked = true;
  in.mouse_left_down = true;
  in.mouse_x = x;
  in.mouse_y = y;
  if (!game_update(g_game, &in, 16)) {
    return false;
  }
  memset(&in, 0, sizeof(in));
  in.mouse_left_released = true;
  in.mouse_x = x;
  in.mouse_y = y;
  return game_update(g_game, &in, 16);
}

static void render(const char* what) {
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = g_pixels};
  ColonizePalette pal;
  game_render(g_game, &fb, &pal);
  printf("  rendered %s\n", what);
}

/* Dismiss whatever modal is up; Landfall picks "Make Landfall" (choice 1). */
static bool dismiss_modal(void) {
  if (game_ai_popup_pending(g_game)) {
    return frame(COLONIZE_KEY_NONE); /* let it present; keys next frame */
  }
  if (game_ai_popup_tag(g_game) == AI_POPUP_TAG_LANDFALL) {
    if (!frame(COLONIZE_KEY_DOWN)) return false;
  }
  return frame(COLONIZE_KEY_ENTER);
}

static bool fail(const char* msg) {
  fprintf(stderr, "smoke_play: %s (status: %s)\n", msg, game_status_text(g_game));
  return false;
}

/* AI nations can found before the player (#530). The all-nation colony
 * count is not evidence that the human Build flow ran. */
static int human_colony_count(void) {
  return colonies_count_for_nation(&g_game->colonies, g_game->human_nation);
}

static bool run(void) {
  /* Title → NEW WORLD (index 0). */
  if (!frame(COLONIZE_KEY_ENTER)) return fail("enter NEW WORLD");
  if (!game_in_new_game(g_game)) return fail("wizard did not open");
  /* Wizard: Enter through every page; the sail cinematic only skips on click. */
  for (int i = 0; i < 80 && game_in_new_game(g_game); ++i) {
    if (!frame(COLONIZE_KEY_ENTER)) return fail("wizard enter");
    if (game_in_new_game(g_game) && !click(160, 100)) return fail("wizard click");
  }
  if (game_in_new_game(g_game) || game_in_menu(g_game)) return fail("wizard never committed");
  render("map after sail");

  /* Sail west until landfall, found a colony with the landed unit. */
  int last_x = -1, last_y = -1, stuck = 0;
  const ColonizeKey west[3] = {COLONIZE_KEY_KP4, COLONIZE_KEY_KP7, COLONIZE_KEY_KP1};
  int west_i = 0;
  for (int i = 0; i < 4000 && human_colony_count() == 0; ++i) {
    if (game_modal_open(g_game) || game_ai_popup_pending(g_game)) {
      if (!dismiss_modal()) return fail("modal dismiss");
      continue;
    }
    if (game_turn_busy(g_game)) {
      if (!frame(COLONIZE_KEY_NONE)) return fail("turn tick");
      continue;
    }
    const int sel = game_selected_unit(g_game);
    int x = 0, y = 0, mp = 0;
    bool sea = false;
    if (sel < 0 || !game_unit_info(g_game, sel, &x, &y, &sea, &mp)) {
      /* Nothing selected: End of Turn prompt or idle — Space cycles/ends. */
      if (!frame(COLONIZE_KEY_SPACE)) return fail("space");
      if (!click(300, 190)) return fail("end-turn click");
      continue;
    }
    if (!sea) {
      if (!frame(COLONIZE_KEY_B)) return fail("found colony key");
      if (human_colony_count() == 0 && !game_modal_open(g_game)) {
        /* Not foundable here (too near / mountain): walk west one step. */
        if (!frame(west[west_i % 3])) return fail("land step");
        west_i++;
      }
      continue;
    }
    if (mp <= 0) {
      if (!frame(COLONIZE_KEY_SPACE)) return fail("ship no-orders");
      continue;
    }
    if (x == last_x && y == last_y) {
      stuck++;
    } else {
      stuck = 0;
      west_i = 0;
    }
    last_x = x;
    last_y = y;
    if (stuck > 2) {
      /* Blocked without a landfall prompt: try the diagonals, then give up MP. */
      west_i++;
      stuck = 0;
      if (west_i >= 3) {
        if (!frame(COLONIZE_KEY_SPACE)) return fail("ship stuck space");
        west_i = 0;
        continue;
      }
    }
    if (!frame(west[west_i % 3])) return fail("sail west");
  }
  if (human_colony_count() == 0) return fail("no colony founded within budget");
  /* port_bugs.md #1178: the first ship move with land in the destination 3x3 named the New World. */
  if (g_game->col1_ok && !g_game->col1.player[g_game->human_nation].named_new_world) {
    return fail("landfall never raised @LANDHO (named_new_world still clear)");
  }
  while (game_modal_open(g_game) || game_ai_popup_pending(g_game)) {
    if (!dismiss_modal()) return fail("post-found modal");
  }
  printf("  colony founded on turn %u\n", game_turn_number(g_game));
  render("map with colony");

  /*
   * Colony screen. port_bugs.md #682: DOS FUN_479b_076e raw 77043-77048 runs the
   * blocking "Building a Colony" woodcut and only THEN FUN_281f_0608, so the
   * screen comes up by itself once the woodcut is dismissed — the founder's
   * own Enter is never needed. A few frames cover the dismiss plus the
   * deferred open in game_update_services.
   */
  for (int i = 0; i < 8 && !game_in_colony_screen(g_game); ++i) {
    if (!frame(COLONIZE_KEY_ENTER)) return fail("enter colony");
  }
  if (!game_in_colony_screen(g_game)) {
    return fail("colony screen did not open after the found woodcut");
  }
  render("colony screen");
  if (!frame(COLONIZE_KEY_ESCAPE)) return fail("leave colony");
  while (game_modal_open(g_game) || game_ai_popup_pending(g_game)) {
    if (!dismiss_modal()) return fail("post-colony modal");
  }
  if (game_in_colony_screen(g_game)) return fail("colony screen did not close on Esc");

  /* Europe screen. port_bugs.md #682: the founding woodcut now plays before the
   * colony screen, so nothing is parked over the map here and one E lands. */
  if (!frame(COLONIZE_KEY_E)) return fail("E key");
  if (!game_in_europe_screen(g_game)) return fail("Europe screen did not open");
  render("Europe screen");
  if (!frame(COLONIZE_KEY_ESCAPE)) return fail("leave Europe");
  if (game_in_europe_screen(g_game)) return fail("Europe screen did not close on Esc");

  /* Save: S sentries a selected land unit first; repeat until the dialog opens. */
  for (int i = 0; i < 400 && !game_save_dialog_open(g_game); ++i) {
    if (game_modal_open(g_game) || game_ai_popup_pending(g_game)) {
      if (!dismiss_modal()) return fail("pre-save modal");
    } else if (game_turn_busy(g_game)) {
      if (!frame(COLONIZE_KEY_NONE)) return fail("pre-save turn tick");
    } else if (!frame(COLONIZE_KEY_S)) {
      return fail("S key");
    }
  }
  if (!game_save_dialog_open(g_game)) return fail("save dialog did not open");
  render("save dialog");
  if (!frame(COLONIZE_KEY_ENTER)) return fail("save confirm");
  if (game_save_dialog_open(g_game)) return fail("save dialog still open");
  if (strstr(game_status_text(g_game), "Saved COLONY") == NULL) return fail("save status");
  render("map after save");
  /* C4-B2: the colony screen lays out buildings from the DS:0x8d80 value the save carries. */
  if (g_game->col1.post_map.boot_timer == 0 ||
      colony_screen_layout_seed(&g_game->colony_screen) != g_game->col1.post_map.boot_timer) {
    return fail("colony layout seed is not the saved post_map.boot_timer");
  }

  /*
   * smell_audit_2026-09-09 #15: title → NEW WORLD reopens the wizard
   * with this campaign still loaded, and the wizard owns the display — the
   * game underneath must not keep simulating (popup-blocking invariant). It
   * is serviced ahead of every campaign service in game_update, so it must
   * keep the frame and nothing (colony/Europe screen, modal, queued popup,
   * turn advance) may surface behind it.
   */
  while (game_modal_open(g_game) || game_ai_popup_pending(g_game)) {
    if (!dismiss_modal()) return fail("pre-wizard modal");
  }
  {
    const uint32_t turn_before = game_turn_number(g_game);
    /* C2-B1: Esc on the map asks @DOSYES (FUN_2b5a_303c -> 2b5a:3104, default
     * Yes); No keeps the campaign. The title over a live campaign is then
     * reached the way Retire -> Hall of Fame leaves it. */
    if (!frame(COLONIZE_KEY_ESCAPE)) return fail("Esc on map");
    while (game_ai_popup_pending(g_game)) {
      if (!frame(COLONIZE_KEY_NONE)) return fail("present quit confirm");
    }
    if (game_ai_popup_tag(g_game) != AI_POPUP_TAG_MAP_CONFIRM) return fail("Esc did not ask @DOSYES");
    if (!frame(COLONIZE_KEY_DOWN) || !frame(COLONIZE_KEY_ENTER)) return fail("quit confirm No");
    if (game_in_menu(g_game) || game_ai_popup_tag(g_game) >= 0) return fail("No left the map");
    /* #1257: Ctrl-X (DOS key 0x18) asks the same @DOSYES, not VIEW's ~X zoom. */
    {
      ColonizeInputState in = {0};
      in.last_key = COLONIZE_KEY_X;
      in.ctrl_held = true;
      if (!game_update(g_game, &in, 16)) return fail("Ctrl-X on map");
    }
    while (game_ai_popup_pending(g_game)) {
      if (!frame(COLONIZE_KEY_NONE)) return fail("present Ctrl-X confirm");
    }
    if (game_ai_popup_tag(g_game) != AI_POPUP_TAG_MAP_CONFIRM) return fail("Ctrl-X did not ask @DOSYES");
    if (!frame(COLONIZE_KEY_DOWN) || !frame(COLONIZE_KEY_ENTER)) return fail("Ctrl-X confirm No");
    if (!frame(COLONIZE_KEY_Q) || game_ai_popup_pending(g_game) || game_ai_popup_tag(g_game) >= 0) {
      return fail("plain Q must not ask to quit (no DOS map key)");
    }
    g_game->in_menu = true;
    if (!frame(COLONIZE_KEY_ENTER)) return fail("title NEW WORLD");
    if (!game_in_new_game(g_game)) return fail("wizard did not reopen over the campaign");
    for (int i = 0; i < 60; ++i) {
      if (!frame(COLONIZE_KEY_NONE)) return fail("wizard idle frame");
      if (!game_in_new_game(g_game)) return fail("wizard lost the display while idle");
      if (game_in_colony_screen(g_game) || game_in_europe_screen(g_game) ||
          game_modal_open(g_game) || game_ai_popup_pending(g_game)) {
        return fail("campaign chrome opened under the new-game wizard");
      }
    }
    if (game_turn_number(g_game) != turn_before) {
      return fail("turn advanced under the new-game wizard");
    }
    render("new-game wizard over a live campaign");
    if (!frame(COLONIZE_KEY_ESCAPE)) return fail("Esc out of the wizard");
    if (!game_in_menu(g_game)) return fail("Esc from the wizard did not reach the title menu");
  }
  return true;
}

int main(void) {
  {
    const int gate = tr_single_flow_gate("play_smoke");
    if (gate >= 0) {
      return gate;
    }
  }
  const char* data_dir = "./COLONIZE";
  const char* save_dir = "./test-saves-play";
  struct stat st;
  if (stat(data_dir, &st) != 0) {
    printf("SKIP smoke_play: %s not present\n", data_dir);
    return TR_SKIP;
  }
  mkdir(save_dir, 0755);
  char path[512];
  if (savegame_colony_slot_path(save_dir, 0, path, sizeof(path))) {
    remove(path);
  }

  ColonizeGameConfig cfg = {.data_dir = data_dir, .save_dir = save_dir};
  g_game = game_create(&cfg);
  if (!g_game) {
    fprintf(stderr, "game_create failed\n");
    return 1;
  }
  const bool ok = run();
  game_destroy(g_game);
  if (!ok) {
    return 1;
  }
  if (stat(path, &st) != 0) {
    fprintf(stderr, "smoke_play: %s missing after save\n", path);
    return 1;
  }
  printf("smoke_play ok\n");
  return 0;
}
