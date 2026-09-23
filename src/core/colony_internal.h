#ifndef COLONIZE_CORE_COLONY_INTERNAL_H
#define COLONIZE_CORE_COLONY_INTERNAL_H

/* ===== Cross-file seams of the colony.c split (2026-09-23) =====
 * colony.c was 4483 lines; it is now split along its banners into
 * colony{,_plots,_workers,_build,_goods}.c. The declarations below are the
 * only symbols used across those files; everything else stayed `static` in
 * its own file. Code moved verbatim - these are the only de-static'd names,
 * and shared file-scope variables carry the `colony_` prefix.
 * ============================================================== */

#include <stdbool.h>

#include "core/colony.h"
#include "core/col1_save.h"
#include "core/map.h"

/* --- owned by colony.c --- */
/* Names as loaded, by row — the only place a building name can be matched. */
extern char colony_building_row_names[COLONIZE_BUILDING_TYPES_MAX][40];
extern int colony_building_row_name_count;
extern ColoniesBuildingRowNameResolver colony_building_row_name_resolver;
extern ColonizeWorldMap* g_colonies_occupancy_map;
extern ColonizeCol1Save* g_colonies_col1;
void colonies_mark_settlement_tile(int x, int y, bool on);
void colonies_col1_rebel_divisor_adjust(ColonizeCol1Save* col1, int x, int y, int delta);

/* --- owned by colony_plots.c --- */
void colonies_clear_colonist_tile(ColonizeColony* col, int colonist_index);

#endif /* COLONIZE_CORE_COLONY_INTERNAL_H */
