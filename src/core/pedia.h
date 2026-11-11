#ifndef COLONIZE_PEDIA_H
#define COLONIZE_PEDIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/pik.h"
#include "core/popup_msg.h"
#include "core/ss.h"
#include "core/ui_colors.h"
#include "platform/platform.h"

/*
 * Colonizopedia (MENU.TXT @PEDIA / PEDIA.TXT).
 *
 * Category menu items open an encyclopedia *list* on WOODPANL.PIK (DOS
 * FUN_6cb2_24b8): one alphabetical column of NAMES short names with a
 * highlight bar. Enter / Space / a click opens the article; any key or click
 * from an article returns to the list (DOS FUN_281f_03c0); Esc, (Exit) or a
 * click off the rows returns to the map.
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
/* UNIT0–22; PEDIA.TXT's @UNIT23 is a stub duplicate of Colonists — dropped. */
#define PEDIA_UNIT_COUNT 23
#define PEDIA_TERRAIN_COUNT 29
#define PEDIA_JOB_COUNT 28
#define PEDIA_BUILDING_COUNT 42
#define PEDIA_FATHER_COUNT 25
#define PEDIA_MISC_COUNT 12 /* stock Game Concept rows */
/* Stock rows plus the ones data/mods PEDIA.TXT appends to @MISCELLANEOUS
 * (bodies @MODMISC<n>); see pedia_set_misc_catalog. */
#define PEDIA_MISC_MAX 16

#define PEDIA_PREVIEW_PHYS0_MAX 4
#define PEDIA_TITLE_LEN 48
#define PEDIA_BODY_MAX_LINES 20
#define PEDIA_BODY_LINE_LEN 160

/* Link greens from NAMES.TXT @COLORS (basic / hilite). */
#define PEDIA_COL_HEADER 15
#define PEDIA_COL_LINK COLONIZE_COL_BASIC
#define PEDIA_COL_LINK_HOVER COLONIZE_COL_HILITE

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
} PediaTerrainPreview;

typedef struct PediaPage {
  PediaCategory category;
  int index;
  char title[PEDIA_TITLE_LEN];
  char body[PEDIA_BODY_MAX_LINES][PEDIA_BODY_LINE_LEN];
  int body_line_count;
  PediaPreviewKind preview_kind;
  PediaTerrainPreview terrain;
  int icon_sprite;
  int building_sprite;
  int father_index;
} PediaPage;

/*
 * Encyclopedia list (DOS FUN_6cb2_2322 fill, 0178 sort, 214a paint, 02c4 hit,
 * 24b8 loop). One entry per listed row in stricmp title order; id = the
 * category row the article opens with.
 */
#define PEDIA_LIST_MAX 48

typedef struct PediaList {
  int count;
  int id[PEDIA_LIST_MAX];
  char title[PEDIA_LIST_MAX][PEDIA_TITLE_LEN]; /* sort key (NAMES short name) */
  bool forest[PEDIA_LIST_MAX];                 /* terrain 8..15: + " " + @OTHER_NAMES 0 */
} PediaList;

/* FUN_6cb2_02c4 results besides a slot index. */
#define PEDIA_LIST_HIT_NONE (-1)
#define PEDIA_LIST_HIT_PAGE (-2) /* top strip, x < 160 */
#define PEDIA_LIST_HIT_EXIT (-3) /* top strip, x >= 160 */

typedef enum PediaListAction {
  PEDIA_LIST_STAY = 0,
  PEDIA_LIST_OPEN, /* open the article of list->id[*sel] */
  PEDIA_LIST_EXIT
} PediaListAction;

/* Bind LABELS.TXT / MENU.TXT for the list screen's own chrome (header,
 * "(Exit)" row, category names). Call once after the catalogs load. */
void pedia_set_chrome_catalogs(
  const struct ColonizeMsgCatalog* labels, const struct ColonizeMsgCatalog* menu
);

/* Count the Game Concept rows from the loaded PEDIA.TXT: the 12 stock rows,
 * plus any data/mods appends after them (up to PEDIA_MISC_MAX), listed only
 * while a mod is on (mods_campaign_mask). NULL = stock. */
void pedia_set_misc_catalog(const struct ColonizeMsgCatalog* pedia);

int pedia_category_count(PediaCategory category);
const char* pedia_category_label(PediaCategory category);

/*
 * Leading caret run on a PEDIA.TXT body line, as DOS FUN_6f74_0c32 parses it:
 * "^^" sets flag 1, a single "^" ORs flag 2, and the FUN_6f74_1198 wrap loop
 * treats either (flags & 3) as "draw this line on its own, unwrapped" — but
 * only flag 1 takes the extra centring measure, so "^" is LEFT-aligned.
 * Returns that flag byte (0 = ordinary flowing prose) and, when out_rest is
 * given, the text after the caret run.
 *
 * PEDIA.TXT and GAME.TXT go through the same DOS parser, so this is a thin
 * alias of popup_msg_caret_flags() — keep the two in lockstep.
 */
#define PEDIA_CARET_CENTER POPUP_MSG_CARET_CENTER     /* "^^" — own line, centred */
#define PEDIA_CARET_OWN_LINE POPUP_MSG_CARET_OWN_LINE /* "^" — own line, left-aligned */
int pedia_caret_flags(const char* line, const char** out_rest);

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

void pedia_list_build(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  PediaList* out
);

/* Row text as painted (FUN_6cb2_033a). */
void pedia_list_display(
  const PediaList* list, const ColonizeMsgCatalog* names, int slot, char* out, size_t out_size
);

/* shown = the slot painted on the highlight bar, or PEDIA_LIST_HIT_EXIT to
 * hilite "(Exit)" (DOS local_c). */
void pedia_list_render(
  const PediaList* list,
  const ColonizeMsgCatalog* names,
  const ColonizePikImage* wood_bg,
  const ColonizeFont* font,
  int shown,
  ColonizeFramebuffer8* framebuffer
);

int pedia_list_hit(const PediaList* list, int mouse_x, int mouse_y);

/*
 * One pass of the FUN_6cb2_24b8 loop. sel = selected slot (local_6), shown =
 * painted slot (local_c). held = DS:0x7f6 (button down or released this
 * frame), released = DS:0x7f4.
 */
PediaListAction pedia_list_step(
  const PediaList* list,
  ColonizeKey key,
  bool held,
  bool released,
  int mouse_x,
  int mouse_y,
  int* sel,
  int* shown
);

/*
 * DOS-fidelity article page (FUN_6cb2_05ce/07e6/0eac/1820/1ba8/1f28/203c):
 * WOODPANL background, centered "ENCYCLOPEDIA OF COLONIZATION" header,
 * centered "(Name: Category)" title, per-category preview + stats, then the
 * PEDIA.TXT section body rendered by the popup text rules (^ line breaks,
 * {} hilite, @width wrap, centered lines). Any key or click dismisses
 * (handled by the caller).
 */
typedef struct PediaArticleAssets {
  const ColonizeMsgCatalog* pedia;      /* PEDIA.TXT */
  const ColonizeMsgCatalog* names;      /* NAMES.TXT */
  const ColonizeMsgCatalog* labels;     /* LABELS.TXT (@MISC strings) */
  const ColonizePikImage* wood_bg;      /* WOODPANL.PIK */
  const ColonizeFont* font;             /* FONTTINY */
  const ColonizeFont* chrome_font;      /* orders-letter font for unit chrome */
  const ColonizeSpriteSheet* icons;     /* ICONS.SS */
  const ColonizeSpriteSheet* buildings; /* BUILDING.SS */
  const ColonizeSpriteSheet* terrain;   /* TERRAIN.SS */
  const ColonizeSpriteSheet* phys0;     /* PHYS0.SS */
  const ColonizePalette* palette;       /* active output palette (wood) */
  int human_nation;
  int town_hall_art; /* MOD_TOWN_HALL_TIERS: first appended BDARK sprite in buildings, 0 = none */
} PediaArticleAssets;

void pedia_article_render(
  const PediaArticleAssets* assets,
  PediaCategory category,
  int index,
  ColonizeFramebuffer8* framebuffer
);

#endif
