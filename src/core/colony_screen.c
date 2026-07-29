#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

void colony_screen_set_status(ColonyScreenView* view, const char* text) {
  if (!view) {
    return;
  }
  snprintf(view->status, sizeof(view->status), "%s", text ? text : "");
}

static bool colony_screen_load_pik(
  const char* data_dir,
  const char* filename,
  ColonizePikImage* out_image,
  char* err,
  size_t err_size
) {
  char pik_path[512];
  char pik_err[256];
  if (!dos_compat_normalize_asset_path(data_dir, filename, pik_path, sizeof(pik_path))) {
    snprintf(err, err_size, "%s path resolve failed", filename);
    return false;
  }
  if (!pik_load(pik_path, out_image, pik_err, sizeof(pik_err))) {
    snprintf(err, err_size, "%s: %s", filename, pik_err);
    return false;
  }
  return true;
}

/* Remap sprite pixels from src_pal colors onto nearest indices in dst_pal. */
static void remap_sheet_to_palette(
  ColonizeSpriteSheet* sheet,
  const ColonizePalette* dst_pal
) {
  if (!sheet || !dst_pal || !sheet->has_palette) {
    return;
  }

  uint8_t lut[256];
  for (int i = 0; i < 256; ++i) {
    if (i == COLONIZE_SS_TRANSPARENT) {
      lut[i] = (uint8_t)COLONIZE_SS_TRANSPARENT;
      continue;
    }
    const int sr = sheet->palette.rgb[i][0];
    const int sg = sheet->palette.rgb[i][1];
    const int sb = sheet->palette.rgb[i][2];
    int best = 0;
    int best_d = 1 << 30;
    for (int j = 0; j < 256; ++j) {
      const int dr = sr - dst_pal->rgb[j][0];
      const int dg = sg - dst_pal->rgb[j][1];
      const int db = sb - dst_pal->rgb[j][2];
      const int d = dr * dr + dg * dg + db * db;
      if (d < best_d) {
        best_d = d;
        best = j;
      }
    }
    lut[i] = (uint8_t)best;
  }

  for (int s = 0; s < sheet->sprite_count; ++s) {
    ColonizeSprite* spr = &sheet->sprites[s];
    if (!spr->pixels) {
      continue;
    }
    const int n = spr->width * spr->height;
    for (int p = 0; p < n; ++p) {
      spr->pixels[p] = lut[spr->pixels[p]];
    }
  }
  sheet->palette = *dst_pal;
}

bool colony_screen_load(ColonyScreenView* view, const char* data_dir, char* err, size_t err_size) {
  if (!view || !data_dir) {
    snprintf(err, err_size, "colony_screen_load bad args");
    return false;
  }
  memset(view, 0, sizeof(*view));

  if (!colony_screen_load_pik(data_dir, "WOODPANL.PIK", &view->frame, err, err_size)) {
    return false;
  }
  view->frame_ok = true;

  char ss_path[512];
  char ss_err[256];
  if (!dos_compat_normalize_asset_path(data_dir, "WOODFRAM.SS", ss_path, sizeof(ss_path))) {
    snprintf(err, err_size, "WOODFRAM.SS path resolve failed");
    colony_screen_free(view);
    return false;
  }
  if (!ss_load(ss_path, &view->wood_frame, ss_err, sizeof(ss_err))) {
    snprintf(err, err_size, "WOODFRAM.SS: %s", ss_err);
    colony_screen_free(view);
    return false;
  }
  /* WOODFRAM ships its own palette; remap into WOODPANL indices for shared DAC. */
  remap_sheet_to_palette(&view->wood_frame, &view->frame.palette);
  view->wood_frame_ok = true;

  if (!colony_screen_load_pik(data_dir, "COLONY.PIK", &view->bottom_panel, err, err_size)) {
    colony_screen_free(view);
    return false;
  }
  view->bottom_panel_ok = true;

  colony_screen_set_status(view, "Colony ready. Esc or C returns to map.");
  diag_info(
    "Colony screen loaded (WOODPANL %dx%d, WOODFRAM %d sprites, COLONY.PIK %dx%d)",
    view->frame.width,
    view->frame.height,
    view->wood_frame.sprite_count,
    view->bottom_panel.width,
    view->bottom_panel.height
  );
  return true;
}

void colony_screen_free(ColonyScreenView* view) {
  if (!view) {
    return;
  }
  pik_free(&view->frame);
  ss_free(&view->wood_frame);
  pik_free(&view->bottom_panel);
  memset(view, 0, sizeof(*view));
}

static void colony_screen_render_minimap(
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  int colony_x,
  int colony_y,
  ColonizeFramebuffer8* framebuffer
) {
  if (!map || !terrain || !framebuffer) {
    return;
  }

  const int half = COLONY_MINIMAP_GRID / 2;
  for (int dy = -half; dy <= half; ++dy) {
    for (int dx = -half; dx <= half; ++dx) {
      const int mx = colony_x + dx;
      const int my = colony_y + dy;
      const int tile_x = COLONY_MINIMAP_X + (dx + half) * COLONY_MINIMAP_TILE;
      const int tile_y = COLONY_MINIMAP_Y + (dy + half) * COLONY_MINIMAP_TILE;
      const int sprite = map_terrain_sprite_at(map, mx, my);
      if (sprite >= 0 && sprite < terrain->sprite_count) {
        ss_blit_sprite(terrain, sprite, framebuffer, tile_x, tile_y);
      }
    }
  }
}

void colony_screen_render(
  const ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);

  if (view && view->frame_ok) {
    pik_blit(&view->frame, framebuffer, 0, 0);
  }

  if (view && view->wood_frame_ok && view->wood_frame.sprite_count > 0) {
    ss_blit_sprite(&view->wood_frame, 0, framebuffer, 0, 0);
  }

  if (colony && map && terrain) {
    colony_screen_render_minimap(map, terrain, colony->x, colony->y, framebuffer);
  }

  if (view && view->bottom_panel_ok) {
    pik_blit(&view->bottom_panel, framebuffer, 0, COLONY_BOTTOM_PANEL_Y);
  }

  if (colony) {
    char line[96];
    snprintf(line, sizeof(line), "%s", colony->name);
    font_draw_text(font, framebuffer, 8, 8, line, 15);

    snprintf(line, sizeof(line), "Pop %d", colony->population);
    font_draw_text(font, framebuffer, 8, 18, line, 14);
  }

  if (view) {
    if (!view->frame_ok) {
      font_draw_text(font, framebuffer, 4, 100, "WOODPANL.PIK failed to load", 12);
    }
    if (!view->bottom_panel_ok) {
      font_draw_text(font, framebuffer, 4, 112, "COLONY.PIK failed to load", 12);
    }
  }
}
