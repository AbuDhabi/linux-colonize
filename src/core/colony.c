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

int colonies_found(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y
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

  slot->id = pool->next_id++;
  slot->x = x;
  slot->y = y;
  slot->population = 1;
  slot->active = true;
  snprintf(slot->name, sizeof(slot->name), "%s", colonies_next_name(pool));
  pool->colony_count++;

  diag_info("Founded colony '%s' at (%d,%d)", slot->name, x, y);
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
  int tile_h
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

    const int px = sx * tile_w;
    const int py = sy * tile_h;

    /* Filled square in bright cyan (palette index 11). */
    for (int row = py; row < py + tile_h && row < framebuffer->height; ++row) {
      for (int col = px; col < px + tile_w && col < framebuffer->width; ++col) {
        framebuffer->pixels[row * framebuffer->width + col] = 11;
      }
    }

    /* Colony name label below tile. */
    if (font) {
      font_draw_text(font, framebuffer, px, py + tile_h + 1, c->name, 15);
    }
  }
}
