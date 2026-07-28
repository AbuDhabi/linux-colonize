#include "core/ff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/madspack.h"
#include "platform/diagnostics.h"

#define FF_HEADER_SIZE (2 + 128 + 256)

static uint16_t read_u16_le(const uint8_t* p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

bool ff_load(const char* path, ColonizeFont* out_font, char* err, size_t err_size) {
  if (!path || !out_font) {
    snprintf(err, err_size, "ff_load bad args");
    return false;
  }
  memset(out_font, 0, sizeof(*out_font));

  MadspackFile pack;
  char pack_err[256];
  if (!madspack_load(path, &pack, pack_err, sizeof(pack_err))) {
    snprintf(err, err_size, "%s", pack_err);
    return false;
  }

  if (pack.section_count < 1 || !pack.sections[0].data) {
    madspack_free(&pack);
    snprintf(err, err_size, "FF missing section 0 in %s", path);
    return false;
  }

  const MadspackSection* section = &pack.sections[0];
  if (section->data_size < FF_HEADER_SIZE) {
    madspack_free(&pack);
    snprintf(err, err_size, "FF header too small in %s", path);
    return false;
  }

  const uint8_t* data = section->data;
  out_font->max_height = data[0];
  out_font->max_width = data[1];
  if (out_font->max_height == 0 || out_font->max_width == 0) {
    madspack_free(&pack);
    snprintf(err, err_size, "FF invalid dimensions in %s", path);
    return false;
  }

  out_font->char_widths[0] = 0;
  for (int ch = 1; ch < 128; ++ch) {
    out_font->char_widths[ch] = data[2 + (ch - 1)];
  }

  const uint16_t glyph_base = (uint16_t)FF_HEADER_SIZE;
  out_font->char_offsets[0] = glyph_base;
  for (int ch = 1; ch < 128; ++ch) {
    out_font->char_offsets[ch] = read_u16_le(data + 2 + 128 + (ch - 1) * 2);
  }

  out_font->section_size = section->data_size;
  out_font->section_data = malloc(section->data_size);
  if (!out_font->section_data) {
    madspack_free(&pack);
    snprintf(err, err_size, "oom copying FF section in %s", path);
    return false;
  }
  memcpy(out_font->section_data, data, section->data_size);
  madspack_free(&pack);

  diag_info(
    "Loaded FF font %s (%ux%u, section=%zu bytes)",
    path,
    out_font->max_width,
    out_font->max_height,
    out_font->section_size
  );
  return true;
}

void ff_free(ColonizeFont* font) {
  if (!font) {
    return;
  }
  free(font->section_data);
  memset(font, 0, sizeof(*font));
}
