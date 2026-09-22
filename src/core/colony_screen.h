#ifndef COLONIZE_COLONY_SCREEN_H
#define COLONIZE_COLONY_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_preview.h"
#include "core/colony_yield.h"
#include "core/font.h"
#include "core/map.h"
#include "core/pik.h"
#include "core/popup.h"
#include "core/ss.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/platform.h"
#include "core/world.h"

/*
 * DOS colony screen layout (320×200):
 *   WOODPANL.PIK  — full-screen wood chrome (authoritative palette)
 *   PARCH.SS      — beige scrollwork tiled across the upper-left buildings section
 *   BUILDING.SS   — building sprites (indices match NAMES.TXT @BUILDING);
 *                   #16 fence (bottom-right); #45 empty coast above fence;
 *                   sprites 42–47 are tree clumps
 *   WOODTILE.SS   — wood grain tiled in the square top-right minimap section
 *   TERRAIN+PHYS0 — 3×3 catchment tiles centered in the minimap section
 *   COLONY.PIK    — bottom panel (people / transport / multifunction / cargo)
 *   ICONS.SS      — cargo #22–37, settlement #0–3, crosses #56, fish #57, bells #62, …
 */
#define COLONY_SCREEN_WIDTH 320
#define COLONY_SCREEN_HEIGHT 200
#define COLONY_BOTTOM_PANEL_HEIGHT 72
#define COLONY_BOTTOM_PANEL_Y (COLONY_SCREEN_HEIGHT - COLONY_BOTTOM_PANEL_HEIGHT)

#define COLONY_TOP_BAR_H 7 /* golden-measured (new_amsterdam_production.png): separator row at
   native y=7, content starts y=8 — was 11 (4px too tall), pushing the
   settlement/minimap panels down; both are positioned off COLONY_MIDDLE_Y
   below, so this one constant fixes both. */
#define COLONY_TOP_SEPARATOR_Y COLONY_TOP_BAR_H
#define COLONY_MIDDLE_Y (COLONY_TOP_SEPARATOR_Y + 1)
#define COLONY_BOTTOM_SEPARATOR_Y (COLONY_BOTTOM_PANEL_Y - 1)

#define COLONY_MINIMAP_SECTION_H (COLONY_BOTTOM_SEPARATOR_Y - COLONY_MIDDLE_Y)
/* Golden (new_amsterdam_production.png): parchment ends x=198, black divider
   column at x=199, wood grain from x=200 — section is 120 wide, not square. */
#define COLONY_MINIMAP_SECTION_X 200
#define COLONY_MINIMAP_SECTION_W (COLONY_SCREEN_WIDTH - COLONY_MINIMAP_SECTION_X)
#define COLONY_MINIMAP_SECTION_Y COLONY_MIDDLE_Y
#define COLONY_MINIMAP_GRID 3
#define COLONY_MINIMAP_TILE 24 /* 16px terrain × 1.5, centered in WOODTILE */

#define COLONY_VIEWPORT_X 1
#define COLONY_VIEWPORT_Y COLONY_MIDDLE_Y
#define COLONY_VIEWPORT_W 202
#define COLONY_VIEWPORT_H 114

/* Parchment fill extent — separate from COLONY_VIEWPORT_W/H (building/hit-test
   space) so the beige tiling reaches exactly to the minimap section's left
   edge and the bottom separator, with no leftover chrome strip showing
   through (player-reported: 1px right, 5px bottom uncovered). */
#define COLONY_PARCH_FILL_W (COLONY_MINIMAP_SECTION_X - COLONY_VIEWPORT_X)
#define COLONY_PARCH_FILL_H (COLONY_BOTTOM_SEPARATOR_Y - COLONY_VIEWPORT_Y)

#define COLONY_CARGO_SLOT_X0 1
#define COLONY_CARGO_SLOT_W 18
#define COLONY_CARGO_PITCH 19
#define COLONY_CARGO_ICON_BASE 22
#define COLONY_CARGO_GREY_BASE 38
#define COLONY_CARGO_STRIP_Y (COLONY_BOTTOM_PANEL_Y + 52)
#define COLONY_CARGO_NUM_Y (COLONY_BOTTOM_PANEL_Y + 64)

#define COLONY_ICON_CROSS 56
#define COLONY_ICON_FISH 57 /* fisherman food (colony view only; still cargo food) */
#define COLONY_ICON_BELL 62
#define COLONY_ICON_HAMMER 54
#define COLONY_ICON_FLAG 123
#define COLONY_ICON_CROWN 124
#define COLONY_ICON_HOUSE 67
#define COLONY_ICON_RIFLE 68
#define COLONY_ICON_HAMMER_BTN 69
#define COLONY_ICON_EMPTY_HOLD 122
#define COLONY_ICON_TOTEM 108 /* red totem pole (8x16) — Indian-claimed tile */

#define COLONY_EXIT_X 306
#define COLONY_EXIT_Y 179
#define COLONY_BUILDABLE_MAX 32
#define COLONY_BUILDING_SLOT_W 48
#define COLONY_BUILDING_SLOT_H 32
#define COLONY_JOB_LIST_MAX (COLONIZE_FIELD_JOB_COUNT + 1)
/* Sentinel row id in job_ids[]: FUN_2f2b_348c's "Clear Specialty" row
 * (menu id 0x61) — offered when the selected colonist is a specialist. */
#define COLONY_JOB_CLEAR_SPECIALTY (-2)

/* COLONY.PIK bands above the warehouse strip (transport shifted +30 for People/Tory). */
#define COLONY_PEOPLE_X 0
#define COLONY_PEOPLE_W 118
#define COLONY_TRANSPORT_X 121
#define COLONY_TRANSPORT_W 82
#define COLONY_MULTI_X 207
#define COLONY_MULTI_W 111
#define COLONY_MULTI_BTN_X 307
#define COLONY_MULTI_BTN_W 12
#define COLONY_PANEL_CONTENT_Y 131
#define COLONY_PANEL_CONTENT_H (COLONY_CARGO_STRIP_Y - COLONY_PANEL_CONTENT_Y - 4)

#define COLONY_TRANSPORT_MAX 8
/* bugs.md #11: transport icons sat 10px too high; nudged down. */
#define COLONY_TRANSPORT_ICON_Y (COLONY_BOTTOM_PANEL_Y + 18)
#define COLONY_TRANSPORT_PITCH 18
/*
 * bugs.md: hold-box geometry measured off COLONY.PIK itself (the panel art
 * carries six 9x12 box interiors at panel-local x = 13 + i*12, y = 37..48,
 * i.e. screen x = 128 + i*12, y = 165). The old 14px pitch drifted up to
 * 10px right of the painted boxes by the sixth hold.
 */
#define COLONY_HOLD_X 128
#define COLONY_HOLD_Y 165
#define COLONY_HOLD_W 9
#define COLONY_HOLD_H 12
#define COLONY_HOLD_PITCH 12

/* bugs.md: 12 dropped fence units outright in crowded colonies. */
#define COLONY_OUTSIDE_MAX 24

typedef enum ColonyMultiMode {
  COLONY_MULTI_PRODUCTION = 0,
  COLONY_MULTI_UNITS = 1,
  COLONY_MULTI_CONSTRUCTION = 2
} ColonyMultiMode;

/*
 * Docked-unit orders popup (DOS FUN_2f2b_5746; GAME.TXT @COLONYUNIT title +
 * @UNITOPTIONS land / @SHIPOPTIONS sea options). Second click on an already-
 * selected docked transport opens this (matches the port's existing
 * select-then-click assignment convention).
 */
#define COLONY_DOCK_ORDERS_MAX 6
#define COLONY_DOCK_ORDER_LABEL_LEN COLONIZE_MSG_LINE_LEN

typedef enum ColonyDockOrderAction {
  COLONY_DOCK_ORDER_ACTIVATE = 0, /* "Move to front" — select as the colony's active transport */
  COLONY_DOCK_ORDER_CLEAR,        /* "Clear orders" */
  COLONY_DOCK_ORDER_SENTRY,
  COLONY_DOCK_ORDER_FORTIFY,      /* land Fortify / sea "Anchor in harbor" */
  COLONY_DOCK_ORDER_UNLOAD_ALL,   /* sea only: unload all goods holds to warehouse */
  COLONY_DOCK_ORDER_CANCEL        /* "No changes" */
} ColonyDockOrderAction;

typedef enum ColonyScreenHit {
  COLONY_HIT_NONE = 0,
  COLONY_HIT_COLONIST,
  COLONY_HIT_BUILDING,
  COLONY_HIT_CONSTRUCTION_ROW,
  COLONY_HIT_CONSTRUCTION_CLEAR,
  COLONY_HIT_CONSTRUCTION_BUY,
  COLONY_HIT_CONSTRUCTION_MORE, /* bugs.md #436: DOS More... page row */
  COLONY_HIT_EXIT,
  COLONY_HIT_CONSTRUCTION_OUTSIDE,
  COLONY_HIT_AREA_TILE,
  COLONY_HIT_AREA_INTERIOR, /* area-view centre / non-assignable spot: numbers toggle */
  COLONY_HIT_JOBS_ROW,
  COLONY_HIT_JOBS_CLEAR,
  COLONY_HIT_JOBS_OUTSIDE,
  COLONY_HIT_CARGO_SLOT,
  COLONY_HIT_TRANSPORT,
  COLONY_HIT_HOLD,
  COLONY_HIT_MULTI_BTN,
  COLONY_HIT_MULTI_PANE,
  COLONY_HIT_MULTI_UNIT_ICON,
  COLONY_HIT_MULTI_BUY,
  COLONY_HIT_MULTI_CHANGE,
  COLONY_HIT_OUTSIDE_UNIT,
  COLONY_HIT_FENCE, /* fortification strip (not a specific unit icon) */
  COLONY_HIT_PEOPLE_COLONIST,
  COLONY_HIT_EJECT_ROW,
  COLONY_HIT_EJECT_OUTSIDE,
  COLONY_HIT_DOCK_ORDERS_ROW,
  COLONY_HIT_DOCK_ORDERS_OUTSIDE,
  COLONY_HIT_MESSAGE_YES,
  COLONY_HIT_MESSAGE_NO,
  COLONY_HIT_MESSAGE_OK,
  COLONY_HIT_MESSAGE_OUTSIDE,
  COLONY_HIT_CUSTOM_HOUSE_ROW,
  COLONY_HIT_CUSTOM_HOUSE_OUTSIDE
} ColonyScreenHit;

typedef enum ColonyMessageKind {
  COLONY_MSG_NONE = 0,
  COLONY_MSG_OK,      /* single OK dismiss */
  COLONY_MSG_CONFIRM  /* Yes / No */
} ColonyMessageKind;

/*
 * One colony-screen sub-dialog's on-screen geometry (audit CO-5/CO-6/CO-7).
 * The six pickers — Construction, Field job, Leave as, dock orders, Custom
 * House checklist and the message box — each carried this same six-field
 * group, and their hit-tests each re-derived the row index from it by hand.
 * `list_y0` is the top of the first row; `line_h` the row pitch.
 */
typedef struct ColonyDialogRect {
  int x;
  int y;
  int w;
  int h;
  int list_y0;
  int line_h;
} ColonyDialogRect;

typedef struct ColonyScreenHitResult {
  ColonyScreenHit kind;
  int index;
} ColonyScreenHitResult;

/*
 * Multifunction "Units" tab roster: land units at the colony (colonist-class
 * + Artillery) — DOS FUN_2f2b_1e46 does not list ships/wagons, those stay on
 * the Transport strip. Not an armed-only subset either (unarmed colonists,
 * scouts, missionaries, pioneers all belong). Click handler FUN_2f2b_59a0
 * double-click opens the same docked-unit orders popup, FUN_2f2b_5746, as
 * the Transport strip. Wraps rows within the pane instead of DOS's paged
 * 3-column grid (thin — no paging/scroll).
 */
#define COLONY_MULTI_UNITS_SLOT_MAX COLONY_OUTSIDE_MAX
/* "Units Present" title (LABELS.TXT @CMISC) reserves this much height above
 * the roster grid — golden-confirmed, matched at both the draw and
 * hit-test call sites so click regions never drift from what's drawn. */
#define COLONY_MULTI_UNITS_TITLE_H 8
/* Units on the lower (first) row of the Units pane before it wraps upward. */
#define COLONY_MULTI_UNITS_ROW0 5

/*
 * DOS FUN_2f2b_1e46 / 59a0 geometry (2f2b:1ecc..1ffd, click 2f2b:59a0):
 * bottom row of 5 full icons at x=0xd5 step 0x12, y=0x9e; up to two
 * overflow rows above it (y=0x98, then 0x90) of 17 miniature 3x5 unit
 * sprites at step 5.
 */
#define COLONY_MULTI_UNITS_X 0xd5
#define COLONY_MULTI_UNITS_ROW0_Y 0x9e
#define COLONY_MULTI_UNITS_ROW0_STEP 0x12
#define COLONY_MULTI_UNITS_ROW1_Y 0x98
#define COLONY_MULTI_UNITS_ROW2_Y 0x90
#define COLONY_MULTI_UNITS_OVERFLOW_STEP 5
#define COLONY_MULTI_UNITS_OVERFLOW_PER_ROW 0x11
#define COLONY_MULTI_UNITS_MINI_W 3
#define COLONY_MULTI_UNITS_MINI_H 5

typedef struct ColonyMultiUnitSlot {
  int unit_id;
  int x;
  int y;
  int w;
  int h;
  bool mini; /* overflow-row 3x5 miniature (rows past the first 5 units) */
} ColonyMultiUnitSlot;

typedef struct ColonyScreenView {
  ColonizePikImage frame;
  ColonizeSpriteSheet parch;
  ColonizeSpriteSheet wood_tile;
  ColonizeSpriteSheet buildings;
  ColonizeSpriteSheet icons;
  ColonizePikImage bottom_panel;
  /* FONTINTR.FF — DOS's default dialog font slot (game_loop.c's
   * begin_menu_font: "@smallfont → FONTTINY, default dialog slot is
   * FONTINTR"). The colony screen body itself is drawn in FONTTINY, which
   * arrives as colony_screen_render()'s `font`, so a popup section carrying
   * @smallfont keeps that one and every other popup takes this. Optional:
   * dialog_font_ok false falls the popups back to the screen font. */
  ColonizeFont dialog_font;
  bool dialog_font_ok;
  bool frame_ok;
  bool parch_ok;
  bool wood_tile_ok;
  bool buildings_ok;
  bool icons_ok;
  bool bottom_panel_ok;
  char status[96];

  int selected_colonist;
  int selected_outside_unit; /* map unit id, or -1 */
  bool show_production_numbers;
  ColonyMultiMode multi_mode;
  int selected_cargo; /* warehouse cargo highlighted for =/+ load; -1 none */

  bool construction_open;
  int construction_selection;
  int buildable_ids[COLONY_BUILDABLE_MAX];
  int buildable_count;
  ColonyDialogRect construction_rect;
  int construction_rows_per_col; /* DOS 2f2b_5bd2: 16 rows per PAGE when > 22 rows */
  int construction_col_w;
  int construction_page; /* bugs.md #436: DOS pages (More...), never columns */

  bool jobs_open;
  int jobs_tile_index;
  int jobs_selection;
  int job_ids[COLONY_JOB_LIST_MAX];
  int job_count;
  ColonyDialogRect jobs_rect;

  bool eject_open;
  int eject_colonist_index;
  int eject_unit_id; /* outside unit id when re-equipping at fence; else -1 */
  int eject_selection;
  int eject_roles[COLONIZE_EJECT_ROLE_COUNT];
  /* false = DOS's 0xffff row: listed, drawn greyed, not pickable. */
  bool eject_role_enabled[COLONIZE_EJECT_ROLE_COUNT];
  int eject_role_count;
  ColonyDialogRect eject_rect;

  bool dock_orders_open;
  int dock_orders_unit_id;
  int dock_orders_selection;
  ColonyDockOrderAction dock_orders_actions[COLONY_DOCK_ORDERS_MAX];
  char dock_orders_labels[COLONY_DOCK_ORDERS_MAX][COLONY_DOCK_ORDER_LABEL_LEN];
  int dock_orders_count;
  char dock_orders_title[COLONY_DOCK_ORDER_LABEL_LEN];
  int dock_orders_width; /* GAME.TXT @COLONYUNIT @width; 0 = measure the rows */
  ColonyDialogRect dock_orders_rect;

  /* Custom House per-cargo autosell checklist — clicking the Custom House
   * building opens this; each row toggles one cargo's bit and the popup
   * stays open (a checklist, not a pick-one-and-close list like Jobs). */
  bool custom_house_open;
  char custom_house_title[96]; /* GAME.TXT @CUSTOM, tokens/braces stripped */
  int custom_house_width;      /* GAME.TXT @CUSTOM @width; 0 = measure the title */
  bool custom_house_smallfont; /* GAME.TXT @CUSTOM @smallfont → FONTTINY rows */
  int custom_house_cargo_ids[COLONIZE_CARGO_COUNT];
  int custom_house_count;
  ColonyDialogRect custom_house_rect;

  ColonyMessageKind message_kind;
  char message_text[240];
  char message_choice0[48];
  char message_choice1[48];
  int message_selection; /* 0=Yes/OK, 1=No for confirm */
  int pending_eject_colonist;
  int pending_eject_role;
  ColonyDialogRect message_rect;

  ColonizeColonyProdDelta last_delta;
  bool last_delta_valid;
  ColonizeColonyPreview preview;
  bool preview_valid;

  int transport_unit_id;
  int docked_transport_ids[COLONY_TRANSPORT_MAX];
  int docked_transport_count;

  int outside_unit_ids[COLONY_OUTSIDE_MAX];
  int outside_unit_count;

  /* Multifunction "Units" tab selection (docked transport or outside unit id;
   * -1 none). Second click on the already-selected id opens dock orders. */
  int multi_unit_selected_id;

  /* DS:0x8d80 — the game's BIOS boot tick (post_map.boot_timer, saved in
   * the tail @608), the per-game half of the building-layout seed
   * (FUN_2f2b_0434). bugs.md #578. Unset = the screenshot goldens' value. */
  bool layout_seed_set;
  uint32_t layout_seed_base;
} ColonyScreenView;

/* Hand the loaded/new game's post_map.boot_timer to the colony screen. */
void colony_screen_set_layout_seed(ColonyScreenView* view, uint32_t boot_timer);

bool colony_screen_load(ColonyScreenView* view, const char* data_dir, char* err, size_t err_size);
void colony_screen_free(ColonyScreenView* view);
void colony_screen_set_status(ColonyScreenView* view, const char* text);
void colony_screen_reset_ui(ColonyScreenView* view);
void colony_screen_set_delta(ColonyScreenView* view, const ColonizeColonyProdDelta* delta);

/*
 * Close every colony-screen sub-panel (message, jobs, eject, dock orders,
 * Custom House, construction) in one call. The six panels are mutually
 * exclusive — DOS never stacks two of them — so every colony_screen_open_*
 * starts here, and so does any caller that opens a panel by hotkey. Each
 * opener used to hand-roll its own five-line cascade and they had drifted
 * (open_jobs left a message popup up), which is what let a second panel
 * survive underneath a freshly opened one.
 */
void colony_screen_close_subpanels(ColonyScreenView* view);

void colony_screen_open_construction(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  int colony_id,
  const ColoniesBuildableOpts* buildable_opts
);
void colony_screen_close_construction(ColonyScreenView* view);
/* bugs.md #436: advance the construction picker's More... page. */
void colony_screen_construction_next_page(ColonyScreenView* view);

void colony_screen_open_jobs(
  ColonyScreenView* view,
  const ColonizeWorldMap* map,
  const ColonizeColony* colony,
  int tile_index
);
void colony_screen_close_jobs(ColonyScreenView* view);

void colony_screen_open_eject(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index
);
void colony_screen_close_eject(ColonyScreenView* view);

/* Custom House per-cargo autosell checklist. Lists every export-eligible
 * cargo (europe_cargo_export_eligible) regardless of current toggle state —
 * row draw reads the toggle live off `colony->custom_house_bits`. Title
 * comes from GAME.TXT @CUSTOM ("Which cargos shall our Custom House
 * export?") when `messages` is given; a plain fallback otherwise. */
void colony_screen_open_custom_house(
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeMsgCatalog* messages
);
void colony_screen_close_custom_house(ColonyScreenView* view);

/* GAME.TXT @COLONYUNIT title + @UNITOPTIONS (land) / @SHIPOPTIONS (sea) —
 * only currently-legal actions are listed (DOS FUN_2f2b_5746 omits, not
 * grays, ineligible rows), so dock_orders_count varies. */
void colony_screen_open_dock_orders(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeMsgCatalog* messages,
  int unit_id
);
void colony_screen_close_dock_orders(ColonyScreenView* view);

void colony_screen_open_message_ok(ColonyScreenView* view, const char* text);
void colony_screen_close_message(ColonyScreenView* view);

void colony_screen_minimap_origin(int* out_x, int* out_y);

void colony_screen_refresh_transports(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeColony* colony
);

void colony_screen_refresh_outside(
  ColonyScreenView* view,
  const ColonizeUnitPool* units,
  const ColonizeColony* colony
);

void colony_screen_refresh_preview_w(
  const ColonizeWorld* w,
  ColonyScreenView* view,
  const ColonizeColony* colony
);

ColonyScreenHitResult colony_screen_hit_test(
  const ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  int mx,
  int my
);

/* Shared by the Units-tab draw and hit-test paths so geometry never drifts
 * apart; exposed for tests. Returns the slot count written to out (<= max). */
int colony_screen_multi_units_layout(
  const ColonyScreenView* view,
  const ColonizeUnitPool* units,
  int px,
  int py,
  int pane_w,
  int pane_h,
  ColonyMultiUnitSlot* out,
  int max
);

void colony_screen_render_w(
  const ColonizeWorld* w,
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  const ColonizeFont* font,
  bool debug_building_rects,
  const ColonizeMsgCatalog* labels,
  ColonizeFramebuffer8* framebuffer
);
#endif
