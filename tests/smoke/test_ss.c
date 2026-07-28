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

  ColonizeSpriteSheet phys0;
  if (!ss_load("COLONIZE/PHYS0.SS", &phys0, err, sizeof(err))) {
    fprintf(stderr, "phys0 load failed: %s\n", err);
    ss_free(&cursor);
    ss_free(&terrain);
    return 1;
  }
  if (phys0.sprite_count < 107) {
    fprintf(stderr, "phys0 expected >=107 sprites, got %d\n", phys0.sprite_count);
    ss_free(&phys0);
    ss_free(&cursor);
    ss_free(&terrain);
    return 1;
  }
  fprintf(stderr, "phys0 sprites=%d overlay101=%dx%d\n",
    phys0.sprite_count,
    phys0.sprites[101].width,
    phys0.sprites[101].height);

  ss_free(&phys0);
  ss_free(&cursor);
  ss_free(&terrain);
  diag_shutdown();
  return 0;
}
