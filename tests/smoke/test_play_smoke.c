/*
 * Headless play smoke (port_plan P1.2): real COLONIZE assets, new game →
 * sail west → landfall → found colony → colony screen → Europe screen → save.
 * Every screen is rendered once so draw-path crashes surface here, not in
 * the user's session. Runs from the source dir (WORKING_DIRECTORY).
 */
#include <stdbool.h>
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
  for (int i = 0; i < 4000 && game_colony_count(g_game) == 0; ++i) {
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
      if (game_colony_count(g_game) == 0 && !game_modal_open(g_game)) {
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
  if (game_colony_count(g_game) == 0) return fail("no colony founded within budget");
  while (game_modal_open(g_game) || game_ai_popup_pending(g_game)) {
    if (!dismiss_modal()) return fail("post-found modal");
  }
  printf("  colony founded on turn %u\n", game_turn_number(g_game));
  render("map with colony");

  /* Colony screen: Enter with cursor on the colony tile (founder stands there). */
  if (!game_in_colony_screen(g_game)) {
    if (!frame(COLONIZE_KEY_ENTER)) return fail("enter colony");
  }
  if (!game_in_colony_screen(g_game)) {
    int cx = 0, cy = 0;
    if (!game_colony_pos(g_game, 0, &cx, &cy)) return fail("colony pos");
    return fail("colony screen did not open on Enter");
  }
  render("colony screen");
  if (!frame(COLONIZE_KEY_ESCAPE)) return fail("leave colony");
  while (game_modal_open(g_game) || game_ai_popup_pending(g_game)) {
    if (!dismiss_modal()) return fail("post-colony modal");
  }
  if (game_in_colony_screen(g_game)) return fail("colony screen did not close on Esc");

  /* Europe screen. */
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
  return true;
}

int main(void) {
  const char* data_dir = "./COLONIZE";
  const char* save_dir = "./test-saves-play";
  struct stat st;
  if (stat(data_dir, &st) != 0) {
    printf("smoke_play skipped: %s not present\n", data_dir);
    return 0;
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
