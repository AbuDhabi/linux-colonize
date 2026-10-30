#include "core/explore_auto.h"

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

#define EXPLORE_AUTO_PAYLOAD_VERSION 1u
#define EXPLORE_AUTO_GOTO_TRIES 8
/* Rumours and unvisited settlements win over a frontier tile this much closer. */
#define EXPLORE_AUTO_POI_BONUS 4

static bool explore_auto_is_ship(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  return pool && u && units_is_sea(pool, u->id);
}

static bool explore_auto_is_scout(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  return pool && u && units_type_is_scout(units_type(pool, u->type_index));
}

bool explore_auto_is_explorer(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  return explore_auto_is_scout(pool, u) || explore_auto_is_ship(pool, u);
}

bool explore_auto_allowed(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  const ColonizeSettings* s = settings_get();
  return (s->scout_autoexplore && explore_auto_is_scout(pool, u)) ||
         (s->ship_autoexplore && explore_auto_is_ship(pool, u));
}

void explore_auto_reset(void) {
  memset(s_visited, 0, sizeof(s_visited));
}

void explore_auto_note_village(int nation, int x, int y) {
  if (nation >= 0 && nation <= 3 && x >= 0 && y >= 0 && x <= 255 && y <= 255) {
    s_visited[y * 256 + x] |= (uint8_t)(1u << nation);
  }
}

static bool explore_auto_hostile(const ColonizeWorld* w, int nation, int other) {
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
static bool explore_auto_unsafe(const ColonizeWorld* w, const ColonizeUnit* u, int x, int y) {
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
      if ((dx == 0 && dy == 0) || explore_auto_hostile(w, u->nation_id, o->nation_id)) {
        return true;
      }
    }
  }
  return false;
}

static bool explore_auto_ocean(const ColonizeWorldMap* map, int x, int y) {
  return !map_tile_is_land(map, x, y) && !map_tile_is_lake(map, x, y);
}

/* Unexplored ground next to (x,y); for a ship only unexplored ocean counts,
 * which it can actually go and see. The 1-tile map rim is never revealed by
 * sight (map_sight_each), so it is not unexplored ground. */
static bool explore_auto_frontier(const ColonizeWorldMap* map, int nation, int x, int y, bool ship) {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      const int nx = x + dx;
      const int ny = y + dy;
      if (map_coords_inset(map, nx, ny) && !map_tile_seen_by(map, nx, ny, nation) &&
          (!ship || explore_auto_ocean(map, nx, ny))) {
        return true;
      }
    }
  }
  return false;
}

/* A settlement on (x,y) worth a visit: not yet entered by automation, its
 * skill still unknown to the nation (the "visited by the player's Scout"
 * proxy, docs/explore_auto.md), tribe not at war. The DOS scouted bit is shared
 * by all nations and hidden from the player, so it is not read.
 * -1 = no settlement there, 0 = one not worth it, 1 = worth it. */
static int explore_auto_village(const ColonizeWorld* w, int nation, int x, int y) {
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

typedef struct ExploreAutoGain {
  const ColonizeWorldMap* map;
  int nation;
  int unseen;
} ExploreAutoGain;

static void explore_auto_gain_tile(void* ctx, int x, int y, bool outer) {
  ExploreAutoGain* g = ctx;
  (void)outer;
  g->unseen += !map_tile_seen_by(g->map, x, y, g->nation);
}

/* Fog a ship's sight would lift standing on (x,y): the same tiles map_sight_each reveals. */
static int explore_auto_ship_gain(const ColonizeWorldMap* map, int nation, int x, int y, int radius) {
  ExploreAutoGain g = {map, nation, 0};
  map_sight_each(map, x, y, radius, true, explore_auto_gain_tile, &g);
  return g.unseen;
}

static void explore_auto_cancel(ColonizeUnitPool* pool, ColonizeUnit* u) {
  u->automated = false;
  if (u->orders == UNITS_ORDER_GOTO) {
    units_clear_orders(pool, u->id);
  }
}

void explore_auto_step(const ColonizeWorld* w, ColonizeUnit* u) {
  if (!w || !w->map || !w->units || !u || !u->automated || !explore_auto_is_explorer(w->units, u)) {
    return;
  }
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  if (!explore_auto_allowed(pool, u) || u->nation_id < 0 || u->nation_id > 3) {
    explore_auto_cancel(pool, u);
    return;
  }
  if (!units_is_on_map(u)) {
    return; /* aboard a ship / in Europe: resumes once back on the map */
  }
  const bool ship = explore_auto_is_ship(pool, u);
  const int sight = ship ? units_sight_radius(pool, u, w->col1) : 0;
  const int nation = u->nation_id;
  const int continent = map_continent_id_at(map, u->x, u->y);
  int tried_x[EXPLORE_AUTO_GOTO_TRIES];
  int tried_y[EXPLORE_AUTO_GOTO_TRIES];
  for (int attempt = 0; attempt < EXPLORE_AUTO_GOTO_TRIES; ++attempt) {
    /* ponytail: full-map scan per decision; fine at DOS map sizes. */
    int best_x = -1;
    int best_y = -1;
    int best_score = 1 << 30;
    for (int y = 0; y < (int)map->height; ++y) {
      for (int x = 0; x < (int)map->width; ++x) {
        if ((x == u->x && y == u->y) || !map_tile_seen_by(map, x, y, nation)) {
          continue;
        }
        if (ship ? !explore_auto_ocean(map, x, y)
                 : (!map_tile_is_land(map, x, y) || map_continent_id_at(map, x, y) != continent)) {
          continue;
        }
        const int d = map_dos_dist(x - u->x, y - u->y);
        if (!ship && d - EXPLORE_AUTO_POI_BONUS >= best_score) {
          continue;
        }
        int score;
        if (ship) {
          if (!explore_auto_frontier(map, nation, x, y, true)) {
            continue;
          }
          /* Most fog lifted per tile sailed: distance per revealed tile, x256. */
          const int gain = explore_auto_ship_gain(map, nation, x, y, sight);
          if (gain <= 0) {
            continue;
          }
          score = d * 256 / gain;
        } else if (map_tile_has_city(map, x, y)) {
          if (explore_auto_village(w, nation, x, y) != 1) {
            continue;
          }
          score = d - EXPLORE_AUTO_POI_BONUS;
        } else if (map_tile_has_rumour(map, x, y)) {
          score = d - EXPLORE_AUTO_POI_BONUS;
        } else if (explore_auto_frontier(map, nation, x, y, false)) {
          score = d;
        } else {
          continue;
        }
        bool tried = false;
        for (int t = 0; t < attempt; ++t) {
          tried = tried || (tried_x[t] == x && tried_y[t] == y);
        }
        if (score >= best_score || tried || explore_auto_unsafe(w, u, x, y)) {
          continue;
        }
        best_x = x;
        best_y = y;
        best_score = score;
      }
    }
    if (best_x < 0) {
      explore_auto_cancel(pool, u);
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
  explore_auto_cancel(pool, u);
}

bool explore_auto_automate(const ColonizeWorld* w, ColonizeUnit* u) {
  if (!w || !u || !explore_auto_allowed(w->units, u)) {
    return false;
  }
  if (u->orders != UNITS_ORDER_NONE) {
    units_clear_orders(w->units, u->id);
  }
  u->automated = true;
  explore_auto_step(w, u);
  return u->automated;
}

/* Payload: u8 version, u8 0, u16 automated-explorer count, (u8 x, u8 y, u8
 * nation | 0x80 for a ship) per unit (keyed by tile like PPLN), u16 visited count, (u8 x, u8 y, u8
 * nation mask) per settlement tile. */
uint8_t* explore_auto_serialize(const ColonizeUnitPool* units, size_t* out_size) {
  *out_size = 0;
  if (!settings_get()->scout_autoexplore && !settings_get()->ship_autoexplore) {
    return NULL;
  }
  int autos = 0;
  for (int i = 0; units && i < units_slot_end(units); ++i) {
    const ColonizeUnit* u = &units->units[i];
    autos += u->active && u->automated && explore_auto_is_explorer(units, u);
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
  *p++ = EXPLORE_AUTO_PAYLOAD_VERSION;
  *p++ = 0;
  *p++ = (uint8_t)(autos & 0xff);
  *p++ = (uint8_t)(autos >> 8);
  for (int i = 0; units && i < units_slot_end(units); ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (u->active && u->automated && explore_auto_is_explorer(units, u)) {
      *p++ = (uint8_t)u->x;
      *p++ = (uint8_t)u->y;
      *p++ = (uint8_t)(u->nation_id | (explore_auto_is_ship(units, u) ? 0x80 : 0));
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

void explore_auto_deserialize(const uint8_t* data, size_t size, ColonizeUnitPool* units) {
  explore_auto_reset();
  if (!data || size < 4 || data[0] != EXPLORE_AUTO_PAYLOAD_VERSION || data[1] != 0) {
    return;
  }
  const int autos = data[2] | (data[3] << 8);
  const uint8_t* p = data + 4;
  if (size < 4u + 3u * (size_t)autos + 2u) {
    return;
  }
  for (int k = 0; k < autos; ++k, p += 3) {
    /* ponytail: two explorers of one kind sharing a tile may swap which one is automated. */
    for (int i = 0; units && i < units_slot_end(units); ++i) {
      ColonizeUnit* u = &units->units[i];
      if (u->active && !u->automated && u->x == p[0] && u->y == p[1] &&
          u->nation_id == (p[2] & 0x7f) &&
          ((p[2] & 0x80) ? explore_auto_is_ship(units, u) : explore_auto_is_scout(units, u))) {
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
