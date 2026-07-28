#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/game_loop.h"

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

static bool create_required_assets(const char* dir) {
  char path[512];
  if (mkdir(dir, 0755) != 0) {
    /* continue if already exists */
  }

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
        "{COLONIZATION} Version %STRING0\r\n"
        "@options\r\n"
        "Start a Game in NEW WORLD\r\n"
        "LOAD Game\r\n"
        "Exit to DOS\r\n"
      )) {
    return false;
  }
  snprintf(path, sizeof(path), "%s/VICEROY.PAL", dir);
  if (!write_palette(path)) return false;
  return true;
}

int main(void) {
  const char* data_dir = "./test-assets";
  const char* save_dir = "./test-saves-flow";
  if (!create_required_assets(data_dir)) {
    fprintf(stderr, "failed to create test assets\n");
    return 1;
  }

  ColonizeGameConfig cfg = {.data_dir = data_dir, .save_dir = save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed\n");
    return 1;
  }

  ColonizeInputState input = {0};
  input.last_key = COLONIZE_KEY_ENTER;
  if (!game_update(game, &input, 16)) {
    fprintf(stderr, "game_update failed on enter\n");
    game_destroy(game);
    return 1;
  }

  for (int i = 0; i < 500; ++i) {
    ColonizeInputState frame = {0};
    if (i % 30 == 0) {
      frame.last_key = COLONIZE_KEY_SPACE;
    } else if (i == 200) {
      frame.last_key = COLONIZE_KEY_S;
    } else if (i == 300) {
      frame.last_key = COLONIZE_KEY_L;
    }
    if (!game_update(game, &frame, 16)) {
      fprintf(stderr, "game loop aborted unexpectedly at frame %d\n", i);
      game_destroy(game);
      return 1;
    }
  }

  ColonizeFramebuffer8 fb;
  ColonizePalette pal;
  uint8_t pixels[320 * 200];
  fb.width = 320;
  fb.height = 200;
  fb.pixels = pixels;
  game_render(game, &fb, &pal);

  game_destroy(game);
  return 0;
}
