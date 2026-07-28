#ifndef COLONIZE_PEDIA_H
#define COLONIZE_PEDIA_H

#include <stdbool.h>
#include <stddef.h>

#include "core/assets.h"

#define PEDIA_TERRAIN_COUNT 29
#define PEDIA_PREVIEW_PHYS0_MAX 4
#define PEDIA_TITLE_LEN 48
#define PEDIA_BODY_MAX_LINES 16
#define PEDIA_BODY_LINE_LEN 160

typedef struct PediaTerrainPreview {
  int terrain_sprite; /* TERRAIN.SS index, or -1 */
  int phys0_sprites[PEDIA_PREVIEW_PHYS0_MAX];
  int phys0_count;
  uint8_t class_flag; /* from viceroy_terrain_meta[0] */
} PediaTerrainPreview;

typedef struct PediaTerrainPage {
  int index;
  char title[PEDIA_TITLE_LEN];
  char body[PEDIA_BODY_MAX_LINES][PEDIA_BODY_LINE_LEN];
  int body_line_count;
  PediaTerrainPreview preview;
} PediaTerrainPage;

/* Resolve TERRAIN/PHYS0 sprites for Colonizopedia terrain index 0-28. */
void pedia_terrain_preview(int terrain_index, PediaTerrainPreview* out);

/*
 * Fill a terrain page from a loaded PEDIA.TXT catalog.
 * Uses section TERRAIN<n>. Missing text still yields a valid preview.
 */
bool pedia_terrain_page(const ColonizeMsgCatalog* catalog, int terrain_index, PediaTerrainPage* out);

#endif
