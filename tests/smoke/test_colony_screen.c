#include <stdio.h>
#include <string.h>

#include "core/colony.h"
#include "core/colony_screen.h"
#include "core/map.h"
#include "core/ss.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  ColonyScreenView view;
  char err[256];
  if (!colony_screen_load(&view, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "colony_screen_load failed: %s\n", err);
    return 1;
  }

  if (!view.frame_ok || view.frame.width != 320 || view.frame.height != 200) {
    fprintf(
      stderr,
      "WOODPANL.PIK expected 320x200, got %dx%d ok=%d\n",
      view.frame.width,
      view.frame.height,
      view.frame_ok ? 1 : 0
    );
    colony_screen_free(&view);
    return 1;
  }
  if (!view.frame.has_palette) {
    fprintf(stderr, "WOODPANL.PIK should carry the colony screen palette\n");
    colony_screen_free(&view);
    return 1;
  }

  if (!view.wood_frame_ok || view.wood_frame.sprite_count < 1) {
    fprintf(stderr, "WOODFRAM.SS missing after load\n");
    colony_screen_free(&view);
    return 1;
  }

  if (!view.bottom_panel_ok || view.bottom_panel.width != 320 || view.bottom_panel.height != 72) {
    fprintf(
      stderr,
      "COLONY.PIK expected 320x72, got %dx%d ok=%d\n",
      view.bottom_panel.width,
      view.bottom_panel.height,
      view.bottom_panel_ok ? 1 : 0
    );
    colony_screen_free(&view);
    return 1;
  }

  ColonizeWorldMap map;
  ColonizeSpriteSheet terrain;
  memset(&map, 0, sizeof(map));
  memset(&terrain, 0, sizeof(terrain));
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
    fprintf(stderr, "map_load_mp failed: %s\n", err);
    colony_screen_free(&view);
    return 1;
  }
  if (!ss_load("COLONIZE/TERRAIN.SS", &terrain, err, sizeof(err))) {
    fprintf(stderr, "ss_load TERRAIN failed: %s\n", err);
    map_free(&map);
    colony_screen_free(&view);
    return 1;
  }

  ColonizeColony sample;
  memset(&sample, 0, sizeof(sample));
  sample.active = true;
  sample.id = 1;
  snprintf(sample.name, sizeof(sample.name), "%s", "Jamestown");
  sample.x = 39;
  sample.y = 10;
  sample.population = 3;

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  colony_screen_render(&view, &sample, &map, &terrain, NULL, &fb);

  if (pixels[0] == 0) {
    fprintf(stderr, "render produced empty top-left pixel (WOODPANL missing?)\n");
    ss_free(&terrain);
    map_free(&map);
    colony_screen_free(&view);
    return 1;
  }

  const int bottom_idx = COLONY_BOTTOM_PANEL_Y * 320;
  if (pixels[bottom_idx] == 0) {
    fprintf(stderr, "bottom panel row looks empty at y=%d\n", COLONY_BOTTOM_PANEL_Y);
    ss_free(&terrain);
    map_free(&map);
    colony_screen_free(&view);
    return 1;
  }

  colony_screen_set_status(&view, "test status");
  if (strcmp(view.status, "test status") != 0) {
    fprintf(stderr, "set_status failed\n");
    ss_free(&terrain);
    map_free(&map);
    colony_screen_free(&view);
    return 1;
  }

  fprintf(
    stderr,
    "colony screen tests ok (WOODPANL=%dx%d WOODFRAM=%d COLONY=%dx%d)\n",
    view.frame.width,
    view.frame.height,
    view.wood_frame.sprite_count,
    view.bottom_panel.width,
    view.bottom_panel.height
  );
  ss_free(&terrain);
  map_free(&map);
  colony_screen_free(&view);
  diag_shutdown();
  return 0;
}
