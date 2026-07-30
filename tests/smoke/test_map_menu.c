#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/map_menu.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  ColonizeMsgCatalog menu_txt;
  assets_msg_init(&menu_txt);
  if (!assets_msg_load_file(&menu_txt, "COLONIZE/MENU.TXT")) {
    fprintf(stderr, "Failed to load MENU.TXT\n");
    return 1;
  }

  MapMenuBar bar;
  map_menu_init(&bar);
  if (!map_menu_load(&bar, &menu_txt)) {
    fprintf(stderr, "map_menu_load failed\n");
    assets_msg_free(&menu_txt);
    return 1;
  }

  if (bar.menu_count < 6) {
    fprintf(stderr, "expected at least 6 menus, got %d\n", bar.menu_count);
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  if (strcmp(bar.menus[0].title, "GAME") != 0 ||
      strcmp(bar.menus[1].title, "VIEW") != 0 ||
      strcmp(bar.menus[5].title, "COLONIZOPEDIA") != 0) {
    fprintf(
      stderr,
      "unexpected titles: '%s' '%s' ... '%s'\n",
      bar.menus[0].title,
      bar.menus[1].title,
      bar.menus[bar.menu_count - 1].title
    );
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  bool found_save = false;
  bool found_report_stub = false;
  for (int i = 0; i < bar.menus[0].item_count; ++i) {
    if (bar.menus[0].items[i].action == MAP_MENU_ACTION_SAVE && bar.menus[0].items[i].enabled) {
      found_save = true;
    }
  }
  for (int i = 0; i < bar.menus[3].item_count; ++i) {
    if (!bar.menus[3].items[i].enabled &&
        bar.menus[3].items[i].action == MAP_MENU_ACTION_UNIMPLEMENTED) {
      found_report_stub = true;
    }
  }
  if (!found_save || !found_report_stub) {
    fprintf(
      stderr,
      "expected enabled Save + stubbed Reports (save=%d stub=%d)\n",
      found_save ? 1 : 0,
      found_report_stub ? 1 : 0
    );
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  ColonizeFont font;
  memset(&font, 0, sizeof(font));
  char err[128];
  const bool font_ok = ff_load("COLONIZE/FONTSMAL.FF", &font, err, sizeof(err));
  const ColonizeFont* f = font_ok ? &font : NULL;

  uint8_t pixels[320 * 200];
  memset(pixels, 0, sizeof(pixels));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  map_menu_render(&bar, f, &fb);
  if (pixels[2] == 0 && pixels[320 * 2 + 10] == 0) {
    fprintf(stderr, "menu bar render produced empty pixels\n");
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  ColonizeInputState input;
  memset(&input, 0, sizeof(input));
  input.mouse_left_clicked = true;
  input.mouse_x = bar.menus[0].title_x + 2;
  input.mouse_y = 2;
  MapMenuAction action = map_menu_handle_input(&bar, &input, f, false);
  if (action != MAP_MENU_ACTION_NONE || bar.open_index != 0) {
    fprintf(stderr, "expected GAME menu to open (action=%d open=%d)\n", (int)action, bar.open_index);
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  /* Save Game is item index 4 under GAME. */
  int save_index = -1;
  for (int i = 0; i < bar.menus[0].item_count; ++i) {
    if (bar.menus[0].items[i].action == MAP_MENU_ACTION_SAVE) {
      save_index = i;
      break;
    }
  }
  if (save_index < 0) {
    fprintf(stderr, "Save Game item missing\n");
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  const int item_h = f ? (f->max_height + 2 < 8 ? 8 : f->max_height + 2) : 8;
  input.mouse_x = bar.menus[0].title_x + 8;
  input.mouse_y = MAP_MENU_BAR_H + 2 + save_index * item_h + 1;
  input.mouse_left_clicked = true;
  action = map_menu_handle_input(&bar, &input, f, false);
  if (action != MAP_MENU_ACTION_SAVE) {
    fprintf(
      stderr,
      "expected Save Game action, got %d (%s) at (%d,%d) open=%d\n",
      (int)action,
      map_menu_action_name(action),
      input.mouse_x,
      input.mouse_y,
      bar.open_index
    );
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  if (font_ok) {
    ff_free(&font);
  }
  map_menu_free(&bar);
  assets_msg_free(&menu_txt);
  fprintf(stderr, "map menu tests ok (menus=%d)\n", 6);
  diag_shutdown();
  return 0;
}
