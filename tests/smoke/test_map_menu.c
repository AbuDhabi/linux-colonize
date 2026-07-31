#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
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

  if (strcmp(bar.menus[0].title, "~GAME") != 0 ||
      strcmp(bar.menus[1].title, "~VIEW") != 0 ||
      strcmp(bar.menus[5].title, "~COLONIZOPEDIA") != 0) {
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

  /* REPORTS items are enabled (open report screens). */
  bool found_save = false;
  bool found_report = false;
  for (int i = 0; i < bar.menus[0].item_count; ++i) {
    if (bar.menus[0].items[i].action == MAP_MENU_ACTION_SAVE && bar.menus[0].items[i].enabled) {
      found_save = true;
    }
  }
  for (int i = 0; i < bar.menus[3].item_count; ++i) {
    if (bar.menus[3].items[i].action == MAP_MENU_ACTION_REPORT_CONGRESS &&
        bar.menus[3].items[i].enabled) {
      found_report = true;
    }
  }
  if (!found_save || !found_report) {
    fprintf(
      stderr,
      "expected enabled Save + Reports Congress (save=%d report=%d)\n",
      found_save ? 1 : 0,
      found_report ? 1 : 0
    );
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  /* COLONIZOPEDIA menu is index 5 — 7 categories + divider after Terrain Types. */
  if (bar.menus[5].item_count != 8) {
    fprintf(stderr, "pedia menu expected 8 items (7 + separator) got %d\n", bar.menus[5].item_count);
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  bool found_sep = false;
  for (int i = 0; i < bar.menus[5].item_count; ++i) {
    if (bar.menus[5].items[i].separator) {
      found_sep = true;
      if (bar.menus[5].items[i].enabled ||
          bar.menus[5].items[i].action != MAP_MENU_ACTION_SEPARATOR) {
        fprintf(stderr, "pedia separator item %d malformed\n", i);
        map_menu_free(&bar);
        assets_msg_free(&menu_txt);
        return 1;
      }
      /* Divider sits between Terrain Types and Colonist Skills. */
      if (i < 1 || strcmp(bar.menus[5].items[i - 1].label, "Terrain Types") != 0 ||
          i + 1 >= bar.menus[5].item_count ||
          strcmp(bar.menus[5].items[i + 1].label, "Colonist Skills") != 0) {
        fprintf(stderr, "pedia separator not between Terrain Types and Colonist Skills\n");
        map_menu_free(&bar);
        assets_msg_free(&menu_txt);
        return 1;
      }
      continue;
    }
    if (!bar.menus[5].items[i].enabled ||
        bar.menus[5].items[i].action == MAP_MENU_ACTION_UNIMPLEMENTED) {
      fprintf(stderr, "pedia item %d still stubbed (%s)\n", i, bar.menus[5].items[i].label);
      map_menu_free(&bar);
      assets_msg_free(&menu_txt);
      return 1;
    }
  }
  if (!found_sep) {
    fprintf(stderr, "pedia menu missing separator\n");
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
  map_menu_render(&bar, f, NULL, &fb);
  if (bar.menus[0].title_x != 12) {
    fprintf(stderr, "GAME title_x expected 12, got %d\n", bar.menus[0].title_x);
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  const int inner_x0 = MAP_PANEL_X + 2;
  const int inner_w = 319 - inner_x0 + 1;
  const int mx = inner_x0 + (inner_w - MAP_PANEL_MINIMAP_W) / 2;
  const int minimap_cx = mx + MAP_PANEL_MINIMAP_W / 2;
  const int title_cx = bar.menus[5].title_x + bar.menus[5].title_w / 2;
  /* COLONIZOPEDIA title center should align with the minimap center. */
  if (title_cx < minimap_cx - 12 || title_cx > minimap_cx + 12) {
    fprintf(
      stderr,
      "COLONIZOPEDIA should be centered over minimap (title_cx=%d minimap_cx=%d title_x=%d)\n",
      title_cx,
      minimap_cx,
      bar.menus[5].title_x
    );
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
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
