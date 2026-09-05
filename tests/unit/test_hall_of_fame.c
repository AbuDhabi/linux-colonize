#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include "core/game_loop.h"

/*
 * Hall of Fame: ranked HOF.TXT table (score|leader|nation|year|difficulty,
 * highest score first; backward-compat with the old single-integer-per-line
 * stub; re-ranks an unsorted/hand-edited file; caps at COLONIZE_HOF_MAX (10)
 * entries on load) AND the title-menu screen it opens into (a real
 * in_hall_of_fame render state via reports_render_hall_of_fame — not a
 * window-title status string). See docs/roadmap.md Phase 1 /
 * docs/manual_gap.md "Hall of Fame".
 */

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

/* Minimal title-menu assets: enough for game_create to reach the menu with a
 * "View Hall of Fame" option, nothing else (no wizard pages needed). */
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
      )) {
    return false;
  }
  snprintf(path, sizeof(path), "%s/NAMES.TXT", dir);
  if (!write_text_file(
        path,
        "@LEADERNAME\r\n"
        "Walter Raleigh, 1, -1, 0\r\n"
        "\r\n"
        "@SCENARIO\r\n"
        "AMER2, 34, 20, 39, 10, 47, 61, 50, 33\r\n"
        "\r\n"
        "@UNIT\r\n"
        "Colonists, 0, 0, 0, 0, 0, 0, 0, 0\r\n"
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

/* Move DOWN to the "View Hall of Fame" option (index 3) and activate it. */
static bool open_hall_of_fame(ColonizeGameState* game) {
  for (int i = 0; i < 3; ++i) {
    if (!step(game, COLONIZE_KEY_DOWN)) return false;
  }
  return step(game, COLONIZE_KEY_ENTER);
}

int main(void) {
  const char* data_dir = "./test-assets-hof";
  const char* save_dir = "./test-saves-hof";
  if (!create_assets(data_dir)) {
    fprintf(stderr, "failed to create test assets\n");
    return 1;
  }
  mkdir(save_dir, 0755);

  char hof_path[512];
  snprintf(hof_path, sizeof(hof_path), "%s/HOF.TXT", data_dir);
  remove(hof_path);

  ColonizeGameConfig cfg = {.data_dir = data_dir, .save_dir = save_dir};

  /* 1. No HOF.TXT yet: empty table; menu action opens a real screen (not a
   * status-text hack), and Esc closes it back to the title menu. */
  {
    ColonizeGameState* game = game_create(&cfg);
    if (!game) {
      fprintf(stderr, "game_create failed (empty)\n");
      return 1;
    }
    if (game_hof_count(game) != 0) {
      fprintf(stderr, "expected empty HoF table, got %d\n", game_hof_count(game));
      return 1;
    }
    if (!open_hall_of_fame(game)) {
      fprintf(stderr, "open Hall of Fame failed\n");
      return 1;
    }
    if (!game_in_hall_of_fame(game) || game_in_menu(game)) {
      fprintf(stderr, "expected Hall of Fame screen open, left title menu\n");
      return 1;
    }
    /* Idle frames (no key) must not close it or bounce back to the menu —
     * game_update runs every frame, not just on keypress. */
    if (!step(game, COLONIZE_KEY_NONE) || !game_in_hall_of_fame(game)) {
      fprintf(stderr, "Hall of Fame screen closed on idle frame\n");
      return 1;
    }
    if (!step(game, COLONIZE_KEY_ESCAPE)) {
      fprintf(stderr, "Esc from Hall of Fame failed\n");
      return 1;
    }
    if (game_in_hall_of_fame(game) || !game_in_menu(game)) {
      fprintf(stderr, "expected Esc to return to title menu\n");
      return 1;
    }
    game_destroy(game);
  }

  /* 2. Legacy single-integer-per-line HOF.TXT still loads (as one entry). */
  {
    if (!write_text_file(hof_path, "1200\n")) {
      fprintf(stderr, "failed to seed legacy HOF.TXT\n");
      return 1;
    }
    ColonizeGameState* game = game_create(&cfg);
    if (!game) {
      fprintf(stderr, "game_create failed (legacy)\n");
      return 1;
    }
    if (game_hof_count(game) != 1) {
      fprintf(stderr, "expected 1 legacy entry, got %d\n", game_hof_count(game));
      return 1;
    }
    ColonizeHofEntryView e;
    if (!game_hof_entry(game, 0, &e) || e.score != 1200) {
      fprintf(stderr, "legacy entry score mismatch\n");
      return 1;
    }
    game_destroy(game);
  }

  /* 3. Unsorted, multi-field HOF.TXT re-ranks highest-first on load. */
  {
    if (!write_text_file(
          hof_path,
          "800|Ann Bonny|English|1700|2\n"
          "1500|Jacques Cartier|French|1750|3\n"
          "1100|Miguel de Soto|Spanish|1720|1\n"
        )) {
      fprintf(stderr, "failed to seed ranked HOF.TXT\n");
      return 1;
    }
    ColonizeGameState* game = game_create(&cfg);
    if (!game) {
      fprintf(stderr, "game_create failed (ranked)\n");
      return 1;
    }
    if (game_hof_count(game) != 3) {
      fprintf(stderr, "expected 3 ranked entries, got %d\n", game_hof_count(game));
      return 1;
    }
    static const struct {
      int score;
      const char* leader;
      const char* nation;
      int year;
    } want[3] = {
      {1500, "Jacques Cartier", "French", 1750},
      {1100, "Miguel de Soto", "Spanish", 1720},
      {800, "Ann Bonny", "English", 1700},
    };
    for (int i = 0; i < 3; ++i) {
      ColonizeHofEntryView e;
      if (!game_hof_entry(game, i, &e)) {
        fprintf(stderr, "rank %d entry missing\n", i);
        return 1;
      }
      if (e.score != want[i].score || strcmp(e.leader, want[i].leader) != 0 ||
          strcmp(e.nation, want[i].nation) != 0 || e.year != want[i].year) {
        fprintf(
          stderr,
          "rank %d mismatch: got %d %s (%s) %d want %d %s (%s) %d\n",
          i,
          e.score,
          e.leader,
          e.nation,
          e.year,
          want[i].score,
          want[i].leader,
          want[i].nation,
          want[i].year
        );
        return 1;
      }
    }
    if (!open_hall_of_fame(game)) {
      fprintf(stderr, "open Hall of Fame (ranked) failed\n");
      return 1;
    }
    if (!game_in_hall_of_fame(game) || game_in_menu(game)) {
      fprintf(stderr, "expected Hall of Fame screen open\n");
      return 1;
    }
    /* Screen actually renders (background blit + text), not a blank/black
     * frame — weak sanity check that reports_render_hall_of_fame ran. */
    static uint8_t pixels[320 * 200];
    ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
    ColonizePalette palette;
    memset(pixels, 0, sizeof(pixels));
    game_render(game, &fb, &palette);
    bool any_nonzero = false;
    for (size_t i = 0; i < sizeof(pixels) && !any_nonzero; ++i) {
      any_nonzero = pixels[i] != 0;
    }
    if (!any_nonzero) {
      fprintf(stderr, "Hall of Fame screen rendered a blank frame\n");
      return 1;
    }
    /* Enter closes it back to the title menu (Esc covered in case 1). */
    if (!step(game, COLONIZE_KEY_ENTER)) {
      fprintf(stderr, "Enter from Hall of Fame failed\n");
      return 1;
    }
    if (game_in_hall_of_fame(game) || !game_in_menu(game)) {
      fprintf(stderr, "expected Enter to return to title menu\n");
      return 1;
    }
    game_destroy(game);
  }

  /* 4. Table caps at COLONIZE_HOF_MAX (10): 12 distinct scores, lowest 2
   * dropped, order still highest-first. */
  {
    FILE* f = fopen(hof_path, "wb");
    if (!f) {
      fprintf(stderr, "failed to seed overflow HOF.TXT\n");
      return 1;
    }
    for (int i = 1; i <= 12; ++i) {
      fprintf(f, "%d|Leader%d|Nation|1700|0\n", i * 100, i);
    }
    fclose(f);
    ColonizeGameState* game = game_create(&cfg);
    if (!game) {
      fprintf(stderr, "game_create failed (overflow)\n");
      return 1;
    }
    if (game_hof_count(game) != 10) {
      fprintf(stderr, "expected cap of 10 entries, got %d\n", game_hof_count(game));
      return 1;
    }
    ColonizeHofEntryView top, bottom;
    if (!game_hof_entry(game, 0, &top) || top.score != 1200) {
      fprintf(stderr, "expected top score 1200\n");
      return 1;
    }
    if (!game_hof_entry(game, 9, &bottom) || bottom.score != 300) {
      fprintf(stderr, "expected 10th-place score 300 (100/200 dropped), got %d\n", bottom.score);
      return 1;
    }
    game_destroy(game);
  }

  remove(hof_path);
  printf("unit_hall_of_fame ok\n");
  return 0;
}
