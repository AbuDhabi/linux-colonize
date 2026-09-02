#ifndef COLONIZE_SAVEGAME_H
#define COLONIZE_SAVEGAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/col1_save.h"

/*
 * Save I/O:
 *   - Original DOS COLONY##.SAV  → ColonizeCol1Save (col1_save_*)
 */

/* Default: <exe>/COLONIZE (created if missing). Override with --save-dir. */
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

/* Prefix-only probe for slot list UI (no map / colony load). */
typedef struct ColonizeSaveSlotInfo {
  bool occupied;
  char leader_name[24];
  uint16_t year;
  uint16_t autumn;
  uint16_t turn;
  uint8_t difficulty; /* head.difficulty 0..4 */
  uint8_t human_nation; /* first player with control == 0 */
} ColonizeSaveSlotInfo;

bool savegame_probe_col1_slot(
  const char* save_dir,
  int slot /*0..9*/,
  ColonizeSaveSlotInfo* out
);

#endif
