#include "core/colony.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/font.h"
#include "platform/diagnostics.h"

static void colony_trim(char* s) {
  char* start = s;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
    s[--n] = '\0';
  }
}

void colonies_init(ColonizeColonyPool* pool) {
  if (!pool) {
    return;
  }
  memset(pool, 0, sizeof(*pool));
}

bool colonies_load_names(ColonizeColonyPool* pool, const char* colony_txt_path) {
  if (!pool || !colony_txt_path) {
    return false;
  }
  pool->name_count = 0;
  pool->name_next = 0;

  FILE* f = fopen(colony_txt_path, "r");
  if (!f) {
    diag_warn("Cannot open %s for colony names", colony_txt_path);
    return false;
  }

  char line[128];
  bool in_english = false;
  while (fgets(line, sizeof(line), f)) {
    colony_trim(line);
    if (line[0] == '@') {
      in_english = (strncmp(line + 1, "ENGLISH", 7) == 0);
      continue;
    }
    if (!in_english) {
      continue;
    }
    if (line[0] == '\0' || line[0] == ';') {
      continue;
    }
    /* Lines may have a year suffix: "Jamestown,1607" — strip it. */
    char* comma = strchr(line, ',');
    if (comma) {
      *comma = '\0';
    }
    colony_trim(line);
    if (line[0] == '\0') {
      continue;
    }
    if (pool->name_count >= COLONIZE_COLONY_NAMES_MAX) {
      break;
    }
    snprintf(
      pool->names[pool->name_count],
      COLONIZE_COLONY_NAME_MAX,
      "%s",
      line
    );
    pool->name_count++;
  }
  fclose(f);
  diag_info("Loaded %d colony names from %s", pool->name_count, colony_txt_path);
  return pool->name_count > 0;
}

bool colonies_load_buildings(ColonizeColonyPool* pool, const ColonizeMsgCatalog* names) {
  if (!pool || !names) {
    return false;
  }
  pool->building_type_count = 0;

  const ColonizeMsgSection* section = assets_msg_find(names, "BUILDING");
  if (!section) {
    diag_warn("NAMES.TXT missing @BUILDING section.");
    return false;
  }

  for (int i = 0; i < section->line_count && pool->building_type_count < COLONIZE_BUILDING_TYPES_MAX; ++i) {
    char line[COLONIZE_MSG_LINE_LEN];
    snprintf(line, sizeof(line), "%s", section->lines[i]);
    if (line[0] == ';' || line[0] == '\0') {
      continue;
    }
    char* semi = strchr(line, ';');
    if (semi) {
      *semi = '\0';
    }
    char* comma = strchr(line, ',');
    if (!comma) {
      continue;
    }
    *comma = '\0';
    colony_trim(line);
    if (line[0] == '\0') {
      continue;
    }

    const char* p = comma + 1;
    int hammers = 0;
    int tools_cost = 0;
    int a = 0;
    int b = 0;
    int min_pop = 0;
    /* name, hammers, tools, ?, ?, min_population — trailing fields optional. */
    sscanf(p, " %d , %d , %d , %d , %d", &hammers, &tools_cost, &a, &b, &min_pop);
    (void)a;
    (void)b;

    ColonizeBuildingType* t = &pool->building_types[pool->building_type_count++];
    snprintf(t->name, sizeof(t->name), "%s", line);
    t->hammers = hammers;
    t->tools_cost = tools_cost;
    t->min_population = min_pop;
  }

  diag_info("Loaded %d building types from NAMES.TXT @BUILDING", pool->building_type_count);
  return pool->building_type_count > 0;
}

int colonies_find_building(const ColonizeColonyPool* pool, const char* name) {
  if (!pool || !name) {
    return -1;
  }
  for (int i = 0; i < pool->building_type_count; ++i) {
    if (strcmp(pool->building_types[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

const ColonizeBuildingType* colonies_building_type(const ColonizeColonyPool* pool, int type_index) {
  if (!pool || type_index < 0 || type_index >= pool->building_type_count) {
    return NULL;
  }
  return &pool->building_types[type_index];
}

bool colonies_can_found(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y
) {
  if (!pool || !map) {
    return false;
  }
  if (!map_tile_is_land(map, x, y)) {
    return false;
  }
  if (colonies_id_at(pool, x, y) >= 0) {
    return false;
  }
  return true;
}

static const char* colonies_next_name(ColonizeColonyPool* pool) {
  if (pool->name_count == 0) {
    return "New Colony";
  }
  const char* n = pool->names[pool->name_next % pool->name_count];
  pool->name_next++;
  return n;
}

static void colonies_grant_building(ColonizeColonyPool* pool, ColonizeColony* slot, const char* name) {
  const int idx = colonies_find_building(pool, name);
  if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX) {
    slot->has_building[idx] = true;
  }
}

/*
 * Classic free starters: craft houses + carpenter + town hall.
 * Warehouse, Stockade, and Docks are buildable (not free).
 * Coastal colonies without Docks show BUILDING.SS #45 coast placeholder;
 * without Stockade the screen draws fence art (BUILDING.SS #16).
 */
static void colonies_grant_starters(ColonizeColonyPool* pool, ColonizeColony* slot) {
  static const char* k_starters[] = {
    "Town Hall",
    "Carpenter's Shop",
    "Blacksmith's House",
    "Weaver's House",
    "Tobacconist's House",
    "Rum Distiller's House",
    "Fur Trader's House",
  };
  for (size_t i = 0; i < sizeof(k_starters) / sizeof(k_starters[0]); ++i) {
    colonies_grant_building(pool, slot, k_starters[i]);
  }
}

int colonies_found(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int founder_type_index,
  int tools,
  int muskets,
  int horses
) {
  if (!colonies_can_found(pool, map, x, y)) {
    return -1;
  }
  if (pool->colony_count >= COLONIZE_COLONIES_MAX) {
    diag_warn("Colony pool full");
    return -1;
  }

  ColonizeColony* slot = NULL;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (!pool->colonies[i].active) {
      slot = &pool->colonies[i];
      break;
    }
  }
  if (!slot) {
    return -1;
  }

  memset(slot, 0, sizeof(*slot));
  slot->id = pool->next_id++;
  slot->x = x;
  slot->y = y;
  slot->nation_id = 0;
  slot->building_in_production = -1;
  slot->active = true;
  snprintf(slot->name, sizeof(slot->name), "%s", colonies_next_name(pool));
  colonies_grant_starters(pool, slot);

  if (tools > 0) {
    slot->stock[COLONIZE_CARGO_TOOLS] += tools;
  }
  if (muskets > 0) {
    slot->stock[COLONIZE_CARGO_MUSKETS] += muskets;
  }
  if (horses > 0) {
    slot->stock[COLONIZE_CARGO_HORSES] += horses;
  }
  /* New colonies start with a little food in the warehouse stub. */
  slot->stock[COLONIZE_CARGO_FOOD] = 200;

  if (founder_type_index >= 0 && slot->colonist_count < COLONIZE_COLONY_POP_MAX) {
    ColonizeColonist* c = &slot->colonists[slot->colonist_count++];
    c->active = true;
    c->unit_type_index = founder_type_index;
    c->building_type = colonies_find_building(pool, "Town Hall");
    slot->population = slot->colonist_count;
  } else {
    slot->population = 0;
  }

  /* Default first project so carpenter hammers have a target. */
  {
    const int stockade = colonies_find_building(pool, "Stockade");
    if (stockade >= 0 && !slot->has_building[stockade]) {
      slot->building_in_production = stockade;
    }
  }

  pool->colony_count++;
  diag_info(
    "Founded colony '%s' at (%d,%d) pop=%d tools=%d muskets=%d horses=%d",
    slot->name,
    x,
    y,
    slot->population,
    slot->stock[COLONIZE_CARGO_TOOLS],
    slot->stock[COLONIZE_CARGO_MUSKETS],
    slot->stock[COLONIZE_CARGO_HORSES]
  );
  return slot->id;
}

const ColonizeColony* colonies_get(const ColonizeColonyPool* pool, int colony_id) {
  if (!pool || colony_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].id == colony_id) {
      return &pool->colonies[i];
    }
  }
  return NULL;
}

ColonizeColony* colonies_get_mut(ColonizeColonyPool* pool, int colony_id) {
  if (!pool || colony_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].id == colony_id) {
      return &pool->colonies[i];
    }
  }
  return NULL;
}

int colonies_id_at(const ColonizeColonyPool* pool, int x, int y) {
  if (!pool) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].x == x && pool->colonies[i].y == y) {
      return pool->colonies[i].id;
    }
  }
  return -1;
}

bool colonies_assign_workplace(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int building_type
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count) {
    return false;
  }
  ColonizeColonist* c = &col->colonists[colonist_index];
  if (!c->active) {
    return false;
  }
  if (building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  if (!col->has_building[building_type]) {
    return false;
  }
  c->building_type = building_type;
  return true;
}

bool colonies_set_construction(ColonizeColonyPool* pool, int colony_id, int building_type) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  if (col->has_building[building_type]) {
    return false;
  }
  const ColonizeBuildingType* bt = &pool->building_types[building_type];
  if (bt->min_population > 0 && col->population < bt->min_population) {
    return false;
  }
  col->building_in_production = building_type;
  return true;
}

bool colonies_clear_construction(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col) {
    return false;
  }
  col->building_in_production = -1;
  return true;
}

int colonies_list_buildable(
  const ColonizeColonyPool* pool,
  int colony_id,
  int* out_ids,
  int out_max
) {
  if (!pool || !out_ids || out_max <= 0) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(pool, colony_id);
  if (!col) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < pool->building_type_count && n < out_max; ++i) {
    if (col->has_building[i]) {
      continue;
    }
    const ColonizeBuildingType* bt = &pool->building_types[i];
    if (bt->name[0] == '\0') {
      continue;
    }
    if (bt->min_population > 0 && col->population < bt->min_population) {
      continue;
    }
    /* Skip zero-hammer fluff / duplicates that aren't real projects. */
    if (bt->hammers <= 0) {
      continue;
    }
    out_ids[n++] = i;
  }
  return n;
}

/* Draw a small filled square in colour 11 (bright cyan) with the colony name
   rendered one pixel below the tile.  We skip blitting an icon sprite because
   no suitable 16x16 colony marker exists in ICONS.SS. */
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
) {
  (void)icons; /* reserved for future icon */
  if (!pool || !framebuffer) {
    return;
  }

  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (!c->active) {
      continue;
    }
    const int sx = c->x - view_x;
    const int sy = c->y - view_y;
    if (sx < 0 || sy < 0 || sx >= view_cols || sy >= view_rows) {
      continue;
    }

    const int px = origin_x + sx * tile_w;
    const int py = origin_y + sy * tile_h;

    /* Filled square in bright cyan (palette index 11). */
    for (int row = py; row < py + tile_h && row < framebuffer->height; ++row) {
      if (row < 0) {
        continue;
      }
      for (int col = px; col < px + tile_w && col < framebuffer->width; ++col) {
        if (col < 0) {
          continue;
        }
        framebuffer->pixels[row * framebuffer->width + col] = 11;
      }
    }

    /* Colony name label below tile. */
    if (font) {
      font_draw_text(font, framebuffer, px, py + tile_h + 1, c->name, 15);
    }
  }
}
