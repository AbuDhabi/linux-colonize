#ifndef COLONIZE_UNITS_H
#define COLONIZE_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/map.h"
#include "core/ss.h"

#define COLONIZE_UNITS_MAX 64
#define COLONIZE_UNIT_TYPES_MAX 32

typedef enum ColonizeUnitDomain {
  COLONIZE_UNIT_DOMAIN_LAND = 0,
  COLONIZE_UNIT_DOMAIN_SEA = 1
} ColonizeUnitDomain;

typedef struct ColonizeUnitType {
  char name[32];
  int icon_sprite; /* ICONS.SS sprite index from NAMES.TXT @UNIT */
  int movement;
  int attack;
  int defense;
  int cargo;
  ColonizeUnitDomain domain;
} ColonizeUnitType;

typedef struct ColonizeUnit {
  int id;
  int type_index;
  int x;
  int y;
  int moves_left;
  bool active;
} ColonizeUnit;

typedef struct ColonizeUnitPool {
  ColonizeUnitType types[COLONIZE_UNIT_TYPES_MAX];
  int type_count;
  ColonizeUnit units[COLONIZE_UNITS_MAX];
  int unit_count;
  int selected_id;
  int next_id;
} ColonizeUnitPool;

bool units_load_types(ColonizeUnitPool* pool, const ColonizeMsgCatalog* names);
void units_reset(ColonizeUnitPool* pool);

int units_find_type(const ColonizeUnitPool* pool, const char* name);
int units_spawn(ColonizeUnitPool* pool, int type_index, int x, int y);
bool units_despawn(ColonizeUnitPool* pool, int unit_id);
int units_id_at(const ColonizeUnitPool* pool, int x, int y);
ColonizeUnit* units_get(ColonizeUnitPool* pool, int unit_id);
const ColonizeUnit* units_get_const(const ColonizeUnitPool* pool, int unit_id);
const ColonizeUnitType* units_type(const ColonizeUnitPool* pool, int type_index);
bool units_is_sea(const ColonizeUnitPool* pool, int unit_id);

bool units_can_enter(
  const ColonizeUnitPool* pool,
  int type_index,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int occupant_id
);
bool units_try_move(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y
);

/* True for high-seas / sea-lane tiles (terrain index 26). */
bool units_on_high_seas(const ColonizeWorldMap* map, int x, int y);
/* Nearest free water tile for a sea unit (occupant_id excluded when checking occupancy). */
bool units_find_water_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int occupant_id,
  int* out_x,
  int* out_y
);
/* Free high-seas tile near start; falls back to any free high-seas tile. */
bool units_find_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int* out_x,
  int* out_y
);

void units_end_turn(ColonizeUnitPool* pool);
void units_new_world_start(ColonizeUnitPool* pool, const ColonizeWorldMap* map);

/* Spawn a Colonist from a recruited immigrant name (Europe dock). */
bool units_deploy_colonist(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  const char* immigrant_name
);

int units_map_sprite(const ColonizeUnitPool* pool, int unit_id);
void units_render_on_map(
  const ColonizeUnitPool* pool,
  const ColonizeSpriteSheet* nation_sheet,
  ColonizeFramebuffer8* framebuffer,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h
);

#endif
