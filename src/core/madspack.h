#ifndef COLONIZE_MADSPACK_H
#define COLONIZE_MADSPACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MadspackSection {
  uint16_t flags;
  uint32_t uncompressed_size;
  uint32_t compressed_size;
  uint8_t* data;
  size_t data_size;
} MadspackSection;

typedef struct MadspackFile {
  MadspackSection* sections;
  int section_count;
} MadspackFile;

bool madspack_load(const char* path, MadspackFile* out_file, char* err, size_t err_size);
void madspack_free(MadspackFile* file);

/* Decode FAB-compressed payload into caller buffer of expected_size bytes. */
bool fab_decompress(
  const uint8_t* src,
  size_t src_size,
  uint8_t* dst,
  size_t expected_size,
  char* err,
  size_t err_size
);

#endif
