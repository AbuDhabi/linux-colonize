#ifndef COLONIZE_COL1_BRIDGE_INTERNAL_H
#define COLONIZE_COL1_BRIDGE_INTERNAL_H

/*
 * Stage seam for col1_bridge_apply_w / col1_bridge_capture_w (bugs.md #1107,
 * docs/conventions.md "Big-function rule" / "Internal-header test seam").
 * Each public entry point is a dispatcher over record-family stages in file
 * order; every local that crosses a stage lives in the ctx below and is
 * written back on every stage exit.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/col1_bridge.h"
#include "core/internal.h"

/* A stage that can hit a COL1_FAIL path: ABORT = err filled, dispatcher returns false. */
typedef enum Col1StageStatus {
  COL1_STAGE_OK = 0,
  COL1_STAGE_ABORT
} Col1StageStatus;

typedef struct Col1ApplyCtx {
  const ColonizeCol1Save* save;
  ColonizeWorldMap* map;
  ColonizeUnitPool* units;
  ColonizeColonyPool* colonies;
  EuropeScreen* europe;
  char* err;
  size_t err_size;
  ColonizeCol1BridgeResult local; /* copied to *out by the dispatcher */
  int* id_by_index;               /* save unit index -> runtime id (units_reset .. active_unit) */
  bool* consumed;                 /* save unit index taken by a Europe lane (units_reset .. active_unit) */
} Col1ApplyCtx;

typedef struct Col1CaptureCtx {
  ColonizeCol1Save* save;
  const ColonizeWorldMap* map;
  ColonizeUnitPool* units;
  const ColonizeColonyPool* colonies;
  const EuropeScreen* europe;
  char* err;
  size_t err_size;
  uint16_t year;
  uint16_t autumn;
  uint32_t turn_number;
  int human_nation; /* clamped by the head stage */
  int cursor_x;
  int cursor_y;
  int view_x;
  int view_y;
  int active_unit_id;
  bool view_pieces_mode;
  /* Unit export, units_alloc .. units_commit. */
  int capacity;
  ColonizeCol1Unit* neu;
  uint64_t* stack_order_by_col1;
  int* runtime_to_col1;
  int written;
  int active_col1;
} Col1CaptureCtx;

#ifdef COLONIZE_TESTING
COLONIZE_INTERNAL Col1StageStatus col1_bridge_apply_head(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_apply_map_planes(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_apply_colonies(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL Col1StageStatus col1_bridge_apply_units_reset(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_apply_europe_lanes(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL Col1StageStatus col1_bridge_apply_units(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL Col1StageStatus col1_bridge_apply_boarding(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_apply_active_unit(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_apply_europe_nation(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_apply_view_occupancy(Col1ApplyCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_apply_ext_chunks(Col1ApplyCtx* ctx);

COLONIZE_INTERNAL Col1StageStatus col1_bridge_capture_head(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_capture_map_planes(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_capture_nation_europe(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL Col1StageStatus col1_bridge_capture_colonies(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL Col1StageStatus col1_bridge_capture_units_alloc(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_capture_unit_records(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_capture_unit_chains(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_capture_dock_rows(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_capture_europe_lanes(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_capture_units_commit(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL void col1_bridge_capture_occupancy(Col1CaptureCtx* ctx);
COLONIZE_INTERNAL Col1StageStatus col1_bridge_capture_ext_chunks(Col1CaptureCtx* ctx);
#endif

#endif /* COLONIZE_COL1_BRIDGE_INTERNAL_H */
