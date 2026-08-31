#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/assets.h"
#include "core/game_loop.h"
#include "core/new_game.h"

static bool write_text_file(const char* path, const char* content) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  fwrite(content, 1, strlen(content), f);
  fclose(f);
  return true;
}

static bool write_palette(const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  unsigned char raw[1024];
  for (int i = 0; i < 256; ++i) {
    raw[i * 4 + 0] = (unsigned char)(i & 0x3f);
    raw[i * 4 + 1] = (unsigned char)((i * 2) & 0x3f);
    raw[i * 4 + 2] = (unsigned char)((255 - i) & 0x3f);
    raw[i * 4 + 3] = 0;
  }
  bool ok = fwrite(raw, 1, sizeof(raw), f) == sizeof(raw);
  fclose(f);
  return ok;
}

static bool create_assets(const char* dir) {
  char path[512];
  mkdir(dir, 0755);

  snprintf(path, sizeof(path), "%s/MODULES.DB", dir);
  if (!write_text_file(path, "<Matte>\r\n")) return false;
  snprintf(path, sizeof(path), "%s/ERRORS.DB", dir);
  if (!write_text_file(path, "SeriesListFull\r\n")) return false;
  snprintf(path, sizeof(path), "%s/MENU.TXT", dir);
  if (!write_text_file(path, "@GAME\r\n~GAME\r\n  Exit\r\n")) return false;
  snprintf(path, sizeof(path), "%s/GAME.TXT", dir);
  if (!write_text_file(
        path,
        "@BEGINMENU\r\n"
        "@width=160\r\n"
        "{COLONIZATION} Version test\r\n"
        "@options\r\n"
        "Start a Game in NEW WORLD\r\n"
        "Start a Game in AMERICA\r\n"
        "LOAD Game\r\n"
        "View Hall of Fame\r\n"
        "Exit to DOS\r\n"
        "\r\n"
        "@AMERICA\r\n"
        "@width=160\r\n"
        "Would you like to use the original Americas map, or\r\n"
        "a map prepared with the map editor?\r\n"
        "Original Americas\r\n"
        "Map Editor\r\n"
        "\r\n"
        "@DIFFICULTY\r\n"
        "@width=190\r\n"
        "Select a Difficulty Level\r\n"
        "Discoverer\r\n"
        "Explorer\r\n"
        "Conquistador\r\n"
        "Governor\r\n"
        "Viceroy\r\n"
        "\r\n"
        "@PICKNATION\r\n"
        "@default=1\r\n"
        "Select a European Power\r\n"
        "England\r\n"
        "France\r\n"
        "Spain\r\n"
        "Netherlands\r\n"
        "\r\n"
        "@LEADERNAME\r\n"
        "@width=300\r\n"
        "Please Enter Your Name.\r\n"
        "@options\r\n"
        "______________________\r\n"
        "\r\n"
        "@NATION1A\r\n"
        "@width=300\r\n"
        "FRANCE\r\n"
        "History page A.\r\n"
        "\r\n"
        "@NATION1B\r\n"
        "@width=300\r\n"
        "FRANCE\r\n"
        "Bonus page B.\r\n"
        "\r\n"
        "@VICEROY\r\n"
        "@width=78\r\n"
        "@x=232\r\n"
        "@y=21\r\n"
        "An Audience With\r\n"
        "The King of %COUNTRY\r\n"
        "\r\n"
        "@BUILD1\r\n"
        "@width=310\r\n"
        "@y=30\r\n"
        "In the Year of Our Lord\r\n"
        "\r\n"
        "@BUILD2\r\n"
        "@y=30\r\n"
        "led by %STRING0, %STRING1\r\n"
      )) {
    return false;
  }
  snprintf(path, sizeof(path), "%s/NAMES.TXT", dir);
  if (!write_text_file(
        path,
        "@LEADERNAME\r\n"
        "Walter Raleigh, 1, -1, 0\r\n"
        "Jacques Cartier, 0, 1, 0\r\n"
        "Christopher Columbus, 1, 0, -1\r\n"
        "Michiel De Ruyter, -1, 0, 1\r\n"
        "\r\n"
        "@SCENARIO\r\n"
        "AMER2, 34, 20, 39, 10, 47, 61, 50, 33\r\n"
        "\r\n"
        "@UNIT\r\n"
        "Colonists, 0, 0, 0, 0, 0, 0, 0, 0\r\n"
        "Pioneers, 0, 0, 0, 0, 0, 0, 0, 0\r\n"
        "Caravel, 0, 0, 0, 0, 0, 0, 0, 0\r\n"
      )) {
    return false;
  }
  snprintf(path, sizeof(path), "%s/VICEROY.PAL", dir);
  if (!write_palette(path)) return false;
  return true;
}

static bool step(ColonizeGameState* game, ColonizeKey key) {
  ColonizeInputState input = {0};
  input.last_key = key;
  return game_update(game, &input, 16);
}

static bool step_click(ColonizeGameState* game) {
  ColonizeInputState input = {0};
  input.mouse_left_clicked = true;
  input.mouse_x = 160;
  input.mouse_y = 100;
  return game_update(game, &input, 16);
}

int main(void) {
  const char* data_dir = "./test-assets-newgame";
  const char* save_dir = "./test-saves-newgame";
  if (!create_assets(data_dir)) {
    fprintf(stderr, "failed to create test assets\n");
    return 1;
  }

  /* Unit: scenario start parsing. */
  {
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    char path[512];
    snprintf(path, sizeof(path), "%s/NAMES.TXT", data_dir);
    if (!assets_msg_load_file(&names, path)) {
      fprintf(stderr, "NAMES load failed\n");
      return 1;
    }
    int x = 0, y = 0;
    /* @SCENARIO is four pairs, one per nation, in nation order. */
    new_game_scenario_start(&names, "AMER2", 0, &x, &y);
    if (x != 34 || y != 20) {
      fprintf(stderr, "scenario England start expected 34,20 got %d,%d\n", x, y);
      return 1;
    }
    new_game_scenario_start(&names, "AMER2", 1, &x, &y);
    if (x != 39 || y != 10) {
      fprintf(stderr, "scenario France start expected 39,10 got %d,%d\n", x, y);
      return 1;
    }
    new_game_scenario_start(&names, "AMER2", 2, &x, &y);
    if (x != 47 || y != 61) {
      fprintf(stderr, "scenario Spain start expected 47,61 got %d,%d\n", x, y);
      return 1;
    }
    new_game_scenario_start(&names, "AMER2", 3, &x, &y);
    if (x != 50 || y != 33) {
      fprintf(stderr, "scenario Netherlands start expected 50,33 got %d,%d\n", x, y);
      return 1;
    }
    assets_msg_free(&names);
  }

  /* Unit: CUSTOMIZE path defaults + confirm preserves axes. */
  {
    NewGameWizard ng;
    new_game_init(&ng);
    if (!new_game_begin(&ng, NEW_GAME_PATH_CUSTOMIZE, data_dir, NULL, NULL)) {
      fprintf(stderr, "customize begin failed\n");
      return 1;
    }
    if (ng.phase != NEW_GAME_PHASE_CUSTOMIZE) {
      fprintf(stderr, "expected CUSTOMIZE phase got %d\n", (int)ng.phase);
      return 1;
    }
    if (!ng.generate_map || ng.gen_params.land_mass != 1 || ng.gen_params.land_form != 1 ||
        ng.gen_params.temperature != 1 || ng.gen_params.climate != 1 ||
        ng.gen_params.forest_extra != 1) {
      fprintf(stderr, "customize defaults expected all 1\n");
      return 1;
    }
    ColonizeInputState input = {0};
    input.last_key = COLONIZE_KEY_DOWN;
    if (!new_game_handle_input(&ng, &input)) {
      fprintf(stderr, "customize down failed\n");
      return 1;
    }
    if (ng.gen_params.land_mass != 2) {
      fprintf(stderr, "expected land_mass 2 after DOWN got %d\n", ng.gen_params.land_mass);
      return 1;
    }
    /* Click Temperature / Cool cell (col 2, row 0). */
    memset(&input, 0, sizeof(input));
    input.mouse_left_clicked = true;
    input.mouse_x = 162 + 36;
    input.mouse_y = 16 + 24;
    if (!new_game_handle_input(&ng, &input)) {
      fprintf(stderr, "customize cell click failed\n");
      return 1;
    }
    if (ng.customize_focus != 2 || ng.gen_params.temperature != 0) {
      fprintf(
        stderr,
        "expected focus 2 temp 0 got focus=%d temp=%d\n",
        ng.customize_focus,
        ng.gen_params.temperature
      );
      return 1;
    }
    memset(&input, 0, sizeof(input));
    input.last_key = COLONIZE_KEY_ENTER;
    if (!new_game_handle_input(&ng, &input)) {
      fprintf(stderr, "customize enter failed\n");
      return 1;
    }
    if (ng.phase != NEW_GAME_PHASE_DIFFICULTY) {
      fprintf(stderr, "expected DIFFICULTY after customize confirm got %d\n", (int)ng.phase);
      return 1;
    }
    if (ng.gen_params.land_mass != 2 || ng.gen_params.temperature != 0) {
      fprintf(stderr, "customize params wiped after confirm\n");
      return 1;
    }
    new_game_free(&ng);
  }

  ColonizeGameConfig cfg = {.data_dir = data_dir, .save_dir = save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed\n");
    return 1;
  }

  if (!game_in_menu(game)) {
    fprintf(stderr, "expected title menu\n");
    return 1;
  }

  /* Select NEW WORLD (index 0) and activate. */
  if (!step(game, COLONIZE_KEY_ENTER)) {
    fprintf(stderr, "enter NEW WORLD failed\n");
    return 1;
  }
  if (!game_in_new_game(game) || game_in_menu(game)) {
    fprintf(stderr, "expected new-game wizard after NEW WORLD\n");
    return 1;
  }

  /* Difficulty: Discoverer(0) is default; RIGHT → Explorer(1), then confirm. */
  if (!step(game, COLONIZE_KEY_RIGHT)) return 1;
  if (!step(game, COLONIZE_KEY_ENTER)) return 1;

  /* Nation: England(0) default; RIGHT → France(1). */
  if (!step(game, COLONIZE_KEY_RIGHT)) return 1;
  if (!step(game, COLONIZE_KEY_ENTER)) return 1;

  /* Leader name: confirm default. */
  if (!step(game, COLONIZE_KEY_ENTER)) return 1;

  /* Lore A → B → King → Sail */
  if (!step(game, COLONIZE_KEY_ENTER)) return 1;
  if (!step(game, COLONIZE_KEY_ENTER)) return 1;
  if (!step(game, COLONIZE_KEY_ENTER)) return 1;

  /* Skip sail with LMB. */
  if (!step_click(game)) {
    fprintf(stderr, "sail skip failed\n");
    return 1;
  }

  if (game_in_new_game(game) || game_in_menu(game)) {
    fprintf(stderr, "expected map after sail skip (menu=%d new=%d)\n",
            game_in_menu(game), game_in_new_game(game));
    return 1;
  }
  if (game_difficulty(game) != 1) {
    fprintf(stderr, "expected difficulty 1 got %d\n", game_difficulty(game));
    return 1;
  }
  if (game_human_nation(game) != 1) {
    fprintf(stderr, "expected nation France(1) got %d\n", game_human_nation(game));
    return 1;
  }
  if (strcmp(game_leader_name(game), "Jacques Cartier") != 0) {
    fprintf(stderr, "expected Jacques Cartier got '%s'\n", game_leader_name(game));
    return 1;
  }

  /* Hall of Fame must not start a game. */
  game_destroy(game);
  game = game_create(&cfg);
  if (!game) {
    return 1;
  }
  /* Move to Hall of Fame (index 3) */
  for (int i = 0; i < 3; ++i) {
    if (!step(game, COLONIZE_KEY_DOWN)) return 1;
  }
  if (!step(game, COLONIZE_KEY_ENTER)) return 1;
  /* Opens the Hall of Fame screen (see unit_hall_of_fame), not a new game;
   * Esc returns to the title menu. */
  if (!game_in_hall_of_fame(game) || game_in_menu(game) || game_in_new_game(game)) {
    fprintf(stderr, "Hall of Fame should open its own screen, not start a game\n");
    return 1;
  }
  if (!step(game, COLONIZE_KEY_ESCAPE)) return 1;
  if (game_in_hall_of_fame(game) || !game_in_menu(game)) {
    fprintf(stderr, "Esc from Hall of Fame should return to title menu\n");
    return 1;
  }

  game_destroy(game);
  printf("unit_new_game ok\n");
  return 0;
}
