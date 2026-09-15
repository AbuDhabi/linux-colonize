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
bool assets_detect_madspack(const char* path, char* info, size_t info_size);

/* Nearest-colour remap of a sprite sheet onto a screen palette. Only for
 * sheets that share the screen's palette block and reserve no DAC block of
 * their own; reserved-block popup art must be merged instead (see the
 * implementation comment in assets.c). Bakes into the sheet's pixels, so one
 * remapped instance per destination palette. */
void assets_sheet_remap_to_palette(ColonizeSpriteSheet* sheet, const ColonizePalette* dst_pal);

/*
 * Nearest palette index to an 8-bit RGB triple (squared distance over all 256
 * entries, first best wins). Six private copies existed — popup.c,
 * game_loop.c, new_game.c, unit_chrome.c, colony_screen.c and the inline loop
 * in assets_sheet_remap_to_palette. Three carried a `d == 0` early break,
 * three did not; the break cannot change the answer, since a squared distance
 * is never negative, so no later entry can beat an exact hit and the
 * strictly-less-than test already keeps the first of any tie.
 */
uint8_t assets_palette_nearest_rgb(const ColonizePalette* pal, int r, int g, int b);

void assets_msg_init(ColonizeMsgCatalog* catalog);
void assets_msg_free(ColonizeMsgCatalog* catalog);
bool assets_msg_load_file(ColonizeMsgCatalog* catalog, const char* path);
const ColonizeMsgSection* assets_msg_find(const ColonizeMsgCatalog* catalog, const char* section_name);

/*
 * NAMES.TXT / LABELS.TXT row-and-field access.
 *
 * ROW INDEXING (resolves the pedia-vs-reports divergence, audit SC-11/SC-14):
 * assets_msg_load_file already drops blank lines and ';' comment lines while
 * building section->lines[], so section->lines[i] is ALREADY a
 * blank-and-comment-skipped ordinal — exactly what DOS's own parser walks. The
 * extra "skip blank / ';' lines" loops inside reports_names_field and
 * ai_contact_job_expert_name therefore test conditions that no stored line can
 * satisfy and are unreachable; pedia_names_field / trade_label / europe_label /
 * turn_label, which index section->lines[] directly, are the correct form.
 * Verified against the shipped data too: COLONIZE/LABELS.TXT @MISC is 223 raw
 * lines with blanks only at rows 222-223 (the tail), so every meaningful row
 * has the same index either way. Callers migrating off the private copies do
 * NOT need to re-add any skipping.
 *
 * Fields are comma-separated; leading and trailing spaces/tabs are trimmed.
 */

/* n-th (0-based) comma field of `line`, trimmed, into out[out_sz].
 * Returns false (and writes an empty string) when the field does not exist. */
bool assets_msg_csv_field(const char* line, int n, char* out, size_t out_sz);

/* `field`-th comma field of row `row` of `section`. Same result as
 * assets_msg_csv_field(assets_msg_line_or(cat, section, row, ""), field, ...). */
bool assets_msg_row_field(
  const ColonizeMsgCatalog* catalog,
  const char* section,
  int row,
  int field,
  char* out,
  size_t out_sz
);

/* Line `idx` of `section`, or `fallback` when the catalog, section, index or
 * line is missing/empty. The returned pointer is owned by the catalog. */
const char* assets_msg_line_or(
  const ColonizeMsgCatalog* catalog,
  const char* section,
  int idx,
  const char* fallback
);

#endif
