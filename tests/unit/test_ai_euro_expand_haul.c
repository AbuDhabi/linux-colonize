/* Slice of the former tests/unit/test_ai_euro_expand.c (split by feature 2026-09-23):
 * wagon and ship haul errands, food delivery, cargo/specialty haul preference. */
#include "test_ai_euro_expand_common.h"
#include "core/ai_euro_internal.h"

/*
 * LABOR bind: idle Free Colonist adjacent to own colony with food_short
 * (and a distant FOUND lure) → LABOR goto / join, not yank to FOUND.
 * Cite: 5b66 unload/labor + 5cf6 food_short; no invented production.
 */

/*
 * Col1 +0x8e labor_shortage: Free Colonist on colony tile joins LABOR and
 * decrements the counter (FUN_521d_5b66 ~91589). Cite: euro_unit_act case 0x0b.
 */

/*
 * Col1 +0x8d specialty_cargo does NOT steer what a hauler loads. The DOS LOAD
 * matrix (FUN_521d_20e6 raw 3059-3134) scores by cargo and stock only; the
 * specialty byte's surviving role is the port's Series R +32 tie-break inside
 * the 4393 work-queue pick (covered by unit_specialty_flag_a_haul_match).
 * Second half of this case is a direct FUN_5952_0306 unit test: a
 * warehouse-full stock clears the specialty.
 *
 * 2026-09-06g rewrite. The old fixture asserted the retired Linux ladder's
 * "specialty first" reorder by expecting 20 LUMBER aboard; cargo 5 is skipped
 * for every hauler in DOS, so that load cannot happen. The colony keeps its
 * LUMBER specialty and now also holds 80 ORE, and the assertion is that the
 * matrix takes the ORE and leaves the specialty Lumber alone.
 */

/*
 * Col1 +0x90 cargo_produced_mask in the DOS LOAD matrix (FUN_521d_20e6 raw
 * 3059-3134): the mask gates cargo 0xe/0xf (Tools / Muskets), and that arm is
 * SHIPS ONLY, so a Wagon Train standing on a colony that produces Tools still
 * refuses them. Cargo 5 (Lumber) is skipped for every hauler, ship or wagon.
 * What the wagon does take is the highest-scoring cargo the matrix allows —
 * here ORE.
 *
 * 2026-09-06g rewrite. The old fixture asserted the retired Linux ladder's
 * "prefer produced surplus" reorder by expecting the wagon to load 20 LUMBER;
 * DOS never loads Lumber onto anything. Fixture keeps the same shape (a
 * producing supply colony and a second colony to travel to) with DOS's own
 * numbers: 80 ORE clears the matrix's `term >= 0x32` floor, TOOLS stay marked
 * produced so the ships-only rejection is what keeps them off the wagon, and
 * the far colony holds 80 RUM so it clears the 0x4a work-queue gate.
 */
static int unit_cargo_produced_mask_haul_prefer(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("produced-mask alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* supply = &colonies.colonies[0];
  supply->id = 0;
  supply->active = true;
  supply->nation_id = nation;
  supply->x = 4;
  supply->y = 4;
  supply->population = 3;
  supply->colonist_count = 3;
  supply->stock[COLONIZE_CARGO_TOOLS] = 50; /* produced, but ships only */
  supply->stock[COLONIZE_CARGO_LUMBER] = 50; /* cargo 5 — skipped for everyone */
  supply->stock[COLONIZE_CARGO_ORE] = 80;    /* over the matrix 0x32 floor */
  supply->stock[COLONIZE_CARGO_FOOD] = 5;
  supply->building_in_production = -1;
  supply->specialty_cargo = 0xff;
  supply->cargo_produced_mask =
    (uint16_t)((1u << COLONIZE_CARGO_LUMBER) | (1u << COLONIZE_CARGO_TOOLS));

  ColonizeColony* shortc = &colonies.colonies[1];
  shortc->id = 1;
  shortc->active = true;
  shortc->nation_id = nation;
  shortc->x = 8;
  shortc->y = 4;
  shortc->population = 3;
  shortc->colonist_count = 3;
  shortc->stock[COLONIZE_CARGO_LUMBER] = 0;
  shortc->stock[COLONIZE_CARGO_TOOLS] = 40;
  shortc->stock[COLONIZE_CARGO_FOOD] = 80;
  shortc->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers work */
  shortc->building_in_production = -1;
  shortc->specialty_cargo = 0xff;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("produced-mask spawn wagon");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;

  ai_goals_reset();
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 63;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  int lumber_loaded = 0;
  int tools_loaded = 0;
  int ore_loaded = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (wagon->hold_goods_amount[h] <= 0 || wagon->hold_goods_amount[h] >= 255) {
        continue;
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_LUMBER) {
        lumber_loaded += wagon->hold_goods_amount[h];
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
        tools_loaded += wagon->hold_goods_amount[h];
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_ORE) {
        ore_loaded += wagon->hold_goods_amount[h];
      }
    }
  }
  if (ore_loaded < 20 || lumber_loaded != 0 || tools_loaded != 0) {
    fprintf(stderr,
      "unit_ai_euro_expand: produced ore=%d lumber=%d tools=%d mask=0x%x "
      "active=%d pos=(%d,%d) c0ore=%d c1ore=%d\n",
      ore_loaded, lumber_loaded, tools_loaded,
      (unsigned)colonies.colonies[0].cargo_produced_mask,
      wagon ? (int)wagon->active : -1, wagon ? wagon->x : -1, wagon ? wagon->y : -1,
      colonies.colonies[0].stock[COLONIZE_CARGO_ORE],
      colonies.colonies[1].stock[COLONIZE_CARGO_ORE]);
    fx_map_free(&map);
    return fail("expected wagon to load ORE, never Lumber or produced Tools");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: cargo_produced_mask haul prefer ok\n");
  return 0;
}

static int unit_specialty_cargo_haul_prefer(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("specialty alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Pioneer");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* supply = &colonies.colonies[0];
  supply->id = 0;
  supply->active = true;
  supply->nation_id = nation;
  supply->x = 4;
  supply->y = 4;
  supply->population = 3;
  supply->colonist_count = 3;
  supply->stock[COLONIZE_CARGO_TOOLS] = 10;
  supply->stock[COLONIZE_CARGO_LUMBER] = 50; /* the specialty — cargo 5, never loaded */
  supply->stock[COLONIZE_CARGO_ORE] = 80;    /* what the matrix actually takes */
  supply->stock[COLONIZE_CARGO_FOOD] = 5; /* not food surplus (avoids specialty=FOOD) */
  supply->building_in_production = -1;
  supply->specialty_cargo = (uint8_t)COLONIZE_CARGO_LUMBER;
  /* Short colony so haul binds. */
  ColonizeColony* shortc = &colonies.colonies[1];
  shortc->id = 1;
  shortc->active = true;
  shortc->nation_id = nation;
  shortc->x = 8;
  shortc->y = 4;
  shortc->population = 3;
  shortc->colonist_count = 3;
  shortc->stock[COLONIZE_CARGO_LUMBER] = 0;
  shortc->stock[COLONIZE_CARGO_TOOLS] = 40;
  shortc->stock[COLONIZE_CARGO_FOOD] = 80;
  shortc->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers work */
  shortc->building_in_production = -1;
  shortc->specialty_cargo = 0xff;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("specialty spawn wagon");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 60;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  int lumber_loaded = 0;
  int ore_loaded = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (wagon->hold_goods_amount[h] <= 0 || wagon->hold_goods_amount[h] >= 255) {
        continue;
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_LUMBER) {
        lumber_loaded += wagon->hold_goods_amount[h];
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_ORE) {
        ore_loaded += wagon->hold_goods_amount[h];
      }
    }
  }
  if (ore_loaded < 20 || lumber_loaded != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: specialty ore=%d lumber=%d specialty=%u tools=%d\n",
      ore_loaded,
      lumber_loaded,
      (unsigned)colonies.colonies[0].specialty_cargo,
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS]
    );
    fx_map_free(&map);
    return fail("expected wagon to load matrix ORE, not the LUMBER specialty");
  }

  /* FUN_5952_0306: warehouse-full clears specialty. */
  ColonizeColony* c0 = &colonies.colonies[0];
  c0->specialty_cargo = (uint8_t)COLONIZE_CARGO_LUMBER;
  c0->stock[COLONIZE_CARGO_LUMBER] = colonies_warehouse_capacity(&colonies, c0, COLONIZE_CARGO_LUMBER);
  colonies_specialty_cargo_update(&colonies, c0, COLONIZE_CARGO_LUMBER, 1, 0);
  if (c0->specialty_cargo != 0xff) {
    fx_map_free(&map);
    return fail("expected specialty clear when stock >= warehouse cap");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: specialty_cargo haul prefer ok\n");
  return 0;
}

/*
 * Series R: 4393 specialty match — two equal-distance colonies that BOTH
 * register work, with distinct specialties; the hauler holds only one type →
 * goto the matching colony. Cite: move_scoring_ship.md thin 4393; Series R.
 *
 * 2026-09-07b rewrite: the 4393 queue is SHIPS-ONLY now (DOS gate
 * `0xc < type < 0x13`, raw :89877; the wagon substitute is retired for the
 * LAB_457e origin walk), so the consumer under test is a Caravel on an ocean
 * row, carrying LUMBER (not a delivery cargo, so the delivery matrix stays
 * out of the way and the laden gate — 1 of 2 holds — does not fire). The
 * colonies move beside the water; the +32 specialty tie-break must aim the
 * ship at the LUMBER-specialty colony A.
 *
 * 2026-09-06g rewrite: registration is DOS's `bVar5` now (raw
 * viceroy_unpacked.c:87663 `if (0x4a < local_2a) bVar5 = true;`), not the old
 * Linux "is this colony short of a haul cargo" boolean, and the queue is a
 * PICKUP queue. The old fixture registered both colonies by making each SHORT
 * of something; a short colony has nothing to collect and no longer registers
 * at all. Both colonies now hold 80 RUM instead — cargo 9 is counted by the
 * DOS gate (it is not FOOD/LUMBER/TRADE_GOODS, and unlike TOOLS/MUSKETS needs
 * no `cargo_produced_mask` bit), 80 > 0x4a arms the gate, and RUM is absent
 * from `ai_euro.c`'s specialty ladder so it leaves the two specialties (and
 * therefore the +32 tie-break this test is about) untouched. The assertion is
 * unchanged: the wagon aims at (4,8).
 *
 * Also the guard for the 2026-09-06d queue-decrement tail: the tip colony's
 * work slot is consumed (and freed) the moment this wagon claims it, and the
 * dispatcher re-enters ai_euro_unit_act for a unit that still has moves — so
 * without the one-claim-per-unit-per-tick latch the second pass re-picks the
 * *other* colony and re-aims the wagon at (8,4). Cite:
 * move_scoring_20e6_full.md 2026-09-06d.
 */
static int unit_specialty_flag_a_haul_match(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("flag_a alloc map");
  }

  /* Ocean row y=1: the ship consumer lives there; both colonies at y=2 are
   * coastal. */
  for (int x = 0; x < 16; ++x) {
    map.terrain[1 * 16 + x] = 25; /* MAP_OCEAN_INDEX */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* Equal MD=4 (same `(d>>2)+1` bucket) from the ship at (5,1). Inventory
   * refreshes specialty from surplus: A lumber surplus → specialty LUMBER;
   * B tools surplus → specialty TOOLS. Both hold 80 RUM so both register on
   * DOS's bVar5 gate (see header). Ship holds LUMBER → +32 picks A. */
  ColonizeColony* a = &colonies.colonies[0];
  a->id = 0;
  a->active = true;
  a->nation_id = nation;
  a->x = 8;
  a->y = 2;
  a->population = 3;
  a->colonist_count = 3;
  /* 2026-09-18: the invented "surplus haul ladder" that used to stamp +0x8d
   * from stock is gone — DOS's only AI writer is FUN_5952_035e's five
   * FUN_5952_0306 calls, which touch ONLY Muskets / Trade Goods / Horses /
   * Tools. LUMBER is not one of those four tags, so a Lumber specialty is
   * exactly what a DOS save carries untouched across the tick, and it is set
   * here directly instead of being conjured from stock. The stock numbers
   * below keep all five DOS arms at want = 0 so none of them re-stamps +0x8d
   * over it: muskets 200 > (belligerence 0 + 2) * 0x32, horses 60 >= 0x32,
   * tools 50 > 0x13 (local_16 latched), and no WAGON_TRAIN flag. */
  a->stock[COLONIZE_CARGO_TOOLS] = 50;
  a->stock[COLONIZE_CARGO_MUSKETS] = 200;
  a->stock[COLONIZE_CARGO_HORSES] = 60;
  a->stock[COLONIZE_CARGO_LUMBER] = 50;
  a->stock[COLONIZE_CARGO_FOOD] = 10; /* not FOOD surplus (avoids specialty overwrite) */
  a->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers this colony */
  a->building_in_production = -1;
  a->cargo_idle_turns = 0;
  a->specialty_cargo = (uint8_t)COLONIZE_CARGO_LUMBER;

  ColonizeColony* b = &colonies.colonies[1];
  b->id = 1;
  b->active = true;
  b->nation_id = nation;
  b->x = 2;
  b->y = 2;
  b->population = 3;
  b->colonist_count = 3;
  b->stock[COLONIZE_CARGO_LUMBER] = 0;
  b->stock[COLONIZE_CARGO_TOOLS] = 50; /* same five-arm quieting as A, above */
  b->stock[COLONIZE_CARGO_MUSKETS] = 200;
  b->stock[COLONIZE_CARGO_HORSES] = 60;
  b->stock[COLONIZE_CARGO_FOOD] = 10; /* not FOOD surplus (avoids specialty overwrite) */
  b->stock[COLONIZE_CARGO_RUM] = 80; /* same, so both slots are eligible */
  b->building_in_production = -1;
  b->cargo_idle_turns = 0;
  b->specialty_cargo = 0xff; /* no specialty → the +32 tie-break must pick A */
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 5, 1);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("flag_a spawn ship");
  }
  wagon->nation_id = nation;
  wagon->moves = 4 * UNITS_MP_PER_TILE;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_LUMBER, 20) <= 0) {
    fx_map_free(&map);
    return fail("flag_a load lumber");
  }

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 62;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  /* Berth = a water tile beside A (8,2); the pick may also have sailed the
   * ship onto that berth already this beat. */
  const int near_a =
    wagon && wagon->active &&
    ((units_orders_follow_goto(wagon->orders) && abs(wagon->goto_x - 8) <= 1 &&
      abs(wagon->goto_y - 2) <= 1) ||
     (abs(wagon->x - 8) <= 1 && abs(wagon->y - 2) <= 1));
  if (!near_a) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: flag_a pos=(%d,%d) goto=(%d,%d) orders=%d specA=%u specB=%u\n",
      wagon ? wagon->x : -1,
      wagon ? wagon->y : -1,
      wagon ? wagon->goto_x : -1,
      wagon ? wagon->goto_y : -1,
      wagon ? wagon->orders : -1,
      (unsigned)colonies.colonies[0].specialty_cargo,
      (unsigned)colonies.colonies[1].specialty_cargo
    );
    fx_map_free(&map);
    return fail("expected ship goto specialty-matching lumber colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: specialty flag_a haul match ok\n");
  return 0;
}

/*
 * Col1 +0x8f cargo_idle_turns: `score += idle * 8` in the 0a60 work-queue
 * REGISTRATION score (raw viceroy_unpacked.c:87677, inside the `if (bVar5)`
 * branch), so of two otherwise identical registered colonies the one that has
 * been waiting longer wins the 4393 tip; inventory INC; goods unload clears.
 * Cite: FUN_5952_035e; :87677.
 *
 * 2026-09-06g rewrite. The old fixture made both colonies merely SHORT and
 * relied on `ai_euro_nearest_haul_short_colony`'s own `idle*8 − d` score —
 * a Linux delivery-direction scan that is now deleted, and DOS's idle bonus
 * was carried only on that Linux arm. Both colonies now hold the same 80 RUM
 * so they register identically under `bVar5`; the only difference left is
 * idle 0 vs 20, i.e. exactly the DOS term. Same assertion: goto (4,8).
 */
static int unit_cargo_idle_turns_haul_prefer(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("cargo-idle alloc map");
  }

  /* 2026-09-07b: ships-only 4393 — ocean row y=1, ship consumer at (5,1). */
  for (int x = 0; x < 16; ++x) {
    map.terrain[1 * 16 + x] = 25; /* MAP_OCEAN_INDEX */
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* Equal MD from the ship at (5,1): A idle=0, B idle=20. */
  ColonizeColony* a = &colonies.colonies[0];
  a->id = 0;
  a->active = true;
  a->nation_id = nation;
  a->x = 8;
  a->y = 2;
  a->population = 3;
  a->colonist_count = 3;
  a->stock[COLONIZE_CARGO_TOOLS] = 0;
  a->stock[COLONIZE_CARGO_FOOD] = 80;
  a->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers work */
  a->building_in_production = -1;
  a->cargo_idle_turns = 0;
  a->specialty_cargo = 0xff;

  ColonizeColony* b = &colonies.colonies[1];
  b->id = 1;
  b->active = true;
  b->nation_id = nation;
  b->x = 2;
  b->y = 2;
  b->population = 3;
  b->colonist_count = 3;
  b->stock[COLONIZE_CARGO_TOOLS] = 0;
  b->stock[COLONIZE_CARGO_FOOD] = 80;
  b->stock[COLONIZE_CARGO_RUM] = 80; /* identical goods — only idle differs */
  b->building_in_production = -1;
  b->cargo_idle_turns = 20;
  b->specialty_cargo = 0xff;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 5, 1);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("cargo-idle spawn ship");
  }
  wagon->nation_id = nation;
  /* One step of MP: enough to commit and move toward the tip, not enough to
   * reach the berth and bounce through the arrival block this same beat. */
  wagon->moves = 1 * UNITS_MP_PER_TILE;
  wagon->orders = 0;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 61;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  const int near_b =
    wagon && wagon->active &&
    ((units_orders_follow_goto(wagon->orders) && abs(wagon->goto_x - 2) <= 1 &&
      abs(wagon->goto_y - 2) <= 1) ||
     (abs(wagon->x - 2) <= 1 && abs(wagon->y - 2) <= 1));
  if (!near_b) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: cargo-idle goto=(%d,%d) orders=%d idleA=%u idleB=%u\n",
      wagon ? wagon->goto_x : -1,
      wagon ? wagon->goto_y : -1,
      wagon ? wagon->orders : -1,
      (unsigned)colonies.colonies[0].cargo_idle_turns,
      (unsigned)colonies.colonies[1].cargo_idle_turns
    );
    fx_map_free(&map);
    return fail("expected ship goto the higher cargo_idle registered colony");
  }
  /* Inventory INC both shorts (cap 0x7f). */
  if (colonies.colonies[0].cargo_idle_turns < 1 ||
      colonies.colonies[1].cargo_idle_turns < 21) {
    fx_map_free(&map);
    return fail("expected cargo_idle INC during inventory");
  }

  /* Unload clears idle. */
  ColonizeColony* dest = &colonies.colonies[1];
  dest->x = 4;
  dest->y = 4; /* same tile as the hauler for transfer */
  wagon->x = 4;
  wagon->y = 4;
  dest->cargo_idle_turns = 30;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_TOOLS, 20) <= 0) {
    fx_map_free(&map);
    return fail("cargo-idle load tools for unload check");
  }
  const int moved =
    colonies_transfer_from_unit(&colonies, dest->id, &units, wid, 0, NULL);
  if (moved <= 0 || dest->cargo_idle_turns != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: unload moved=%d idle=%u\n",
      moved,
      (unsigned)dest->cargo_idle_turns
    );
    fx_map_free(&map);
    return fail("expected goods unload to clear cargo_idle_turns");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: cargo_idle_turns haul prefer ok\n");
  return 0;
}

/*
 * DELETED 2026-09-23 (bugs.md #746): unit_treasure_europe_cash asserted the
 * invented AI "Treasure aboard a ship at Europe → europe_cash_treasure with
 * the Crown cut" arm. No DOS site boards an AI treasure (FUN_4720_049e, the
 * AI ship cargo pick at raw 76067-76513, has no +0x3146 == 0x0a term) and
 * FUN_521d_20e6's treasure band never leaves the map, so the behaviour it
 * pinned does not exist. The AI cash-in is unit_ai_treasure_colony_cash
 * above; europe_cash_treasure itself is exercised by the human Europe tests.
 */

/*
 * Idle Wagon with hold capacity → AI_MOVE toward tools-short colony.
 * Cite: euro_unit_act §2d wagon haul / tools delivery.
 */
static int unit_wagon_haul_tools_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wagon-haul alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* short_c = fx_colony_add(&colonies, nation, 4, 4, 3);
  short_c->stock[COLONIZE_CARGO_TOOLS] = 5;
  short_c->stock[COLONIZE_CARGO_FOOD] = 40;

  /* Idle empty wagon inland — capacity only, no TOOLS yet. */
  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("wagon-haul spawn");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 31;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    fx_map_free(&map);
    return fail("wagon-haul should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    fx_map_free(&map);
    return fail("expected Wagon AI_MOVE toward tools-short colony (4,4)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul tools-short ok\n");
  return 0;
}

/*
 * Idle Caravel with goods-hold capacity → AI_SAIL toward the registered
 * coastal colony's berth (4393 pickup tip). 2026-09-07b: the colony holds 80
 * RUM so it registers on DOS's bVar5 gate — the old fixture relied on the
 * retired Linux-only `nearest_short_coastal_colony` scan (a merely SHORT
 * colony holds nothing to collect and never registers in DOS).
 */
static int unit_ship_trade_haul_tools_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("ship-haul alloc map");
  }
  /* Water corridor: colony (4,4) coastal via (3,4); ship starts at (3,10). */
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("ship-haul colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_TOOLS] = 5; /* tools-short */
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers the colony */

  const int sid = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("ship-haul spawn");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;

  ai_goals_reset();
  /* Distant FOUND must not steal idle cargo haul. */
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 32;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  if (!ship || !ship->active) {
    fx_map_free(&map);
    return fail("ship-haul should remain active");
  }
  /* Expect sail toward water near (4,4) — typically (3,4). */
  const int near_colony =
    abs(ship->goto_x - 4) <= 1 && abs(ship->goto_y - 4) <= 1 &&
    (ship->goto_x != 4 || ship->goto_y != 4);
  const int sailed = ship->orders == UNITS_ORDER_AI_SAIL && near_colony;
  /* Or already moved onto berth water. */
  const int at_berth = abs(ship->x - 4) <= 1 && abs(ship->y - 4) <= 1 &&
                       map_tile_is_water(&map, ship->x, ship->y);
  if (!sailed && !at_berth) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: ship orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y
    );
    fx_map_free(&map);
    return fail("expected Caravel AI_SAIL toward tools-short coastal colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: ship trade haul tools-short ok\n");
  return 0;
}

/*
 * Idle Caravel with MUSKETS cargo → AI_SAIL toward muskets-short coastal colony
 * (tools/food OK). Cite: euro_unit_act §2d2 ship haul muskets; wagon §2d.
 */
static int unit_ship_trade_haul_muskets_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("ship-muskets alloc map");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("ship-muskets colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_MUSKETS] = 2; /* muskets-short */

  const int sid = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("ship-muskets spawn");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;
  ship->hold_goods_type[0] = COLONIZE_CARGO_MUSKETS;
  ship->hold_goods_amount[0] = 10;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 32;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  if (!ship || !ship->active) {
    fx_map_free(&map);
    return fail("ship-muskets should remain active");
  }
  const int near_colony =
    abs(ship->goto_x - 4) <= 1 && abs(ship->goto_y - 4) <= 1 &&
    (ship->goto_x != 4 || ship->goto_y != 4);
  const int sailed = ship->orders == UNITS_ORDER_AI_SAIL && near_colony;
  const int at_berth = abs(ship->x - 4) <= 1 && abs(ship->y - 4) <= 1 &&
                       map_tile_is_water(&map, ship->x, ship->y);
  const int delivered = c->stock[COLONIZE_CARGO_MUSKETS] > 2;
  if (!sailed && !at_berth && !delivered) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: ship-muskets orders=%d goto=(%d,%d) pos=(%d,%d) muskets=%d\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y,
      c->stock[COLONIZE_CARGO_MUSKETS]
    );
    fx_map_free(&map);
    return fail("expected Caravel AI_SAIL toward muskets-short coastal colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: ship trade haul muskets-short ok\n");
  return 0;
}

/*
 * Idle Merchantman with goods-hold capacity → AI_SAIL toward tools-short coastal
 * colony (same haul ladder as Merchantman). Cite: euro_unit_act §2d2; docs/assets.md
 * Europe purchase ladder (Merchantman cargo ship).
 */
static int unit_galleon_trade_haul_tools_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("galleon-haul alloc map");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("galleon-haul colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Galleon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 6;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_TOOLS] = 5;
  c->stock[COLONIZE_CARGO_FOOD] = 40;
  c->stock[COLONIZE_CARGO_RUM] = 80; /* 2026-09-07b: bVar5 registers the colony */

  const int sid = units_spawn(&units, 0, 3, 10);
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("galleon-haul spawn");
  }
  ship->nation_id = nation;
  ship->moves = 4 * UNITS_MP_PER_TILE;
  ship->orders = 0;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 32;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  if (!ship || !ship->active) {
    fx_map_free(&map);
    return fail("galleon-haul should remain active");
  }
  const int near_colony =
    abs(ship->goto_x - 4) <= 1 && abs(ship->goto_y - 4) <= 1 &&
    (ship->goto_x != 4 || ship->goto_y != 4);
  const int sailed = ship->orders == UNITS_ORDER_AI_SAIL && near_colony;
  const int at_berth = abs(ship->x - 4) <= 1 && abs(ship->y - 4) <= 1 &&
                       map_tile_is_water(&map, ship->x, ship->y);
  if (!sailed && !at_berth) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: galleon-haul orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      ship->orders,
      ship->goto_x,
      ship->goto_y,
      ship->x,
      ship->y
    );
    fx_map_free(&map);
    return fail("expected Galleon AI_SAIL toward tools-short coastal colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: galleon trade haul tools-short ok\n");
  return 0;
}

/*
 * Idle Wagon with MUSKETS cargo → AI_MOVE toward the registered colony (which
 * is also muskets-short, so the DOS own-colony dump sweep delivers on arrival).
 * Cite: euro_unit_act §2d wagon haul muskets; COLONIZE_CARGO_MUSKETS.
 *
 * 2026-09-06g: the colony now also holds 80 RUM. The work queue registers on
 * DOS's `bVar5` (goods present, raw viceroy_unpacked.c:87663), not on the old
 * Linux "is short of a haul cargo" boolean, so a colony with nothing to
 * collect no longer enters the queue and no hauler is tipped at it. The
 * assertion is unchanged and still concrete — the wagon is aimed at (4,4),
 * where `ai_euro_try_wagon_haul`'s own-colony block dumps its MUSKETS.
 */
static int unit_wagon_haul_muskets_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wagon-muskets alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* short_c = fx_colony_add(&colonies, nation, 4, 4, 3);
  short_c->stock[COLONIZE_CARGO_TOOLS] = 40; /* not tools-short */
  short_c->stock[COLONIZE_CARGO_MUSKETS] = 2; /* muskets-short */
  short_c->stock[COLONIZE_CARGO_FOOD] = 40;
  short_c->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers work */

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("wagon-muskets spawn");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;
  /* Prefill MUSKETS cargo so haul prefers muskets-short colony. */
  if (units_load_goods(&units, wid, COLONIZE_CARGO_MUSKETS, 10) <= 0) {
    fx_map_free(&map);
    return fail("wagon-muskets load");
  }

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 32;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    fx_map_free(&map);
    return fail("wagon-muskets should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-muskets orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    fx_map_free(&map);
    return fail("expected Wagon AI_MOVE toward muskets-short colony (4,4)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul muskets-short ok\n");
  return 0;
}

/*
 * Idle Wagon with LUMBER cargo → AI_MOVE toward lumber-short colony (tools OK).
 * Cite: euro_unit_act §2d wagon haul lumber; COLONIZE_CARGO_LUMBER (the
 * inv->lumber_short tally this once cited was deleted, bugs.md #891).
 */
static int unit_wagon_haul_lumber_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wagon-lumber alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* short_c = fx_colony_add(&colonies, nation, 4, 4, 3);
  short_c->stock[COLONIZE_CARGO_TOOLS] = 40;
  short_c->stock[COLONIZE_CARGO_LUMBER] = 5; /* lumber-short */
  short_c->stock[COLONIZE_CARGO_FOOD] = 40;
  short_c->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers work */

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("wagon-lumber spawn");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_LUMBER, 20) <= 0) {
    fx_map_free(&map);
    return fail("wagon-lumber load");
  }

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 32;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    fx_map_free(&map);
    return fail("wagon-lumber should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-lumber orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    fx_map_free(&map);
    return fail("expected Wagon AI_MOVE toward lumber-short colony (4,4)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul lumber-short ok\n");
  return 0;
}

/*
 * Idle Wagon with ORE cargo → AI_MOVE toward ore-short colony (tools OK).
 * Cite: euro_unit_act §2d wagon haul ore; COLONIZE_CARGO_ORE (the
 * inv->ore_short tally this once cited was deleted, bugs.md #891).
 */
static int unit_wagon_haul_ore_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wagon-ore alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* short_c = fx_colony_add(&colonies, nation, 4, 4, 3);
  short_c->stock[COLONIZE_CARGO_TOOLS] = 40;
  short_c->stock[COLONIZE_CARGO_ORE] = 5; /* ore-short */
  short_c->stock[COLONIZE_CARGO_FOOD] = 40;
  short_c->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers work */

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("wagon-ore spawn");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_ORE, 20) <= 0) {
    fx_map_free(&map);
    return fail("wagon-ore load");
  }

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 32;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    fx_map_free(&map);
    return fail("wagon-ore should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-ore orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    fx_map_free(&map);
    return fail("expected Wagon AI_MOVE toward ore-short colony (4,4)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul ore-short ok\n");
  return 0;
}

/* (unit_wagon_europe_export_feeder / _unload removed 2026-09-07b with the
 * Linux-only wagon Europe-export feeder — DOS's LAB_457e origin walk owns
 * every off-errand wagon beat; surplus reaches Europe via the ships-only
 * 4393 pickup queue.) */

/*
 * Idle Wagon with FOOD cargo → AI_MOVE toward food-short colony (tools OK).
 * Cite: Colonization.pdf Wagon Train; euro_unit_act §2d; 5cf6 food_short
 * (stock < pop*TURN_FOOD_PER_COLONIST).
 */
static int unit_wagon_haul_food_short(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wagon-food-haul alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* short_c = fx_colony_add(&colonies, nation, 4, 4, 4);
  short_c->stock[COLONIZE_CARGO_TOOLS] = 40; /* not tools-short */
  short_c->stock[COLONIZE_CARGO_MUSKETS] = 20;
  short_c->stock[COLONIZE_CARGO_HORSES] = 20;
  short_c->stock[COLONIZE_CARGO_FOOD] = 2; /* food-short vs pop*2=8 */

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("wagon-food-haul spawn");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;
  if (units_load_goods(&units, wid, COLONIZE_CARGO_FOOD, 8) <= 0) {
    fx_map_free(&map);
    return fail("wagon-food-haul load");
  }

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 33;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  if (!wagon || !wagon->active) {
    fx_map_free(&map);
    return fail("wagon-food-haul should remain active");
  }
  if (wagon->orders != UNITS_ORDER_AI_MOVE || wagon->goto_x != 4 || wagon->goto_y != 4) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-food orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      wagon->orders,
      wagon->goto_x,
      wagon->goto_y,
      wagon->x,
      wagon->y
    );
    fx_map_free(&map);
    return fail("expected Wagon AI_MOVE toward food-short colony (4,4)");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: wagon haul food-short ok\n");
  return 0;
}

/*
 * Wagon on food-short colony with FOOD hold → colonies_transfer_from_unit.
 * Cite: Colonization.pdf Wagon Train; 5cf6 food_short; transfer APIs.
 */
static int unit_wagon_food_delivery(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wagon-food-deliv alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 4);
  c->stock[COLONIZE_CARGO_TOOLS] = 40;
  c->stock[COLONIZE_CARGO_FOOD] = 2; /* food-short */

  const int wid = units_spawn(&units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("wagon-food-deliv spawn");
  }
  wagon->nation_id = nation;
  wagon->orders = 0;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->hold_goods_type[0] = COLONIZE_CARGO_FOOD;
  wagon->hold_goods_amount[0] = 8;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 34;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int food_before = c->stock[COLONIZE_CARGO_FOOD];
  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  int hold_left = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < 2; ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
        hold_left += wagon->hold_goods_amount[h];
      }
    }
  }
  const int food_after = colonies.colonies[0].stock[COLONIZE_CARGO_FOOD];
  if (food_after < food_before + 8 || hold_left != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: wagon-food %d→%d hold_left=%d (want +8, hold 0)\n",
      food_before,
      food_after,
      hold_left
    );
    fx_map_free(&map);
    return fail("expected wagon FOOD transfer into food-short colony");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: wagon-food delivery ok (food %d→%d)\n",
    food_before,
    food_after
  );
  return 0;
}

/*
 * Surplus FOOD colony + empty wagon + a distant colony that has registered
 * work → the DOS LOAD matrix takes one hold of FOOD, then the work-queue tip
 * aims the wagon at the registered colony. Cite: Colonization.pdf Wagon Train;
 * FUN_521d_20e6 load matrix raw 3059-3134.
 *
 * 2026-09-06g rewrite. The old fixture asserted the retired Linux ladder:
 * 30 FOOD (a "surplus" only by the port's pop*4 rule) loaded by the
 * tools>lumber>ore>muskets>horses>food ladder, then hauled to whichever
 * colony was FOOD-short. DOS has neither half — its wagon load arm needs
 * `term >= 0x32` (raw: `if (iStack_XX < 0x32) score = -1`), so 30 FOOD never
 * loads, and its work queue is a PICKUP queue that a merely-hungry colony
 * never enters. Fixture updated to DOS's own numbers: 120 FOOD at the wagon's
 * colony (over the 0x32 floor, so the matrix takes min(stock,100) = 100), and
 * 80 RUM at the far colony so it clears the 0x4a registration gate. The
 * assertions are unchanged and still concrete: food leaves the stock, food is
 * aboard, and the wagon is aimed at (4,4).
 */
static int unit_wagon_food_load_haul(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wagon-food-load alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* Surplus FOOD at wagon tile. */
  ColonizeColony* surplus = &colonies.colonies[0];
  surplus->id = 0;
  surplus->active = true;
  surplus->nation_id = nation;
  surplus->x = 10;
  surplus->y = 10;
  surplus->population = 3;
  surplus->colonist_count = 3;
  surplus->stock[COLONIZE_CARGO_TOOLS] = 10; /* not surplus tools */
  surplus->stock[COLONIZE_CARGO_MUSKETS] = 5;
  surplus->stock[COLONIZE_CARGO_HORSES] = 5;
  surplus->stock[COLONIZE_CARGO_FOOD] = 120; /* over the DOS matrix 0x32 floor */
  surplus->building_in_production = -1;
  /* Distant food-short. */
  ColonizeColony* hungry = &colonies.colonies[1];
  hungry->id = 1;
  hungry->active = true;
  hungry->nation_id = nation;
  hungry->x = 4;
  hungry->y = 4;
  hungry->population = 4;
  hungry->colonist_count = 4;
  hungry->stock[COLONIZE_CARGO_TOOLS] = 40;
  hungry->stock[COLONIZE_CARGO_FOOD] = 1; /* food-short */
  hungry->stock[COLONIZE_CARGO_RUM] = 80; /* 80 > 0x4a → bVar5 registers work */
  hungry->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("wagon-food-load spawn");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;

  ai_goals_reset();
  ai_goals_upsert_primary(nation, 14, 14, AI_GOAL_FOUND, 5);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 35;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int stock_before = surplus->stock[COLONIZE_CARGO_FOOD];
  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  int food_aboard = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < 2; ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
        food_aboard += wagon->hold_goods_amount[h];
      }
    }
  }
  const int stock_after = colonies.colonies[0].stock[COLONIZE_CARGO_FOOD];
  /*
   * 2026-09-07b: DOS shape. The load matrix takes one hold at the wagon's own
   * colony, but a wagon never runs cross-colony deliveries — the 4393 queue
   * is ships-only and the LAB_457e origin walk parks an off-errand wagon at
   * its bound colony (no villages in this fixture, so the errand clears).
   * The old assertion (AI_MOVE to the food-short colony at (4,4)) tested the
   * retired wagon queue substitute.
   */
  const int left_home =
    wagon && wagon->active && (wagon->x != 10 || wagon->y != 10) &&
    !(wagon->x == 10 && wagon->y == 10);
  if (!wagon || !wagon->active || food_aboard <= 0 || stock_after >= stock_before ||
      left_home ||
      (units_orders_follow_goto(wagon->orders) && wagon->goto_x == 4 && wagon->goto_y == 4)) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: food-load stock %d→%d aboard=%d orders=%d goto=(%d,%d)\n",
      stock_before,
      stock_after,
      food_aboard,
      wagon ? wagon->orders : -1,
      wagon ? wagon->goto_x : -1,
      wagon ? wagon->goto_y : -1
    );
    fx_map_free(&map);
    return fail("expected wagon FOOD load + park at bound colony (DOS 457e)");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_expand: wagon food load+haul ok (aboard=%d)\n",
    food_aboard
  );
  return 0;
}

/*
 * A Wagon Train at a colony holding both TOOLS and FOOD loads the FOOD and
 * never the TOOLS. Cite: FUN_521d_20e6 LOAD matrix raw 3059-3134 — cargo 0xe
 * (Tools) and 0xf (Muskets) are `iStack_34 != 0` arms, i.e. SHIPS ONLY, and
 * are skipped for a land hauler even when the colony produces them.
 *
 * 2026-09-06g rewrite. The old fixture asserted the retired Linux ladder's
 * `food_short > 20` reorder (30 FOOD beating 50 TOOLS because the nation's
 * inventory was hungry). DOS has no such reorder and no such trigger; it
 * refuses TOOLS to a wagon outright and needs `term >= 0x32` for the FOOD it
 * does take. Fixture updated to those numbers (120 FOOD; TOOLS additionally
 * marked as PRODUCED so the ships-only rejection is the only thing keeping
 * them off the wagon). Assertions unchanged: FOOD aboard, TOOLS not.
 */
static int unit_wagon_food_prefer_over_tools(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("wagon-food-prefer alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Wagon Train");
  units.types[0].movement = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* surplus = &colonies.colonies[0];
  surplus->id = 0;
  surplus->active = true;
  surplus->nation_id = nation;
  surplus->x = 10;
  surplus->y = 10;
  surplus->population = 3;
  surplus->colonist_count = 3;
  surplus->stock[COLONIZE_CARGO_TOOLS] = 50; /* produced below — ships only */
  surplus->stock[COLONIZE_CARGO_FOOD] = 120; /* over the DOS matrix 0x32 floor */
  surplus->cargo_produced_mask = (uint16_t)(1u << COLONIZE_CARGO_TOOLS);
  surplus->building_in_production = -1;
  /* Large food deficit → inventory food_short > 20. */
  ColonizeColony* hungry = &colonies.colonies[1];
  hungry->id = 1;
  hungry->active = true;
  hungry->nation_id = nation;
  hungry->x = 4;
  hungry->y = 4;
  hungry->population = 12;
  hungry->colonist_count = 12;
  hungry->stock[COLONIZE_CARGO_TOOLS] = 5; /* tools-short too */
  hungry->stock[COLONIZE_CARGO_FOOD] = 0;  /* food_short += 24 */
  hungry->building_in_production = -1;
  colonies.colony_count = 2;
  colonies.next_id = 2;

  const int wid = units_spawn(&units, 0, 10, 10);
  ColonizeUnit* wagon = units_get(&units, wid);
  if (!wagon) {
    fx_map_free(&map);
    return fail("wagon-food-prefer spawn");
  }
  wagon->nation_id = nation;
  wagon->moves = 2 * UNITS_MP_PER_TILE;
  wagon->orders = 0;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 35;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  wagon = units_get(&units, wid);
  int food_aboard = 0;
  int tools_aboard = 0;
  if (wagon && wagon->active) {
    for (int h = 0; h < 2; ++h) {
      if (wagon->hold_goods_amount[h] <= 0 || wagon->hold_goods_amount[h] >= 255) {
        continue;
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
        food_aboard += wagon->hold_goods_amount[h];
      }
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_TOOLS) {
        tools_aboard += wagon->hold_goods_amount[h];
      }
    }
  }
  if (!wagon || !wagon->active || food_aboard <= 0 || tools_aboard > 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: food-prefer food=%d tools=%d orders=%d\n",
      food_aboard,
      tools_aboard,
      wagon ? wagon->orders : -1
    );
    fx_map_free(&map);
    return fail("expected FOOD prefer over tools when food_short>20");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: wagon food prefer over tools ok\n");
  return 0;
}

/*
 * Caravel with FOOD cargo adjacent to food-short coastal colony → unload via
 * colonies_transfer_from_unit. Cite: Colonization.pdf naval transport; §2d2;
 * 5cf6 food_short.
 */
static int unit_ship_food_delivery(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("ship-food alloc map");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }
  if (!map_tile_is_coastal(&map, 4, 4)) {
    fx_map_free(&map);
    return fail("ship-food colony should be coastal");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Caravel");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 4);
  c->stock[COLONIZE_CARGO_TOOLS] = 40; /* not tools-short */
  c->stock[COLONIZE_CARGO_FOOD] = 1; /* food-short */

  const int sid = units_spawn(&units, 0, 3, 4); /* adjacent water */
  ColonizeUnit* ship = units_get(&units, sid);
  if (!ship) {
    fx_map_free(&map);
    return fail("ship-food spawn");
  }
  ship->nation_id = nation;
  ship->orders = 0;
  ship->moves = 4 * UNITS_MP_PER_TILE;
  ship->hold_goods_type[0] = COLONIZE_CARGO_FOOD;
  ship->hold_goods_amount[0] = 8;

  ai_goals_reset();

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.nation[nation].gold = 200;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;

  uint32_t turn = 36;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  const int food_before = c->stock[COLONIZE_CARGO_FOOD];
  ai_euro_dispatcher_turn(&ctx, nation);

  ship = units_get(&units, sid);
  int hold_left = 0;
  if (ship && ship->active) {
    for (int h = 0; h < 2; ++h) {
      if (ship->hold_goods_type[h] == COLONIZE_CARGO_FOOD) {
        hold_left += ship->hold_goods_amount[h];
      }
    }
  }
  const int food_after = colonies.colonies[0].stock[COLONIZE_CARGO_FOOD];
  if (food_after < food_before + 8 || hold_left != 0) {
    fprintf(
      stderr,
      "unit_ai_euro_expand: ship-food %d→%d hold_left=%d\n",
      food_before,
      food_after,
      hold_left
    );
    fx_map_free(&map);
    return fail("expected ship FOOD unload into food-short coastal colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: ship food delivery ok\n");
  return 0;
}

/*
 * bugs.md #877 — LAB_521d_4393 entry gate (raw 89877-89878):
 * `0xc < type && type < 0x13 && (bVar20 || bVar7)`. bVar20 is seeded
 * `type != 0x12` (raw 88556), so a Man-O-War only reaches the colony haul
 * queue through bVar7's re-allow arm (raw 88574-88579: odd pool index, or
 * unit_type_counts[nation][0x12] == 1). With an EVEN pool index and two
 * Man-O-Wars on the census both are false and the hull gets no haul; a
 * Merchantman (0x0e) on the same slot does.
 */
static int unit_4393_haul_pick_manowar_gate(void) {
  const int nation = 1;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("877 alloc map");
  }
  for (int y = 0; y < 16; ++y) {
    map.terrain[y * 16 + 3] = 25;
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Man-O-War");
  units.types[0].movement = 5;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].cargo = 6;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Merchantman");
  units.types[1].movement = 5;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, nation, 4, 4, 3);
  c->stock[COLONIZE_CARGO_RUM] = 80;

  const int mow_id = units_spawn(&units, 0, 3, 10);
  const int mm_id = units_spawn(&units, 1, 3, 11);
  ColonizeUnit* mow = units_get(&units, mow_id);
  ColonizeUnit* mm = units_get(&units, mm_id);
  if (!mow || !mm) {
    fx_map_free(&map);
    return fail("877 spawn");
  }
  mow->nation_id = nation;
  mm->nation_id = nation;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  col1.head.game_options.woi = 0; /* outside the War of Independence */
  /* Two Man-O-Wars: bVar7's `unit_type_counts[n][0x12] == 1` arm is false. */
  col1.stuff.unit_type_counts[nation][0x12] = 2;

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  /* military = 1 so the inner accept gate (raw 89898) is never the reason. */
  ai_goals_reset();
  ai_goals_clear_work_queue();
  ai_goals_upsert_work(0, 400, 4, 1);

  int x = 0;
  int y = 0;
  const int mow_even = ((int)(mow - units.units) & 1) == 0;
  if (!mow_even) {
    fx_map_free(&map);
    return fail("877 fixture: Man-O-War must sit on an EVEN pool slot");
  }
  if (ai_euro_4393_work_queue_haul_pick(&ctx, nation, 3, 10, mow, &x, &y)) {
    fx_map_free(&map);
    return fail("Man-O-War (0x12) outside WoI with bVar7 false must get no colony haul");
  }

  ai_goals_clear_work_queue();
  ai_goals_upsert_work(0, 400, 4, 1);
  if (!ai_euro_4393_work_queue_haul_pick(&ctx, nation, 3, 10, mm, &x, &y) || x != 4 || y != 4) {
    fx_map_free(&map);
    return fail("Merchantman (0x0e) should take the registered colony haul");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_expand: 4393 Man-O-War entry gate ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_specialty_flag_a_haul_match", unit_specialty_flag_a_haul_match},
    {"unit_wagon_haul_tools_short", unit_wagon_haul_tools_short},
    {"unit_wagon_haul_muskets_short", unit_wagon_haul_muskets_short},
    {"unit_wagon_haul_lumber_short", unit_wagon_haul_lumber_short},
    {"unit_wagon_haul_ore_short", unit_wagon_haul_ore_short},
    {"unit_wagon_haul_food_short", unit_wagon_haul_food_short},
    {"unit_wagon_food_delivery", unit_wagon_food_delivery},
    {"unit_wagon_food_load_haul", unit_wagon_food_load_haul},
    {"unit_wagon_food_prefer_over_tools", unit_wagon_food_prefer_over_tools},
    {"unit_ship_trade_haul_tools_short", unit_ship_trade_haul_tools_short},
    {"unit_ship_trade_haul_muskets_short", unit_ship_trade_haul_muskets_short},
    {"unit_galleon_trade_haul_tools_short", unit_galleon_trade_haul_tools_short},
    {"unit_ship_food_delivery", unit_ship_food_delivery},
    {"unit_cargo_produced_mask_haul_prefer", unit_cargo_produced_mask_haul_prefer},
    {"unit_specialty_cargo_haul_prefer", unit_specialty_cargo_haul_prefer},
    {"unit_cargo_idle_turns_haul_prefer", unit_cargo_idle_turns_haul_prefer},
    {"unit_4393_haul_pick_manowar_gate", unit_4393_haul_pick_manowar_gate},
};
TEST_MAIN(k_cases)
