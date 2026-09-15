#include "tests/common/test_assets.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

bool test_assets_write_text_file(const char* path, const char* content) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  fwrite(content, 1, strlen(content), f);
  fclose(f);
  return true;
}

bool test_assets_write_palette(const char* path) {
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

static const char k_menu_min[] =
  "@BEGINMENU\r\n"
  "@width=160\r\n"
  "{COLONIZATION} Version %STRING0\r\n"
  "@options\r\n"
  "Start a Game in NEW WORLD\r\n"
  "LOAD Game\r\n"
  "Exit to DOS\r\n";

static const char k_menu_full[] =
  "@BEGINMENU\r\n"
  "@width=160\r\n"
  "{COLONIZATION} Version test\r\n"
  "@options\r\n"
  "Start a Game in NEW WORLD\r\n"
  "Start a Game in AMERICA\r\n"
  "LOAD Game\r\n"
  "View Hall of Fame\r\n"
  "Exit to DOS\r\n";

static const char k_wizard_pages[] =
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
  "led by %STRING0, %STRING1\r\n";

static const char k_names_min[] =
  "@LEADERNAME\r\n"
  "Walter Raleigh, 1, -1, 0\r\n"
  "\r\n"
  "@SCENARIO\r\n"
  "AMER2, 34, 20, 39, 10, 47, 61, 50, 33\r\n"
  "\r\n"
  "@UNIT\r\n"
  "Colonists, 0, 0, 0, 0, 0, 0, 0, 0\r\n";

static const char k_names_full[] =
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
  "Caravel, 0, 0, 0, 0, 0, 0, 0, 0\r\n";

bool test_assets_create(const char* dir, unsigned flags) {
  char path[512];
  char game_txt[4096];

  if (mkdir(dir, 0755) != 0) {
    /* continue if already exists */
  }

  snprintf(path, sizeof(path), "%s/MODULES.DB", dir);
  if (!test_assets_write_text_file(path, "<Matte>\r\n")) return false;
  snprintf(path, sizeof(path), "%s/ERRORS.DB", dir);
  if (!test_assets_write_text_file(path, "SeriesListFull\r\n")) return false;
  snprintf(path, sizeof(path), "%s/MENU.TXT", dir);
  if (!test_assets_write_text_file(path, "@GAME\r\n~GAME\r\n  Exit\r\n")) return false;

  snprintf(
    game_txt, sizeof(game_txt), "%s%s",
    (flags & TEST_ASSETS_FULL_MENU) ? k_menu_full : k_menu_min,
    (flags & TEST_ASSETS_WIZARD_PAGES) ? k_wizard_pages : ""
  );
  snprintf(path, sizeof(path), "%s/GAME.TXT", dir);
  if (!test_assets_write_text_file(path, game_txt)) return false;

  if (flags & (TEST_ASSETS_NAMES | TEST_ASSETS_NAMES_FULL)) {
    snprintf(path, sizeof(path), "%s/NAMES.TXT", dir);
    if (!test_assets_write_text_file(
          path, (flags & TEST_ASSETS_NAMES_FULL) ? k_names_full : k_names_min
        )) {
      return false;
    }
  }

  snprintf(path, sizeof(path), "%s/VICEROY.PAL", dir);
  if (!test_assets_write_palette(path)) return false;
  return true;
}

bool test_assets_step(ColonizeGameState* game, ColonizeKey key) {
  ColonizeInputState input = {0};
  input.last_key = key;
  return game_update(game, &input, 16);
}
