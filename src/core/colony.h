#ifndef COLONIZE_COLONY_H
#define COLONIZE_COLONY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/map.h"
#include "core/ss.h"

/* Forward declaration to avoid pulling in font headers. */
typedef struct ColonizeFont ColonizeFont;

#define COLONIZE_COLONIES_MAX 32
#define COLONIZE_COLONY_NAME_MAX 28
#define COLONIZE_COLONY_NAMES_MAX 48
#define COLONIZE_BUILDING_TYPES_MAX 48
#define COLONIZE_COLONY_POP_MAX 32
#define COLONIZE_COLONY_FIELD_TILES 8

/* NAMES.TXT @JOB field jobs (Farmer … Fisherman). */
#define COLONIZE_JOB_FARMER 0
#define COLONIZE_JOB_SUGAR_PLANTER 1
#define COLONIZE_JOB_TOBACCO_PLANTER 2
#define COLONIZE_JOB_COTTON_PLANTER 3
#define COLONIZE_JOB_FUR_TRAPPER 4
#define COLONIZE_JOB_LUMBERJACK 5
#define COLONIZE_JOB_ORE_MINER 6
#define COLONIZE_JOB_SILVER_MINER 7
#define COLONIZE_JOB_FISHERMAN 8
#define COLONIZE_FIELD_JOB_COUNT 9

/* Warehouse cargo order matches NAMES.TXT @CARGO (and ICONS.SS 22..37). */
#define COLONIZE_CARGO_FOOD 0
#define COLONIZE_CARGO_SUGAR 1
#define COLONIZE_CARGO_TOBACCO 2
#define COLONIZE_CARGO_COTTON 3
#define COLONIZE_CARGO_FURS 4
#define COLONIZE_CARGO_LUMBER 5
#define COLONIZE_CARGO_ORE 6
#define COLONIZE_CARGO_SILVER 7
#define COLONIZE_CARGO_HORSES 8
#define COLONIZE_CARGO_RUM 9
#define COLONIZE_CARGO_CIGARS 10
#define COLONIZE_CARGO_CLOTH 11
#define COLONIZE_CARGO_COATS 12
#define COLONIZE_CARGO_TRADE_GOODS 13
#define COLONIZE_CARGO_TOOLS 14
#define COLONIZE_CARGO_MUSKETS 15
#define COLONIZE_CARGO_COUNT 16

typedef struct ColonizeBuildingType {
  char name[40];
  int hammers;
  int tools_cost;
  int min_population;
} ColonizeBuildingType;

/* One person living in a colony (disbanded map unit). */
typedef struct ColonizeColonist {
  int unit_type_index; /* into ColonizeUnitPool types */
  int building_type;   /* workplace @BUILDING index, or -1 */
  int field_job;       /* @JOB field index 0..8, or -1 */
  bool active;
} ColonizeColonist;

typedef struct ColonizeColony {
  int id;
  char name[COLONIZE_COLONY_NAME_MAX];
  int x;
  int y;
  int nation_id; /* 0..3 European owner */
  int population; /* == colonist_count while active */
  bool active;
  ColonizeColonist colonists[COLONIZE_COLONY_POP_MAX];
  int colonist_count;
  bool has_building[COLONIZE_BUILDING_TYPES_MAX];
  /* Surrounding field slots: colonist index or -1. Order N,NE,E,SE,S,SW,W,NW. */
  int8_t tiles[COLONIZE_COLONY_FIELD_TILES];
  /* Warehouse + build queue — production ticks in src/core/turn.c. */
  int stock[COLONIZE_CARGO_COUNT];
  int hammers;
  int building_in_production; /* @BUILDING index, or -1 */
} ColonizeColony;

typedef struct ColonizeColonyPool {
  ColonizeColony colonies[COLONIZE_COLONIES_MAX];
  int colony_count;
  int next_id;
  char names[COLONIZE_COLONY_NAMES_MAX][COLONIZE_COLONY_NAME_MAX];
  int name_count;
  int name_next;
  ColonizeBuildingType building_types[COLONIZE_BUILDING_TYPES_MAX];
  int building_type_count;
} ColonizeColonyPool;

void colonies_init(ColonizeColonyPool* pool);
/* Load colony name list from COLONY.TXT @ENGLISH (or another @section). */
bool colonies_load_names(ColonizeColonyPool* pool, const char* colony_txt_path);
/* Load @BUILDING definitions from NAMES.TXT. */
bool colonies_load_buildings(ColonizeColonyPool* pool, const ColonizeMsgCatalog* names);

int colonies_find_building(const ColonizeColonyPool* pool, const char* name);

bool colonies_can_found(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y
);

/*
 * Found a colony. founder_type_index < 0 skips population (tests).
 * Tools/muskets/horses from the disbanded unit go into the stockpile stub.
 */
int colonies_found(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int founder_type_index,
  int tools,
  int muskets,
  int horses
);

const ColonizeColony* colonies_get(const ColonizeColonyPool* pool, int colony_id);
ColonizeColony* colonies_get_mut(ColonizeColonyPool* pool, int colony_id);
int colonies_id_at(const ColonizeColonyPool* pool, int x, int y);
const ColonizeBuildingType* colonies_building_type(const ColonizeColonyPool* pool, int type_index);

/* Assign colonist to a built workplace (@BUILDING index). Clears any field tile. */
bool colonies_assign_workplace(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int building_type
);
/* Assign colonist to a surround tile with a field @JOB. Clears workplace. */
bool colonies_assign_field(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int tile_index,
  int field_job
);
bool colonies_clear_field(ColonizeColonyPool* pool, int colony_id, int tile_index);
/* Tile index (0..7) for colonist, or -1 if not on a field. */
int colonies_colonist_tile(const ColonizeColony* colony, int colonist_index);
/* Map tile index ↔ (dx,dy) offsets from colony center (N=0 … NW=7). */
bool colonies_field_tile_delta(int tile_index, int* out_dx, int* out_dy);
int colonies_field_tile_index(int dx, int dy);

/* Set construction target; building_type must be unowned and meet min_population. */
bool colonies_set_construction(ColonizeColonyPool* pool, int colony_id, int building_type);
bool colonies_clear_construction(ColonizeColonyPool* pool, int colony_id);
/* Fill out_ids with buildable @BUILDING indices; returns count. */
int colonies_list_buildable(
  const ColonizeColonyPool* pool,
  int colony_id,
  int* out_ids,
  int out_max
);

void colonies_render_on_map(
  const ColonizeColonyPool* pool,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
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
