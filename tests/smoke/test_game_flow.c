#include <stdio.h>

#include "core/game_loop.h"

#include "tests/common/test_assets.h"

int main(void) {
  const char* data_dir = "./test-assets";
  const char* save_dir = "./test-saves-flow";
  if (!test_assets_create(data_dir, 0)) {
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
  /* Stay on title: move to Exit and do not activate Start (wizard). */
  input.last_key = COLONIZE_KEY_DOWN;
  if (!game_update(game, &input, 16)) {
    fprintf(stderr, "game_update failed on menu down\n");
    game_destroy(game);
    return 1;
  }
  input = (ColonizeInputState){0};
  input.last_key = COLONIZE_KEY_DOWN;
  if (!game_update(game, &input, 16)) {
    fprintf(stderr, "game_update failed on menu down 2\n");
    game_destroy(game);
    return 1;
  }

  for (int i = 0; i < 500; ++i) {
    ColonizeInputState frame = {0};
    if (i % 30 == 0) {
      frame.last_key = COLONIZE_KEY_UP;
    } else if (i == 200) {
      frame.last_key = COLONIZE_KEY_DOWN;
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
