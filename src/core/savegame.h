#ifndef COLONIZE_SAVEGAME_H
#define COLONIZE_SAVEGAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/col1_save.h"

/*
 * Save I/O:
 *   - Original DOS COLONY##.SAV  → ColonizeCol1Save (col1_save_*)
 *   - Legacy native COLZ POC     → ColonizeSavePayload (bring-up only;
 *     not original-compatible; game_loop still uses this until full
 *     state mapping exists)
 */

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

/* Original DOS slot path: <dir>/COLONY00.SAV .. COLONY09.SAV (slot 0..9). */
bool savegame_colony_slot_path(
  const char* save_dir,
  int slot /*0..9*/,
  char* out_path,
  size_t out_path_size
);

bool savegame_write_col1(
  const char* save_dir,
  int slot,
  const ColonizeCol1Save* save,
  char* err_buf,
  size_t err_buf_size
);
bool savegame_read_col1(
  const char* save_dir,
  int slot,
  ColonizeCol1Save* out_save,
  char* err_buf,
  size_t err_buf_size
);

/* Legacy native POC (COLZ). */
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
