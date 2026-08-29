/* Smoke: FUN_521d_20e6 land arms — patrol return + 8-dir wander step. */
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_euro_20e6: FAIL %s\n", msg);
  return 1;
}

typedef struct Fixture {
  ColonizeWorldMap map;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  ColonizeCol1Save col1;
  ColonizeDosRng rng;
  ColonizeTurnContext ctx;
  uint32_t turn;
  uint16_t year;
  char status[256];
} Fixture;

static int fixture_init(Fixture* f, int nation) {
  memset(f, 0, sizeof(*f));
  f->map.width = 16;
  f->map.height = 16;
  f->map.tile_count = 256;
  f->map.terrain = calloc(256, 1);
  f->map.layer2 = calloc(256, 1);
  f->map.layer3 = calloc(256, 1);
  f->map.seen = calloc(256, 1);
  if (!f->map.terrain || !f->map.layer2 || !f->map.layer3 || !f->map.seen) {
    return fail("alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    f->map.terrain[i] = 2; /* plains */
  }
  units_reset(&f->units);
  f->units.type_count = 3;
  snprintf(f->units.types[0].name, sizeof(f->units.types[0].name), "Free Colonist");
  f->units.types[0].movement = 1;
  f->units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(f->units.types[1].name, sizeof(f->units.types[1].name), "Soldier");
  f->units.types[1].movement = 1;
  f->units.types[1].attack = 2;
  f->units.types[1].defense = 2;
  f->units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(f->units.types[2].name, sizeof(f->units.types[2].name), "Caravel");
  f->units.types[2].movement = 4;
  f->units.types[2].cargo = 2;
  f->units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  colonies_init(&f->colonies);
  col1_save_init(&f->col1);
  memset(f->col1.nation, 0, sizeof(f->col1.nation));
  memset(f->col1.head.nation_relation, 0, sizeof(f->col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    f->col1.player[i].control = 0;
    f->col1.player[i].diplomacy = 0;
  }
  f->col1.nation[nation].gold = 300;
  dos_rng_seed(&f->rng, 100);
  f->turn = 5;
  f->year = 1500;
  f->ctx.turn_number = &f->turn;
  f->ctx.game_year = &f->year;
  f->ctx.human_nation = 0;
  f->ctx.units = &f->units;
  f->ctx.colonies = &f->colonies;
  f->ctx.map = &f->map;
  f->ctx.col1 = &f->col1;
  f->ctx.col1_ok = true;
  f->ctx.rng = &f->rng;
  f->ctx.rng_seed = 100;
  f->ctx.status = f->status;
  f->ctx.status_size = sizeof(f->status);
  return 0;
}

static void fixture_free(Fixture* f) {
  free(f->map.terrain);
  free(f->map.layer2);
  free(f->map.layer3);
  free(f->map.seen);
}

static int cheb(int ax, int ay, int bx, int by) {
  const int dx = ax > bx ? ax - bx : bx - ax;
  const int dy = ay > by ? ay - by : by - ay;
  return dx > dy ? dx : dy;
}

/*
 * No colonies, peace, idle Soldier: DOS falls through every arm to the
 * 8-direction wander scorer (LAB_521d_4d2e) and commits ONE adjacent tile
 * (epilogue unit+0x314c=0xc). The old placeholder walked "2 tiles west".
 */
static int unit_wander_step_is_adjacent(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  const int sid = units_spawn(&f.units, 1, 8, 8);
  ColonizeUnit* s = units_get(&f.units, sid);
  if (!s) {
    fixture_free(&f);
    return fail("spawn soldier");
  }
  s->nation_id = nation;
  s->moves_left = 1 * UNITS_MP_PER_TILE;
  s->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  s = units_get(&f.units, sid);
  if (!s || !s->active) {
    fixture_free(&f);
    return fail("soldier vanished");
  }
  if (cheb(s->x, s->y, 8, 8) > 1) {
    fixture_free(&f);
    return fail("wander moved more than one tile");
  }
  if (units_orders_follow_goto(s->orders) && s->goto_x < 200 && cheb(s->goto_x, s->goto_y, 8, 8) > 1) {
    fprintf(stderr, "goto=(%d,%d)\n", s->goto_x, s->goto_y);
    fixture_free(&f);
    return fail("wander goto not adjacent to start");
  }
  fixture_free(&f);
  return 0;
}

/*
 * One own colony, G stance 0 on its continent (no pressure, cap reached on
 * a zero-tally synthetic map), idle Soldier 4 tiles away: LAB_521d_277a
 * SCOUT/PATROL arm sends a combat unit back to its bound colony.
 */
static int unit_patrol_returns_to_colony(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  ColonizeColony* own = &f.colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 4;
  own->y = 4;
  own->population = 3;
  own->colonist_count = 3;
  own->stock[COLONIZE_CARGO_FOOD] = 60;
  own->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;

  const int sid = units_spawn(&f.units, 1, 8, 8);
  ColonizeUnit* s = units_get(&f.units, sid);
  if (!s) {
    fixture_free(&f);
    return fail("spawn soldier");
  }
  s->nation_id = nation;
  s->moves_left = 1 * UNITS_MP_PER_TILE;
  s->orders = 0;
  const int before = cheb(8, 8, 4, 4);

  ai_euro_dispatcher_turn(&f.ctx, nation);

  s = units_get(&f.units, sid);
  if (!s || !s->active) {
    fixture_free(&f);
    return fail("soldier vanished");
  }
  const int after = cheb(s->x, s->y, 4, 4);
  const int goto_home = units_orders_follow_goto(s->orders) && s->goto_x == 4 && s->goto_y == 4;
  if (!(after < before || goto_home)) {
    fprintf(stderr, "pos=(%d,%d) orders=%d goto=(%d,%d)\n", s->x, s->y, s->orders, s->goto_x, s->goto_y);
    fixture_free(&f);
    return fail("patrol arm did not head home");
  }
  fixture_free(&f);
  return 0;
}

int main(void) {
  if (unit_wander_step_is_adjacent() != 0) {
    return 1;
  }
  if (unit_patrol_returns_to_colony() != 0) {
    return 1;
  }
  printf("unit_ai_euro_20e6: OK\n");
  return 0;
}
