/*
 * MOD_AI_IMPROVEMENTS (not DOS; docs/ai_euro_improvement_spec.md): the shared
 * gate and the pieces that have no DOS home.
 *
 *   - ai_euro_improved: the mod applies to this nation's Euro AI turn (the
 *     col1-level test is ai_euro_improved_gate.c, linked by slim tests too).
 *   - Pioneer jobs (4.2): the colony tick's improve and road arms post a
 *     (tile, kind) job per nation instead of running the phantom Pioneer; an
 *     AI Pioneer with tools walks to the nearest open job on its continent and
 *     does it with the player's Clear/Plow/Road bodies. Saved in RTST record 19.
 *   - Forest reserve (4.2, Q1): a colony never clears below the workable
 *     forests it needs for lumberjacks, nor a forest with a resource.
 *
 * Mod off: nothing here is reached and the AI is DOS.
 */
#include "core/internal.h"
#include "core/ai_euro.h"
#include "core/ai_euro_internal.h"
#include "core/ai_diplo.h"
#include "core/ai_king.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_governor.h"
#include "core/colony_production.h"
#include "core/map.h"
#include "core/mods.h"
#include "core/pioneer_plan.h"
#include "core/units.h"
#include "core/units_move.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool ai_euro_improved(const ColonizeTurnContext* ctx, int nation_id) {
  return ctx && ai_euro_improved_nation(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation, nation_id);
}

/* ===== Pioneer jobs ===== */

typedef struct {
  uint8_t x, y, kind; /* kind: one PIONEER_PLAN_* bit */
} AiPioneerJob;

static AiPioneerJob s_jobs[4][AI_EURO_PJOB_MAX];
static int s_job_count[4];

void ai_euro_pjob_reset(void) {
  memset(s_jobs, 0, sizeof(s_jobs));
  memset(s_job_count, 0, sizeof(s_job_count));
}

static void ai_euro_pjob_drop(int nation, int i) {
  s_jobs[nation][i] = s_jobs[nation][--s_job_count[nation]];
}

static uint8_t ai_euro_pjob_kind_at(int nation, int x, int y) {
  for (int i = 0; nation >= 0 && nation < 4 && i < s_job_count[nation]; ++i) {
    if (s_jobs[nation][i].x == x && s_jobs[nation][i].y == y) {
      return s_jobs[nation][i].kind;
    }
  }
  return 0;
}

bool ai_euro_pjob_listed(int nation, int x, int y) {
  return ai_euro_pjob_kind_at(nation, x, y) != 0;
}

void ai_euro_pjob_post(int nation, int x, int y, uint8_t kind) {
  if (nation < 0 || nation > 3 || x < 0 || y < 0 || x > 255 || y > 255 ||
      ai_euro_pjob_listed(nation, x, y) || s_job_count[nation] >= AI_EURO_PJOB_MAX) {
    return;
  }
  s_jobs[nation][s_job_count[nation]++] = (AiPioneerJob){(uint8_t)x, (uint8_t)y, kind};
  if (getenv("AI_IMPROVED_TRACE")) {
    fprintf(stderr, "[pjob] n%d post (%d,%d) kind %u, %d open\n", nation, x, y, kind, s_job_count[nation]);
  }
}

/* The job can still be done: a road where there is none (and no settlement),
 * a clear on forest, a plow on open unplowed land. */
static bool ai_euro_pjob_live(const ColonizeTurnContext* ctx, const AiPioneerJob* j) {
  const ColonizeWorldMap* map = ctx->map;
  if (!map_in_bounds(map, j->x, j->y) || !map_tile_is_land(map, j->x, j->y)) {
    return false;
  }
  if (j->kind == PIONEER_PLAN_ROAD) {
    return !map_tile_has_road(map, j->x, j->y) && !map_tile_has_city(map, j->x, j->y) &&
           (!ctx->colonies || colonies_id_at(ctx->colonies, j->x, j->y) < 0);
  }
  bool clearing = false;
  if (!units_pioneer_tile_can_clear_or_plow(map, j->x, j->y, &clearing)) {
    return false;
  }
  return clearing == (j->kind == PIONEER_PLAN_CLEAR);
}

int ai_euro_pjob_open_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  if (!ctx || !ctx->map || nation < 0 || nation > 3) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < s_job_count[nation]; ++i) {
    n += map_continent_id_at(ctx->map, s_jobs[nation][i].x, s_jobs[nation][i].y) == cid &&
         ai_euro_pjob_live(ctx, &s_jobs[nation][i]);
  }
  return n;
}

int ai_euro_pjob_open_near(const ColonizeTurnContext* ctx, int nation, int x, int y, int r) {
  if (!ctx || !ctx->map || nation < 0 || nation > 3) {
    return 0;
  }
  const int cid = map_continent_id_at(ctx->map, x, y);
  int n = 0;
  for (int i = 0; i < s_job_count[nation]; ++i) {
    const AiPioneerJob* j = &s_jobs[nation][i];
    n += map_dos_dist(j->x - x, j->y - y) <= r && map_continent_id_at(ctx->map, j->x, j->y) == cid &&
         ai_euro_pjob_live(ctx, j);
  }
  return n;
}

int ai_euro_pioneers_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  if (!ctx || !ctx->map || !ctx->units) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < units_slot_end(ctx->units); ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    n += u->active && u->nation_id == nation && units_is_on_map(u) &&
         u->tools >= UNITS_EQUIP_TOOLS_STEP && units_is_pioneer(ctx->units, u->id) &&
         map_continent_id_at(ctx->map, u->x, u->y) == cid;
  }
  return n;
}

int ai_euro_scouts_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  if (!ctx || !ctx->map || !ctx->units) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < units_slot_end(ctx->units); ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    n += u->active && u->nation_id == nation && units_is_on_map(u) &&
         units_type_kind(units_type(ctx->units, u->type_index)) == UNITS_KIND_SCOUT &&
         map_continent_id_at(ctx->map, u->x, u->y) == cid;
  }
  return n;
}

int ai_euro_missionaries_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  if (!ctx || !ctx->map || !ctx->units) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < units_slot_end(ctx->units); ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    n += u->active && u->nation_id == nation && units_is_on_map(u) &&
         units_type_is_missionary(units_type(ctx->units, u->type_index)) &&
         map_continent_id_at(ctx->map, u->x, u->y) == cid;
  }
  return n;
}

/* A village on (x, y)'s continent within `r` tiles with no mission and no own
 * Missionary already walking to it. */
bool ai_euro_mission_site_near(const ColonizeTurnContext* ctx, int nation, int x, int y, int r) {
  if (!ctx || !ctx->map || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return false;
  }
  const int cid = map_continent_id_at(ctx->map, x, y);
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if (t->mission == COL1_TRIBE_MISSION_NONE && t->nation_id >= 4 && t->nation_id <= 11 &&
        map_dos_dist(t->x - x, t->y - y) <= r && map_continent_id_at(ctx->map, t->x, t->y) == cid &&
        !ai_euro_mission_claimed(ctx->units, nation, t->x, t->y)) {
      return true;
    }
  }
  return false;
}

bool ai_euro_mission_claimed(const ColonizeUnitPool* pool, int nation, int x, int y) {
  for (int i = 0; i < units_slot_end(pool); ++i) {
    const ColonizeUnit* o = &pool->units[i];
    if (o->active && o->nation_id == nation && o->goto_x == x && o->goto_y == y &&
        units_type_is_missionary(units_type(pool, o->type_index))) {
      return true;
    }
  }
  return false;
}

size_t ai_euro_pjob_export(uint8_t* out, size_t cap) {
  size_t n = 0;
  for (int nation = 0; nation < 4; ++nation) {
    for (int i = 0; i < s_job_count[nation] && n + 4 <= cap; ++i) {
      out[n++] = (uint8_t)nation;
      out[n++] = s_jobs[nation][i].x;
      out[n++] = s_jobs[nation][i].y;
      out[n++] = s_jobs[nation][i].kind;
    }
  }
  return n;
}

void ai_euro_pjob_import(const uint8_t* in, size_t len) {
  ai_euro_pjob_reset();
  for (size_t k = 0; k + 4 <= len; k += 4) {
    ai_euro_pjob_post(in[k], in[k + 1], in[k + 2], in[k + 3]);
  }
}

/* Another unit of the nation is already working (x,y) or walking there. */
static bool ai_euro_pjob_claimed(const ColonizeUnitPool* pool, const ColonizeUnit* self, int x, int y) {
  for (int i = 0; i < units_slot_end(pool); ++i) {
    const ColonizeUnit* o = &pool->units[i];
    if (o == self || !o->active || o->nation_id != self->nation_id) {
      continue;
    }
    /* #1871: any Pioneer headed there holds the claim, whatever its orders
     * (a Go To leg clears AI_EURO_ACT_GOAL and two Pioneers met on one tile). */
    if ((o->goto_x == x && o->goto_y == y && units_is_pioneer(pool, o->id)) ||
        ((o->orders == UNITS_ORDER_CLEAR_PLOW || o->orders == UNITS_ORDER_BUILD_ROAD) &&
         o->x == x && o->y == y)) {
      return true;
    }
  }
  return false;
}

/* MOD_AI_IMPROVEMENTS (2026-11-16 expansion gap): while a nation holds fewer
 * than this many colonies on a continent, a Pioneer between jobs is a founder
 * first. DOS lets Pioneers found through the 20e6 explore scan; holding every
 * Pioneer on the job list cut pt5 Pioneer founders from 12 to 3. */
#define AI_EURO_PIONEER_EXPANSION_COLONIES 3

/* True when the Pioneer skips the job list this turn and takes the DOS path:
 * not mid-job, not on a colony tile, and either its continent is still in the
 * expansion phase or the 20e6 ring scan finds a site (2026-11-17: in DOS the
 * nation founds with Pioneers; held on jobs, pt6 France fell 101 -> 77). */
bool ai_euro_pioneer_released(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->map || !ctx->colonies || !u || u->nation_id < 0 || u->nation_id > 3) {
    return false;
  }
  if ((u->orders == AI_EURO_ACT_GOAL && ai_euro_pjob_listed(u->nation_id, u->goto_x, u->goto_y)) ||
      u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD ||
      (u->col1_counter16 > 0 && ai_euro_pjob_listed(u->nation_id, u->x, u->y)) ||
      colonies_id_at(ctx->colonies, u->x, u->y) >= 0) {
    return false;
  }
  const int cid = map_continent_id_at(ctx->map, u->x, u->y);
  int own = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    own += c->active && c->nation_id == u->nation_id && map_continent_id_at(ctx->map, c->x, c->y) == cid;
  }
  return own < AI_EURO_PIONEER_EXPANSION_COLONIES || ai_euro_20e6_site_probe(ctx, u, u->nation_id) != 0;
}

/* #1876: a unit of a Euro at war with `nation` or of a tribe alarmed past
 * 0x4a stands next to (x, y) (pt7h: 13 English and 30 Dutch Pioneers died
 * walking to or working jobs). */
static bool ai_euro_pjob_threatened(const ColonizeTurnContext* ctx, int nation, int x, int y) {
  if (!ctx->col1_ok || !ctx->col1) {
    return false;
  }
  for (int d = 0; d < 8; ++d) {
    const int uid = units_id_at(ctx->units, x + MAP_DIR8_DX[d], y + MAP_DIR8_DY[d]);
    const ColonizeUnit* o = uid >= 0 ? units_get_const(ctx->units, uid) : NULL;
    if (!o || o->nation_id == nation || o->nation_id < 0) {
      continue;
    }
    if (o->nation_id < 4 ? (ctx->col1->nation[nation].euro_relation[o->nation_id] & AI_DIPLO_WAR) != 0
                         : o->nation_id <= 11 && ai_diplo_indian_alarm(ctx->col1, o->nation_id, nation) > 0x4a) {
      return true;
    }
  }
  return false;
}

int ai_euro_pioneer_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units || !u || !u->active || units_is_sea(ctx->units, u->id) ||
      !units_is_on_map(u) || !units_is_pioneer(ctx->units, u->id) ||
      u->tools < UNITS_EQUIP_TOOLS_STEP || u->orders == UNITS_ORDER_CLEAR_PLOW ||
      u->orders == UNITS_ORDER_BUILD_ROAD || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (ai_euro_pioneer_released(ctx, u)) {
    return 0;
  }
  const int cid = map_continent_id_at(ctx->map, u->x, u->y);
  int best = -1;
  int best_d = 1 << 30;
  for (int i = 0; i < s_job_count[nation_id]; ++i) {
    const AiPioneerJob* j = &s_jobs[nation_id][i];
    if (!ai_euro_pjob_live(ctx, j)) {
      ai_euro_pjob_drop(nation_id, i--);
      continue;
    }
    const int d = map_dos_dist(j->x - u->x, j->y - u->y);
    if (d < best_d && map_continent_id_at(ctx->map, j->x, j->y) == cid &&
        !ai_euro_pjob_claimed(ctx->units, u, j->x, j->y) &&
        !ai_euro_pjob_threatened(ctx, nation_id, j->x, j->y)) {
      best = i;
      best_d = d;
    }
  }
  const bool trace = getenv("AI_IMPROVED_TRACE") != NULL;
  if (best < 0) {
    if (trace) {
      fprintf(stderr, "[pjob] n%d u%d (%d,%d) tools %d: no job (%d listed)\n", nation_id, u->id, u->x,
              u->y, u->tools, s_job_count[nation_id]);
    }
    if (ai_euro_pjob_open_on(ctx, nation_id, cid) > 0) {
      /* pt2 P5: every job here is taken; the colony keeps this Pioneer out
       * for them (P3), so wait for the next post instead of wandering off. */
      units_mp_exhaust_unit(ctx->units, u->id);
      return 1;
    }
    return 0;
  }
  const AiPioneerJob job = s_jobs[nation_id][best];
  if (trace) {
    fprintf(stderr, "[pjob] n%d u%d (%d,%d) tools %d orders %d -> (%d,%d) kind %u\n", nation_id, u->id,
            u->x, u->y, u->tools, u->orders, job.x, job.y, job.kind);
  }
  if (job.x == u->x && job.y == u->y) {
    const ColonizeWorld w = world_from_turn_ctx(ctx);
    /* Work under way here (the counter is zeroed on completion): 0a60's
     * contact-claim probe overwrote its order with 10 at the nation-turn top,
     * so restore it and let the body resume instead of restarting at 0. */
    u->orders = u->col1_counter16 > 0
      ? (job.kind == PIONEER_PLAN_ROAD ? UNITS_ORDER_BUILD_ROAD : UNITS_ORDER_CLEAR_PLOW)
      : UNITS_ORDER_NONE;
    const bool ok = job.kind == PIONEER_PLAN_ROAD
      ? units_pioneer_road_w(&w, u->id, NULL, NULL, NULL)
      : units_pioneer_plow_w(&w, u->id, NULL, NULL, NULL);
    if (!ok) {
      ai_euro_pjob_drop(nation_id, best); /* the body refused it: forget the job */
      return 0;
    }
    units_mp_exhaust_unit(ctx->units, u->id);
    return 1;
  }
  const int id = u->id;
  const int ox = u->x;
  const int oy = u->y;
  u->orders = AI_EURO_ACT_GOAL;
  u->goto_x = job.x;
  u->goto_y = job.y;
  ai_euro_goal_walk_479b(ctx, u);
  ColonizeUnit* after = units_get(ctx->units, id);
  if (after && after->active && after->x == ox && after->y == oy &&
      !(after->x == job.x && after->y == job.y)) {
    /* No step toward it: unreachable for now; the colony re-posts it later. */
    ai_euro_pjob_drop(nation_id, best);
    after->orders = UNITS_ORDER_NONE;
    return 0;
  }
  return 1;
}

/* ===== Europe training ===== */

int ai_euro_europe_train_expert(ColonizeTurnContext* ctx, int nation, int profession) {
  if (!ctx || !ctx->units || nation < 0 || nation > 3) {
    return 0;
  }
  const int dock = 236 + nation; /* the nation's Europe park tile, as the 5d04 hires use */
  for (int i = 0; i < units_slot_end(ctx->units); ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (o->active && o->nation_id == nation && o->x == dock && o->y == dock &&
        o->profession == profession) {
      return 0;
    }
  }
  const int lt = ai_euro_5d04_port_type_for(ctx->units, 0);
  if (lt < 0 || !units_spawn_room(ctx->units, nation)) {
    return 0;
  }
  const int id = units_spawn_allow_stack(ctx->units, lt, dock, dock);
  ColonizeUnit* u = units_get(ctx->units, id);
  if (!u) {
    return 0;
  }
  units_set_nation(u, nation);
  u->moves = units_max_mp(ctx->units, id);
  u->orders = UNITS_ORDER_SENTRY; /* parked in the harbour like a 5d04 recruit */
  u->profession = (uint8_t)profession;
  return 1;
}

/* ===== Colony focus ===== */

/* ponytail: flag-driven pick. A small colony works for export (Cash), not
 * growth (Farming): its crops pay for the hulls whose Europe goods buys bring
 * the muskets the 5952 equip arm needs for Soldier and Dragoon founders
 * (spec 7, 2026-11-17: TURN1 hulls 122 -> 144, Dragoon founders 4 -> 7). */
int ai_euro_colony_focus(const ColonizeTurnContext* ctx, const ColonizeColony* col) {
  if ((col->ai_flags & (COLONIZE_COLONY_AI_NEEDS_GARRISON | COLONIZE_COLONY_AI_SHORT_DEFENDERS)) != 0) {
    return COLONY_GOVERNOR_MILITARY;
  }
  const ColonizeCol1Save* col1 = ctx->col1_ok ? ctx->col1 : NULL;
  const int pop = colonies_headcount(col);
  const int sol = colony_prod_sol_percent(col1, col);
  const int tories = (pop * (100 - sol)) / 100;
  const int thresh = 6 + (col1 ? (int)col1->head.difficulty : 0); /* the F4 mirror */
  if (tories + 1 >= thresh || (sol < 50 && pop >= 4)) {
    return COLONY_GOVERNOR_POLITICS;
  }
  /* #1868: past 50% tax a small colony's exports pay little (pt7g t528, 75%:
   * ~20 Fur Traders on 2-5 furs a turn, food +3 a colony, no growth);
   * Farming there doubled the 24 small colonies' food surplus at equal
   * bells. ponytail: the 50% line is a guess, not measured. */
  if (pop < 4) {
    const int tax = col1 && col->nation_id >= 0 && col->nation_id < 4
                      ? (int)col1->nation[col->nation_id].tax_rate : 0;
    return tax >= 50 ? COLONY_GOVERNOR_FARMING : COLONY_GOVERNOR_CASH;
  }
  return COLONY_GOVERNOR_INDUSTRY;
}

/* ===== Forest reserve ===== */

static bool ai_euro_is_forest_class(int cls) {
  return cls >= 8 && cls <= 0x17;
}

int ai_euro_forest_workable(const ColonizeTurnContext* ctx, const ColonizeColony* col) {
  if (!ctx || !ctx->map || !ctx->colonies || !col) {
    return 0;
  }
  const ColonizeWorld w = world_from_turn_ctx(ctx);
  const int ring = colonies_work_plot_count(ctx->colonies, col);
  int n = 0;
  for (int ti = 0; ti < ring; ++ti) {
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = col->x + dx;
    const int ty = col->y + dy;
    if (!map_coords_inset(ctx->map, tx, ty) ||
        !ai_euro_is_forest_class(map_dos_terr_class_at(ctx->map, tx, ty)) ||
        colonies_plot_blocked_mask(&w, col, ti) != 0 ||
        colonies_indian_claim_tribe_from_w(&w, col->nation_id, col->x, col->y, tx, ty) >= 0 ||
        ai_euro_pjob_kind_at(col->nation_id, tx, ty) == PIONEER_PLAN_CLEAR) {
      continue; /* already posted for clearing */
    }
    ++n;
  }
  return n;
}

/* ponytail: fixed reserve, 2 forests plus one for a colony of 8+; size it from
 * the build queue's lumber need if colonies still run dry. */
int ai_euro_forest_reserve(const ColonizeColony* col) {
  return 2 + (col && col->population >= 8 ? 1 : 0);
}

bool ai_euro_forest_keep(const ColonizeTurnContext* ctx, const ColonizeColony* col, int tx, int ty) {
  if (!ctx || !ctx->map || !ai_euro_is_forest_class(map_dos_terr_class_at(ctx->map, tx, ty))) {
    return false;
  }
  return map_resource_type_at(ctx->map, tx, ty) != -1 ||
         ai_euro_forest_workable(ctx, col) <= ai_euro_forest_reserve(col);
}
