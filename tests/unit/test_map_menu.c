#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "platform/diagnostics.h"

static int find_section(const MapMenuBar* bar, const char* section) {
  for (int i = 0; i < bar->menu_count; ++i) {
    if (strcmp(bar->menus[i].section_name, section) == 0) {
      return i;
    }
  }
  return -1;
}

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

  const int expected_menus = COLONIZE_DEBUG_MENU ? 8 : 7;
  if (bar.menu_count != expected_menus) {
    fprintf(stderr, "expected %d menus, got %d\n", expected_menus, bar.menu_count);
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  const int game_i = find_section(&bar, "GAME");
  const int view_i = find_section(&bar, "VIEW");
  const int cup_i = find_section(&bar, "CUP");
  const int pedia_i = find_section(&bar, "PEDIA");
  const int debug_i = find_section(&bar, "DEBUG");
  if (game_i < 0 || view_i < 0 || cup_i < 0 || pedia_i < 0) {
    fprintf(stderr, "missing GAME/VIEW/CUP/PEDIA sections\n");
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  if (strcmp(bar.menus[game_i].title, "~GAME") != 0 ||
      strcmp(bar.menus[view_i].title, "~VIEW") != 0 ||
      strcmp(bar.menus[pedia_i].title, "~COLONIZOPEDIA") != 0) {
    fprintf(
      stderr,
      "unexpected titles: '%s' '%s' ... '%s'\n",
      bar.menus[game_i].title,
      bar.menus[view_i].title,
      bar.menus[pedia_i].title
    );
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  /* CHEAT is loaded but hidden until unlock. */
  if (bar.cheat_visible || bar.menus[cup_i].visible) {
    fprintf(stderr, "CHEAT should start hidden\n");
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  if (bar.menus[cup_i].item_count < 11) {
    fprintf(stderr, "CHEAT expected >=11 items, got %d\n", bar.menus[cup_i].item_count);
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  for (int i = 0; i < bar.menus[cup_i].item_count; ++i) {
    const MapMenuAction a = bar.menus[cup_i].items[i].action;
    if (bar.menus[cup_i].items[i].separator) {
      continue;
    }
    /* All 11 CHEAT items are implemented (game_loop.c MAP_MENU_ACTION_CHEAT_*). */
    const bool should_enable = true;
    if (bar.menus[cup_i].items[i].enabled != should_enable) {
      fprintf(
        stderr,
        "CHEAT item %d enabled=%d expected %d (%s)\n",
        i,
        bar.menus[cup_i].items[i].enabled ? 1 : 0,
        should_enable ? 1 : 0,
        bar.menus[cup_i].items[i].label
      );
      map_menu_free(&bar);
      assets_msg_free(&menu_txt);
      return 1;
    }
    if (a == MAP_MENU_ACTION_UNIMPLEMENTED) {
      fprintf(stderr, "CHEAT item %d unclassified (%s)\n", i, bar.menus[cup_i].items[i].label);
      map_menu_free(&bar);
      assets_msg_free(&menu_txt);
      return 1;
    }
  }

#if COLONIZE_DEBUG_MENU
  if (debug_i < 0 || debug_i != cup_i + 1) {
    fprintf(stderr, "DEBUG should sit immediately after CHEAT (cup=%d debug=%d)\n", cup_i, debug_i);
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  if (!bar.menus[debug_i].visible || bar.menus[debug_i].item_count != 2) {
    fprintf(stderr, "DEBUG menu malformed\n");
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  if (!bar.menus[debug_i].items[0].enabled ||
      bar.menus[debug_i].items[0].action != MAP_MENU_ACTION_DEBUG_SPRITE_VIEWER ||
      !bar.menus[debug_i].items[1].enabled ||
      bar.menus[debug_i].items[1].action != MAP_MENU_ACTION_DEBUG_TOGGLE_MOUSE_COORDS) {
    fprintf(stderr, "DEBUG items unexpected\n");
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
#else
  (void)debug_i;
#endif

  /* REPORTS items are enabled (open report screens). */
  bool found_save = false;
  bool found_pick_music = false;
  bool found_game_options = false;
  bool found_report = false;
  for (int i = 0; i < bar.menus[game_i].item_count; ++i) {
    if (bar.menus[game_i].items[i].action == MAP_MENU_ACTION_SAVE &&
        bar.menus[game_i].items[i].enabled) {
      found_save = true;
    }
    if (bar.menus[game_i].items[i].action == MAP_MENU_ACTION_PICK_MUSIC &&
        bar.menus[game_i].items[i].enabled) {
      found_pick_music = true;
    }
    if (bar.menus[game_i].items[i].action == MAP_MENU_ACTION_OPTIONS &&
        strcmp(bar.menus[game_i].items[i].label, "Game Options") == 0 &&
        bar.menus[game_i].items[i].enabled) {
      found_game_options = true;
    }
  }
  const int reports_i = find_section(&bar, "REPORTS");
  for (int i = 0; i < bar.menus[reports_i].item_count; ++i) {
    if (bar.menus[reports_i].items[i].action == MAP_MENU_ACTION_REPORT_CONGRESS &&
        bar.menus[reports_i].items[i].enabled) {
      found_report = true;
    }
  }
  if (!found_save || !found_pick_music || !found_game_options || !found_report) {
    fprintf(
      stderr,
      "expected enabled Game Options + Save + Pick Music + Reports Congress "
      "(options=%d save=%d pick=%d report=%d)\n",
      found_game_options ? 1 : 0,
      found_save ? 1 : 0,
      found_pick_music ? 1 : 0,
      found_report ? 1 : 0
    );
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  /* COLONIZOPEDIA — 7 categories + divider after Terrain Types. */
  if (bar.menus[pedia_i].item_count != 8) {
    fprintf(
      stderr, "pedia menu expected 8 items (7 + separator) got %d\n", bar.menus[pedia_i].item_count
    );
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  bool found_sep = false;
  for (int i = 0; i < bar.menus[pedia_i].item_count; ++i) {
    if (bar.menus[pedia_i].items[i].separator) {
      found_sep = true;
      if (bar.menus[pedia_i].items[i].enabled ||
          bar.menus[pedia_i].items[i].action != MAP_MENU_ACTION_SEPARATOR) {
        fprintf(stderr, "pedia separator item %d malformed\n", i);
        map_menu_free(&bar);
        assets_msg_free(&menu_txt);
        return 1;
      }
      if (i < 1 || strcmp(bar.menus[pedia_i].items[i - 1].label, "Terrain Types") != 0 ||
          i + 1 >= bar.menus[pedia_i].item_count ||
          strcmp(bar.menus[pedia_i].items[i + 1].label, "Colonist Skills") != 0) {
        fprintf(stderr, "pedia separator not between Terrain Types and Colonist Skills\n");
        map_menu_free(&bar);
        assets_msg_free(&menu_txt);
        return 1;
      }
      continue;
    }
    if (!bar.menus[pedia_i].items[i].enabled ||
        bar.menus[pedia_i].items[i].action == MAP_MENU_ACTION_UNIMPLEMENTED) {
      fprintf(stderr, "pedia item %d still stubbed (%s)\n", i, bar.menus[pedia_i].items[i].label);
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
  if (bar.menus[game_i].title_x != 12) {
    fprintf(stderr, "GAME title_x expected 12, got %d\n", bar.menus[game_i].title_x);
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
  const int title_cx = bar.menus[pedia_i].title_x + bar.menus[pedia_i].title_w / 2;
  if (title_cx < minimap_cx - 12 || title_cx > minimap_cx + 12) {
    fprintf(
      stderr,
      "COLONIZOPEDIA should be centered over minimap (title_cx=%d minimap_cx=%d title_x=%d)\n",
      title_cx,
      minimap_cx,
      bar.menus[pedia_i].title_x
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

#if COLONIZE_DEBUG_MENU
  /* Fixed DEBUG slot: revealing CHEAT must not move DEBUG. */
  const int debug_x_hidden = bar.menus[debug_i].title_x;
  map_menu_set_cheat_visible(&bar, true);
  map_menu_render(&bar, f, NULL, &fb);
  if (!bar.menus[cup_i].visible || !bar.cheat_visible) {
    fprintf(stderr, "map_menu_set_cheat_visible(true) failed\n");
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  if (bar.menus[debug_i].title_x != debug_x_hidden) {
    fprintf(
      stderr,
      "DEBUG title_x shifted when CHEAT revealed (%d -> %d)\n",
      debug_x_hidden,
      bar.menus[debug_i].title_x
    );
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  map_menu_set_cheat_visible(&bar, false);
#else
  map_menu_set_cheat_visible(&bar, true);
  if (!bar.cheat_visible || !bar.menus[cup_i].visible) {
    fprintf(stderr, "map_menu_set_cheat_visible(true) failed\n");
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  map_menu_set_cheat_visible(&bar, false);
#endif

  ColonizeInputState input;
  memset(&input, 0, sizeof(input));
  input.mouse_left_clicked = true;
  input.mouse_x = bar.menus[game_i].title_x + 2;
  input.mouse_y = 2;
  MapMenuAction action = map_menu_handle_input(&bar, &input, f, false);
  if (action != MAP_MENU_ACTION_NONE || bar.open_index != game_i) {
    fprintf(stderr, "expected GAME menu to open (action=%d open=%d)\n", (int)action, bar.open_index);
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }

  /* Chrome: 1px gap under bar rule, black dropdown outline. */
  memset(pixels, 7, sizeof(pixels)); /* non-black sentinel in gap row */
  map_menu_render(&bar, f, NULL, &fb);
  {
    const int dx = bar.menus[game_i].title_x;
    const int gap_y = MAP_MENU_BAR_H;
    const int drop_y = MAP_MENU_BAR_H + 1;
    if (pixels[gap_y * 320 + dx + 4] == 0) {
      fprintf(stderr, "expected 1px non-black gap between bar rule and dropdown\n");
      if (font_ok) {
        ff_free(&font);
      }
      map_menu_free(&bar);
      assets_msg_free(&menu_txt);
      return 1;
    }
    if (pixels[drop_y * 320 + dx + 4] != 0) {
      fprintf(stderr, "expected black dropdown top border, got %u\n", pixels[drop_y * 320 + dx + 4]);
      if (font_ok) {
        ff_free(&font);
      }
      map_menu_free(&bar);
      assets_msg_free(&menu_txt);
      return 1;
    }
  }

  /* Save Game is item index 4 under GAME. */
  int save_index = -1;
  for (int i = 0; i < bar.menus[game_i].item_count; ++i) {
    if (bar.menus[game_i].items[i].action == MAP_MENU_ACTION_SAVE) {
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
  /* Dropdown sits 1px below the bar rule (MAP_MENU_BAR_H + 1). */
  const int dropdown_y = MAP_MENU_BAR_H + 1;
  input.mouse_x = bar.menus[game_i].title_x + 8;
  input.mouse_y = dropdown_y + 2 + save_index * item_h + 1;
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

  /* Hidden CHEAT title must not be clickable. */
  map_menu_handle_input(&bar, &input, f, true);
  input.mouse_x = bar.menus[cup_i].title_x + 2;
  input.mouse_y = 2;
  input.mouse_left_clicked = true;
  action = map_menu_handle_input(&bar, &input, f, false);
  if (bar.open_index == cup_i) {
    fprintf(stderr, "hidden CHEAT title should not open\n");
    if (font_ok) {
      ff_free(&font);
    }
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  (void)action;

  if (font_ok) {
    ff_free(&font);
  }
  map_menu_free(&bar);
  assets_msg_free(&menu_txt);
  fprintf(stderr, "map menu tests ok (menus=%d debug=%d)\n", expected_menus, COLONIZE_DEBUG_MENU);
  diag_shutdown();
  return 0;
}
