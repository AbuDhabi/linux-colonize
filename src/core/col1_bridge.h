#ifndef COLONIZE_COL1_BRIDGE_H
#define COLONIZE_COL1_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/units.h"

/*
 * Bridge between original COLONY##.SAV (ColonizeCol1Save) and live bring-up
 * pools. Import is playable (map/units/colonies/Europe/turn). Export updates
 * a loaded Col1 snapshot in place (read-modify-write) so unknown bytes and
 * natives stay intact for original-game compatibility.
 */

uint8_t col1_tile_to_mp_terrain(uint8_t col1_tile_byte);
uint8_t col1_mp_terrain_to_tile(uint8_t mp_terrain_byte);

typedef struct ColonizeCol1BridgeResult {
  uint16_t year;
  uint16_t autumn;
  uint32_t turn_number;
  int human_nation; /* 0..3 */
  int cursor_x;
  int cursor_y;
  int imported_units;
  int imported_colonies;
  int skipped_europe_units;
} ColonizeCol1BridgeResult;

bool col1_bridge_apply(
  const ColonizeCol1Save* save,
  ColonizeWorldMap* map,
  ColonizeUnitPool* units,
  ColonizeColonyPool* colonies,
  EuropeScreen* europe,
  ColonizeCol1BridgeResult* out,
  char* err,
  size_t err_size
);

/*
 * Update *save in place from live state (must already be a valid loaded or
 * template Col1 save with matching map size / allocated sections).
 */
bool col1_bridge_capture(
  ColonizeCol1Save* save,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  uint16_t year,
  uint16_t autumn,
  uint32_t turn_number,
  int human_nation,
  int cursor_x,
  int cursor_y,
  int active_unit_id,
  char* err,
  size_t err_size
);

/* Build a minimal standard-size Col1 template (empty natives/unknown). */
bool col1_bridge_init_template(
  ColonizeCol1Save* save,
  uint16_t map_w,
  uint16_t map_h,
  char* err,
  size_t err_size
);

/*
 * Rebuild Col1 mask / live layer2 occupancy bits (has_unit / has_city) from
 * live units, colonies, and tribe villages. Clears bits 0–1 then sets them;
 * preserves road/plow/suppress/purchased/pacific and other high mask bits.
 * Pass NULL for save or map to skip that side. Required before Linux→DOS write.
 * tribe_save supplies village tiles (may be the same pointer as save, or a
 * const apply-time snapshot when save is NULL).
 */
void col1_bridge_sync_map_occupancy(
  ColonizeCol1Save* save,
  ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* tribe_save
);

/*
 * Synthesize Col1 mask density bits (suppress/purchased/pacific) from live
 * terrain + layer2 (FUN_684c_08c0 / FUN_137f_015e). Preserves purchased when
 * neither plane tracks a clear; ORs layer2 deplete/purchase/pacific.
 * Call after road/plow sync, before or after occupancy rebuild.
 */
void col1_bridge_sync_map_density(ColonizeCol1Save* save, const ColonizeWorldMap* map);

/*
 * After a European unit steps onto (x,y): if adjacent to a tribe village,
 * bump that tribe's alarm for european_nation (0..3) and indian.alarm_by_player.
 * Does NOT set euro_diplo — first contact is ai_contact_try_first_welcome.
 * If out_first_indian_nation is non-NULL, writes Col1 nation id 4..11 for the
 * first unmet tribe this call (else -1).
 * Writes a short status when a new contact happens (optional).
 * Returns true if any village was contacted this call.
 */
bool col1_contact_adjacent_tribe(
  ColonizeCol1Save* save,
  int x,
  int y,
  int european_nation,
  char* status_out,
  size_t status_size,
  int* out_first_indian_nation
);

#endif
