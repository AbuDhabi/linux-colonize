#ifndef COLONIZE_TRAVEL_DEST_H
#define COLONIZE_TRAVEL_DEST_H

#include <stdbool.h>

#include "core/colony.h"
#include "core/map.h"
#include "core/units.h"

/*
 * Destination-picker reachability: the filter behind DOS's colony list
 * FUN_647e_01c6 (ORDERS "Go to Port"/"Go to Place" via FUN_2b5a_1dfc, and the
 * trade-route editor's stop rows via FUN_647e_0dd4). Every row it shows is
 * gated by thunk_FUN_2a1f_0762 -> FUN_647e_0094(dest, unit) (asm
 * 647e:0094-01c0), ported as travel_dest_ok. Wording and layout stay with the
 * callers; this file is the rule only. Authority: docs/unit_orders.md
 * "Go to Port"; trade-route pickers in game_loop_menus.c.
 */

/* DOS destination id of the Europe row (0x3e7). */
#define TRAVEL_DEST_EUROPE 999

/*
 * FUN_647e_0094's unit argument: a real unit, or the trade-route editor's
 * phantom unit (FUN_478c_007e: Caravel for a sea route, Colonist for a land
 * route, parked on the route's first stop; Europe-first = 236+nation, off the
 * interior). A NULL traveller is DOS's unit = -1 (the create wizard): no
 * reachability test, only the owner test.
 */
typedef struct TravelDestWho {
  bool ship; /* DOS unit type 0xd..0x12 */
  int x;
  int y;
} TravelDestWho;

/* Traveller for a live unit; false if the unit does not exist. */
bool travel_dest_who_unit(const ColonizeUnitPool* units, int unit_id, TravelDestWho* out);

/*
 * FUN_281f_0d12 -> FUN_15eb_00a2 (raw 9340-9374): the layer3 low nibble (water
 * region) of the lowest-region water neighbour of (x, y) over the 8 INSET
 * neighbours (DS:0x8dba/0x8dbc, read back through FUN_281f_06b4), or -1 when
 * no neighbour is water (DOS returns 0). Region 0 ranks last (as 0x10) but is
 * reported as 0, exactly what FUN_281f_06b4 reads there.
 */
int travel_dest_water_region(const ColonizeWorldMap* map, int x, int y);

/*
 * FUN_647e_0094 (asm 647e:0094-01c0). `dest` is a colony id or
 * TRAVEL_DEST_EUROPE. A colony must be active and owned by `nation` (DS:0x5394
 * nation_turn). With a traveller:
 *  - land unit: the colony tile's continent (layer3 low nibble) equals the
 *    unit tile's (06b4 compare; Europe is never offered);
 *  - ship: the colony's lowest-region water neighbour has the ship's water
 *    region, where the ship's region is that of its own lowest-region water
 *    neighbour (the tile itself if it has none), or of tile (W-2, 1) when the
 *    ship stands off the interior (Europe dock, phantom). Europe passes when
 *    the ship's region is the one of tile (W-2, 1), the Atlantic.
 */
bool travel_dest_ok(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation,
  int dest,
  const TravelDestWho* who
);

#endif
