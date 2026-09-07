/*
 * Focused unit tests for three recently ported FUN_521d_20e6 arms that the
 * existing tests/unit/test_ai_euro_20e6.c smoke only touches in passing:
 *
 *   ai_euro_20e6_transport_assemble   (FUN_1427_10be, raw 3024-3051 + 0x3609)
 *   ai_euro_20e6_wagon_origin_walk    (LAB_521d_457e wagon arm, raw 2256-2289)
 *   ai_euro_20e6_delivery_sell_tail   (raw 2140-2163 + the 4393 fall-through)
 *
 * All three are static inside src/core/ai_euro.c, so the seam is the same one
 * the existing 20e6 test uses: ai_euro_dispatcher_turn on a hand-built
 * fixture. No production code is touched by these tests.
 *
 * Every 10be case below stages its passengers as ordinary LAND-tile members
 * of the berth stack (in the colony the ship is berthed at), so the
 * assertions ride on the mark/size arms only — they stay valid if 10be later
 * grows extra force-board arms for off-map or ocean-tile passengers.
 *
 * Fixture rule (learned the hard way, see the -O0/-O3 split of 2026-09-07):
 * every fixture field the code under test reads must be set DELIBERATELY —
 * census window, founding_father[], colony population, euro prices — never
 * left at whatever memset(0) happens to mean this week.
 * See original_sources_annotated/ai/move_scoring_20e6_full.md for the raw
 * line numbers cited in each case.
 */
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_euro_20e6_ports: FAIL %s\n", msg);
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

/*
 * Unit roster: the DOS @UNIT code the AI reads is derived from the type NAME
 * (ai_euro_5d04_dos_type_of / ai_euro_20e6_dos_type), so the names matter:
 *   0 Free Colonist (0x00)  1 Soldier (0x01)  2 Caravel (0x0d, 2 holds)
 *   3 Wagon Train  (0x0c)   4 Pioneer (0x02, size 1 = one ship slot)
 */
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
  f->units.type_count = 5;
  snprintf(f->units.types[0].name, sizeof(f->units.types[0].name), "Free Colonist");
  f->units.types[0].movement = 1;
  f->units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  f->units.types[0].space = 1;
  snprintf(f->units.types[1].name, sizeof(f->units.types[1].name), "Soldier");
  f->units.types[1].movement = 1;
  f->units.types[1].attack = 2;
  f->units.types[1].defense = 2;
  f->units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  f->units.types[1].space = 1;
  snprintf(f->units.types[2].name, sizeof(f->units.types[2].name), "Caravel");
  f->units.types[2].movement = 4;
  f->units.types[2].cargo = 2;
  f->units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
  snprintf(f->units.types[3].name, sizeof(f->units.types[3].name), "Wagon Train");
  f->units.types[3].movement = 2;
  f->units.types[3].cargo = 2;
  f->units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(f->units.types[4].name, sizeof(f->units.types[4].name), "Pioneer");
  f->units.types[4].movement = 1;
  f->units.types[4].domain = COLONIZE_UNIT_DOMAIN_LAND;
  f->units.types[4].space = 1; /* @UNIT size column (DS:0x5238) */
  colonies_init(&f->colonies);
  col1_save_init(&f->col1);
  memset(f->col1.nation, 0, sizeof(f->col1.nation));
  memset(f->col1.head.nation_relation, 0, sizeof(f->col1.head.nation_relation));
  /* Founding fathers: nobody has any. Zeroing this array by accident has
   * produced false passes before (a zeroed slot reads as "nation 0 owns FF
   * 0"), so it is set here on purpose and asserted nowhere. */
  memset(f->col1.head.founding_father, 0xff, sizeof(f->col1.head.founding_father));
  for (int i = 0; i < 4; ++i) {
    f->col1.player[i].control = 0;
    f->col1.player[i].diplomacy = 0;
  }
  f->col1.nation[nation].gold = 300;
  /* Census window: deliberately non-blank. ship_counts != 0 keeps the 5d04
   * planner off the "no ships" gold floor; ship_cargo_totals > the
   * cargo_short threshold keeps bVar7 clear; the ship-buy ladder's own gate
   * (`ship_cargo_totals > census_pop_proxy/2 + colony_counts`) is already
   * false at 4 vs 1, so no Europe purchase perturbs these fixtures. */
  f->col1.stuff.ship_counts[nation] = 1;
  f->col1.stuff.ship_cargo_totals[nation] = 4;
  f->col1.stuff.colony_counts[nation] = 1;
  f->col1.stuff.census_pop_proxy[nation] = 0;
  f->col1.stuff.free_colonist_counts[nation] = 1;
  f->col1.head.turn = 5;
  f->col1.head.year = 1500;
  f->col1.head.difficulty = 0;
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

/* Own coastal colony at (11,4) with the east side of the map as ocean. */
static ColonizeColony* fixture_coastal_colony(Fixture* f, int nation) {
  for (int y = 0; y < 16; ++y) {
    for (int x = 12; x < 16; ++x) {
      f->map.terrain[y * 16 + x] = 25; /* MAP_OCEAN_INDEX */
    }
  }
  ColonizeColony* c = &f->colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 11;
  c->y = 4;
  c->population = 3;   /* >= 3: no NEEDS_COLONISTS flag */
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 200; /* no shortage -> no labor admission */
  c->building_in_production = -1;
  f->colonies.colony_count = 1;
  f->colonies.next_id = 1;
  return c;
}

static int ship_goods_holds(const ColonizeUnit* ship) {
  int n = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    const int amt = ship->hold_goods_amount[h];
    if (amt > 0 && amt < 255) {
      n++;
    }
  }
  return n;
}

/*
 * === Target 1: ai_euro_20e6_transport_assemble =============================
 *
 * Case 1a — the mark scan reserves the WHOLE hull and 10be re-derives that
 * capacity for itself (`free = 0x5237[type] − +0x3150`, raw 8658 + the 10be
 * prologue). Two Pioneers stand in the colony a 2-hold Caravel is berthed at:
 * the raw 3024-3051 scan marks both and debits iStack_d2 to 0, the load
 * matrix (raw 3052-3131) therefore has NOTHING to spend, and the fall-through
 * to 0x3609 must board both.
 *
 * The colony holds 60 Silver on purpose: without the reservation the load
 * matrix would take a hold for it (the existing smoke's one-passenger case
 * proves it does when a hold is left free) and 10be's own
 * `capacity − holds_occupied` would then have room for only ONE Pioneer — so
 * "both aboard" is only reachable if the scan's budget survived into 10be.
 * AI_20E6_SHIP_DUMP_TRACE on this fixture shows the DOS shape exactly:
 *   MARKS unit 2 / MARKS unit 3 / (no [load] line) / assembles 2 / assembles 3.
 * The dispatcher's outer any_acted loop then gives the ship a SECOND act, in
 * which the (passenger-blind) `ai_euro_hauler_free_holds` does take a hold of
 * Silver; that is a separate question from the mark→assemble hand-off, so the
 * assertion here is about the passengers only.
 */
static int assemble_boards_whole_reserved_hull(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  ColonizeColony* c = fixture_coastal_colony(&f, nation);
  c->stock[COLONIZE_CARGO_SILVER] = 60; /* the load matrix's only candidate */
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_SILVER] = 20;

  const int ship_id = units_spawn(&f.units, 2, 12, 4);
  const int p1 = units_spawn(&f.units, 4, 11, 4);
  const int p2 = units_spawn_allow_stack(&f.units, 4, 11, 4);
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  ColonizeUnit* a = units_get(&f.units, p1);
  ColonizeUnit* b = units_get(&f.units, p2);
  if (!ship || !a || !b) {
    fixture_free(&f);
    return fail("spawn assemble units");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  a->nation_id = nation;
  /* moves_left = 0 keeps the land act loop off these two (the dispatcher
   * skips a spent unit once the nation owns a colony), so the only thing that
   * can move them this turn is the ship's own berth act. The raw 3024-3051
   * scan does not look at movement points. */
  a->moves_left = 0;
  a->orders = 0;
  b->nation_id = nation;
  b->moves_left = 0;
  b->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  ship = units_get(&f.units, ship_id);
  a = units_get(&f.units, p1);
  b = units_get(&f.units, p2);
  if (!ship || !ship->active || !a || !a->active || !b || !b->active) {
    fixture_free(&f);
    return fail("assemble unit vanished");
  }
  if (a->aboard_ship_id != ship_id || b->aboard_ship_id != ship_id) {
    fprintf(stderr, "aboard=(%d,%d) want (%d,%d) goods_holds=%d\n", a->aboard_ship_id,
            b->aboard_ship_id, ship_id, ship_id, ship_goods_holds(ship));
    fixture_free(&f);
    return fail("10be did not board the whole reserved hull");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Case 1b — the size gate. Same berth, ONE Pioneer, but the @UNIT size column
 * (0x5238, ColonizeUnitType.space) says the unit needs 3 ship slots and the
 * Caravel has 2. Both the mark scan (`space > free_holds`) and 10be's own
 * copy of the test must refuse it, and the hull the scan did NOT reserve then
 * goes to the load matrix instead.
 */
static int assemble_refuses_oversize_passenger(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  f.units.types[4].space = 3; /* bigger than the Caravel's 2 slots */
  ColonizeColony* c = fixture_coastal_colony(&f, nation);
  c->stock[COLONIZE_CARGO_SILVER] = 60;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_SILVER] = 20;

  const int ship_id = units_spawn(&f.units, 2, 12, 4);
  const int pid = units_spawn(&f.units, 4, 11, 4);
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  ColonizeUnit* p = units_get(&f.units, pid);
  if (!ship || !p) {
    fixture_free(&f);
    return fail("spawn oversize units");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  p->nation_id = nation;
  p->moves_left = 0; /* see the note in the two-passenger case */
  p->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  ship = units_get(&f.units, ship_id);
  p = units_get(&f.units, pid);
  if (!ship || !ship->active || !p || !p->active) {
    fixture_free(&f);
    return fail("oversize unit vanished");
  }
  if (p->aboard_ship_id >= 0) {
    fprintf(stderr, "oversize passenger aboard=%d holds=%d\n", p->aboard_ship_id,
            ship_goods_holds(ship));
    fixture_free(&f);
    return fail("10be boarded a passenger larger than the free hull");
  }
  if (ship_goods_holds(ship) == 0) {
    fixture_free(&f);
    return fail("unreserved hull should have gone to the load matrix");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Case 1c — 10be boards ONLY what this act's scan marked (raw 8658:
 * `if (act_state != 1) continue`). A Free Colonist standing in the colony the
 * ship is berthed at is on the berth stack and the hull is empty, but the raw
 * 3024-3051 scan has no arm for it: its 0x5236 combat is not > 1 and it is not
 * a Pioneer (type 0x02), so it must still be ashore at the end of the beat.
 *
 * This is the gate that makes the stale board-mark sweep at the head of the
 * arrival block (raw 2991-2997) matter at all — a leftover act_state = 1 on a
 * berth-stack member is by itself enough to board it — and it is the half of
 * that mechanism the dispatcher seam can observe: the sweep itself only bites
 * on a passenger landed EARLIER in the same act, which no fixture can stage
 * from outside ai_euro.c (see the notes at the end of this file).
 */
static int assemble_ignores_unmarked_stack_member(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  ColonizeColony* c = fixture_coastal_colony(&f, nation);
  c->stock[COLONIZE_CARGO_SILVER] = 60;
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_SILVER] = 20;

  const int ship_id = units_spawn(&f.units, 2, 12, 4);
  const int cid = units_spawn(&f.units, 0, 11, 4); /* Free Colonist in the colony */
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  ColonizeUnit* col = units_get(&f.units, cid);
  if (!ship || !col) {
    fixture_free(&f);
    return fail("spawn unmarked stack member");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  col->nation_id = nation;
  col->moves_left = 0; /* the land act loop skips it; the mark scan does not care */
  col->orders = 0;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  col = units_get(&f.units, cid);
  ship = units_get(&f.units, ship_id);
  if (!col || !col->active || !ship || !ship->active) {
    fixture_free(&f);
    return fail("unmarked stack member vanished");
  }
  if (col->aboard_ship_id >= 0) {
    fprintf(stderr, "unmarked colonist aboard=%d at (%d,%d)\n", col->aboard_ship_id, col->x,
            col->y);
    fixture_free(&f);
    return fail("10be boarded a stack member the mark scan never marked");
  }
  fixture_free(&f);
  return 0;
}

/*
 * === Target 2: ai_euro_20e6_wagon_origin_walk ==============================
 *
 * Case 2a — bind + park (raw 2263-2270). A wagon standing on an own colony
 * whose +0x314a origin resolves to that colony writes orders 0x55 and jumps
 * to LAB_5899: the beat is claimed with NO goto. (The 5952_035e colony tick
 * binds an unbound land unit standing on a colony first, exactly as DOS does,
 * so this case asserts the settled state: bound to THIS colony, parked on it.)
 */
static int wagon_binds_and_parks_at_own_colony(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  ColonizeColony* c = fixture_coastal_colony(&f, nation);
  c->x = 4;
  c->y = 4;
  /* Nothing loadable: the arrival LOAD matrix must not latch a village errand
   * and steal the beat (raw 3136 = +0x3158). */
  c->stock[COLONIZE_CARGO_FOOD] = 8;

  const int wid = units_spawn(&f.units, 3, 4, 4);
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!w) {
    fixture_free(&f);
    return fail("spawn wagon");
  }
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;
  w->col1_origin = 0xff; /* unbound */

  ai_euro_dispatcher_turn(&f.ctx, nation);

  w = units_get(&f.units, wid);
  if (!w || !w->active) {
    fixture_free(&f);
    return fail("parked wagon was destroyed");
  }
  if (w->col1_origin != 0) {
    fprintf(stderr, "wagon origin=%u want 0\n", (unsigned)w->col1_origin);
    fixture_free(&f);
    return fail("origin walk did not bind the wagon to the colony it stands on");
  }
  if (w->x != 4 || w->y != 4) {
    fprintf(stderr, "wagon at (%d,%d) want (4,4)\n", w->x, w->y);
    fixture_free(&f);
    return fail("parked wagon should not leave its bound colony");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Case 2b — walk home to the BOUND colony, not the nearest one (raw
 * 2271-2289: `bind colony +0x314a; goto LAB_4567`). Two own colonies: the
 * near one at (7,8) and the far one at (2,8); the wagon at (8,8) is already
 * bound to the FAR one. A port that re-derived "nearest own colony" here
 * (which is what the retired 4393 substitute effectively did) would send it
 * one tile west to (7,8).
 */
static int wagon_walks_home_to_bound_colony(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  ColonizeColony* far_c = &f.colonies.colonies[0];
  far_c->id = 0;
  far_c->active = true;
  far_c->nation_id = nation;
  far_c->x = 2;
  far_c->y = 8;
  far_c->population = 3;
  far_c->colonist_count = 3;
  far_c->stock[COLONIZE_CARGO_FOOD] = 8;
  far_c->building_in_production = -1;
  ColonizeColony* near_c = &f.colonies.colonies[1];
  near_c->id = 1;
  near_c->active = true;
  near_c->nation_id = nation;
  near_c->x = 7;
  near_c->y = 8;
  near_c->population = 3;
  near_c->colonist_count = 3;
  near_c->stock[COLONIZE_CARGO_FOOD] = 8;
  near_c->building_in_production = -1;
  f.colonies.colony_count = 2;
  f.colonies.next_id = 2;
  f.col1.stuff.colony_counts[nation] = 2;

  const int wid = units_spawn(&f.units, 3, 8, 8);
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!w) {
    fixture_free(&f);
    return fail("spawn wagon");
  }
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;
  w->col1_origin = 0; /* bound to the FAR colony */

  ai_euro_dispatcher_turn(&f.ctx, nation);

  w = units_get(&f.units, wid);
  if (!w || !w->active) {
    fixture_free(&f);
    return fail("bound wagon was destroyed");
  }
  const int heads_far = (units_orders_follow_goto(w->orders) && w->goto_x == 2 &&
                         w->goto_y == 8) ||
                        (w->x == 2 && w->y == 8);
  if (!heads_far || w->col1_origin != 0) {
    fprintf(stderr, "wagon origin=%u orders=%d pos=(%d,%d) goto=(%d,%d)\n",
            (unsigned)w->col1_origin, w->orders, w->x, w->y, w->goto_x, w->goto_y);
    fixture_free(&f);
    return fail("origin walk should walk the wagon to its BOUND colony");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Case 2c — the destroy arm (LAB_521d_47b9, raw 2277-2282): unbound wagon,
 * `iStack_2c != iStack_38` — the nearest own colony is not on this landmass.
 * The existing smoke covers the "no colonies at all" reading of that compare
 * (iStack_2c = −2); this covers the real one, an own colony on ANOTHER
 * landmass, which is the only way the two continent ids can differ.
 */
static int wagon_off_landmass_is_destroyed(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  /*
   * Two landmasses. map_continent_id_at reads the layer3 LOW NIBBLE (water
   * answers -1), so the split has to be written there as well as in the
   * terrain: a terrain-only channel leaves every land tile on body 0 and the
   * `iStack_2c != iStack_38` compare can never fire.
   */
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      f.map.layer3[y * 16 + x] = (uint8_t)(x < 8 ? 2 : 3);
    }
    f.map.terrain[y * 16 + 8] = 25; /* MAP_OCEAN_INDEX channel */
    f.map.layer3[y * 16 + 8] = 1;   /* open sea body */
  }
  ColonizeColony* c = &f.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 3; /* west landmass */
  c->y = 8;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 8;
  c->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;

  const int wid = units_spawn(&f.units, 3, 12, 8); /* east landmass */
  ColonizeUnit* w = units_get(&f.units, wid);
  if (!w) {
    fixture_free(&f);
    return fail("spawn wagon");
  }
  w->nation_id = nation;
  w->moves_left = 2 * UNITS_MP_PER_TILE;
  w->orders = 0;
  w->col1_origin = 0xff; /* unbound */

  ai_euro_dispatcher_turn(&f.ctx, nation);

  w = units_get(&f.units, wid);
  if (w && w->active) {
    fprintf(stderr, "wagon survived at (%d,%d) origin=%u orders=%d\n", w->x, w->y,
            (unsigned)w->col1_origin, w->orders);
    fixture_free(&f);
    return fail("unbound wagon with no own colony on its landmass must be destroyed");
  }
  fixture_free(&f);
  return 0;
}

/*
 * === Target 4: ai_euro_20e6_delivery_sell_tail =============================
 *
 * Case 4a — the DOS double book plus the UNTAXED treasury credit (raw
 * 2140-2163). FUN_291f_0a2e (= europe_apply_trade_volume) has already
 * credited trade.gold[g] with the TAX-ADJUSTED proceeds and trade.tons[g] /
 * tons2[g] with the amount when the tail then adds euro_price*qty to gold[g]
 * and qty to tons[g] a SECOND time — and the +0x2a treasury credit is single
 * and pays no Crown cut at all.
 *
 * 100 Tools at euro_price 2 (bid 3 → DOS sell price = bid−1 = 2) with a 50%
 * tax rate therefore lands as:
 *   nation gold   += 2*100          = 200   (untaxed, single)
 *   trade.gold[T] += 100 + 200      = 300   (taxed ledger + untaxed tail)
 *   trade.tons[T] += 100 + 100      = 200   (double booked)
 *   trade.tons2[T]+= 100                    (ledger only — the tail never
 *                                            touches +0xfc)
 * A port that "fixed" the double book, or that taxed the treasury credit,
 * fails on the exact numbers.
 *
 * The colony is INLAND so the delivery matrix's coastal gate (raw 2054)
 * rejects it and the tail is the only consumer of the hold.
 */
static int sell_tail_untaxed_credit_and_double_book(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  EuropeScreen* eu = calloc(1, sizeof(EuropeScreen));
  if (!eu) {
    fixture_free(&f);
    return fail("alloc europe screen");
  }
  /* Headless market: only the fields europe_apply_trade_volume reads. */
  eu->cargo_count = 16;
  eu->difficulty = 0;
  for (int g = 0; g < 16; ++g) {
    snprintf(eu->cargo[g].name, sizeof(eu->cargo[g].name), "cargo%d", g);
    eu->cargo[g].bid = 1;
    eu->cargo[g].ask = 2;
    eu->cargo[g].volatility = 0;
    eu->cargo[g].attrition = 0;
  }
  eu->cargo[COLONIZE_CARGO_TOOLS].bid = 3; /* sell price = bid − 1 = 2 */
  eu->cargo[COLONIZE_CARGO_TOOLS].ask = 4;
  f.ctx.europe = eu;

  for (int y = 0; y < 16; ++y) {
    for (int x = 12; x < 16; ++x) {
      f.map.terrain[y * 16 + x] = 25; /* MAP_OCEAN_INDEX */
    }
  }
  ColonizeColony* c = &f.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 5; /* inland: raw 2054 coastal gate fails for every neighbour */
  c->y = 4;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 60;
  c->stock[COLONIZE_CARGO_TOOLS] = 150;
  c->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;

  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_TOOLS] = 2;
  f.col1.nation[nation].tax_rate = 50;

  const int ship_id = units_spawn(&f.units, 2, 14, 4);
  ColonizeUnit* ship = units_get(&f.units, ship_id);
  if (!ship) {
    free(eu);
    fixture_free(&f);
    return fail("spawn ship");
  }
  ship->nation_id = nation;
  ship->moves_left = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  if (units_load_goods(&f.units, ship_id, COLONIZE_CARGO_TOOLS, 100) <= 0) {
    free(eu);
    fixture_free(&f);
    return fail("load tools");
  }
  const uint32_t gold_before = f.col1.nation[nation].gold;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  const ColonizeCol1NationTrade* t = &f.col1.nation[nation].trade;
  const uint32_t gold = f.col1.nation[nation].gold;
  if (gold != gold_before + 200u || t->gold[COLONIZE_CARGO_TOOLS] != 300 ||
      t->tons[COLONIZE_CARGO_TOOLS] != 200 || t->tons2[COLONIZE_CARGO_TOOLS] != 100) {
    fprintf(stderr,
            "gold %u->%u (want +200) ledger gold=%d (want 300) tons=%d (want 200) tons2=%d "
            "(want 100)\n",
            (unsigned)gold_before, (unsigned)gold, (int)t->gold[COLONIZE_CARGO_TOOLS],
            (int)t->tons[COLONIZE_CARGO_TOOLS], (int)t->tons2[COLONIZE_CARGO_TOOLS]);
    free(eu);
    fixture_free(&f);
    return fail("sell tail ledgers do not match the DOS double book");
  }
  free(eu);
  fixture_free(&f);
  return 0;
}

/*
 * Case 4b — the fall-through (raw 2163-2168 → LAB_004393). Once the tail has
 * zeroed holds_occupied the arrival block's own consumer
 * (`capacity == occupied || 1 < occupied → LAB_3fa6`) cannot fire, so the
 * emptied ship reaches the 4393 work-queue peel in the SAME beat.
 *
 * Fixture: one own coastal colony that (a) rejects the Tools delivery because
 * it PRODUCES tools and already holds 150 (raw 2067-2070) and (b) is sitting
 * on 150 Ore, which registers it in the 0a60 pickup queue. So: sell, then
 * sail for that colony's berth with an empty hull.
 */
static int sell_tail_falls_through_to_work_queue(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation) != 0) {
    return 1;
  }
  f.turn = 9;
  f.col1.head.turn = 9;
  for (int x = 0; x < 16; ++x) {
    f.map.terrain[3 * 16 + x] = 25; /* ocean row: colonies at y=4 are coastal */
  }
  ColonizeColony* rich = &f.colonies.colonies[0];
  rich->id = 0;
  rich->active = true;
  rich->nation_id = nation;
  rich->x = 10;
  rich->y = 4;
  rich->population = 3;
  rich->colonist_count = 3;
  rich->stock[COLONIZE_CARGO_FOOD] = 60;
  rich->stock[COLONIZE_CARGO_TOOLS] = 150; /* > 99 and produced → delivery says no */
  rich->stock[COLONIZE_CARGO_ORE] = 150;   /* the pickup queue's goods */
  rich->cargo_produced_mask = (uint16_t)(1u << COLONIZE_CARGO_TOOLS);
  rich->building_in_production = -1;
  f.colonies.colony_count = 1;
  f.colonies.next_id = 1;

  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_TOOLS] = 2;

  const int ship_id = units_spawn(&f.units, 2, 1, 3); /* far west, berthed nowhere */
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
  if (ship_goods_holds(ship) != 0 || f.col1.nation[nation].gold != gold_before + 200u) {
    fprintf(stderr, "holds=%d gold %u->%u\n", ship_goods_holds(ship), (unsigned)gold_before,
            (unsigned)f.col1.nation[nation].gold);
    fixture_free(&f);
    return fail("sell tail should have dumped the hold for euro_price gold");
  }
  const int heads_for_rich =
    (units_orders_follow_goto(ship->orders) && ship->goto_x >= 0 &&
     abs(ship->goto_x - 10) <= 1 && abs(ship->goto_y - 4) <= 1) ||
    (abs(ship->x - 10) <= 1 && abs(ship->y - 4) <= 1);
  if (!heads_for_rich) {
    fprintf(stderr, "ship orders=%d pos=(%d,%d) goto=(%d,%d)\n", ship->orders, ship->x, ship->y,
            ship->goto_x, ship->goto_y);
    fixture_free(&f);
    return fail("emptied hull did not fall through to the 4393 work-queue tip");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Not covered here, and why:
 *
 *   - The stale board-mark sweep itself (ai_euro_20e6_clear_stale_board_marks,
 *     raw 2991-2997). It only bites on a passenger that was put ashore at the
 *     berth EARLIER IN THE SAME ACT, and the arrival block is the first thing
 *     a berthed ship's act runs, so no fixture built from outside ai_euro.c
 *     can stage that ordering: a passenger the fixture boards is still aboard
 *     when the sweep runs (which skips aboard units), and the port's own
 *     unload arms fire after it. Case 1c covers the gate that makes the sweep
 *     matter (10be boards on the mark alone). Reaching the sweep directly
 *     would need a production seam that does not exist today.
 *
 *   - Observation, not asserted: after the two-passenger case boards both
 *     Pioneers, the dispatcher's outer any_acted loop gives the ship a second
 *     act in which `ai_euro_hauler_free_holds` (goods holds only) hands the
 *     load matrix a hold, leaving a 2-slot Caravel carrying 2 passengers AND
 *     1 cargo hold. DOS's `+0x3150` counts passengers and goods together
 *     (see ai_euro_20e6_transport_assemble's own header), so this looks like
 *     a real divergence — but fixing it is production work, not test work.
 */

int main(void) {
  if (assemble_boards_whole_reserved_hull() != 0) {
    return 1;
  }
  if (assemble_refuses_oversize_passenger() != 0) {
    return 1;
  }
  if (assemble_ignores_unmarked_stack_member() != 0) {
    return 1;
  }
  if (wagon_binds_and_parks_at_own_colony() != 0) {
    return 1;
  }
  if (wagon_walks_home_to_bound_colony() != 0) {
    return 1;
  }
  if (wagon_off_landmass_is_destroyed() != 0) {
    return 1;
  }
  if (sell_tail_untaxed_credit_and_double_book() != 0) {
    return 1;
  }
  if (sell_tail_falls_through_to_work_queue() != 0) {
    return 1;
  }
  printf("unit_ai_euro_20e6_ports: OK\n");
  return 0;
}
