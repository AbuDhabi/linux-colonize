#ifndef COLONIZE_PEDIA_H
#define COLONIZE_PEDIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/pik.h"
#include "platform/platform.h"

/*
 * Colonizopedia (MENU.TXT @PEDIA / PEDIA.TXT).
 *
 * Category menu items open an encyclopedia *list* on WOODPANL.PIK
 * ("ENCYCLOPEDIA OF COLONIZATION" + green entry titles in up to 3 columns).
 * Clicking an entry opens the article page. Esc from article returns to the
 * list; Esc / (Exit) from the list returns to the map.
 */
typedef enum PediaCategory {
  PEDIA_CAT_CARGO = 0,
  PEDIA_CAT_UNIT,
  PEDIA_CAT_TERRAIN,
  PEDIA_CAT_JOB,
  PEDIA_CAT_BUILDING,
  PEDIA_CAT_FATHER,
  PEDIA_CAT_MISC,
  PEDIA_CAT_COUNT
} PediaCategory;

typedef enum PediaViewMode {
  PEDIA_VIEW_LIST = 0,
  PEDIA_VIEW_ARTICLE
} PediaViewMode;

#define PEDIA_CARGO_COUNT 16
#define PEDIA_UNIT_COUNT 24 /* UNIT0–23; 23 is a stub duplicate of Colonists */
#define PEDIA_TERRAIN_COUNT 29
#define PEDIA_JOB_COUNT 28
#define PEDIA_BUILDING_COUNT 42
#define PEDIA_FATHER_COUNT 25
#define PEDIA_MISC_COUNT 12

#define PEDIA_PREVIEW_PHYS0_MAX 4
#define PEDIA_TITLE_LEN 48
#define PEDIA_BODY_MAX_LINES 20
#define PEDIA_BODY_LINE_LEN 160
#define PEDIA_CATEGORY_LABEL_LEN 32

#define PEDIA_LIST_COLS 3
#define PEDIA_LIST_HEADER "ENCYCLOPEDIA OF COLONIZATION"
#define PEDIA_LIST_EXIT "(Exit)"
/* VGA greens that read well on WOODPANL; white for the header. */
#define PEDIA_COL_HEADER 15
#define PEDIA_COL_LINK 2
#define PEDIA_COL_LINK_HOVER 10

typedef enum PediaPreviewKind {
  PEDIA_PREVIEW_NONE = 0,
  PEDIA_PREVIEW_TERRAIN,
  PEDIA_PREVIEW_ICON,     /* ICONS.SS */
  PEDIA_PREVIEW_BUILDING, /* BUILDING.SS */
  PEDIA_PREVIEW_FATHER    /* CC-NN.SS (loaded by caller) */
} PediaPreviewKind;

typedef struct PediaTerrainPreview {
  int terrain_sprite; /* TERRAIN.SS index, or -1 */
  int phys0_sprites[PEDIA_PREVIEW_PHYS0_MAX];
  int phys0_count;
  uint8_t class_flag; /* from viceroy_terrain_meta[0] */
} PediaTerrainPreview;

typedef struct PediaPage {
  PediaCategory category;
  int index;
  int flat_index;
  int flat_count;
  char category_label[PEDIA_CATEGORY_LABEL_LEN];
  char title[PEDIA_TITLE_LEN];
  char body[PEDIA_BODY_MAX_LINES][PEDIA_BODY_LINE_LEN];
  int body_line_count;
  PediaPreviewKind preview_kind;
  PediaTerrainPreview terrain;
  int icon_sprite;
  int building_sprite;
  int father_index;
} PediaPage;

typedef enum PediaListHitKind {
  PEDIA_LIST_HIT_NONE = 0,
  PEDIA_LIST_HIT_EXIT,
  PEDIA_LIST_HIT_ENTRY
} PediaListHitKind;

typedef struct PediaListHit {
  PediaListHitKind kind;
  int entry_index;
} PediaListHit;

int pedia_category_count(PediaCategory category);
const char* pedia_category_label(PediaCategory category);
const char* pedia_category_section_prefix(PediaCategory category);

void pedia_terrain_preview(int terrain_index, PediaTerrainPreview* out);

/* Short title for encyclopedia list rows (no body). */
bool pedia_entry_title(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  int index,
  char* out,
  size_t out_size
);

bool pedia_page(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  int index,
  PediaPage* out
);

bool pedia_terrain_page(const ColonizeMsgCatalog* catalog, int terrain_index, PediaPage* out);

void pedia_list_render(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  const ColonizePikImage* wood_bg,
  const ColonizeFont* font,
  int hover_entry, /* -1 none */
  ColonizeFramebuffer8* framebuffer
);

PediaListHit pedia_list_hit(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  const ColonizeFont* font,
  int mouse_x,
  int mouse_y
);

#endif
