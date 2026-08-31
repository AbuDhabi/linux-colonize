#include "core/ss.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/madspack.h"
#include "platform/diagnostics.h"

#define SS_LM_FILL_LINE 0xFF
#define SS_LM_PIXEL 0xFE
#define SS_LM_MULTIPixel 0xFD
#define SS_LM_END_IMAGE 0xFC

typedef struct SsHeader {
  uint8_t mode;
  uint8_t pflag;
  uint16_t nsprites;
  /* Popup-placement words (MSSn/MYRn decorations); see ss.h. */
  int16_t place_offset_y;
  int16_t place_mode;
  int16_t place_offset_x;
} SsHeader;

typedef struct SpriteHeader {
  uint32_t start_offset;
  uint32_t length;
  int16_t anchor_x;
  int16_t anchor_y;
  uint16_t width;
  uint16_t height;
} SpriteHeader;

static uint16_t read_u16(const uint8_t* p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t* p) {
  return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static bool parse_ss_header(const uint8_t* data, size_t size, SsHeader* out) {
  if (!data || size < 0x98 || !out) {
    return false;
  }
  out->mode = data[0];
  out->pflag = data[0x0c];
  out->place_offset_y = (int16_t)read_u16(data + 0x0e);
  out->place_mode = (int16_t)read_u16(data + 0x10);
  out->place_offset_x = (int16_t)read_u16(data + 0x12);
  out->nsprites = read_u16(data + 0x26);
  return out->nsprites > 0;
}

static bool parse_sprite_header(const uint8_t* data, SpriteHeader* out) {
  if (!data || !out) {
    return false;
  }
  out->start_offset = read_u32(data + 0);
  out->length = read_u32(data + 4);
  out->anchor_x = (int16_t)read_u16(data + 8);
  out->anchor_y = (int16_t)read_u16(data + 10);
  out->width = read_u16(data + 12);
  out->height = read_u16(data + 14);
  return true;
}

static size_t sprite_compressed_size(
  const SpriteHeader* headers,
  int header_count,
  int sprite_index,
  size_t section_size
) {
  const SpriteHeader* current = &headers[sprite_index];
  if (sprite_index + 1 < header_count) {
    uint32_t next_offset = headers[sprite_index + 1].start_offset;
    if (next_offset > current->start_offset) {
      return (size_t)(next_offset - current->start_offset);
    }
  }
  if (section_size > current->start_offset) {
    return section_size - (size_t)current->start_offset;
  }
  return 0;
}

static bool decode_sprite(
  const uint8_t* encoded,
  size_t encoded_size,
  const SpriteHeader* header,
  size_t compressed_size,
  uint8_t mode,
  uint8_t* out_pixels,
  char* err,
  size_t err_size
) {
  if (!encoded || !header || !out_pixels) {
    snprintf(err, err_size, "decode_sprite bad args");
    return false;
  }
  if (header->width == 0 || header->height == 0) {
    memset(out_pixels, 0, 1);
    return true;
  }
  if (header->start_offset + compressed_size > encoded_size) {
    snprintf(err, err_size, "sprite data out of bounds (off=%u comp=%zu sec=%zu)",
      header->start_offset, compressed_size, encoded_size);
    return false;
  }

  const uint8_t* src = encoded + header->start_offset;
  size_t src_len = compressed_size;
  uint8_t* decoded = NULL;
  const uint8_t* stream = src;
  size_t stream_len = src_len;

  if (mode == 1) {
    decoded = malloc(header->length > 0 ? header->length : 1);
    if (!decoded) {
      snprintf(err, err_size, "oom sprite fab buffer");
      return false;
    }
    char fab_err[256];
    if (!fab_decompress(src, src_len, decoded, header->length, fab_err, sizeof(fab_err))) {
      free(decoded);
      snprintf(err, err_size, "sprite FAB failed: %s", fab_err);
      return false;
    }
    stream = decoded;
    stream_len = header->length;
  }

  size_t k = 0;
  int i = 0;
  int j = 0;
  const uint8_t bg = COLONIZE_SS_TRANSPARENT;

  memset(out_pixels, bg, (size_t)header->width * (size_t)header->height);

  if (stream_len == 0) {
    free(decoded);
    return true;
  }

  if (k >= stream_len) {
    free(decoded);
    return true;
  }

  uint8_t lm = stream[k++];

  while (1) {
    if (lm == SS_LM_END_IMAGE) {
      break;
    }

    if (lm == SS_LM_FILL_LINE) {
      while (i < header->width && j < header->height) {
        out_pixels[j * header->width + i++] = bg;
      }
      i = 0;
      j++;
      if (k >= stream_len || j >= header->height) {
        break;
      }
      lm = stream[k++];
      continue;
    }

    if (k >= stream_len) {
      break;
    }
    uint8_t x = stream[k++];

    if (x == SS_LM_FILL_LINE) {
      while (i < header->width && j < header->height) {
        out_pixels[j * header->width + i++] = bg;
      }
      i = 0;
      j++;
      if (k >= stream_len || j >= header->height) {
        break;
      }
      lm = stream[k++];
      continue;
    }

    if (lm == SS_LM_PIXEL) {
      if (x == SS_LM_PIXEL) {
        if (k + 2 > stream_len) {
          break;
        }
        uint8_t run = stream[k++];
        uint8_t color = stream[k++];
        while (run > 0 && j < header->height) {
          if (i < header->width) {
            out_pixels[j * header->width + i++] = color;
          }
          run--;
        }
      } else if (j < header->height && i < header->width) {
        out_pixels[j * header->width + i++] = x;
      }
    } else if (lm == SS_LM_MULTIPixel) {
      if (k >= stream_len) {
        break;
      }
      uint8_t color = stream[k++];
      while (x > 0 && j < header->height) {
        if (i < header->width) {
          out_pixels[j * header->width + i++] = color;
        }
        x--;
      }
    } else {
      free(decoded);
      snprintf(err, err_size, "unknown sprite linemode 0x%02x", lm);
      return false;
    }
  }

  free(decoded);
  return true;
}

bool ss_load(const char* path, ColonizeSpriteSheet* out_sheet, char* err, size_t err_size) {
  if (!path || !out_sheet) {
    snprintf(err, err_size, "ss_load bad args");
    return false;
  }
  memset(out_sheet, 0, sizeof(*out_sheet));

  MadspackFile pack;
  if (!madspack_load(path, &pack, err, err_size)) {
    return false;
  }
  if (pack.section_count < 4) {
    madspack_free(&pack);
    snprintf(err, err_size, "SS needs 4 sections: %s", path);
    return false;
  }

  SsHeader ss_hdr;
  if (!parse_ss_header(pack.sections[0].data, pack.sections[0].data_size, &ss_hdr)) {
    madspack_free(&pack);
    snprintf(err, err_size, "invalid SS header: %s", path);
    return false;
  }

  if (pack.sections[2].data_size >= 768) {
    assets_palette_from_col768(pack.sections[2].data, pack.sections[2].data_size, &out_sheet->palette);
    out_sheet->has_palette = true;
  } else {
    diag_warn("SS %s palette section too small (%zu)", path, pack.sections[2].data_size);
  }

  const size_t header_bytes = (size_t)ss_hdr.nsprites * 16u;
  if (pack.sections[1].data_size < header_bytes) {
    madspack_free(&pack);
    snprintf(err, err_size, "SS sprite header table truncated: %s", path);
    return false;
  }

  out_sheet->sprites = calloc(ss_hdr.nsprites, sizeof(*out_sheet->sprites));
  if (!out_sheet->sprites) {
    madspack_free(&pack);
    snprintf(err, err_size, "oom SS sprites");
    return false;
  }
  out_sheet->sprite_count = ss_hdr.nsprites;
  out_sheet->place_offset_y = ss_hdr.place_offset_y;
  out_sheet->place_mode = ss_hdr.place_mode;
  out_sheet->place_offset_x = ss_hdr.place_offset_x;

  SpriteHeader* sprite_headers = calloc(ss_hdr.nsprites, sizeof(*sprite_headers));
  if (!sprite_headers) {
    ss_free(out_sheet);
    madspack_free(&pack);
    snprintf(err, err_size, "oom SS sprite headers");
    return false;
  }
  for (uint16_t si = 0; si < ss_hdr.nsprites; ++si) {
    parse_sprite_header(pack.sections[1].data + (size_t)si * 16u, &sprite_headers[si]);
  }

  const uint8_t* sprite_data = pack.sections[3].data;
  const size_t sprite_data_size = pack.sections[3].data_size;

  for (uint16_t si = 0; si < ss_hdr.nsprites; ++si) {
    const SpriteHeader* sh = &sprite_headers[si];
    ColonizeSprite* sprite = &out_sheet->sprites[si];
    sprite->width = sh->width;
    sprite->height = sh->height;
    sprite->anchor_x = sh->anchor_x;
    sprite->anchor_y = sh->anchor_y;

    if (sh->width == 0 || sh->height == 0) {
      continue;
    }

    size_t pixel_count = (size_t)sh->width * (size_t)sh->height;
    sprite->pixels = malloc(pixel_count);
    if (!sprite->pixels) {
      free(sprite_headers);
      ss_free(out_sheet);
      madspack_free(&pack);
      snprintf(err, err_size, "oom sprite pixels");
      return false;
    }

    size_t comp_size = sprite_compressed_size(sprite_headers, ss_hdr.nsprites, si, sprite_data_size);
    char decode_err[256];
    if (!decode_sprite(sprite_data, sprite_data_size, sh, comp_size, ss_hdr.mode, sprite->pixels, decode_err, sizeof(decode_err))) {
      diag_warn("SS %s sprite %u decode failed: %s", path, si, decode_err);
      free(sprite->pixels);
      sprite->pixels = NULL;
      sprite->width = 0;
      sprite->height = 0;
    }
  }

  free(sprite_headers);
  madspack_free(&pack);
  diag_info("Loaded SS %s (%d sprites palette=%s mode=%u pflag=%u)",
    path, out_sheet->sprite_count, out_sheet->has_palette ? "yes" : "no", ss_hdr.mode, ss_hdr.pflag);
  return true;
}

void ss_free(ColonizeSpriteSheet* sheet) {
  if (!sheet) {
    return;
  }
  if (sheet->sprites) {
    for (int i = 0; i < sheet->sprite_count; ++i) {
      free(sheet->sprites[i].pixels);
    }
    free(sheet->sprites);
  }
  sheet->sprites = NULL;
  sheet->sprite_count = 0;
  sheet->has_palette = false;
}

void ss_blit_sprite(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y
) {
  if (!sheet || !framebuffer || !framebuffer->pixels || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sprite = &sheet->sprites[sprite_index];
  if (!sprite->pixels || sprite->width <= 0 || sprite->height <= 0) {
    return;
  }

  for (int y = 0; y < sprite->height; ++y) {
    int fy = dst_y + y;
    if (fy < 0 || fy >= framebuffer->height) {
      continue;
    }
    for (int x = 0; x < sprite->width; ++x) {
      int fx = dst_x + x;
      if (fx < 0 || fx >= framebuffer->width) {
        continue;
      }
      uint8_t color = sprite->pixels[y * sprite->width + x];
      if (color == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      framebuffer->pixels[fy * framebuffer->width + fx] = color;
    }
  }
}

void ss_blit_sprite_color(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y,
  uint8_t replace_color
) {
  if (!sheet || !framebuffer || !framebuffer->pixels || sprite_index < 0 ||
      sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sprite = &sheet->sprites[sprite_index];
  if (!sprite->pixels || sprite->width <= 0 || sprite->height <= 0) {
    return;
  }

  for (int y = 0; y < sprite->height; ++y) {
    int fy = dst_y + y;
    if (fy < 0 || fy >= framebuffer->height) {
      continue;
    }
    for (int x = 0; x < sprite->width; ++x) {
      int fx = dst_x + x;
      if (fx < 0 || fx >= framebuffer->width) {
        continue;
      }
      uint8_t color = sprite->pixels[y * sprite->width + x];
      if (color == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      framebuffer->pixels[fy * framebuffer->width + fx] = replace_color;
    }
  }
}

void ss_blit_sprite_where_dest(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* framebuffer,
  int dst_x,
  int dst_y,
  uint8_t match_color
) {
  if (!sheet || !framebuffer || !framebuffer->pixels || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sprite = &sheet->sprites[sprite_index];
  if (!sprite->pixels || sprite->width <= 0 || sprite->height <= 0) {
    return;
  }

  for (int y = 0; y < sprite->height; ++y) {
    int fy = dst_y + y;
    if (fy < 0 || fy >= framebuffer->height) {
      continue;
    }
    for (int x = 0; x < sprite->width; ++x) {
      int fx = dst_x + x;
      if (fx < 0 || fx >= framebuffer->width) {
        continue;
      }
      const int di = fy * framebuffer->width + fx;
      if (framebuffer->pixels[di] != match_color) {
        continue;
      }
      uint8_t color = sprite->pixels[y * sprite->width + x];
      if (color == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      framebuffer->pixels[di] = color;
    }
  }
}
