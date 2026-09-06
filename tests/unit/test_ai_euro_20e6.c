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
  f->units.type_count = 4;
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
  snprintf(f->units.types[3].name, sizeof(f->units.types[3].name), "Wagon Train");
  f->units.types[3].movement = 2;
  f->units.types[3].cargo = 2;
  f->units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
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

/*
 * Ring-hop latch (unit+0x3155/+0x3156, raw 2416-2458): explorer on a big
 * landlocked plain — the ring scan finds no coastal site (best_nib 0), so
 * the hop arm rolls a ring20 slot and commits a 4-tiles-out goto (Chebyshev
 * 4 or 8 from the start, every slot in bounds on 32x32 from the centre).
 */
static int unit_ring_hop_commits_far_goto(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  /* Rebuild as 32x32 all-plains. */
  fixture_free(&f);
  f.map.width = 32;
  f.map.height = 32;
  f.map.tile_count = 32 * 32;
  f.map.terrain = calloc(32 * 32, 1);
  f.map.layer2 = calloc(32 * 32, 1);
  f.map.layer3 = calloc(32 * 32, 1);
  f.map.seen = calloc(32 * 32, 1);
  if (!f.map.terrain || !f.map.layer2 || !f.map.layer3 || !f.map.seen) {
    return fail("alloc big map");
  }
  for (int i = 0; i < 32 * 32; ++i) {
    f.map.terrain[i] = 2; /* plains */
  }
  const int sid = units_spawn(&f.units, 1, 16, 16);
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
    return fail("hop soldier vanished");
  }
  if (!units_orders_follow_goto(s->orders) || s->goto_x >= 200) {
    fprintf(stderr, "pos=(%d,%d) orders=%d\n", s->x, s->y, s->orders);
    fixture_free(&f);
    return fail("ring hop set no goto");
  }
  const int d = cheb(s->goto_x, s->goto_y, 16, 16);
  if (d != 4 && d != 8) {
    fprintf(stderr, "goto=(%d,%d) cheb=%d\n", s->goto_x, s->goto_y, d);
    fixture_free(&f);
    return fail("ring hop goto not 4 tiles out");
  }
  fixture_free(&f);
  return 0;
}

/*
 * LAB_3558 colony-sail matrix (raw 1933-2031): a Caravel at sea carrying a
 * plain colonist (iStack_82 != 0), no land tile adjacent (empty unload
 * mask), one own coastal colony flagged NEEDS_COLONISTS — the peace score
 * commits a goto onto that colony.
 */
static int unit_colony_sail_targets_needy_colony(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  /* Columns x>=12 become ocean; land keeps x<=11. */
  for (int y = 0; y < 16; ++y) {
    for (int x = 12; x < 16; ++x) {
      f.map.terrain[y * 16 + x] = 25; /* MAP_OCEAN_INDEX */
    }
  }
  ColonizeColony* own = &f.colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 11;
  own->y = 8;
  own->population = 2;
  own->colonist_count = 2;
  own->stock[COLONIZE_CARGO_FOOD] = 60;
  own->building_in_production = -1;
  own->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;

  const int ship_id = units_spawn(&f.units, 2, 14, 8);
  const int pax_id = units_spawn_allow_stack(&f.units, 0, 14, 8);
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  ColonizeUnit* pax = units_get(&f.units, pax_id);
  if (!ship || !pax) {
    fixture_free(&f);
    return fail("spawn ship/pax");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  pax->nation_id = nation;
  if (!units_board_stacked(&f.units, pax_id, ship_id)) {
    fixture_free(&f);
    return fail("board pax");
  }

  ai_euro_dispatcher_turn(&f.ctx, nation);

  ship = units_get(&f.units, ship_id);
  if (!ship || !ship->active) {
    fixture_free(&f);
    return fail("ship vanished");
  }
  const int heads_home =
    (units_orders_follow_goto(ship->orders) && ship->goto_x == 11 && ship->goto_y == 8) ||
    cheb(ship->x, ship->y, 11, 8) < cheb(14, 8, 11, 8);
  if (!heads_home) {
    fprintf(stderr, "ship pos=(%d,%d) orders=%d goto=(%d,%d)\n", ship->x, ship->y, ship->orders,
            ship->goto_x, ship->goto_y);
    fixture_free(&f);
    return fail("colony sail did not head for the needy colony");
  }
  fixture_free(&f);
  return 0;
}

/*
 * LAB_521d_47b9 (raw 2260-2276): an untasked Wagon Train whose +0x314a bind
 * byte is unset and whose nearest own colony is not on this landmass
 * (iStack_2c != iStack_38 — iStack_2c is −2 when the nation owns no colony at
 * all) is a dead end; DOS calls FUN_1000_89f8 = destroy_unit and returns.
 */
static int unit_wagon_dead_end_destroyed(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  /* No colonies at all → uStack_62 < 0 → iStack_2c = −2 ≠ continent 0. */
  const int wid = units_spawn(&f.units, 3, 8, 8);
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!w) {
    fixture_free(&f);
    return fail("spawn wagon");
  }
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  w = units_get(&f.units, wid);
  if (w && w->active) {
    fprintf(stderr, "wagon still alive at (%d,%d) orders=%d\n", w->x, w->y, w->orders);
    fixture_free(&f);
    return fail("dead-end wagon was not destroyed");
  }
  fixture_free(&f);
  return 0;
}

/* Same wagon, but with an own colony on its own landmass: DOS binds +0x314a
 * and hauls (LAB_4701/4567) — the destroy arm must not fire. */
static int unit_wagon_with_target_survives(void) {
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

  const int wid = units_spawn(&f.units, 3, 8, 8);
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!w) {
    fixture_free(&f);
    return fail("spawn wagon");
  }
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  w = units_get(&f.units, wid);
  if (!w || !w->active) {
    fixture_free(&f);
    return fail("wagon with a reachable colony was destroyed");
  }
  fixture_free(&f);
  return 0;
}

/*
 * LAB_521d_457e (raw 2251-2257): an untasked, empty ship on the cadence beat
 * (`((char)id + (char)turn) & 0x1f == 0`) jumps to LAB_3fa6 — spiral out to a
 * High Seas tile (terrain 0x1a) and sail. Fixture puts the HS column at the
 * map's east edge and picks the turn so the beat lands on the ship's id.
 */
static int unit_empty_ship_hs_cadence(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  for (int y = 0; y < 16; ++y) {
    for (int x = 12; x < 16; ++x) {
      f.map.terrain[y * 16 + x] = 25; /* ocean */
    }
    f.map.terrain[y * 16 + 15] = 26; /* high seas */
  }
  ColonizeColony* own = &f.colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 11;
  own->y = 8;
  own->population = 3;
  own->colonist_count = 3;
  /* Well-stocked: the 4393 work-queue haul (ai_euro_try_ship_trade_haul) must
   * decline first — DOS reaches LAB_457e only after that pick fails. */
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    own->stock[c] = 200;
  }
  own->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;

  const int ship_id = units_spawn(&f.units, 2, 13, 8);
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  if (!ship) {
    fixture_free(&f);
    return fail("spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  /* ((char)id + (char)turn) & 0x1f == 0 */
  f.turn = (uint32_t)(32 - (ship_id % 32));

  ai_euro_dispatcher_turn(&f.ctx, nation);

  ship = units_get(&f.units, ship_id);
  if (!ship || !ship->active) {
    fixture_free(&f);
    return fail("ship vanished");
  }
  const int on_hs = map_tile_is_high_seas(&f.map, ship->x, ship->y);
  const int hs_goto = units_orders_follow_goto(ship->orders) && ship->goto_x < 200 &&
                      map_tile_is_high_seas(&f.map, ship->goto_x, ship->goto_y);
  if (!on_hs && !hs_goto) {
    fprintf(stderr, "ship pos=(%d,%d) orders=%d goto=(%d,%d)\n", ship->x, ship->y, ship->orders,
            ship->goto_x, ship->goto_y);
    fixture_free(&f);
    return fail("457e cadence did not send the empty ship to the High Seas");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Hold-cargo colony-delivery matrix (raw 2047-2139): a Caravel holding TOOLS
 * with two own coastal colonies in reach. The near one already PRODUCES tools
 * (+0x90 cargo_produced_mask) and sits on 150 of them (> 99), so the raw
 * 2067-2070 arm rejects it outright no matter how close it is; the far one is
 * empty of tools and wins even after the raw 2126 `score / ((dist>>2)+1)`
 * distance divide. Guards that the port picks on the DOS matrix and not on
 * "nearest short colony".
 */
static int unit_delivery_matrix_skips_full_producer(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  for (int y = 0; y < 16; ++y) {
    for (int x = 12; x < 16; ++x) {
      f.map.terrain[y * 16 + x] = 25; /* MAP_OCEAN_INDEX */
    }
  }
  /* Near colony: produces TOOLS and is full of them → rejected (raw 2067). */
  ColonizeColony* near_c = &f.colonies.colonies[0];
  near_c->id = 0;
  near_c->active = true;
  near_c->nation_id = nation;
  near_c->x = 11;
  near_c->y = 4;
  near_c->population = 3;
  near_c->colonist_count = 3;
  near_c->stock[COLONIZE_CARGO_FOOD] = 60;
  near_c->stock[COLONIZE_CARGO_TOOLS] = 150;
  near_c->cargo_produced_mask = (uint16_t)(1u << COLONIZE_CARGO_TOOLS);
  near_c->building_in_production = -1;
  /* Far colony: no tools at all → the only legal delivery target. */
  ColonizeColony* far_c = &f.colonies.colonies[1];
  far_c->id = 1;
  far_c->active = true;
  far_c->nation_id = nation;
  far_c->x = 11;
  far_c->y = 12;
  far_c->population = 3;
  far_c->colonist_count = 3;
  far_c->stock[COLONIZE_CARGO_FOOD] = 60;
  far_c->building_in_production = -1;
  f.colonies.colony_count = 2;
  f.colonies.next_id = 2;

  const int ship_id = units_spawn(&f.units, 2, 14, 4);
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  if (!ship) {
    fixture_free(&f);
    return fail("spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  if (units_load_goods(&f.units, ship_id, COLONIZE_CARGO_TOOLS, 100) <= 0) {
    fixture_free(&f);
    return fail("load tools");
  }

  ai_euro_dispatcher_turn(&f.ctx, nation);

  ship = units_get(&f.units, ship_id);
  if (!ship || !ship->active) {
    fixture_free(&f);
    return fail("ship vanished");
  }
  const int near_far =
    units_orders_follow_goto(ship->orders) && ship->goto_x < 200
      ? cheb(ship->goto_x, ship->goto_y, 11, 12)
      : cheb(ship->x, ship->y, 11, 12);
  const int near_near =
    units_orders_follow_goto(ship->orders) && ship->goto_x < 200
      ? cheb(ship->goto_x, ship->goto_y, 11, 4)
      : cheb(ship->x, ship->y, 11, 4);
  if (near_far > 1 || near_far >= near_near) {
    fprintf(stderr, "ship pos=(%d,%d) orders=%d goto=(%d,%d) dfar=%d dnear=%d\n", ship->x,
            ship->y, ship->orders, ship->goto_x, ship->goto_y, near_far, near_near);
    fixture_free(&f);
    return fail("delivery matrix did not aim at the tools-short colony");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Delivery SELL TAIL (raw 2140-2163). The nation's only colony is INLAND, so
 * the raw 2054 coastal gate (+0x1c bit 0x40) rejects the sole candidate and
 * the matrix picks nothing. DOS then dumps the whole hold for gold at the
 * −0x7b44 (= trade.euro_price) rate — untaxed into nation+0x2a, and into the
 * +0x7c / +0xbc per-cargo ledgers.
 *
 * Trap this fixture avoids: rejecting via the raw 2067-2070 "produces it and
 * holds > 99" arm needs cargo_produced_mask set, which is exactly what lets
 * the load matrix re-load the same cargo the moment the ship berths — DOS
 * does that too (load → 3558 tallies → matrix skips +0x314a → sell), but it
 * doubles the ledger inside one dispatcher turn and makes the assertion about
 * the pump instead of about the tail.
 */
static int unit_delivery_sell_tail_dumps_cargo(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  for (int y = 0; y < 16; ++y) {
    for (int x = 12; x < 16; ++x) {
      f.map.terrain[y * 16 + x] = 25; /* MAP_OCEAN_INDEX */
    }
  }
  ColonizeColony* c = &f.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 5; /* inland: every neighbour is plains → raw 2054 coastal gate fails */
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 60;
  c->stock[COLONIZE_CARGO_TOOLS] = 150;
  c->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;

  /* NAMES.TXT @CARGO start bid for Tools. */
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_TOOLS] = 2;

  const int ship_id = units_spawn(&f.units, 2, 14, 4);
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  if (!ship) {
    fixture_free(&f);
    return fail("spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  if (units_load_goods(&f.units, ship_id, COLONIZE_CARGO_TOOLS, 100) <= 0) {
    fixture_free(&f);
    return fail("load tools");
  }
  const uint32_t gold_before = f.col1.nation[nation].gold;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  ship = units_get(&f.units, ship_id);
  if (!ship || !ship->active) {
    fixture_free(&f);
    return fail("ship vanished");
  }
  int still_aboard = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] > 0 && ship->hold_goods_amount[h] < 255) {
      still_aboard += ship->hold_goods_amount[h];
    }
  }
  const ColonizeCol1NationTrade* t = &f.col1.nation[nation].trade;
  if (still_aboard != 0 || t->gold[COLONIZE_CARGO_TOOLS] != 200 ||
      t->tons[COLONIZE_CARGO_TOOLS] != 100 ||
      f.col1.nation[nation].gold != gold_before + 200u) {
    fprintf(stderr, "aboard=%d ledger_gold=%d tons=%d gold %u->%u\n", still_aboard,
            (int)t->gold[COLONIZE_CARGO_TOOLS], (int)t->tons[COLONIZE_CARGO_TOOLS],
            (unsigned)gold_before, (unsigned)f.col1.nation[nation].gold);
    fixture_free(&f);
    return fail("sell tail did not dump the hold for euro_price gold");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Ship LOAD-at-colony matrix (raw 3059-3134). An empty Caravel berthed beside
 * an own coastal colony holding equal stocks of Ore and Silver, plus larger
 * stocks of Food and Trade Goods. DOS scores euro_price × stock for a ship,
 * excludes Food / Trade Goods / Lumber and (absent a producing colony) Tools
 * and Muskets outright — so Silver (bid 20) must beat Ore (bid 3) at the same
 * 60 units, and neither of the bigger Food / Trade Goods piles may be taken.
 * The second free hold then takes Ore, the next-best score.
 */
static int unit_load_matrix_picks_priced_cargo(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  for (int y = 0; y < 16; ++y) {
    for (int x = 12; x < 16; ++x) {
      f.map.terrain[y * 16 + x] = 25; /* MAP_OCEAN_INDEX */
    }
  }
  ColonizeColony* c = &f.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 11;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 90;         /* ship arm skips cargo 0 */
  c->stock[COLONIZE_CARGO_TRADE_GOODS] = 80;  /* ship arm skips cargo 0xd */
  c->stock[COLONIZE_CARGO_LUMBER] = 90;       /* cargo 5 skipped for everyone */
  c->stock[COLONIZE_CARGO_ORE] = 60;
  c->stock[COLONIZE_CARGO_SILVER] = 60;
  c->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;

  /* NAMES.TXT @CARGO start bids for the goods this fixture stocks. */
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_FOOD] = 1;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_LUMBER] = 2;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 3;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_SILVER] = 20;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_TRADE_GOODS] = 2;

  const int ship_id = units_spawn(&f.units, 2, 12, 4); /* berthed alongside */
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  if (!ship) {
    fixture_free(&f);
    return fail("spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  ship = units_get(&f.units, ship_id);
  if (!ship || !ship->active) {
    fixture_free(&f);
    return fail("ship vanished");
  }
  int silver = 0;
  int ore = 0;
  int forbidden = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    const int amt = ship->hold_goods_amount[h];
    if (amt <= 0 || amt >= 255) {
      continue;
    }
    const int g = ship->hold_goods_type[h];
    if (g == COLONIZE_CARGO_SILVER) {
      silver += amt;
    } else if (g == COLONIZE_CARGO_ORE) {
      ore += amt;
    } else {
      forbidden += amt;
    }
  }
  if (ship->hold_goods_type[0] != COLONIZE_CARGO_SILVER || silver != 60 || ore != 60 ||
      forbidden != 0) {
    fprintf(stderr, "hold0=%d/%d silver=%d ore=%d other=%d\n", ship->hold_goods_type[0],
            ship->hold_goods_amount[0], silver, ore, forbidden);
    fixture_free(&f);
    return fail("load matrix did not pick the DOS-weighted goods");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Wagon own-colony arrival (raw 2996-3138): dump, then the LOAD matrix wagon
 * arm — one hold of the cheap-priced surplus (Ore, bid 3 < thr 4, stock ≥
 * 0x32) — then the village-errand latch routes the wagon at the nearest
 * same-landmass village (raw 2284-2307).
 */
static int unit_wagon_load_matrix_starts_village_errand(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  ColonizeColony* c = &f.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_ORE] = 80;
  c->stock[COLONIZE_CARGO_FOOD] = 40; /* < 0x32 — matrix floors it to −1 */
  c->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 3;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_FOOD] = 1;

  static ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.x = 12;
  tribe.y = 12;
  tribe.nation_id = 4;
  tribe.population = 5;
  f.col1.tribe = &tribe;
  f.col1.head.tribe_count = 1;

  const int wid = units_spawn(&f.units, 3, 4, 4);
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!w) {
    fixture_free(&f);
    return fail("spawn errand wagon");
  }
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  w = units_get(&f.units, wid);
  if (!w || !w->active) {
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("errand wagon vanished");
  }
  int ore = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (w->hold_goods_amount[h] > 0 && w->hold_goods_amount[h] < 255 &&
        w->hold_goods_type[h] == COLONIZE_CARGO_ORE) {
      ore += w->hold_goods_amount[h];
    }
  }
  if (ore != 80 || c->stock[COLONIZE_CARGO_ORE] != 0) {
    fprintf(stderr, "ore aboard=%d colony=%d\n", ore, c->stock[COLONIZE_CARGO_ORE]);
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("matrix should load the whole Ore surplus");
  }
  if (w->orders != UNITS_ORDER_AI_MOVE || w->goto_x != 12 || w->goto_y != 12) {
    fprintf(stderr, "orders=%d goto=(%d,%d)\n", w->orders, w->goto_x, w->goto_y);
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("errand wagon should aim at the village");
  }
  f.col1.tribe = NULL;
  fixture_free(&f);
  return 0;
}

/*
 * Errand arrival: adjacent to the village, the 4528 AI arm (case 1 Trade)
 * runs the 2820 shell — the hold is sold to the tribe (LAB_002bbc credits
 * Euro gold; alarm 0 accepts) and the MP is forfeited.
 */
static int unit_wagon_errand_trades_at_village(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  ColonizeColony* c = &f.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_ORE] = 80;
  c->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 3;

  static ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.x = 8;
  tribe.y = 4;
  tribe.nation_id = 4;
  tribe.population = 5;
  f.col1.tribe = &tribe;
  f.col1.head.tribe_count = 1;
  f.col1.indian[0].euro_diplo[nation] = 1;
  f.col1.indian[0].alarm_by_player[nation] = 0;

  const int wid = units_spawn(&f.units, 3, 4, 4);
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!w) {
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("spawn trading wagon");
  }
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;

  /* Beat 1: load + latch + goto the village. */
  ai_euro_dispatcher_turn(&f.ctx, nation);
  w = units_get(&f.units, wid);
  if (!w || !w->active || w->goto_x != 8 || w->goto_y != 4) {
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("beat 1 should aim the loaded wagon at the village");
  }
  /* Walk it adjacent by hand; beat 2 is the arrival. */
  w->x = 7;
  w->y = 4;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  const uint32_t gold_before = f.col1.nation[nation].gold;
  f.turn += 1;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  w = units_get(&f.units, wid);
  if (!w || !w->active) {
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("trading wagon vanished");
  }
  int ore = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (w->hold_goods_amount[h] > 0 && w->hold_goods_amount[h] < 255 &&
        w->hold_goods_type[h] == COLONIZE_CARGO_ORE) {
      ore += w->hold_goods_amount[h];
    }
  }
  if (ore != 0) {
    fprintf(stderr, "ore still aboard=%d\n", ore);
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("village trade should take the Ore hold");
  }
  if (f.col1.nation[nation].gold <= gold_before) {
    fprintf(stderr, "gold %u -> %u\n", gold_before, f.col1.nation[nation].gold);
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("accepted sale should credit Euro gold");
  }
  if (w->moves_left != 0) {
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("4528 return forfeits the wagon's MP");
  }
  f.col1.tribe = NULL;
  fixture_free(&f);
  return 0;
}

/*
 * Errand with no village on the wagon's landmass: the scan comes up empty and
 * LAB_47b9 destroys the wagon (raw 2304 `goto` on uStack_24 < 0).
 */
static int unit_wagon_errand_dead_end_destroyed(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  /* Ocean strait splits the map; the only village sits across it. */
  for (int y = 0; y < 16; ++y) {
    for (int x = 8; x < 10; ++x) {
      f.map.terrain[y * 16 + x] = 25; /* MAP_OCEAN_INDEX */
    }
    for (int x = 10; x < 16; ++x) {
      f.map.layer3[y * 16 + x] = 2; /* east landmass, continent 2 */
    }
  }
  ColonizeColony* c = &f.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_ORE] = 80;
  c->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 3;

  static ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.x = 12;
  tribe.y = 4;
  tribe.nation_id = 4;
  tribe.population = 5;
  f.col1.tribe = &tribe;
  f.col1.head.tribe_count = 1;

  const int wid = units_spawn(&f.units, 3, 4, 4);
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!w) {
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("spawn stranded wagon");
  }
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  w = units_get(&f.units, wid);
  if (w && w->active) {
    fprintf(stderr, "wagon alive at (%d,%d) orders=%d\n", w->x, w->y, w->orders);
    f.col1.tribe = NULL;
    fixture_free(&f);
    return fail("errand wagon with no reachable village must be destroyed");
  }
  f.col1.tribe = NULL;
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
  if (unit_ring_hop_commits_far_goto() != 0) {
    return 1;
  }
  if (unit_colony_sail_targets_needy_colony() != 0) {
    return 1;
  }
  if (unit_wagon_dead_end_destroyed() != 0) {
    return 1;
  }
  if (unit_wagon_with_target_survives() != 0) {
    return 1;
  }
  if (unit_empty_ship_hs_cadence() != 0) {
    return 1;
  }
  if (unit_delivery_matrix_skips_full_producer() != 0) {
    return 1;
  }
  if (unit_delivery_sell_tail_dumps_cargo() != 0) {
    return 1;
  }
  if (unit_load_matrix_picks_priced_cargo() != 0) {
    return 1;
  }
  if (unit_wagon_load_matrix_starts_village_errand() != 0) {
    return 1;
  }
  if (unit_wagon_errand_trades_at_village() != 0) {
    return 1;
  }
  if (unit_wagon_errand_dead_end_destroyed() != 0) {
    return 1;
  }
  printf("unit_ai_euro_20e6: OK\n");
  return 0;
}
