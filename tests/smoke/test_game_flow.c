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

static bool create_required_assets(const char* dir) {
  char path[512];
  const char* files[] = {"MODULES.DB", "ERRORS.DB", "GAME.TXT", "MENU.TXT"};
  if (mkdir(dir, 0755) != 0) {
    /* continue if already exists */
  }
  for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
    snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
    if (!write_text_file(path, "stub\n")) {
      return false;
    }
  }
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
