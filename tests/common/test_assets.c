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

/* Option rows follow GAME.TXT @BEGINMENU's own order, because the title menu
 * dispatches by ROW: new world / america / customize / load / hall (/ exit). */
static const char k_menu_min[] =
  "@BEGINMENU\r\n"
  "@width=160\r\n"
  "{TEST} Version %STRING0\r\n"
  "@options\r\n"
  "Menu Option New\r\n"
  "Menu Option America\r\n"
  "Menu Option Customize\r\n"
  "Menu Option Load\r\n"
  "Menu Option Hall\r\n"
  "Menu Option Exit\r\n";

static const char k_menu_full[] =
  "@BEGINMENU\r\n"
  "@width=160\r\n"
  "{TEST} Version test\r\n"
  "@options\r\n"
  "Menu Option New\r\n"
  "Menu Option America\r\n"
  "Menu Option Customize\r\n"
  "Menu Option Load\r\n"
  "Menu Option Hall\r\n"
  "Menu Option Exit\r\n";

static const char k_wizard_pages[] =
  "\r\n"
  "@AMERICA\r\n"
  "@width=160\r\n"
  "Synthetic map question line one\r\n"
  "synthetic question line two?\r\n"
  "Map Option One\r\n"
  "Map Option Two\r\n"
  "\r\n"
  "@DIFFICULTY\r\n"
  "@width=190\r\n"
  "Synthetic Difficulty Prompt\r\n"
  "Level One\r\n"
  "Level Two\r\n"
  "Level Three\r\n"
  "Level Four\r\n"
  "Level Five\r\n"
  "\r\n"
  "@PICKNATION\r\n"
  "@default=1\r\n"
  "Synthetic Nation Prompt\r\n"
  "Nation One\r\n"
  "Nation Two\r\n"
  "Nation Three\r\n"
  "Nation Four\r\n"
  "\r\n"
  "@LEADERNAME\r\n"
  "@width=300\r\n"
  "Synthetic Name Prompt\r\n"
  "@options\r\n"
  "______________________\r\n"
  "\r\n"
  "@NATION1A\r\n"
  "@width=300\r\n"
  "NATION TWO\r\n"
  "History page A.\r\n"
  "\r\n"
  "@NATION1B\r\n"
  "@width=300\r\n"
  "NATION TWO\r\n"
  "Bonus page B.\r\n"
  "\r\n"
  "@VICEROY\r\n"
  "@width=78\r\n"
  "@x=232\r\n"
  "@y=21\r\n"
  "Synthetic Audience Line\r\n"
  "Synthetic Ruler Of %COUNTRY\r\n"
  "\r\n"
  "@BUILD1\r\n"
  "@width=310\r\n"
  "@y=30\r\n"
  "Synthetic Year Line\r\n"
  "\r\n"
  "@BUILD2\r\n"
  "@y=30\r\n"
  "synthetic leader %STRING0, %STRING1\r\n";

static const char k_names_min[] =
  "@LEADERNAME\r\n"
  "Leader One, 1, -1, 0\r\n"
  "\r\n"
  "@SCENARIO\r\n"
  "AMER2, 34, 20, 39, 10, 47, 61, 50, 33\r\n"
  "\r\n"
  "@UNIT\r\n"
  "Unit Row Zero, 0, 0, 0, 0, 0, 0, 0, 0\r\n";

static const char k_names_full[] =
  "@LEADERNAME\r\n"
  "Leader One, 1, -1, 0\r\n"
  "Leader Two, 0, 1, 0\r\n"
  "Leader Three, 1, 0, -1\r\n"
  "Leader Four, -1, 0, 1\r\n"
  "\r\n"
  "@SCENARIO\r\n"
  "AMER2, 34, 20, 39, 10, 47, 61, 50, 33\r\n"
  "\r\n"
  "@UNIT\r\n"
  "Unit Row Zero, 0, 0, 0, 0, 0, 0, 0, 0\r\n"
  "Unit Row One, 0, 0, 0, 0, 0, 0, 0, 0\r\n"
  "Unit Row Two, 0, 0, 0, 0, 0, 0, 0, 0\r\n";

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
  if (!test_assets_write_text_file(path, "@GAME\r\n~GAME\r\n  Menu Option Exit\r\n")) return false;

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

  /*
   * The port compiles none of the game's wording, so every text catalog is a
   * required asset (assets_validate_required_files). Tests that build their
   * own tree must therefore provide all of them; the content here is
   * deliberately synthetic, never the shipped MicroProse text.
   */
  if (!(flags & (TEST_ASSETS_NAMES | TEST_ASSETS_NAMES_FULL))) {
    snprintf(path, sizeof(path), "%s/NAMES.TXT", dir);
    if (!test_assets_write_text_file(path, k_names_min)) return false;
  }
  snprintf(path, sizeof(path), "%s/LABELS.TXT", dir);
  if (!test_assets_write_text_file(path, "@MISC\r\nSynthetic Label\r\n")) return false;
  snprintf(path, sizeof(path), "%s/PEDIA.TXT", dir);
  if (!test_assets_write_text_file(path, "@MISCELLANEOUS\r\n1\r\nSynthetic Topic\r\n")) {
    return false;
  }
  snprintf(path, sizeof(path), "%s/COLONY.TXT", dir);
  if (!test_assets_write_text_file(path, "@ENGLISH\r\nSynthetic Colony\r\n")) return false;
  snprintf(path, sizeof(path), "%s/DEBUG.TXT", dir);
  if (!test_assets_write_text_file(path, "@SETVIEW\r\nSynthetic Cheat Prompt\r\n")) return false;

  snprintf(path, sizeof(path), "%s/VICEROY.PAL", dir);
  if (!test_assets_write_palette(path)) return false;
  return true;
}

bool test_assets_step(ColonizeGameState* game, ColonizeKey key) {
  ColonizeInputState input = {0};
  input.last_key = key;
  return game_update(game, &input, 16);
}
