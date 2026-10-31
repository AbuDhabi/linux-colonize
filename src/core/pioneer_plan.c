#include "core/pioneer_plan.h"

#include <stdlib.h>
#include <string.h>

#include "core/ai_contact.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/explore_auto.h"
#include "core/settings.h"
#include "core/units.h"
#include "core/units_move.h"

/* Indexed y * 256 + x: map dimensions are bytes, so no allocation to track. */
static uint8_t s_plan[256 * 256];

#define PIONEER_PLAN_PAYLOAD_VERSION 1u
#define PIONEER_PLAN_TOOLS_MIN 20

bool pioneer_plan_enabled(void) {
  return settings_get()->pioneer_plan;
}

void pioneer_plan_reset(void) {
  memset(s_plan, 0, sizeof(s_plan));
}

uint8_t pioneer_plan_at(int x, int y) {
  if (x < 0 || y < 0 || x > 255 || y > 255) {
    return 0;
  }
  return s_plan[y * 256 + x];
}

static void pioneer_plan_set(int x, int y, uint8_t bits) {
  s_plan[y * 256 + x] = bits;
}

/* The live answer, explored-ness aside (reconcile only asks about tiles in sight). */
static uint8_t pioneer_plan_live_bits(const ColonizeWorld* w, int nation, int x, int y) {
  const ColonizeWorldMap* map = w->map;
  if (!map || !map_in_bounds(map, x, y) || !map_tile_is_land(map, x, y) ||
      map_tile_is_high_seas(map, x, y)) {
    return 0;
  }
  const int cid = w->colonies ? colonies_id_at(w->colonies, x, y) : -1;
  const ColonizeColony* col = cid >= 0 ? colonies_get(w->colonies, cid) : NULL;
  const bool own_colony = col && col->nation_id == nation;
  if (map_tile_has_city(map, x, y) && !own_colony) {
    return 0; /* native settlement / rival colony */
  }
  uint8_t bits = 0;
  bool clearing = false;
  if (units_pioneer_tile_can_clear_or_plow(map, x, y, &clearing)) {
    bits |= clearing ? (PIONEER_PLAN_CLEAR | PIONEER_PLAN_PLOW) : PIONEER_PLAN_PLOW;
  }
  if (!map_tile_has_road(map, x, y) && !map_tile_has_city(map, x, y) && !col) {
    bits |= PIONEER_PLAN_ROAD;
  }
  return bits;
}

uint8_t pioneer_plan_valid_bits(const ColonizeWorld* w, int nation, int x, int y) {
  if (!w || !w->map || !map_in_bounds(w->map, x, y) || !map_tile_seen_by(w->map, x, y, nation)) {
    return 0;
  }
  return pioneer_plan_live_bits(w, nation, x, y);
}

bool pioneer_plan_toggle(const ColonizeWorld* w, int nation, int x, int y, uint8_t bit) {
  const uint8_t valid = pioneer_plan_valid_bits(w, nation, x, y);
  if (!(valid & bit)) {
    return false;
  }
  uint8_t cur = pioneer_plan_at(x, y) & valid;
  if (bit == PIONEER_PLAN_CLEAR) {
    if (!(cur & PIONEER_PLAN_CLEAR)) {
      cur |= PIONEER_PLAN_CLEAR;
    } else if (!(cur & PIONEER_PLAN_PLOW)) {
      cur |= PIONEER_PLAN_PLOW;
    } else {
      cur &= (uint8_t)~(PIONEER_PLAN_CLEAR | PIONEER_PLAN_PLOW);
    }
  } else {
    cur ^= bit;
  }
  pioneer_plan_set(x, y, cur);
  return true;
}

static void pioneer_plan_reconcile_around(const ColonizeWorld* w, int nation, int cx, int cy, int r) {
  for (int y = cy - r; y <= cy + r; ++y) {
    for (int x = cx - r; x <= cx + r; ++x) {
      const uint8_t plan = pioneer_plan_at(x, y);
      if (plan) {
        pioneer_plan_set(x, y, plan & pioneer_plan_live_bits(w, nation, x, y));
      }
    }
  }
}

void pioneer_plan_reconcile_seen(const ColonizeWorld* w, int nation) {
  if (!w || !w->map || nation < 0 || nation > 3) {
    return;
  }
  if (w->units) {
    for (int i = 0; i < units_slot_end(w->units); ++i) {
      const ColonizeUnit* u = &w->units->units[i];
      if (u->active && u->nation_id == nation && units_is_on_map(u)) {
        pioneer_plan_reconcile_around(w, nation, u->x, u->y, units_sight_radius(w->units, u, w->col1));
      }
    }
  }
  if (w->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &w->colonies->colonies[i];
      if (c->active && c->nation_id == nation) {
        pioneer_plan_reconcile_around(w, nation, c->x, c->y, 1);
      }
    }
  }
}

/* Next job on (x,y), in Road -> Clear -> Plow order; 0 = nothing to do there. */
static uint8_t pioneer_plan_job(const ColonizeWorld* w, int nation, int x, int y) {
  const uint8_t plan = pioneer_plan_at(x, y);
  if (!plan) {
    return 0;
  }
  const uint8_t live = pioneer_plan_live_bits(w, nation, x, y);
  if ((plan & PIONEER_PLAN_ROAD) && (live & PIONEER_PLAN_ROAD)) {
    return PIONEER_PLAN_ROAD;
  }
  if ((plan & (PIONEER_PLAN_CLEAR | PIONEER_PLAN_PLOW)) && (live & PIONEER_PLAN_CLEAR)) {
    return PIONEER_PLAN_CLEAR; /* forest: plowing waits for the clear */
  }
  if ((plan & PIONEER_PLAN_PLOW) && (live & PIONEER_PLAN_PLOW)) {
    return PIONEER_PLAN_PLOW;
  }
  return 0;
}

/* Clear and Road on tribal land raise the @INDIANFOREST / @INDIANROAD choice
 * (game_pioneer_land_gate); automation never answers it, so it leaves those
 * tiles for the player. */
static bool pioneer_plan_job_needs_player(const ColonizeWorld* w, int nation, int x, int y, uint8_t job) {
  /* Same tests as game_request_indian_land_choice: a tribe at peace owns the land
   * and still wants paying for it. */
  if (job == PIONEER_PLAN_PLOW || !w->col1 || !w->col1->tribe) {
    return false;
  }
  const int ti = colonies_indian_land_owner_tribe(w->col1, w->map, x, y);
  if (ti < 0) {
    return false;
  }
  const int tribe_nation = (int)w->col1->tribe[ti].nation_id;
  return tribe_nation >= 4 && tribe_nation <= 11 &&
         ai_contact_indian_has_peace(w->col1, tribe_nation, nation) &&
         colonies_indian_land_purchase_gold(w->col1, w->map, x, y, nation) > 0;
}

/* Another automated Pioneer of the nation already working or heading there. */
static bool pioneer_plan_claimed(const ColonizeUnitPool* pool, const ColonizeUnit* self, int x, int y) {
  for (int i = 0; i < units_slot_end(pool); ++i) {
    const ColonizeUnit* o = &pool->units[i];
    if (o == self || !o->active || !o->automated || o->nation_id != self->nation_id ||
        !units_is_pioneer(pool, o->id)) {
      continue;
    }
    if ((o->orders == UNITS_ORDER_GOTO && o->goto_x == x && o->goto_y == y) ||
        ((o->orders == UNITS_ORDER_CLEAR_PLOW || o->orders == UNITS_ORDER_BUILD_ROAD) &&
         o->x == x && o->y == y)) {
      return true;
    }
  }
  return false;
}

static bool pioneer_plan_is_pioneer(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  return units_is_pioneer(pool, u->id);
}

#define PIONEER_PLAN_GOTO_TRIES 8

bool pioneer_plan_auto_step(
  const ColonizeWorld* w, ColonizeUnit* u, AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!w || !w->map || !w->units || !u || !u->automated || explore_auto_is_explorer(w->units, u)) {
    return false; /* an automated Scout / ship is explore_auto_step's */
  }
  if (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD) {
    return false; /* mid-job: the lasting-order tick runs it */
  }
  ColonizeUnitPool* pool = w->units;
  ColonizeWorldMap* map = w->map;
  if (!pioneer_plan_enabled() || !units_is_pioneer(pool, u->id) ||
      u->tools < PIONEER_PLAN_TOOLS_MIN) {
    units_automate_cancel(pool, u);
    return false;
  }
  const int continent = map_continent_id_at(map, u->x, u->y);
  int tried_x[PIONEER_PLAN_GOTO_TRIES];
  int tried_y[PIONEER_PLAN_GOTO_TRIES];
  for (int attempt = 0; attempt < PIONEER_PLAN_GOTO_TRIES; ++attempt) {
    /* ponytail: full-map scan per decision (~4k tiles); fine at DOS map sizes. */
    int best_x = -1;
    int best_y = -1;
    int best_d = 1 << 30;
    uint8_t best_job = 0;
    for (int y = 0; y < (int)map->height; ++y) {
      for (int x = 0; x < (int)map->width; ++x) {
        if (!pioneer_plan_at(x, y)) {
          continue;
        }
        const int d = map_dos_dist(x - u->x, y - u->y);
        if (d >= best_d || map_continent_id_at(map, x, y) != continent) {
          continue;
        }
        bool tried = false;
        for (int t = 0; t < attempt; ++t) {
          tried = tried || (tried_x[t] == x && tried_y[t] == y);
        }
        const uint8_t job = pioneer_plan_job(w, u->nation_id, x, y);
        if (tried || !job || pioneer_plan_job_needs_player(w, u->nation_id, x, y, job) ||
            pioneer_plan_claimed(pool, u, x, y)) {
          continue;
        }
        best_x = x;
        best_y = y;
        best_d = d;
        best_job = job;
      }
    }
    if (best_x < 0) {
      units_automate_cancel(pool, u);
      return false;
    }
    if (best_x == u->x && best_y == u->y) {
      if (u->orders != UNITS_ORDER_NONE) {
        units_clear_orders(pool, u->id);
      }
      const bool ok = best_job == PIONEER_PLAN_ROAD
        ? units_pioneer_road_w(w, u->id, NULL, ai_popups, messages)
        : units_pioneer_plow_w(w, u->id, NULL, ai_popups, messages);
      if (ok) {
        return true;
      }
      /* The body refused what the plan asked for: forget that job. */
      pioneer_plan_set(best_x, best_y, pioneer_plan_at(best_x, best_y) & (uint8_t)~best_job);
    } else if (u->orders == UNITS_ORDER_GOTO && u->goto_x == best_x && u->goto_y == best_y) {
      return false;
    } else if (units_set_goto_w(w, u->id, best_x, best_y)) {
      return false;
    }
    tried_x[attempt] = best_x;
    tried_y[attempt] = best_y;
  }
  units_automate_cancel(pool, u);
  return false;
}

bool pioneer_plan_automate(
  const ColonizeWorld* w, ColonizeUnit* u, AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!w || !u || !pioneer_plan_enabled() || !units_is_pioneer(w->units, u->id) ||
      u->tools < PIONEER_PLAN_TOOLS_MIN) {
    return false;
  }
  if (u->orders != UNITS_ORDER_NONE) {
    units_clear_orders(w->units, u->id);
  }
  u->automated = true;
  (void)pioneer_plan_auto_step(w, u, ai_popups, messages);
  return u->automated;
}

/* Payload: u16 version, u8 width, u8 height, width*height plan bytes,
 * u16 automated-unit count, then (u8 x, u8 y, u8 nation) per unit. Units are
 * keyed by tile like BQUE keys colonies: record indices move on a save. */
uint8_t* pioneer_plan_serialize(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, size_t* out_size
) {
  *out_size = 0;
  if (!map || !pioneer_plan_enabled()) {
    return NULL;
  }
  const int w = map->width;
  const int h = map->height;
  int autos = 0;
  for (int i = 0; units && i < units_slot_end(units); ++i) {
    autos += units->units[i].active && units->units[i].automated &&
             units_is_pioneer(units, units->units[i].id);
  }
  const size_t size = 4u + (size_t)w * (size_t)h + 2u + 3u * (size_t)autos;
  uint8_t* out = malloc(size);
  if (!out) {
    return NULL;
  }
  uint8_t* p = out;
  *p++ = PIONEER_PLAN_PAYLOAD_VERSION & 0xff;
  *p++ = 0;
  *p++ = (uint8_t)w;
  *p++ = (uint8_t)h;
  for (int y = 0; y < h; ++y) {
    memcpy(p, &s_plan[y * 256], (size_t)w);
    p += w;
  }
  *p++ = (uint8_t)(autos & 0xff);
  *p++ = (uint8_t)(autos >> 8);
  for (int i = 0; units && i < units_slot_end(units); ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (u->active && u->automated && units_is_pioneer(units, u->id)) {
      *p++ = (uint8_t)u->x;
      *p++ = (uint8_t)u->y;
      *p++ = (uint8_t)u->nation_id;
    }
  }
  *out_size = size;
  return out;
}

void pioneer_plan_deserialize(const uint8_t* data, size_t size, ColonizeUnitPool* units) {
  pioneer_plan_reset();
  if (!data || size < 4 || data[0] != PIONEER_PLAN_PAYLOAD_VERSION || data[1] != 0) {
    return;
  }
  const int w = data[2];
  const int h = data[3];
  const size_t grid = (size_t)w * (size_t)h;
  if (size < 4u + grid + 2u) {
    return;
  }
  for (int y = 0; y < h; ++y) {
    memcpy(&s_plan[y * 256], data + 4 + (size_t)y * (size_t)w, (size_t)w);
  }
  const uint8_t* p = data + 4 + grid;
  const int autos = p[0] | (p[1] << 8);
  p += 2;
  if (!units || size < 4u + grid + 2u + 3u * (size_t)autos) {
    return;
  }
  for (int k = 0; k < autos; ++k, p += 3) {
    units_automate_reattach(units, p[0], p[1], p[2], pioneer_plan_is_pioneer);
  }
}
