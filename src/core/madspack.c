#include "core/madspack.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

typedef struct FabBitReader {
  const uint8_t* src;
  size_t src_size;
  size_t pos;
  uint32_t bit_buffer;
  int bits_left;
} FabBitReader;

static bool fab_read_u16(FabBitReader* br, uint16_t* out) {
  if (br->pos + 2 > br->src_size) {
    return false;
  }
  *out = (uint16_t)(br->src[br->pos] | (br->src[br->pos + 1] << 8));
  br->pos += 2;
  return true;
}

static bool fab_get_bit(FabBitReader* br, int* out_bit) {
  br->bits_left -= 1;
  if (br->bits_left == 0) {
    uint16_t next = 0;
    if (!fab_read_u16(br, &next)) {
      return false;
    }
    br->bit_buffer = ((uint32_t)next << 1) | (br->bit_buffer & 1u);
    br->bits_left = 16;
  }
  *out_bit = (int)(br->bit_buffer & 1u);
  br->bit_buffer >>= 1;
  return true;
}

bool fab_decompress(
  const uint8_t* src,
  size_t src_size,
  uint8_t* dst,
  size_t expected_size,
  char* err,
  size_t err_size
) {
  if (!src || !dst || src_size < 6) {
    snprintf(err, err_size, "FAB input too small");
    return false;
  }
  if (memcmp(src, "FAB", 3) != 0) {
    snprintf(err, err_size, "FAB header missing");
    return false;
  }

  uint8_t shift_val = src[3];
  if (shift_val < 10 || shift_val >= 14) {
    snprintf(err, err_size, "FAB invalid shift_val=%u", shift_val);
    return false;
  }

  FabBitReader br = {
    .src = src,
    .src_size = src_size,
    .pos = 4,
    .bit_buffer = 0,
    .bits_left = 16
  };
  uint16_t initial = 0;
  if (!fab_read_u16(&br, &initial)) {
    snprintf(err, err_size, "FAB truncated bit buffer");
    return false;
  }
  br.bit_buffer = initial;

  const int copy_adr_shift = 16 - shift_val;
  const uint8_t copy_adr_fill = (uint8_t)(0xFFu << (shift_val - 8));
  const uint8_t copy_len_mask = (uint8_t)((1u << copy_adr_shift) - 1u);

  size_t j = 0;
  while (1) {
    int bit0 = 0;
    if (!fab_get_bit(&br, &bit0)) {
      snprintf(err, err_size, "FAB truncated at literal/control");
      return false;
    }

    if (bit0 == 0) {
      int bit1 = 0;
      if (!fab_get_bit(&br, &bit1)) {
        snprintf(err, err_size, "FAB truncated at copy mode");
        return false;
      }

      int32_t copy_adr = 0;
      int copy_len = 0;

      if (bit1 == 0) {
        int b1 = 0;
        int b2 = 0;
        if (!fab_get_bit(&br, &b1) || !fab_get_bit(&br, &b2)) {
          snprintf(err, err_size, "FAB truncated at cmd00 bits");
          return false;
        }
        if (br.pos >= br.src_size) {
          snprintf(err, err_size, "FAB truncated at cmd00 byte");
          return false;
        }
        uint8_t raw = br.src[br.pos++];
        copy_len = ((b1 << 1) | b2) + 2;
        copy_adr = (int32_t)(raw | 0xFFFFFF00u);
      } else {
        if (br.pos + 2 > br.src_size) {
          snprintf(err, err_size, "FAB truncated at cmd01 bytes");
          return false;
        }
        uint8_t A = br.src[br.pos++];
        uint8_t B = br.src[br.pos++];
        copy_adr = (int32_t)(((((B >> copy_adr_shift) | copy_adr_fill) << 8) | A) | 0xFFFF0000u);
        copy_len = B & copy_len_mask;
        if (copy_len == 0) {
          if (br.pos >= br.src_size) {
            snprintf(err, err_size, "FAB truncated at extended copy_len");
            return false;
          }
          copy_len = br.src[br.pos++];
          if (copy_len == 0) {
            break; /* HALT */
          }
          if (copy_len == 1) {
            continue; /* NOP */
          }
          copy_len += 1;
        } else {
          copy_len += 2;
        }
      }

      while (copy_len > 0) {
        int64_t from = (int64_t)j + (int64_t)copy_adr;
        if (from < 0 || (size_t)from >= j || j >= expected_size) {
          snprintf(err, err_size, "FAB copy out of range (j=%zu adr=%d len=%d)", j, copy_adr, copy_len);
          return false;
        }
        dst[j] = dst[(size_t)from];
        j++;
        copy_len--;
      }
    } else {
      if (br.pos >= br.src_size || j >= expected_size) {
        snprintf(err, err_size, "FAB literal overflow (j=%zu)", j);
        return false;
      }
      dst[j++] = br.src[br.pos++];
    }
  }

  if (j != expected_size) {
    snprintf(err, err_size, "FAB size mismatch got=%zu expected=%zu", j, expected_size);
    return false;
  }
  return true;
}

static uint16_t read_u16_le(const uint8_t* p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t* p) {
  return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

bool madspack_load(const char* path, MadspackFile* out_file, char* err, size_t err_size) {
  if (!path || !out_file) {
    snprintf(err, err_size, "madspack_load bad args");
    return false;
  }
  memset(out_file, 0, sizeof(*out_file));

  FILE* f = fopen(path, "rb");
  if (!f) {
    snprintf(err, err_size, "cannot open %s", path);
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    snprintf(err, err_size, "seek failed for %s", path);
    return false;
  }
  long file_size = ftell(f);
  if (file_size < 16 + 0xA0) {
    fclose(f);
    snprintf(err, err_size, "file too small: %s", path);
    return false;
  }
  rewind(f);

  uint8_t* raw = malloc((size_t)file_size);
  if (!raw) {
    fclose(f);
    snprintf(err, err_size, "oom reading %s", path);
    return false;
  }
  if (fread(raw, 1, (size_t)file_size, f) != (size_t)file_size) {
    free(raw);
    fclose(f);
    snprintf(err, err_size, "short read: %s", path);
    return false;
  }
  fclose(f);

  if (memcmp(raw, "MADSPACK 2.0", 12) != 0) {
    free(raw);
    snprintf(err, err_size, "not MADSPACK 2.0: %s", path);
    return false;
  }

  uint16_t count = read_u16_le(raw + 14);
  if (count == 0 || count > 16) {
    free(raw);
    snprintf(err, err_size, "invalid section count %u in %s", count, path);
    return false;
  }

  MadspackSection* sections = calloc(count, sizeof(*sections));
  if (!sections) {
    free(raw);
    snprintf(err, err_size, "oom sections for %s", path);
    return false;
  }

  const uint8_t* header = raw + 16;
  size_t data_pos = 16 + 0xA0;
  char fab_err[256];

  for (uint16_t i = 0; i < count; ++i) {
    const uint8_t* h = header + (size_t)i * 10;
    sections[i].flags = read_u16_le(h);
    sections[i].uncompressed_size = read_u32_le(h + 2);
    sections[i].compressed_size = read_u32_le(h + 6);

    if (data_pos + sections[i].compressed_size > (size_t)file_size) {
      snprintf(err, err_size, "section %u exceeds file size in %s", i, path);
      goto fail;
    }

    const uint8_t* payload = raw + data_pos;
    data_pos += sections[i].compressed_size;

    uint8_t* out = malloc(sections[i].uncompressed_size);
    if (!out) {
      snprintf(err, err_size, "oom section %u in %s", i, path);
      goto fail;
    }

    if ((sections[i].flags & 1u) == 0) {
      if (sections[i].compressed_size != sections[i].uncompressed_size) {
        free(out);
        snprintf(err, err_size, "uncompressed size mismatch section %u in %s", i, path);
        goto fail;
      }
      memcpy(out, payload, sections[i].uncompressed_size);
    } else {
      if (!fab_decompress(payload, sections[i].compressed_size, out, sections[i].uncompressed_size, fab_err, sizeof(fab_err))) {
        free(out);
        snprintf(err, err_size, "FAB decompress failed section %u in %s: %s", i, path, fab_err);
        goto fail;
      }
    }

    sections[i].data = out;
    sections[i].data_size = sections[i].uncompressed_size;
  }

  free(raw);
  out_file->sections = sections;
  out_file->section_count = (int)count;
  diag_info("Loaded MADSPACK %s (%u sections)", path, count);
  return true;

fail:
  for (uint16_t i = 0; i < count; ++i) {
    free(sections[i].data);
  }
  free(sections);
  free(raw);
  return false;
}

void madspack_free(MadspackFile* file) {
  if (!file) {
    return;
  }
  if (file->sections) {
    for (int i = 0; i < file->section_count; ++i) {
      free(file->sections[i].data);
    }
    free(file->sections);
  }
  file->sections = NULL;
  file->section_count = 0;
}
