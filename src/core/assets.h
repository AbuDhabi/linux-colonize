#ifndef COLONIZE_ASSETS_H
#define COLONIZE_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/platform.h"

#define COLONIZE_MSG_MAX_LINES 64
#define COLONIZE_MSG_LINE_LEN 160
#define COLONIZE_MSG_SECTION_LEN 48

typedef struct ColonizeMsgSection {
  char name[COLONIZE_MSG_SECTION_LEN];
  char lines[COLONIZE_MSG_MAX_LINES][COLONIZE_MSG_LINE_LEN];
  int line_count;
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

void assets_msg_init(ColonizeMsgCatalog* catalog);
void assets_msg_free(ColonizeMsgCatalog* catalog);
bool assets_msg_load_file(ColonizeMsgCatalog* catalog, const char* path);
const ColonizeMsgSection* assets_msg_find(const ColonizeMsgCatalog* catalog, const char* section_name);

#endif
