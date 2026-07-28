#include "core/pik.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/madspack.h"
#include "platform/diagnostics.h"

static uint16_t read_u16(const uint8_t* p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

bool pik_load(const char* path, ColonizePikImage* out_image, char* err, size_t err_size) {
  if (!path || !out_image) {
    snprintf(err, err_size, "pik_load bad args");
    return false;
  }
  memset(out_image, 0, sizeof(*out_image));

  MadspackFile pack;
  if (!madspack_load(path, &pack, err, err_size)) {
    return false;
  }
  if (pack.section_count < 2) {
    madspack_free(&pack);
    snprintf(err, err_size, "PIK needs >=2 sections: %s", path);
    return false;
  }

  const MadspackSection* header = &pack.sections[0];
  const MadspackSection* pixels = &pack.sections[1];
  if (header->data_size < 8) {
    madspack_free(&pack);
    snprintf(err, err_size, "PIK header too small: %s", path);
    return false;
  }

  int height = read_u16(header->data + 0);
  int width = read_u16(header->data + 2);
  if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
    madspack_free(&pack);
    snprintf(err, err_size, "PIK invalid dimensions %dx%d in %s", width, height, path);
    return false;
  }

  size_t needed = (size_t)width * (size_t)height;
  if (pixels->data_size < needed) {
    madspack_free(&pack);
    snprintf(err, err_size, "PIK pixel section too small (%zu < %zu) in %s", pixels->data_size, needed, path);
    return false;
  }

  uint8_t* copy = malloc(needed);
  if (!copy) {
    madspack_free(&pack);
    snprintf(err, err_size, "oom PIK pixels");
    return false;
  }
  memcpy(copy, pixels->data, needed);
  out_image->width = width;
  out_image->height = height;
  out_image->pixels = copy;

  if (pack.section_count >= 3 && pack.sections[2].data_size >= 768) {
    assets_palette_from_col768(pack.sections[2].data, pack.sections[2].data_size, &out_image->palette);
    out_image->has_palette = true;
  }

  diag_info("Loaded PIK %s (%dx%d palette=%s)", path, width, height, out_image->has_palette ? "yes" : "no");
  madspack_free(&pack);
  return true;
}

void pik_free(ColonizePikImage* image) {
  if (!image) {
    return;
  }
  free(image->pixels);
  image->pixels = NULL;
  image->width = 0;
  image->height = 0;
  image->has_palette = false;
}

void pik_blit(
  const ColonizePikImage* image,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y
) {
  if (!image || !image->pixels || !framebuffer || !framebuffer->pixels) {
    return;
  }
  for (int y = 0; y < image->height; ++y) {
    int fy = dst_y + y;
    if (fy < 0 || fy >= framebuffer->height) {
      continue;
    }
    for (int x = 0; x < image->width; ++x) {
      int fx = dst_x + x;
      if (fx < 0 || fx >= framebuffer->width) {
        continue;
      }
      framebuffer->pixels[fy * framebuffer->width + fx] = image->pixels[y * image->width + x];
    }
  }
}
