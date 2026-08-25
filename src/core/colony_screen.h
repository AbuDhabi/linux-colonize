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
#define COLONY_MINIMAP_SECTION_W COLONY_MINIMAP_SECTION_H
#define COLONY_MINIMAP_SECTION_X (COLONY_SCREEN_WIDTH - COLONY_MINIMAP_SECTION_W)
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

#define COLONY_EXIT_X 306
#define COLONY_EXIT_Y 179
#define COLONY_BUILDABLE_MAX 32
#define COLONY_BUILDING_SLOT_W 48
#define COLONY_BUILDING_SLOT_H 32
#define COLONY_JOB_LIST_MAX COLONIZE_FIELD_JOB_COUNT

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
#define COLONY_TRANSPORT_ICON_Y (COLONY_BOTTOM_PANEL_Y + 8)
#define COLONY_TRANSPORT_PITCH 18
#define COLONY_HOLD_X COLONY_TRANSPORT_X
#define COLONY_HOLD_Y (COLONY_BOTTOM_PANEL_Y + 30)
#define COLONY_HOLD_W 12
#define COLONY_HOLD_H 14
#define COLONY_HOLD_PITCH 14

#define COLONY_OUTSIDE_MAX 12

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
  COLONY_HIT_EXIT,
  COLONY_HIT_CONSTRUCTION_OUTSIDE,
  COLONY_HIT_AREA_TILE,
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
  COLONY_HIT_MESSAGE_OUTSIDE
} ColonyScreenHit;

typedef enum ColonyMessageKind {
  COLONY_MSG_NONE = 0,
  COLONY_MSG_OK,      /* single OK dismiss */
  COLONY_MSG_CONFIRM  /* Yes / No */
} ColonyMessageKind;

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

typedef struct ColonyMultiUnitSlot {
  int unit_id;
  int x;
  int y;
  int w;
  int h;
} ColonyMultiUnitSlot;

typedef struct ColonyScreenView {
  ColonizePikImage frame;
  ColonizeSpriteSheet parch;
  ColonizeSpriteSheet wood_tile;
  ColonizeSpriteSheet buildings;
  ColonizeSpriteSheet icons;
  ColonizePikImage bottom_panel;
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
  int construction_dialog_x;
  int construction_dialog_y;
  int construction_dialog_w;
  int construction_dialog_h;
  int construction_list_y0;
  int construction_line_h;

  bool jobs_open;
  int jobs_tile_index;
  int jobs_selection;
  int job_ids[COLONY_JOB_LIST_MAX];
  int job_count;
  int jobs_dialog_x;
  int jobs_dialog_y;
  int jobs_dialog_w;
  int jobs_dialog_h;
  int jobs_list_y0;
  int jobs_line_h;

  bool eject_open;
  int eject_colonist_index;
  int eject_unit_id; /* outside unit id when re-equipping at fence; else -1 */
  int eject_selection;
  int eject_roles[COLONIZE_EJECT_ROLE_COUNT];
  int eject_role_count;
  int eject_dialog_x;
  int eject_dialog_y;
  int eject_dialog_w;
  int eject_dialog_h;
  int eject_list_y0;
  int eject_line_h;

  bool dock_orders_open;
  int dock_orders_unit_id;
  int dock_orders_selection;
  ColonyDockOrderAction dock_orders_actions[COLONY_DOCK_ORDERS_MAX];
  char dock_orders_labels[COLONY_DOCK_ORDERS_MAX][COLONY_DOCK_ORDER_LABEL_LEN];
  int dock_orders_count;
  char dock_orders_title[COLONY_DOCK_ORDER_LABEL_LEN];
  int dock_orders_dialog_x;
  int dock_orders_dialog_y;
  int dock_orders_dialog_w;
  int dock_orders_dialog_h;
  int dock_orders_list_y0;
  int dock_orders_line_h;

  ColonyMessageKind message_kind;
  char message_text[240];
  char message_choice0[48];
  char message_choice1[48];
  int message_selection; /* 0=Yes/OK, 1=No for confirm */
  int pending_eject_colonist;
  int pending_eject_role;
  int message_dialog_x;
  int message_dialog_y;
  int message_dialog_w;
  int message_dialog_h;
  int message_list_y0;
  int message_line_h;

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
} ColonyScreenView;

bool colony_screen_load(ColonyScreenView* view, const char* data_dir, char* err, size_t err_size);
void colony_screen_free(ColonyScreenView* view);
void colony_screen_set_status(ColonyScreenView* view, const char* text);
void colony_screen_reset_ui(ColonyScreenView* view);
void colony_screen_set_delta(ColonyScreenView* view, const ColonizeColonyProdDelta* delta);

void colony_screen_open_construction(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  int colony_id,
  const ColoniesBuildableOpts* buildable_opts
);
void colony_screen_close_construction(ColonyScreenView* view);

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
void colony_screen_open_abandon_confirm(
  ColonyScreenView* view,
  int colonist_index,
  int role,
  const char* body,
  const char* choice_yes,
  const char* choice_no
);
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

void colony_screen_refresh_preview(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1
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

void colony_screen_render(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeSpriteSheet* terrain,
  const ColonizeSpriteSheet* phys0,
  const ColonizeCol1Save* col1,
  uint16_t game_year,
  uint16_t game_autumn,
  int gold,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

#endif
