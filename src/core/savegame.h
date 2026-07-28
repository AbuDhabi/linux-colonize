#ifndef COLONIZE_SAVEGAME_H
#define COLONIZE_SAVEGAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ColonizeSaveHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t payload_size;
} ColonizeSaveHeader;

typedef struct ColonizeSavePayload {
  uint32_t turn_number;
  uint32_t random_seed;
  uint8_t map_seed;
} ColonizeSavePayload;

const char* savegame_default_dir(void);
bool savegame_write(
  const char* save_dir,
  const char* slot_name,
  const ColonizeSavePayload* payload,
  char* err_buf,
  size_t err_buf_size
);
bool savegame_read(
  const char* save_dir,
  const char* slot_name,
  ColonizeSavePayload* out_payload,
  char* err_buf,
  size_t err_buf_size
);

#endif
