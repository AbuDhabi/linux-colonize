#ifndef COLONIZE_COLONY_H
#define COLONIZE_COLONY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/map.h"
#include "core/ss.h"

/* Forward declaration to avoid pulling in font headers. */
typedef struct ColonizeFont ColonizeFont;

#define COLONIZE_COLONIES_MAX 32
#define COLONIZE_COLONY_NAME_MAX 28
#define COLONIZE_COLONY_NAMES_MAX 48

typedef struct ColonizeColony {
  int id;
  char name[COLONIZE_COLONY_NAME_MAX];
  int x;
  int y;
  int population;
  bool active;
} ColonizeColony;

typedef struct ColonizeColonyPool {
  ColonizeColony colonies[COLONIZE_COLONIES_MAX];
  int colony_count;
  int next_id;
  char names[COLONIZE_COLONY_NAMES_MAX][COLONIZE_COLONY_NAME_MAX];
  int name_count;
  int name_next;
} ColonizeColonyPool;

void colonies_init(ColonizeColonyPool* pool);
/* Load colony name list from COLONY.TXT @ENGLISH (or another @section). */
bool colonies_load_names(ColonizeColonyPool* pool, const char* colony_txt_path);

bool colonies_can_found(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y
);
/* Returns new colony id, or -1 on failure. Consumes unit at (x,y) if suc */
int colonies_found(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y
);

const ColonizeColony* colonies_get(const ColonizeColonyPool* pool, int colony_id);
int colonies_id_at(const ColonizeColonyPool* pool, int x, int y);

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
