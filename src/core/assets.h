#ifndef COLONIZE_ASSETS_H
#define COLONIZE_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/platform.h"
#include "core/ss.h"

#define COLONIZE_MSG_LINE_LEN 160
#define COLONIZE_MSG_SECTION_LEN 48

/*
 * Lines grow on demand: LABELS.TXT @MISC alone is 223 lines, and the old
 * fixed 64-line cap silently dropped everything past it (every @MISC index
 * ≥ 64 fell back to hardcoded text until 2026-08-28).
 */
typedef struct ColonizeMsgSection {
  char name[COLONIZE_MSG_SECTION_LEN];
  char (*lines)[COLONIZE_MSG_LINE_LEN];
  /* Parallel to lines: 1 if one or more blank lines preceded this line in
   * the file. DOS's dialog parser (FUN_6f74_32a4) advances its parse state
   * on every empty line — state 1 = body, 2 = choices — so blank lines are
   * the real body/choice separator, not keyword heuristics. Blank lines
   * themselves are still not stored (ordinal indexing everywhere relies on
   * that). */
  uint8_t* blank_before;
  int line_count;
  int line_capacity;
} ColonizeMsgSection;

typedef struct ColonizeMsgCatalog {
  ColonizeMsgSection* sections;
  int section_count;
  int section_capacity;
} ColonizeMsgCatalog;

/* Resolve data directory: override, then <exe>/COLONIZE, then ./COLONIZE. */
bool assets_resolve_data_dir(const char* override_dir, char* out, size_t out_size);

bool assets_validate_required_files(const char* data_dir, char* err_buf, size_t err_buf_size);
void assets_log_inventory(const char* data_dir);

bool assets_load_palette(const char* data_dir, ColonizePalette* out_palette);
void assets_palette_from_col768(const uint8_t* raw, size_t raw_size, ColonizePalette* out_palette);
void assets_palette_from_viceroy1024(const uint8_t* raw, size_t raw_size, ColonizePalette* out_palette);
bool assets_detect_madspack(const char* path, char* info, size_t info_size);

/* Nearest-colour remap of a sprite sheet onto a screen palette. Only for
 * sheets that share the screen's palette block and reserve no DAC block of
 * their own; reserved-block popup art must be merged instead (see the
 * implementation comment in assets.c). Bakes into the sheet's pixels, so one
 * remapped instance per destination palette. */
void assets_sheet_remap_to_palette(ColonizeSpriteSheet* sheet, const ColonizePalette* dst_pal);

void assets_msg_init(ColonizeMsgCatalog* catalog);
void assets_msg_free(ColonizeMsgCatalog* catalog);
bool assets_msg_load_file(ColonizeMsgCatalog* catalog, const char* path);
const ColonizeMsgSection* assets_msg_find(const ColonizeMsgCatalog* catalog, const char* section_name);

#endif
