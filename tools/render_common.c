#include "tools/render_common.h"

#include <stdio.h>
#include <string.h>

#include "core/cinematic.h"
#include "core/font_file.h"
#include "core/unit_chrome.h"
#include "platform/platform.h"
#include "tests/common/golden_fixture.h"

bool render_load_sheet(const char* data_dir, const char* name, ColonizeSpriteSheet* out) {
  memset(out, 0, sizeof(*out));
  return ss_load_asset(data_dir, name, out, "render");
}

bool render_load_font(const char* data_dir, const char* name, ColonizeFont* out) {
  char path[512];
  char err[256];
  memset(out, 0, sizeof(*out));
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    return false;
  }
  if (!font_file_load(path, out, err, sizeof(err))) {
    fprintf(stderr, "font_file_load warning: %s\n", err);
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

  unit_chrome_load_orders(&out->names);
  const SavLoad sl = {
    .save = &out->save, .names = &out->names, .data_dir = data_dir, .units = &out->units,
    .colonies = &out->colonies, .map = &out->map, .europe = &out->europe, .br = &out->bridge,
  };
  if (!sav_load_into_pools(&sl, err, sizeof(err))) {
    fprintf(stderr, "save load failed: %s\n", err);
    return false;
  }
  return true;
}

void render_free_save(RenderSaveBundle* b) {
  europe_free(&b->europe);
  map_free(&b->map);
  assets_msg_free(&b->labels);
  assets_msg_free(&b->names);
  col1_save_free(&b->save);
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
