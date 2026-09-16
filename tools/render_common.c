#include "tools/render_common.h"

#include <stdio.h>
#include <string.h>

#include "core/ff.h"
#include "core/unit_chrome.h"
#include "platform/platform.h"

bool render_load_sheet(const char* data_dir, const char* name, ColonizeSpriteSheet* out) {
  char path[512];
  char err[256];
  memset(out, 0, sizeof(*out));
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    return false;
  }
  if (!ss_load(path, out, err, sizeof(err))) {
    fprintf(stderr, "ss_load %s warning: %s\n", name, err);
    return false;
  }
  return true;
}

bool render_load_font(const char* data_dir, const char* name, ColonizeFont* out) {
  char path[512];
  char err[256];
  memset(out, 0, sizeof(*out));
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    return false;
  }
  if (!ff_load(path, out, err, sizeof(err))) {
    fprintf(stderr, "ff_load warning: %s\n", err);
    return false;
  }
  return true;
}

static bool render_load_msg(const char* data_dir, const char* name, ColonizeMsgCatalog* out) {
  char path[512];
  memset(out, 0, sizeof(*out));
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    return false;
  }
  return assets_msg_load_file(out, path);
}

bool render_load_save(const char* data_dir, const char* save_path, RenderSaveBundle* out) {
  char err[256];

  memset(out, 0, sizeof(*out));
  if (!col1_save_read_file(save_path, &out->save, err, sizeof(err))) {
    fprintf(stderr, "col1_save_read_file failed: %s\n", err);
    return false;
  }
  out->human = col1_save_human_nation(&out->save);

  if (!render_load_msg(data_dir, "NAMES.TXT", &out->names)) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    return false;
  }
  out->labels_ok = render_load_msg(data_dir, "LABELS.TXT", &out->labels);

  colonies_init(&out->colonies);
  units_load_types(&out->units, &out->names);
  colonies_load_buildings(&out->colonies, &out->names);
  unit_chrome_load_orders(&out->names);

  if (!europe_load(&out->europe, data_dir, err, sizeof(err))) {
    fprintf(stderr, "europe_load failed: %s\n", err);
    return false;
  }
  /* Same pre-bridge reset game_apply_col1_save() does in game_loop.c. */
  out->europe.harbor_ships = 0;
  out->europe.dock_count = 0;

  if (!col1_bridge_apply_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&out->units), .colonies=(ColonizeColonyPool*)(&out->colonies), .map=(ColonizeWorldMap*)(&out->map), .col1=(ColonizeCol1Save*)(&out->save), .col1_ok=true, .europe=(EuropeScreen*)(&out->europe)}, &out->bridge, err, sizeof(err))) {
    fprintf(stderr, "col1_bridge_apply failed: %s\n", err);
    return false;
  }
  return true;
}

void render_fb_init(ColonizeFramebuffer8* fb, uint8_t* pixels) {
  memset(pixels, 0, 320 * 200);
  fb->width = 320;
  fb->height = 200;
  fb->pixels = pixels;
}

bool render_write_ppm(const char* out_path, const uint8_t* pixels, const ColonizePalette* pal) {
  FILE* f = fopen(out_path, "wb");
  if (!f) {
    fprintf(stderr, "cannot open %s for writing\n", out_path);
    return false;
  }
  fprintf(f, "P6\n320 200\n255\n");
  for (int i = 0; i < 320 * 200; ++i) {
    const uint8_t idx = pixels[i];
    const unsigned char rgb[3] = {pal->rgb[idx][0], pal->rgb[idx][1], pal->rgb[idx][2]};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  return true;
}
