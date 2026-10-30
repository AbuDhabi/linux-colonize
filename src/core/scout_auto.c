#include "core/scout_auto.h"

#include <stdlib.h>
#include <string.h>

#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/settings.h"
#include "core/units.h"
#include "core/units_move.h"
#include "core/village_trade_intel.h"

/* Settlement tiles an automated Scout already entered, bit = nation; y * 256 + x. */
static uint8_t s_visited[256 * 256];

#define SCOUT_AUTO_PAYLOAD_VERSION 1u
#define SCOUT_AUTO_GOTO_TRIES 8
/* Rumours and unvisited settlements win over a frontier tile this much closer. */
#define SCOUT_AUTO_POI_BONUS 4

bool scout_auto_enabled(void) {
  return settings_get()->scout_autoexplore;
}

bool scout_auto_is_scout(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  return pool && u && units_type_is_scout(units_type(pool, u->type_index));
}

void scout_auto_reset(void) {
  memset(s_visited, 0, sizeof(s_visited));
}

void scout_auto_note_village(int nation, int x, int y) {
  if (nation >= 0 && nation <= 3 && x >= 0 && y >= 0 && x <= 255 && y <= 255) {
    s_visited[y * 256 + x] |= (uint8_t)(1u << nation);
  }
}

static bool scout_auto_hostile(const ColonizeWorld* w, int nation, int other) {
  if (other == nation) {
    return false;
  }
  if (!w->col1) {
    return true;
  }
  if (other >= 4 && other <= 11) {
    return ai_diplo_indian_at_war(w->col1, nation, other - 4) != 0;
  }
  return other < 0 || other > 3 || ai_diplo_at_war(w->col1, nation, other) != 0;
}

/* A foreign unit or colony on (x,y), or a hostile unit next to it. */
static bool scout_auto_unsafe(const ColonizeWorld* w, const ColonizeUnit* u, int x, int y) {
  const int cid = w->colonies ? colonies_id_at(w->colonies, x, y) : -1;
  const ColonizeColony* col = cid >= 0 ? colonies_get(w->colonies, cid) : NULL;
  if (col && col->active && col->nation_id != u->nation_id) {
    return true;
  }
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      const int occ = units_id_at(w->units, x + dx, y + dy);
      const ColonizeUnit* o = occ >= 0 ? units_get_const(w->units, occ) : NULL;
      if (!o || o->nation_id == u->nation_id) {
        continue;
      }
      if ((dx == 0 && dy == 0) || scout_auto_hostile(w, u->nation_id, o->nation_id)) {
        return true;
      }
    }
  }
  return false;
}

static bool scout_auto_frontier(const ColonizeWorldMap* map, int nation, int x, int y) {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (map_in_bounds(map, x + dx, y + dy) && !map_tile_seen_by(map, x + dx, y + dy, nation)) {
        return true;
      }
    }
  }
  return false;
}

/* A settlement on (x,y) worth a visit: not yet entered by automation, its
 * skill still unknown to the nation (the chief never spoke to it), tribe not
 * at war. -1 = no settlement there, 0 = one not worth it, 1 = worth it. */
static int scout_auto_village(const ColonizeWorld* w, int nation, int x, int y) {
  const ColonizeCol1Save* col1 = w->col1;
  for (uint16_t i = 0; col1 && col1->tribe && i < col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &col1->tribe[i];
    if (t->x != x || t->y != y) {
      continue;
    }
    int skill = 0;
    return !(s_visited[y * 256 + x] & (1u << nation)) &&
           !village_trade_intel_get_skill(nation, x, y, &skill) &&
           !ai_diplo_indian_at_war(col1, nation, (int)t->nation_id - 4);
  }
  return -1;
}

static void scout_auto_cancel(ColonizeUnitPool* pool, ColonizeUnit* u) {
  u->automated = false;
  if (u->orders == UNITS_ORDER_GOTO) {
    units_clear_orders(pool, u->id);
  }
}

void scout_auto_step(const ColonizeWorld* w, ColonizeUnit* u) {
  if (!w || !w->map || !w->units || !u || !u->automated || !scout_auto_is_scout(w->units, u)) {
    return;
  }
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  if (!scout_auto_enabled() || u->nation_id < 0 || u->nation_id > 3) {
    scout_auto_cancel(pool, u);
    return;
  }
  if (!units_is_on_map(u)) {
    return; /* aboard a ship: resumes once landed */
  }
  const int nation = u->nation_id;
  const int continent = map_continent_id_at(map, u->x, u->y);
  int tried_x[SCOUT_AUTO_GOTO_TRIES];
  int tried_y[SCOUT_AUTO_GOTO_TRIES];
  for (int attempt = 0; attempt < SCOUT_AUTO_GOTO_TRIES; ++attempt) {
    /* ponytail: full-map scan per decision; fine at DOS map sizes. */
    int best_x = -1;
    int best_y = -1;
    int best_score = 1 << 30;
    for (int y = 0; y < (int)map->height; ++y) {
      for (int x = 0; x < (int)map->width; ++x) {
        if ((x == u->x && y == u->y) || !map_tile_seen_by(map, x, y, nation) ||
            !map_tile_is_land(map, x, y) || map_continent_id_at(map, x, y) != continent) {
          continue;
        }
        const int d = map_dos_dist(x - u->x, y - u->y);
        if (d - SCOUT_AUTO_POI_BONUS >= best_score) {
          continue;
        }
        int score;
        if (map_tile_has_city(map, x, y)) {
          if (scout_auto_village(w, nation, x, y) != 1) {
            continue;
          }
          score = d - SCOUT_AUTO_POI_BONUS;
        } else if (map_tile_has_rumour(map, x, y)) {
          score = d - SCOUT_AUTO_POI_BONUS;
        } else if (scout_auto_frontier(map, nation, x, y)) {
          score = d;
        } else {
          continue;
        }
        bool tried = false;
        for (int t = 0; t < attempt; ++t) {
          tried = tried || (tried_x[t] == x && tried_y[t] == y);
        }
        if (score >= best_score || tried || scout_auto_unsafe(w, u, x, y)) {
          continue;
        }
        best_x = x;
        best_y = y;
        best_score = score;
      }
    }
    if (best_x < 0) {
      scout_auto_cancel(pool, u);
      return;
    }
    if (u->orders == UNITS_ORDER_GOTO && u->goto_x == best_x && u->goto_y == best_y) {
      return;
    }
    /* Path probe without the rng: the anti-backtrack reroll would draw. */
    ColonizeWorld probe = *w;
    probe.rng = NULL;
    int nx = 0;
    int ny = 0;
    if (units_set_goto_w(w, u->id, best_x, best_y) &&
        units_next_goto_step_w(&probe, u->id, &nx, &ny)) {
      return;
    }
    if (u->orders == UNITS_ORDER_GOTO) {
      units_clear_orders(pool, u->id); /* no path: try the next target */
    }
    tried_x[attempt] = best_x;
    tried_y[attempt] = best_y;
  }
  scout_auto_cancel(pool, u);
}

bool scout_auto_automate(const ColonizeWorld* w, ColonizeUnit* u) {
  if (!w || !u || !scout_auto_enabled() || !scout_auto_is_scout(w->units, u)) {
    return false;
  }
  if (u->orders != UNITS_ORDER_NONE) {
    units_clear_orders(w->units, u->id);
  }
  u->automated = true;
  scout_auto_step(w, u);
  return u->automated;
}

/* Payload: u8 version, u8 0, u16 automated-Scout count, (u8 x, u8 y, u8 nation)
 * per unit (keyed by tile like PPLN), u16 visited count, (u8 x, u8 y, u8
 * nation mask) per settlement tile. */
uint8_t* scout_auto_serialize(const ColonizeUnitPool* units, size_t* out_size) {
  *out_size = 0;
  if (!scout_auto_enabled()) {
    return NULL;
  }
  int autos = 0;
  for (int i = 0; units && i < units_slot_end(units); ++i) {
    const ColonizeUnit* u = &units->units[i];
    autos += u->active && u->automated && scout_auto_is_scout(units, u);
  }
  int visited = 0;
  for (int i = 0; i < 256 * 256; ++i) {
    visited += s_visited[i] != 0;
  }
  if (autos == 0 && visited == 0) {
    return NULL;
  }
  const size_t size = 2u + 2u + 3u * (size_t)autos + 2u + 3u * (size_t)visited;
  uint8_t* out = malloc(size);
  if (!out) {
    return NULL;
  }
  uint8_t* p = out;
  *p++ = SCOUT_AUTO_PAYLOAD_VERSION;
  *p++ = 0;
  *p++ = (uint8_t)(autos & 0xff);
  *p++ = (uint8_t)(autos >> 8);
  for (int i = 0; units && i < units_slot_end(units); ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (u->active && u->automated && scout_auto_is_scout(units, u)) {
      *p++ = (uint8_t)u->x;
      *p++ = (uint8_t)u->y;
      *p++ = (uint8_t)u->nation_id;
    }
  }
  *p++ = (uint8_t)(visited & 0xff);
  *p++ = (uint8_t)(visited >> 8);
  for (int i = 0; i < 256 * 256; ++i) {
    if (s_visited[i]) {
      *p++ = (uint8_t)(i & 0xff);
      *p++ = (uint8_t)(i >> 8);
      *p++ = s_visited[i];
    }
  }
  *out_size = size;
  return out;
}

void scout_auto_deserialize(const uint8_t* data, size_t size, ColonizeUnitPool* units) {
  scout_auto_reset();
  if (!data || size < 4 || data[0] != SCOUT_AUTO_PAYLOAD_VERSION || data[1] != 0) {
    return;
  }
  const int autos = data[2] | (data[3] << 8);
  const uint8_t* p = data + 4;
  if (size < 4u + 3u * (size_t)autos + 2u) {
    return;
  }
  for (int k = 0; k < autos; ++k, p += 3) {
    /* ponytail: two Scouts sharing a tile may swap which one is automated. */
    for (int i = 0; units && i < units_slot_end(units); ++i) {
      ColonizeUnit* u = &units->units[i];
      if (u->active && !u->automated && u->x == p[0] && u->y == p[1] && u->nation_id == p[2] &&
          scout_auto_is_scout(units, u)) {
        u->automated = true;
        break;
      }
    }
  }
  const int visited = p[0] | (p[1] << 8);
  p += 2;
  if (size < (size_t)(p - data) + 3u * (size_t)visited) {
    return;
  }
  for (int k = 0; k < visited; ++k, p += 3) {
    s_visited[p[1] * 256 + p[0]] = p[2];
  }
}
