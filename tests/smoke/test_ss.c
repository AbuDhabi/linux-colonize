#include <stdio.h>

#include "core/ss.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  ColonizeSpriteSheet terrain;
  char err[256];
  if (!ss_load("COLONIZE/TERRAIN.SS", &terrain, err, sizeof(err))) {
    fprintf(stderr, "terrain load failed: %s\n", err);
    return 1;
  }

  for (int i = 0; i < terrain.sprite_count; ++i) {
    fprintf(stderr, "terrain[%d]=%dx%d\n", i, terrain.sprites[i].width, terrain.sprites[i].height);
  }

  ColonizeSpriteSheet cursor;
  if (!ss_load("COLONIZE/CURSOR.SS", &cursor, err, sizeof(err))) {
    fprintf(stderr, "cursor load failed: %s\n", err);
    ss_free(&terrain);
    return 1;
  }
  for (int i = 0; i < cursor.sprite_count; ++i) {
    fprintf(stderr, "cursor[%d]=%dx%d\n", i, cursor.sprites[i].width, cursor.sprites[i].height);
  }

  ss_free(&cursor);
  ss_free(&terrain);
  diag_shutdown();
  return 0;
}
