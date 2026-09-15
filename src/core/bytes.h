#ifndef COLONIZE_BYTES_H
#define COLONIZE_BYTES_H

#include <stdint.h>

/*
 * Little-endian scalar reads from a raw byte buffer. Every DOS asset format in
 * this port stores its words this way; six private copies of these two lines
 * lived in ss.c, pik.c, ff.c, madspack.c, sound.c and gsound_vm.c under four
 * different names (read_u16 / read_u16_le / rd_u16 / rd16) until 2026-09-14.
 * Bounds are the caller's business, exactly as they were before.
 */
static inline uint16_t rd_u16_le(const uint8_t* p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t rd_u32_le(const uint8_t* p) {
  return (uint32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
                    ((uint32_t)p[3] << 24));
}

#endif
