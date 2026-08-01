#ifndef COLONIZE_UNITS_H
#define COLONIZE_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/map.h"
#include "core/ss.h"

/* Original COLONY.SAV can hold well over 64 map units (natives + Europeans). */
#define COLONIZE_UNITS_MAX 256
#define COLONIZE_UNIT_TYPES_MAX 32
#define COLONIZE_UNIT_CARGO_MAX 6 /* Man-O-War hold size */

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
  int nation_id; /* 0..3 European, 4..11 native tribes (COL1) */
  int aboard_ship_id; /* -1 = on map; else id of carrying ship */
  int cargo_ids[COLONIZE_UNIT_CARGO_MAX]; /* passenger unit ids (ships only) */
  int cargo_count;
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
/* Spawn even if the tile already has a unit (COL1 stacks / passengers). */
int units_spawn_allow_stack(ColonizeUnitPool* pool, int type_index, int x, int y);
bool units_despawn(ColonizeUnitPool* pool, int unit_id);
int units_id_at(const ColonizeUnitPool* pool, int x, int y);
ColonizeUnit* units_get(ColonizeUnitPool* pool, int unit_id);
const ColonizeUnit* units_get_const(const ColonizeUnitPool* pool, int unit_id);
const ColonizeUnitType* units_type(const ColonizeUnitPool* pool, int type_index);
bool units_is_sea(const ColonizeUnitPool* pool, int unit_id);
bool units_is_on_map(const ColonizeUnit* unit);

/* Equipment the unit carries into a new colony warehouse when founding. */
void units_founder_loot(
  const ColonizeUnitPool* pool,
  int unit_id,
  int* out_tools,
  int* out_muskets,
  int* out_horses
);

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
bool units_find_water_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int occupant_id,
  int* out_x,
  int* out_y
);
bool units_find_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int* out_x,
  int* out_y
);

/* Board a land unit onto an adjacent ship. Returns false if capacity/adjacency fails. */
bool units_board(ColonizeUnitPool* pool, int land_unit_id, int ship_id);
/* Board without adjacency check (COL1 import; passenger already stacked on ship tile). */
bool units_board_stacked(ColonizeUnitPool* pool, int land_unit_id, int ship_id);
/* Unload oldest passenger from ship onto dest (must be enterable land). */
bool units_unload(
  ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y
);
int units_ship_capacity(const ColonizeUnitPool* pool, int ship_id);
/* Snapshot passenger type indices (for Europe harbor transfer). */
int units_export_cargo_types(
  const ColonizeUnitPool* pool,
  int ship_id,
  int* out_types,
  int out_max
);
/* Despawn ship and all passengers; fills cargo type list for harbor. */
bool units_despawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_id,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max
);
/* Spawn ship at (x,y) and recreate passengers aboard from type indices. */
int units_spawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_type_index,
  int x,
  int y,
  const int* cargo_types,
  int cargo_count
);

void units_end_turn(ColonizeUnitPool* pool);
void units_new_world_start(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y
);

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
  int tile_h,
  int origin_x,
  int origin_y
);

#endif
