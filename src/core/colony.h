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
  bool active;
} ColonizeColonist;

typedef struct ColonizeColony {
  int id;
  char name[COLONIZE_COLONY_NAME_MAX];
  int x;
  int y;
  int population; /* == colonist_count while active */
  bool active;
  ColonizeColonist colonists[COLONIZE_COLONY_POP_MAX];
  int colonist_count;
  bool has_building[COLONIZE_BUILDING_TYPES_MAX];
  /* Warehouse stub — production/trade not wired yet. Indexed by COLONIZE_CARGO_*. */
  int stock[COLONIZE_CARGO_COUNT];
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
  int tile_h
);

#endif
