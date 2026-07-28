#ifndef COLONIZE_PIK_H
#define COLONIZE_PIK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/platform.h"

typedef struct ColonizePikImage {
  int width;
  int height;
  uint8_t* pixels; /* width*height indexed */
  ColonizePalette palette;
  bool has_palette;
} ColonizePikImage;

bool pik_load(const char* path, ColonizePikImage* out_image, char* err, size_t err_size);
void pik_free(ColonizePikImage* image);
void pik_blit(
  const ColonizePikImage* image,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y
);

#endif
