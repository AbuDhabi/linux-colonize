#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_screen.h"
#include "core/map.h"
#include "core/ss.h"
#include "core/units.h"
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

  if (!view.parch_ok || view.parch.sprite_count < 1) {
    fprintf(stderr, "PARCH.SS missing after load\n");
    colony_screen_free(&view);
    return 1;
  }

  if (!view.wood_tile_ok || view.wood_tile.sprite_count < 1) {
    fprintf(stderr, "WOODTILE.SS missing after load\n");
    colony_screen_free(&view);
    return 1;
  }

  if (!view.buildings_ok || view.buildings.sprite_count < 40) {
    fprintf(stderr, "BUILDING.SS missing/short after load (count=%d)\n", view.buildings.sprite_count);
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

  ColonizeColonyPool pool;
  colonies_init(&pool);
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") || !colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "building type load failed\n");
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  const int pioneer = units_find_type(&units, "Pioneers");
  if (pioneer < 0) {
    fprintf(stderr, "Pioneers type missing\n");
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  ColonizeWorldMap map;
  ColonizeSpriteSheet terrain;
  memset(&map, 0, sizeof(map));
  memset(&terrain, 0, sizeof(terrain));
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
    fprintf(stderr, "map_load_mp failed: %s\n", err);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  if (!ss_load("COLONIZE/TERRAIN.SS", &terrain, err, sizeof(err))) {
    fprintf(stderr, "ss_load TERRAIN failed: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  int land_x = -1;
  int land_y = -1;
  for (int y = 0; y < (int)map.height && land_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && land_x < 0; ++x) {
      if (map_tile_is_land(&map, x, y)) {
        land_x = x;
        land_y = y;
      }
    }
  }
  if (land_x < 0) {
    fprintf(stderr, "no land tile for colony\n");
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  const int cid = colonies_found(&pool, &map, land_x, land_y, pioneer, 100, 0, 0);
  if (cid < 0) {
    fprintf(stderr, "colonies_found failed\n");
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  const ColonizeColony* sample = colonies_get(&pool, cid);

  ColonizeSpriteSheet phys0;
  memset(&phys0, 0, sizeof(phys0));
  const bool phys0_ok = ss_load("COLONIZE/PHYS0.SS", &phys0, err, sizeof(err));

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  colony_screen_render(
    &view, &pool, sample, &units, &map, &terrain, phys0_ok ? &phys0 : NULL, NULL, &fb
  );

  if (pixels[0] == 0) {
    fprintf(stderr, "render produced empty top-left pixel (WOODPANL missing?)\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  const int bottom_idx = COLONY_BOTTOM_PANEL_Y * 320;
  if (pixels[bottom_idx] == 0) {
    fprintf(stderr, "bottom panel row looks empty at y=%d\n", COLONY_BOTTOM_PANEL_Y);
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  /* Town Hall sprite is blitted near (8,8) on parchment. */
  bool building_pixel = false;
  for (int y = 8; y < 40 && !building_pixel; ++y) {
    for (int x = 8; x < 70; ++x) {
      if (pixels[y * 320 + x] != 0) {
        building_pixel = true;
        break;
      }
    }
  }
  if (!building_pixel) {
    fprintf(stderr, "expected building pixels in Town Hall slot\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  /* 3×3 minimap centered in the WOODTILE section with equal L/R and T/B margins. */
  const int grid_px = COLONY_MINIMAP_GRID * COLONY_MINIMAP_TILE;
  const int margin_x = (COLONY_MINIMAP_SECTION_W - grid_px) / 2;
  const int margin_y = (COLONY_MINIMAP_SECTION_H - grid_px) / 2;
  if (margin_x != margin_y || margin_x < 1) {
    fprintf(
      stderr,
      "expected equal WOODTILE margins, got L/R=%d T/B=%d (section %dx%d grid %d)\n",
      margin_x,
      margin_y,
      COLONY_MINIMAP_SECTION_W,
      COLONY_MINIMAP_SECTION_H,
      grid_px
    );
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  const int mini_x = COLONY_MINIMAP_SECTION_X + margin_x;
  const int mini_y = COLONY_MINIMAP_SECTION_Y + margin_y;
  const int mini_x1 = mini_x + grid_px - 1;
  const int mini_y1 = mini_y + grid_px - 1;
  if (pixels[mini_y * 320 + mini_x] == 0 || pixels[mini_y1 * 320 + mini_x1] == 0) {
    fprintf(stderr, "minimap corners look empty (expected centered 3x3)\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  colony_screen_set_status(&view, "test status");
  if (strcmp(view.status, "test status") != 0) {
    fprintf(stderr, "set_status failed\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  fprintf(
    stderr,
    "colony screen tests ok (WOODPANL=%dx%d PARCH=%d WOODTILE=%d BUILDING=%d COLONY=%dx%d pop=%d mini=%dx%d@%d,%d margin=%d)\n",
    view.frame.width,
    view.frame.height,
    view.parch.sprite_count,
    view.wood_tile.sprite_count,
    view.buildings.sprite_count,
    view.bottom_panel.width,
    view.bottom_panel.height,
    sample ? sample->population : -1,
    COLONY_MINIMAP_GRID,
    COLONY_MINIMAP_TILE,
    mini_x,
    mini_y,
    margin_x
  );
  if (phys0_ok) {
    ss_free(&phys0);
  }
  ss_free(&terrain);
  map_free(&map);
  assets_msg_free(&names);
  colony_screen_free(&view);
  diag_shutdown();
  return 0;
}
