#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/map.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static int fail(const char* msg) {
  fprintf(stderr, "%s\n", msg);
  return 1;
}

int main(void) {
  diag_init(0, NULL);

  if (MAP_PANEL_X != 240 || MAP_PANEL_W != 80) {
    fprintf(stderr, "panel x/w expected 240/80, got %d/%d\n", MAP_PANEL_X, MAP_PANEL_W);
    return 1;
  }
  if (MAP_VIEW_TILE_COLS != 15 || MAP_VIEW_TILE_ROWS != 12) {
    fprintf(
      stderr,
      "viewport tiles expected 15x12, got %dx%d\n",
      MAP_VIEW_TILE_COLS,
      MAP_VIEW_TILE_ROWS
    );
    return 1;
  }
  if (MAP_MENU_BAR_H != 8) {
    fprintf(stderr, "menu bar height expected 8, got %d\n", MAP_MENU_BAR_H);
    return 1;
  }
  if (MAP_VIEW_W != 240 || MAP_VIEW_H != 192 || MAP_VIEW_ORIGIN_Y != MAP_MENU_BAR_H) {
    return fail("viewport pixel geometry mismatch");
  }
  if (MAP_VIEW_ORIGIN_Y + MAP_VIEW_H != 200) {
    return fail("viewport + menu should fill 200px");
  }
  if (MAP_PANEL_MINIMAP_H != 34 || MAP_PANEL_MINIMAP_W != 56) {
    return fail("minimap window expected 56x34");
  }

  ColonizeMsgCatalog labels;
  assets_msg_init(&labels);
  if (!assets_msg_load_file(&labels, "COLONIZE/LABELS.TXT")) {
    return fail("Failed to load LABELS.TXT");
  }

  MapPanel panel;
  if (!map_panel_load(&panel, "COLONIZE", &labels)) {
    assets_msg_free(&labels);
    return fail("map_panel_load failed");
  }
  if (!panel.wood_ok) {
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return fail("WOODTILE.SS missing");
  }
  if (strcmp(panel.label_moves, "Moves:") != 0) {
    fprintf(stderr, "unexpected @INFO moves label: '%s'\n", panel.label_moves);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }

  ColonizeWorldMap map;
  char err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
    fprintf(stderr, "map_load_mp: %s\n", err);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }
  if (map.width != 58 || map.height != 72) {
    fprintf(stderr, "AMER2 expected 58x72, got %ux%u\n", map.width, map.height);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }

  int mx, my, mw, mh, ox, oy;
  map_panel_minimap_rect(&map, 0, 0, 15, 12, &mx, &my, &mw, &mh, &ox, &oy);
  if (mw != 56 || mh != 34) {
    fprintf(stderr, "minimap size expected 56x34, got %dx%d\n", mw, mh);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }
  if (ox != 0 || oy != 0) {
    fprintf(stderr, "expected origin 0,0 at top-left of map window, got %d,%d\n", ox, oy);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }
  if (my != MAP_PANEL_MINIMAP_ORIGIN_Y) {
    fprintf(stderr, "minimap y expected %d, got %d\n", MAP_PANEL_MINIMAP_ORIGIN_Y, my);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }
  if (mx < MAP_PANEL_X || mx + mw > 320 || my < MAP_MENU_BAR_H) {
    fprintf(stderr, "minimap origin out of panel: %d,%d %dx%d\n", mx, my, mw, mh);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }

  /* Scrolled window near south pole. */
  map_panel_minimap_rect(&map, 0, 50, 15, 12, &mx, &my, &mw, &mh, &ox, &oy);
  if (oy + mh > (int)map.height) {
    fprintf(stderr, "minimap window overflows map: oy=%d mh=%d\n", oy, mh);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }

  int tx = -1;
  int ty = -1;
  if (!map_panel_minimap_click(&map, 0, 0, 15, 12, mx + 10, my + 20, &tx, &ty) ||
      tx != 10 || ty != 20) {
    fprintf(stderr, "minimap click map failed: %d,%d\n", tx, ty);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }
  if (map_panel_minimap_click(&map, 0, 0, 15, 12, mx - 1, my, &tx, &ty) ||
      map_panel_minimap_click(&map, 0, 0, 15, 12, mx + mw, my, &tx, &ty)) {
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return fail("click outside minimap should miss");
  }
  if (!map_panel_contains_xy(MAP_PANEL_X, MAP_MENU_BAR_H) ||
      map_panel_contains_xy(MAP_PANEL_X - 1, MAP_MENU_BAR_H) ||
      map_panel_contains_xy(MAP_PANEL_X, MAP_MENU_BAR_H - 1)) {
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return fail("map_panel_contains_xy geometry wrong");
  }

  uint8_t* pixels = (uint8_t*)calloc(320 * 200, 1);
  if (!pixels) {
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return fail("oom");
  }
  ColonizeFramebuffer8 fb;
  fb.width = 320;
  fb.height = 200;
  fb.pixels = pixels;
  map_panel_render(
    &panel,
    &map,
    NULL,
    NULL,
    NULL,
    NULL,
    5,
    5,
    MAP_VIEW_TILE_COLS,
    MAP_VIEW_TILE_ROWS,
    10,
    20,
    -1,
    1492,
    0,
    1000,
    &fb
  );

  /* Left edge of sidebar is the black rule. */
  if (pixels[MAP_MENU_BAR_H * 320 + MAP_PANEL_X] != 0) {
    fprintf(stderr, "expected black left edge at panel x\n");
    free(pixels);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }

  /* Panel must not write into the map viewport (except the shared left rule column is panel). */
  int panel_nonzero = 0;
  for (int y = MAP_MENU_BAR_H; y < 200; ++y) {
    for (int x = 0; x < MAP_PANEL_X; ++x) {
      if (pixels[y * 320 + x] != 0) {
        fprintf(stderr, "panel render wrote left of x=%d at %d,%d = %u\n", MAP_PANEL_X, x, y, pixels[y * 320 + x]);
        free(pixels);
        map_free(&map);
        map_panel_free(&panel);
        assets_msg_free(&labels);
        return 1;
      }
    }
    for (int x = MAP_PANEL_X + 1; x < 320; ++x) {
      if (pixels[y * 320 + x] != 0) {
        panel_nonzero = 1;
      }
    }
  }
  if (!panel_nonzero) {
    free(pixels);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return fail("panel region stayed blank");
  }

  /* Brown border pixel just above minimap terrain (touches menu rule above). */
  if (pixels[(my - 1) * 320 + mx] != 90) {
    fprintf(stderr, "minimap border color expected 90, got %u\n", pixels[(my - 1) * 320 + mx]);
    free(pixels);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }
  /* No wood gap: section black separator sits on the row under the brown bottom. */
  if (pixels[(my + mh + 1) * 320 + MAP_PANEL_X + 2] != 0) {
    fprintf(stderr, "expected black section separator under minimap border\n");
    free(pixels);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }

  /* View-rect corner should sit ON the edge tile of the view (world 5,5). */
  map_panel_minimap_rect(
    &map, 5, 5, MAP_VIEW_TILE_COLS, MAP_VIEW_TILE_ROWS, &mx, &my, &mw, &mh, &ox, &oy
  );
  const int vx = mx + (5 - ox);
  const int vy = my + (5 - oy);
  if (vx < 0 || vy < 0 || vx >= 320 || vy >= 200 || pixels[vy * 320 + vx] != 15) {
    fprintf(
      stderr,
      "view rect missing at edge tile pixel %d,%d (got %u)\n",
      vx,
      vy,
      (vx >= 0 && vy >= 0 && vx < 320 && vy < 200) ? pixels[vy * 320 + vx] : 0u
    );
    free(pixels);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }

  free(pixels);
  map_free(&map);
  map_panel_free(&panel);
  assets_msg_free(&labels);
  printf("smoke_map_panel: ok\n");
  return 0;
}
