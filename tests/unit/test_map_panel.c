#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/map.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/ss.h"
#include "core/units.h"
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
  if (MAP_PANEL_MINIMAP_H != 39 || MAP_PANEL_MINIMAP_W != 56) {
    return fail("minimap window expected 56x39");
  }
  if (MAP_PANEL_TEXT_MARGIN != 2) {
    return fail("text margin expected 2");
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
  if (mw != 56 || mh != 39) {
    fprintf(stderr, "minimap size expected 56x39, got %dx%d\n", mw, mh);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }
  /* DOS inset: window origin ≥ 1 (never includes the 1-tile rim). */
  if (ox != 1 || oy != 1) {
    fprintf(stderr, "expected origin 1,1 at NW of visible map, got %d,%d\n", ox, oy);
    map_free(&map);
    map_panel_free(&panel);
    assets_msg_free(&labels);
    return 1;
  }

  {
    int vx = -1;
    int vy = -1;
    map_panel_clamp_view_origin(58, 72, 0, 0, 15, 12, &vx, &vy);
    if (vx != 1 || vy != 1) {
      fprintf(stderr, "view origin NW expected 1,1 got %d,%d\n", vx, vy);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return 1;
    }
    map_panel_clamp_view_origin(58, 72, 57, 71, 15, 12, &vx, &vy);
    if (vx != 42 || vy != 59) {
      fprintf(stderr, "view origin SE expected 42,59 got %d,%d\n", vx, vy);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return 1;
    }
    if (map_coords_inset(&map, 0, 10) || map_coords_inset(&map, 57, 10) ||
        !map_coords_inset(&map, 1, 1) || !map_coords_inset(&map, 56, 70)) {
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return fail("map_coords_inset rim/interior mismatch");
    }
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
      tx != 11 || ty != 21) {
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
    0,
    1492,
    0,
    1000,
    0,
    "New England",
    NULL,
    false,
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

  /* Dark-orange border pixel just above minimap terrain (touches menu rule above). */
  if (pixels[(my - 1) * 320 + mx] != 6) {
    fprintf(stderr, "minimap border color expected 6, got %u\n", pixels[(my - 1) * 320 + mx]);
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

  /* Selected ship: With: hold icons (passengers) must paint into the sidebar. */
  {
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
      free(pixels);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return fail("NAMES.TXT load failed for hold icon check");
    }
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    if (!units_load_types(&units, &names)) {
      assets_msg_free(&names);
      free(pixels);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return fail("units_load_types failed");
    }
    units_new_world_start(&units, &map, 39, 10, 0, 0);
    ColonizeUnit* ship = units_get(&units, units.selected_id);
    if (!ship || ship->cargo_count < 2) {
      assets_msg_free(&names);
      free(pixels);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return fail("starter ship missing passengers for With: check");
    }

    ColonizeSpriteSheet icons;
    memset(&icons, 0, sizeof(icons));
    char err2[256];
    const bool icons_ok = ss_load("COLONIZE/ICONS.SS", &icons, err2, sizeof(err2));

    memset(pixels, 0, 320 * 200);
    map_panel_render(
      &panel,
      &map,
      &units,
      NULL,
      icons_ok ? &icons : NULL,
      NULL,
      &names,
      &labels,
      NULL,
      ship->x > 7 ? ship->x - 7 : 0,
      ship->y > 6 ? ship->y - 6 : 0,
      MAP_VIEW_TILE_COLS,
      MAP_VIEW_TILE_ROWS,
      ship->x,
      ship->y,
      ship->id,
      0,
      1492,
      0,
      1000,
      0,
      "England",
      NULL,
      false,
      &fb
    );

    int hold_pixels = 0;
    for (int y = 80; y < 160; ++y) {
      for (int x = MAP_PANEL_X + 2; x < 320; ++x) {
        if (pixels[y * 320 + x] != 0 && pixels[y * 320 + x] != 8) {
          hold_pixels++;
        }
      }
    }
    if (icons_ok) {
      ss_free(&icons);
    }
    assets_msg_free(&names);
    if (hold_pixels < 20) {
      fprintf(stderr, "expected With: hold icons in sidebar (opaque=%d)\n", hold_pixels);
      free(pixels);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return 1;
    }
  }

  /* Tribe settlement (#10 tipis for tech 0) blits on the main map viewport. */
  {
    ColonizeSpriteSheet icons;
    memset(&icons, 0, sizeof(icons));
    char err2[256];
    if (!ss_load("COLONIZE/ICONS.SS", &icons, err2, sizeof(err2))) {
      fprintf(stderr, "ICONS.SS for tribe marker: %s\n", err2);
      free(pixels);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return 1;
    }
    if (icons.sprite_count < 14 || icons.sprites[10].width != 21) {
      fprintf(stderr, "expected ICONS #10 tribe settlement 21px wide\n");
      ss_free(&icons);
      free(pixels);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return 1;
    }
    ColonizeCol1Tribe tribe;
    memset(&tribe, 0, sizeof(tribe));
    tribe.x = 3;
    tribe.y = 4;
    tribe.nation_id = 4;
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.tribe_count = 1;
    col1.tribe = &tribe;
    /* nation 4 → indian[0]; tech 0 → tipis #10 */
    col1.indian[0].tech = 0;

    uint8_t tile_px[16 * 16];
    memset(tile_px, 0, sizeof(tile_px));
    ColonizeFramebuffer8 tile_fb = {.width = 16, .height = 16, .pixels = tile_px};
    map_panel_render_tribes_on_map(&col1, &icons, &tile_fb, 3, 4, 1, 1, 16, 16, 0, 0, NULL, 0);

    int opaque = 0;
    for (int i = 0; i < 16 * 16; ++i) {
      if (tile_px[i] != 0 && tile_px[i] != COLONIZE_SS_TRANSPARENT) {
        ++opaque;
      }
    }
    ss_free(&icons);
    if (opaque < 8) {
      fprintf(stderr, "expected tribe map icon opaque pixels, got %d\n", opaque);
      free(pixels);
      map_free(&map);
      map_panel_free(&panel);
      assets_msg_free(&labels);
      return 1;
    }
  }

  free(pixels);
  map_free(&map);
  map_panel_free(&panel);
  assets_msg_free(&labels);
  printf("unit_map_panel: ok\n");
  return 0;
}
