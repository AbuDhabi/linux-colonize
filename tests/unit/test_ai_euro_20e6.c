/* Smoke: FUN_521d_20e6 land arms — patrol return + 8-dir wander step. */
/* setenv/unsetenv (the AI_20E6_SHIP_DUMP kill-switch case) are POSIX. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/ai_popup.h"
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

/* Truthful census ship count for fixtures whose nation owns a ship (the
 * blank-census "no ships" reading otherwise trips the live 5d04 no-ships
 * gold floor). Minimal on purpose: with fixture gold < 1000 the 5c3c
 * ship-buy ladder can't afford anything, blank-census cargo_short blocks
 * the recruit-slot buy, and ship_cargo_totals==0 blocks the Artillery dock
 * buy — so this one census byte changes nothing else (including the DOS
 * RNG draw order). */
static void quiet_5d04_planner(Fixture* f, int nation) {
  f->col1.stuff.ship_counts[nation] = 1;
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
 * plain colonist plus a Scout, no land tile adjacent (empty unload mask),
 * one own coastal colony flagged NEEDS_COLONISTS — the peace score commits
 * a goto onto that colony.
 *
 * 2026-09-07b: the Scout matters. The raw 1746 goal fold promotes a lone
 * civilian into the founder count, and DOS's sail gate skips a ship whose
 * cargo is ALL founders (`pioneers == a8` with an empty mask) — that hull
 * belongs to the explore/found machinery. The old fixture (colonist only)
 * passed through `ai_euro_nearest_short_coastal_colony`, a Linux-only
 * fallback retired with the still-thin list; the Scout makes the DOS gate
 * itself pass (cargo not all founders), which is what this scenario is for.
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

  const int scout_ti = f.units.type_count++;
  snprintf(f.units.types[scout_ti].name, sizeof(f.units.types[scout_ti].name), "Scout");
  f.units.types[scout_ti].movement = 4;
  f.units.types[scout_ti].domain = COLONIZE_UNIT_DOMAIN_LAND;
  const int ship_id = units_spawn(&f.units, 2, 14, 8);
  const int pax_id = units_spawn_allow_stack(&f.units, 0, 14, 8);
  const int scout_id = units_spawn_allow_stack(&f.units, scout_ti, 14, 8);
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  ColonizeUnit* pax = units_get(&f.units, pax_id);
  ColonizeUnit* scout = units_get(&f.units, scout_id);
  if (!ship || !pax || !scout) {
    fixture_free(&f);
    return fail("spawn ship/pax");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  pax->nation_id = nation;
  scout->nation_id = nation;
  if (!units_board_stacked(&f.units, pax_id, ship_id) ||
      !units_board_stacked(&f.units, scout_id, ship_id)) {
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
  quiet_5d04_planner(&f, nation);
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
 * Ship own-colony arrival (raw 2996-3138), the twin of the wagon sequence.
 *
 * A 2-hold Caravel berths beside its own colony carrying 80 Ore and 40 Furs.
 * Neither is a delivery cargo (the raw 1702-1712 tally keeps only 0x0d..0x0f
 * and 0x08), so the delivery matrix and its sell tail stay out of the way;
 * the colony is not Ore-short (60 ≥ 20) so the port's Linux "colony is short
 * of this" unload arm refuses the hull; and the hull is not empty, so the 06e
 * empty-hull gate refused the load matrix. DOS has none of those rules: raw
 * 3002-3007 dumps EVERY hold into the colony unconditionally, and only then
 * does the load matrix score.
 *
 * Expected, in order:
 *   - both holds land in colony stock (Ore 60→140, Furs 0→40);
 *   - the matrix then fills both holds from the post-dump colony. Silver
 *     (bid 20 × 60 = 1200) beats Ore (stock 140 ≥ cap 100 → term 140<<1 =
 *     280, bid 3 → 840) and Furs (bid 4 × 40 = 160); Food / Trade Goods /
 *     Lumber are skipped outright for ships; Tools and Muskets need
 *     cargo_produced_mask (unset here); Horses score `(stock − cap) + 0x17`
 *     = 15 − 100 + 23 < 0 → clamped to 0 → skipped. The second hold then
 *     takes Ore at qty = min(stock, 100) = 100, leaving the colony 40.
 *
 * Run twice: AI_20E6_SHIP_DUMP=0 must reproduce the old substitution exactly
 * (hull untouched, colony untouched), which pins the kill switch.
 */
static int unit_ship_berth_dumps_whole_hull(void) {
  const int nation = 1;
  for (int pass = 0; pass < 2; ++pass) {
    const int dump_on = (pass == 1);
    if (dump_on) {
      unsetenv("AI_20E6_SHIP_DUMP");
    } else {
      setenv("AI_20E6_SHIP_DUMP", "0", 1);
    }
    Fixture f;
    if (fixture_init(&f, nation) != 0) {
      unsetenv("AI_20E6_SHIP_DUMP");
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
    c->stock[COLONIZE_CARGO_FOOD] = 90;        /* ship arm skips cargo 0 */
    c->stock[COLONIZE_CARGO_TRADE_GOODS] = 80; /* ship arm skips cargo 0xd */
    c->stock[COLONIZE_CARGO_LUMBER] = 90;      /* cargo 5 skipped for everyone */
    c->stock[COLONIZE_CARGO_ORE] = 60;
    c->stock[COLONIZE_CARGO_SILVER] = 60;
    c->stock[COLONIZE_CARGO_MUSKETS] = 15; /* >= 10: not "short" for the port arm */
    c->stock[COLONIZE_CARGO_HORSES] = 15;  /* >= 10: not "short" for the port arm */
    c->stock[COLONIZE_CARGO_FURS] = 0;
    c->building_in_production = -1;
    f.colonies.colony_count = 1;
    f.colonies.next_id = 1;

    /* NAMES.TXT @CARGO start bids. */
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_FOOD] = 1;
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_LUMBER] = 2;
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 3;
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_SILVER] = 20;
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_TRADE_GOODS] = 2;
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_MUSKETS] = 11;
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_HORSES] = 2;
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_FURS] = 4;

    const int ship_id = units_spawn(&f.units, 2, 12, 4); /* berthed alongside */
    ColonizeUnit* ship = units_get(&f.units, ship_id);
    if (!ship) {
      fixture_free(&f);
      unsetenv("AI_20E6_SHIP_DUMP");
      return fail("spawn ship");
    }
    ship->nation_id = nation;
    ship->moves_left = 4 * UNITS_MP_PER_TILE;
    ship->orders = 0;
    if (units_load_goods(&f.units, ship_id, COLONIZE_CARGO_ORE, 80) <= 0 ||
        units_load_goods(&f.units, ship_id, COLONIZE_CARGO_FURS, 40) <= 0) {
      fixture_free(&f);
      unsetenv("AI_20E6_SHIP_DUMP");
      return fail("load ship holds");
    }

    ai_euro_dispatcher_turn(&f.ctx, nation);

    ship = units_get(&f.units, ship_id);
    if (!ship || !ship->active) {
      fixture_free(&f);
      unsetenv("AI_20E6_SHIP_DUMP");
      return fail("ship vanished");
    }
    int aboard[COLONIZE_CARGO_COUNT];
    memset(aboard, 0, sizeof(aboard));
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      const int amt = ship->hold_goods_amount[h];
      if (amt <= 0 || amt >= 255) {
        continue;
      }
      aboard[ship->hold_goods_type[h]] += amt;
    }
    int ok;
    if (dump_on) {
      ok = c->stock[COLONIZE_CARGO_ORE] == 40 && c->stock[COLONIZE_CARGO_FURS] == 40 &&
           c->stock[COLONIZE_CARGO_SILVER] == 0 &&
           ship->hold_goods_type[0] == COLONIZE_CARGO_SILVER &&
           aboard[COLONIZE_CARGO_SILVER] == 60 && aboard[COLONIZE_CARGO_ORE] == 100 &&
           aboard[COLONIZE_CARGO_FURS] == 0;
    } else {
      /* Kill switch: 06e substitution — part-loaded hull, matrix never runs. */
      ok = c->stock[COLONIZE_CARGO_ORE] == 60 && c->stock[COLONIZE_CARGO_FURS] == 0 &&
           c->stock[COLONIZE_CARGO_SILVER] == 60 && aboard[COLONIZE_CARGO_ORE] == 80 &&
           aboard[COLONIZE_CARGO_FURS] == 40 && aboard[COLONIZE_CARGO_SILVER] == 0;
    }
    if (!ok) {
      fprintf(
        stderr,
        "pass %d (dump %s): colony ore=%d furs=%d silver=%d | aboard ore=%d furs=%d "
        "silver=%d hold0=%d\n",
        pass, dump_on ? "on" : "off", (int)c->stock[COLONIZE_CARGO_ORE],
        (int)c->stock[COLONIZE_CARGO_FURS], (int)c->stock[COLONIZE_CARGO_SILVER],
        aboard[COLONIZE_CARGO_ORE], aboard[COLONIZE_CARGO_FURS],
        aboard[COLONIZE_CARGO_SILVER], ship->hold_goods_type[0]
      );
      fixture_free(&f);
      unsetenv("AI_20E6_SHIP_DUMP");
      return fail(
        dump_on ? "ship berth did not run the DOS dump+load sequence"
                : "AI_20E6_SHIP_DUMP=0 did not restore the empty-hull gate"
      );
    }
    fixture_free(&f);
  }
  unsetenv("AI_20E6_SHIP_DUMP");
  return 0;
}

/*
 * Berth passenger boarding, the raw 3024-3051 MARK scan handing off to
 * FUN_1427_10be (ai_euro_20e6_transport_assemble) — one 20e6 act, two phases.
 *
 * Control flow, from viceroy_overlays.asm (see move_scoring_20e6_full.md
 * "2026-09-07e"): the arrival gate at 0x304c falls through into the arrival
 * block, whose scan stamps act_state = 1 and debits `iStack_d2` WITHOUT
 * boarding; the load matrix then spends what is left of that budget on goods;
 * the block falls out at 0x354e into LAB_3558, whose call at 0x3609 is
 * `FUN_1000_8b10` = 10be, which boards the marked members with
 * `free = 0x5237[type] − +0x3150` — exactly the remainder the scan reserved.
 *
 * Fixture: a 2-hold Caravel berthed at (12,4) beside its own colony (11,4),
 * with a Pioneer (DOS type 2, size 1) standing in the colony — the scan's
 * second arm, which marks unless the ship's composite priority is 0 on a
 * non-0-stance continent (stance is 0 here). The colony stocks only Silver,
 * so the load matrix has exactly one thing to want.
 *
 * The scan's OTHER arm (armed land unit) is deliberately not the subject: a
 * Soldier standing in an own colony is claimed as labor by the colony
 * admission loop first and carries order_code 'A', which raw 3033-3037
 * disqualifies — DOS-correct, and it makes that arm untestable from a
 * one-colony fixture.
 *
 * The assertion is the budget hand-off, which is what a broken split would
 * lose: the Soldier ends up ABOARD and exactly ONE of the two holds carries
 * goods. If the scan's reservation did not survive into 10be's own
 * `capacity − holds_occupied`, the load matrix would take both holds and
 * units_board would then find zero free passenger slots.
 */
static int unit_berth_marks_then_assembles_passenger(void) {
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
  f.units.type_count = 5;
  snprintf(f.units.types[4].name, sizeof(f.units.types[4].name), "Pioneer");
  f.units.types[4].movement = 1;
  f.units.types[4].domain = COLONIZE_UNIT_DOMAIN_LAND;
  f.units.types[4].space = 1; /* @UNIT size column (0x5238): one ship slot */
  quiet_5d04_planner(&f, nation);

  ColonizeColony* c = &f.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 11;
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 200;   /* no shortage → no labor admission */
  c->stock[COLONIZE_CARGO_SILVER] = 60;  /* the load matrix's only candidate */
  c->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_SILVER] = 20;

  const int ship_id = units_spawn(&f.units, 2, 12, 4); /* berthed alongside */
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  const int sol_id = units_spawn(&f.units, 4, 11, 4);  /* Pioneer in the colony */
  ColonizeUnit* sol = units_get(&f.units, sol_id);
  if (!ship || !sol) {
    fixture_free(&f);
    return fail("spawn berth boarding units");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  sol->nation_id = nation;
  sol->moves_left = UNITS_MP_PER_TILE;
  sol->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  ship = units_get(&f.units, ship_id);
  sol = units_get(&f.units, sol_id);
  if (!ship || !ship->active || !sol || !sol->active) {
    fixture_free(&f);
    return fail("berth boarding unit vanished");
  }
  int goods_holds = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    const int amt = ship->hold_goods_amount[h];
    if (amt > 0 && amt < 255) {
      goods_holds++;
    }
  }
  if (sol->aboard_ship_id != ship_id || goods_holds != 1) {
    fprintf(stderr, "pioneer aboard=%d (want %d) goods_holds=%d (want 1)\n",
            sol->aboard_ship_id, ship_id, goods_holds);
    fixture_free(&f);
    return fail("berth mark/10be hand-off did not reserve the passenger hold");
  }
  fixture_free(&f);
  return 0;
}

/*
 * 0a60 registration gate = DOS's `bVar5` (raw viceroy_unpacked.c:87622 /
 * :87633 / :87663) and the queue it feeds is a PICKUP queue: the 4393 tip
 * aims a hauler at the colony that HAS goods, not at one that is short.
 *
 * Wagon at (4,4), two own colonies:
 *   poor (5,4), ONE tile away: 5 Ore / 0 Tools / 1 Food at pop 4 — short of
 *               everything and holding nothing worth collecting, so it never
 *               enters the queue at all
 *   rich (8,4), four tiles away: 150 Ore — at/over warehouse capacity so
 *               `local_2a` doubles to 300, clears 0x4a, and registers
 * 2026-09-07b: DOS's 4393 entry gate is SHIPS-ONLY (`0xc < type < 0x13`,
 * raw :89877) and the port now follows it — so the queue consumer under test
 * is a Caravel (ocean row y=3, both colonies coastal), which must sail for
 * the FAR, RICH colony's berth. The wagon in the same scenario exercises the
 * LAB_457e origin walk that replaced its queue substitute: unbound, off
 * colony → bind to the NEAREST own colony (5,4) and walk there.
 */
static int unit_work_queue_pickup_aims_at_goods_colony(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  f.turn = 9;
  /* Ocean row so the ship consumer can exist and both colonies are coastal. */
  for (int x = 0; x < 16; ++x) {
    f.map.terrain[3 * 16 + x] = 25; /* MAP_OCEAN_INDEX */
  }
  ColonizeColony* rich = &f.colonies.colonies[0];
  rich->id = 0;
  rich->active = true;
  rich->nation_id = nation;
  rich->x = 8;
  rich->y = 4;
  rich->population = 3;
  rich->colonist_count = 3;
  rich->stock[COLONIZE_CARGO_ORE] = 150;
  rich->stock[COLONIZE_CARGO_FOOD] = 40;
  rich->building_in_production = -1;
  rich->specialty_cargo = 0xff;

  ColonizeColony* poor = &f.colonies.colonies[1];
  poor->id = 1;
  poor->active = true;
  poor->nation_id = nation;
  poor->x = 5;
  poor->y = 4;
  poor->population = 4;
  poor->colonist_count = 4;
  poor->stock[COLONIZE_CARGO_ORE] = 5;
  poor->stock[COLONIZE_CARGO_TOOLS] = 0;
  poor->stock[COLONIZE_CARGO_FOOD] = 1; /* food-short, tools-short, ore-short */
  poor->building_in_production = -1;
  poor->specialty_cargo = 0xff;
  f.colonies.colony_count = 2;
  f.colonies.next_id = 2;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 3;

  const int sid = units_spawn(&f.units, 2, 4, 3); /* Caravel on the ocean row */
  const int wid = units_spawn(&f.units, 3, 4, 4); /* Wagon Train, empty */
  ColonizeUnit* sh = units_get(&f.units, sid);
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!sh || !w) {
    fixture_free(&f);
    return fail("spawn pickup ship/wagon");
  }
  sh->nation_id = nation;
  sh->moves_left = 4 * UNITS_MP_PER_TILE;
  sh->orders = 0;
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  sh = units_get(&f.units, sid);
  if (!sh || !sh->active) {
    fixture_free(&f);
    return fail("pickup ship vanished");
  }
  /* Ship: the 4393 pickup tip → berth water beside the rich colony (8,4),
   * or it already sailed onto that berth this beat. */
  const int ship_near_rich =
    (units_orders_follow_goto(sh->orders) && abs(sh->goto_x - 8) <= 1 &&
     abs(sh->goto_y - 4) <= 1) ||
    (abs(sh->x - 8) <= 1 && abs(sh->y - 4) <= 1);
  if (!ship_near_rich) {
    fprintf(stderr, "pickup ship orders=%d pos=(%d,%d) goto=(%d,%d)\n", sh->orders, sh->x,
            sh->y, sh->goto_x, sh->goto_y);
    fixture_free(&f);
    return fail("work-queue tip should aim the ship at the goods colony");
  }
  w = units_get(&f.units, wid);
  if (!w || !w->active) {
    fixture_free(&f);
    return fail("pickup wagon vanished");
  }
  /* Wagon: LAB_457e origin walk — bind to nearest own colony (5,4), walk. */
  const int wagon_walks_home =
    (w->orders == UNITS_ORDER_AI_MOVE && w->goto_x == 5 && w->goto_y == 4) ||
    (w->x == 5 && w->y == 4);
  if (!wagon_walks_home) {
    fprintf(stderr, "pickup wagon orders=%d pos=(%d,%d) goto=(%d,%d)\n", w->orders, w->x, w->y,
            w->goto_x, w->goto_y);
    fixture_free(&f);
    return fail("origin walk should aim the wagon at its nearest own colony");
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

/*
 * FUN_521d_20e6 treasure act band, first arm (move_scoring_20e6_full.md raw
 * ~2315): a Treasure (DOS type 0x0a) standing in an own colony (iStack_2e ==
 * 0) credits unit+0x315b * 100 to nation+0x2a with no Crown cut, fires popup
 * 0x1786 (@LOOTFOREIGN) while DS:0x5382 bit0 is clear, then is destroyed
 * (LAB_0047b9). Also asserts DOS's *absence* of any human term: the human
 * nation's own Treasure sitting in its own colony is untouched by the AI
 * nation's dispatcher turn.
 */
static int treasure_add_type(Fixture* f) {
  const int ti = f->units.type_count++;
  snprintf(f->units.types[ti].name, sizeof(f->units.types[ti].name), "Treasure");
  f->units.types[ti].movement = 1;
  f->units.types[ti].cargo = 0;
  f->units.types[ti].domain = COLONIZE_UNIT_DOMAIN_LAND;
  return ti;
}

static void treasure_add_colony(Fixture* f, int idx, int nation, int x, int y) {
  ColonizeColony* c = &f->colonies.colonies[idx];
  c->id = idx;
  c->active = true;
  c->nation_id = nation;
  c->x = x;
  c->y = y;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 60;
  c->building_in_production = -1;
  if (f->colonies.colony_count <= idx) {
    f->colonies.colony_count = idx + 1;
    f->colonies.next_id = idx + 1;
  }
}

static int unit_treasure_in_colony_cash_in(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  quiet_5d04_planner(&f, nation);
  AiPopupState popups;
  ai_popup_init(&popups);
  f.ctx.ai_popups = &popups;

  const int ti = treasure_add_type(&f);
  treasure_add_colony(&f, 0, nation, 4, 4);
  treasure_add_colony(&f, 1, f.ctx.human_nation, 10, 10);

  const int tid = units_spawn(&f.units, ti, 4, 4);
  ColonizeUnit* t = units_get(&f.units, tid);
  if (!t) {
    fixture_free(&f);
    return fail("spawn treasure");
  }
  t->nation_id = nation;
  t->moves_left = 1 * UNITS_MP_PER_TILE;
  t->orders = 0;
  t->profession = 9; /* DOS +0x315b: gold/100 → 900 */

  const int hid = units_spawn(&f.units, ti, 10, 10);
  ColonizeUnit* h = units_get(&f.units, hid);
  if (!h) {
    fixture_free(&f);
    return fail("spawn human treasure");
  }
  h->nation_id = f.ctx.human_nation;
  h->moves_left = 1 * UNITS_MP_PER_TILE;
  h->orders = 0;
  h->profession = 5;

  const uint32_t gold_before = f.col1.nation[nation].gold;
  const uint32_t human_gold_before = f.col1.nation[f.ctx.human_nation].gold;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  t = units_get(&f.units, tid);
  if (t && t->active) {
    fprintf(stderr, "treasure alive at (%d,%d)\n", t->x, t->y);
    fixture_free(&f);
    return fail("in-colony treasure must be destroyed after the cash-in");
  }
  const uint32_t delta = f.col1.nation[nation].gold - gold_before;
  if (delta != 900u) {
    fprintf(stderr, "treasury delta=%u want 900\n", (unsigned)delta);
    fixture_free(&f);
    return fail("cash-in must credit +0x315b * 100, no Crown cut");
  }
  /* Human treasure and treasury untouched by an AI nation's turn. */
  h = units_get(&f.units, hid);
  if (!h || !h->active) {
    fixture_free(&f);
    return fail("human Treasure must survive an AI nation's dispatcher turn");
  }
  if (f.col1.nation[f.ctx.human_nation].gold != human_gold_before) {
    fixture_free(&f);
    return fail("human treasury must not move on an AI nation's turn");
  }
  /* Pre-WoI: @LOOTFOREIGN enqueued. */
  int found = 0;
  for (int i = 0; i < popups.queue_count; ++i) {
    if (strstr(popups.queue[i].body, "treasure fleet") != NULL) {
      found = 1;
    }
  }
  if (!found) {
    fprintf(stderr, "queue_count=%d\n", popups.queue_count);
    for (int i = 0; i < popups.queue_count; ++i) {
      fprintf(stderr, "  [%d] %s\n", i, popups.queue[i].body);
    }
    fixture_free(&f);
    return fail("pre-WoI cash-in must enqueue @LOOTFOREIGN");
  }
  fixture_free(&f);
  return 0;
}

/* Same beat with DS:0x5382 bit0 set: gold and destroy still happen, the
 * popup does not (raw `if ((*(byte *)0x5382 & 1) == 0)`). */
static int unit_treasure_cash_in_silent_under_woi(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  quiet_5d04_planner(&f, nation);
  AiPopupState popups;
  ai_popup_init(&popups);
  f.ctx.ai_popups = &popups;
  f.col1.head.game_options.woi = 1;

  const int ti = treasure_add_type(&f);
  treasure_add_colony(&f, 0, nation, 4, 4);

  const int tid = units_spawn(&f.units, ti, 4, 4);
  ColonizeUnit* t = units_get(&f.units, tid);
  if (!t) {
    fixture_free(&f);
    return fail("spawn treasure (woi)");
  }
  t->nation_id = nation;
  t->moves_left = 1 * UNITS_MP_PER_TILE;
  t->orders = 0;
  t->profession = 4; /* 400 */

  const uint32_t gold_before = f.col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&f.ctx, nation);

  t = units_get(&f.units, tid);
  if (t && t->active) {
    fixture_free(&f);
    return fail("WoI cash-in must still destroy the Treasure");
  }
  if (f.col1.nation[nation].gold - gold_before != 400u) {
    fixture_free(&f);
    return fail("WoI cash-in must still credit the treasury");
  }
  for (int i = 0; i < popups.queue_count; ++i) {
    if (strstr(popups.queue[i].body, "treasure fleet") != NULL) {
      fixture_free(&f);
      return fail("@LOOTFOREIGN must not fire once the WoI flag is set");
    }
  }
  fixture_free(&f);
  return 0;
}

/* iStack_2e != 0: a Treasure that is not standing in an own colony falls to
 * the later arms (bind + goto / rendezvous) and keeps its gold. */
static int unit_treasure_outside_colony_not_cashed(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  quiet_5d04_planner(&f, nation);
  const int ti = treasure_add_type(&f);
  treasure_add_colony(&f, 0, nation, 4, 4);

  const int tid = units_spawn(&f.units, ti, 9, 9);
  ColonizeUnit* t = units_get(&f.units, tid);
  if (!t) {
    fixture_free(&f);
    return fail("spawn treasure (outside)");
  }
  t->nation_id = nation;
  t->moves_left = 1 * UNITS_MP_PER_TILE;
  t->orders = 0;
  t->profession = 9;

  const uint32_t gold_before = f.col1.nation[nation].gold;
  ai_euro_dispatcher_turn(&f.ctx, nation);

  t = units_get(&f.units, tid);
  if (!t || !t->active) {
    fixture_free(&f);
    return fail("Treasure away from a colony must not be cashed/destroyed");
  }
  if (f.col1.nation[nation].gold != gold_before) {
    fixture_free(&f);
    return fail("no treasury credit until the Treasure reaches a colony");
  }
  fixture_free(&f);
  return 0;
}

int main(void) {
  if (unit_treasure_in_colony_cash_in() != 0) {
    return 1;
  }
  if (unit_treasure_cash_in_silent_under_woi() != 0) {
    return 1;
  }
  if (unit_treasure_outside_colony_not_cashed() != 0) {
    return 1;
  }
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
  if (unit_ship_berth_dumps_whole_hull() != 0) {
    return 1;
  }
  if (unit_berth_marks_then_assembles_passenger() != 0) {
    return 1;
  }
  if (unit_work_queue_pickup_aims_at_goods_colony() != 0) {
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
