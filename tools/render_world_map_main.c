/*
 * render_world_map: the whole map of a save at native 16 px per tile, fog off
 * (the CHEAT Complete Map view, fog_view -1), as a binary PPM.
 *
 *   render_world_map <data_dir> <save.SAV> <out.ppm>
 *
 * Brings up a headless game (game_create + game_load_col1_file) and stitches
 * game_render_map_composite's zoom-0 15x12-tile viewport over the map. Each
 * window keeps only its inner tiles (a 2-tile margin, except at the map edge)
 * so labels that spill past a tile are not cut at window seams. Post-
 * decimation overlays (cursor, cheat markers) and the side panel are not drawn.
 */

#define _POSIX_C_SOURCE 200809L /* mkdtemp */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/game_loop.h"
#include "core/game_loop_internal.h"
#include "core/map_panel.h"

enum { VIEW_COLS = 15, VIEW_ROWS = 12, TILE = 16, MARGIN = 2 };

static int clampi(int v, int lo, int hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

int main(int argc, char** argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: %s <data_dir> <save.SAV> <out.ppm>\n", argv[0]);
    return 2;
  }
  char save_dir[] = "/tmp/render_world_map.XXXXXX";
  if (!mkdtemp(save_dir)) {
    perror("mkdtemp");
    return 1;
  }
  ColonizeGameConfig cfg = {.data_dir = argv[1], .save_dir = save_dir};
  ColonizeGameState* game = game_create(&cfg);
  char err[256] = "";
  if (!game || !game_load_col1_file(game, argv[2], err, sizeof(err))) {
    fprintf(stderr, "load %s failed: %s\n", argv[2], err);
    return 1;
  }
  game->fog_view = -1;

  const int w = (int)game->world_map.width;
  const int h = (int)game->world_map.height;
  const int out_w = w * TILE;
  const int out_h = h * TILE;
  uint8_t* out = calloc((size_t)out_w * (size_t)out_h, 1);
  static uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  const int step_x = VIEW_COLS - 2 * MARGIN;
  const int step_y = VIEW_ROWS - 2 * MARGIN;
  for (int ty = 0; ty < h; ty += step_y) {
    const int oy = clampi(ty - MARGIN, 0, h - VIEW_ROWS);
    for (int tx = 0; tx < w; tx += step_x) {
      const int ox = clampi(tx - MARGIN, 0, w - VIEW_COLS);
      game_render_map_composite(game, &fb, 0, ox, oy, VIEW_COLS, VIEW_ROWS);
      for (int y = ty * TILE; y < (ty + step_y) * TILE && y < out_h; ++y) {
        const uint8_t* src = &pixels[(size_t)(MAP_MENU_BAR_H + y - oy * TILE) * 320u + (size_t)(tx - ox) * TILE];
        const int n = ((tx + step_x < w ? tx + step_x : w) - tx) * TILE;
        memcpy(&out[(size_t)y * (size_t)out_w + (size_t)tx * TILE], src, (size_t)n);
      }
    }
  }

  ColonizePalette pal;
  memset(&pal, 0, sizeof(pal));
  game_render_select_palette(game, &fb, &pal, 0);
  FILE* f = fopen(argv[3], "wb");
  if (!f) {
    perror(argv[3]);
    return 1;
  }
  fprintf(f, "P6\n%d %d\n255\n", out_w, out_h);
  for (size_t i = 0; i < (size_t)out_w * (size_t)out_h; ++i) {
    fwrite(pal.rgb[out[i]], 1, 3, f);
  }
  fclose(f);
  free(out);
  game_destroy(game);
  rmdir(save_dir);
  fprintf(stderr, "wrote %s (%dx%d tiles)\n", argv[3], w, h);
  return 0;
}
