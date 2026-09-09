/*
 * Focused unit tests for ai_euro_5d04_hire_ladder_tail (FUN_521d_5d04 raw
 * 86065-86561), specifically the operand derivations resolved 2026-09-07e and
 * the two branches that were wrong before it:
 *
 *   DS:0xa0cc[16]  per-cargo demand = own colonies specialising in it MINUS
 *                  what is already afloat (raw 93107-93163) — the departing
 *                  ship's buy test at raw 93036/93050.
 *   DS:0x945a[n]   land units on this nation's Europe dock, the seed of
 *                  `local_46`, the bar a cargo's demand must clear (raw
 *                  92983-92988).
 *   DS:0xa0b8[n]   own colonies flagged NEEDS_COLONISTS, the recruit-slot
 *                  swap gate at raw 92592-92625.
 *   raw 92745-92748  past turn 99 the Pioneer-training arm is SKIPPED when
 *                  RNG(0,2) <= unit_type_counts[n][2] (own Pioneers).
 *
 * The tail is static, so the seam is ai_euro_dispatcher_turn, as in the other
 * ai_euro tests. Every fixture below pins the whole census window it reads
 * (never a blank one) and keeps the earlier phases of the ladder provably
 * inert, so the assertion is about the branch under test and nothing else:
 *
 *   - ship-buy ladder: `ship_cargo_totals(4) > census_pop_proxy/2 + colony_counts(1)`
 *     → returns immediately, no purchase, no RNG draw;
 *   - bVar7 cargo_short: `4 <= ((0 >> 1) + 1*2) >> 1 = 1` is false;
 *   - bVar21 no_ships: ship_counts != 0, so no gold floor is applied;
 *   - treasury bump: head.turn < 20 (or colony_counts 0 + year 1500) → +0;
 *   - the two-pass hire loop only walks the Europe list, which holds nothing
 *     "skilled" (a ship's dispatch byte is 0x0d) unless the case adds one.
 *
 * Cross-test note: the tail's per-nation scratch (crosses bank / delay byte)
 * is a file-static that no dispatcher call resets, so every case here is
 * written to leave it at zero (no muskets/horses training, no recruit buy on
 * the nations the later cases use).
 */
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
  fprintf(stderr, "unit_ai_euro_5d04_hire: FAIL %s\n", msg);
  return 1;
}

/* The Europe pool tile the AI spawns/keeps dock units on (ai_euro_in_europe:
 * x >= 200 || y >= 200). */
#define EUROPE_X 200
#define EUROPE_Y 100

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

static int fixture_init(Fixture* f, int nation, int turn, unsigned seed) {
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
  for (int y = 0; y < 16; ++y) {
    for (int x = 12; x < 16; ++x) {
      f->map.terrain[y * 16 + x] = 25; /* MAP_OCEAN_INDEX */
    }
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
  snprintf(f->units.types[3].name, sizeof(f->units.types[3].name), "Pioneer");
  f->units.types[3].movement = 1;
  f->units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
  f->units.types[3].space = 1;
  snprintf(f->units.types[4].name, sizeof(f->units.types[4].name), "Missionary");
  f->units.types[4].movement = 1;
  f->units.types[4].domain = COLONIZE_UNIT_DOMAIN_LAND;
  f->units.types[4].space = 1;
  colonies_init(&f->colonies);
  col1_save_init(&f->col1);
  memset(f->col1.nation, 0, sizeof(f->col1.nation));
  memset(f->col1.head.nation_relation, 0, sizeof(f->col1.head.nation_relation));
  memset(f->col1.head.founding_father, 0xff, sizeof(f->col1.head.founding_father));
  for (int i = 0; i < 4; ++i) {
    f->col1.player[i].control = 0;
    f->col1.player[i].diplomacy = 0;
  }
  /* Census window — every cell the 5d04 gates read, set on purpose. */
  f->col1.stuff.ship_counts[nation] = 1;
  f->col1.stuff.ship_cargo_totals[nation] = 4;
  f->col1.stuff.colony_counts[nation] = 1;
  f->col1.stuff.colony_pop_totals[nation] = 3;
  f->col1.stuff.census_pop_proxy[nation] = 0;
  f->col1.stuff.free_colonist_counts[nation] = 1;
  f->col1.stuff.veteran_teach_threshold[nation] = 0;
  /* Nation record: recruit pool + crosses, so the hire prices are exact.
   * recruit[] holds @JOB professions; 0x1c = "none" keeps the profession gate
   * at 0 and makes a popped recruit a plain Free Colonist. */
  f->col1.nation[nation].gold = 1000;
  f->col1.nation[nation].recruit_count = 0;
  f->col1.nation[nation].current_crosses = 0;
  f->col1.nation[nation].needed_crosses = 0;
  for (int i = 0; i < 3; ++i) {
    f->col1.nation[nation].recruit[i] = 0x1c;
  }
  f->col1.head.turn = (uint16_t)turn;
  f->col1.head.year = 1500;
  f->col1.head.difficulty = 0;
  f->col1.head.game_options.woi = 0;
  dos_rng_seed(&f->rng, seed);
  f->turn = (uint32_t)turn;
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
  f->ctx.rng_seed = (int)seed;
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

/*
 * One own colony whose 5952_0306 specialty settles on ORE: Ore 60 is a haul
 * surplus (>= 40) and under the 100 warehouse cap, while every cargo AFTER
 * Ore in the specialty ladder (Muskets, Horses, Food, Sugar, Tobacco, Cotton,
 * Furs, Silver) is deliberately below its own threshold, so none of them
 * overwrites it. That makes DS:0xa0cc[ORE] == 1 and every other cell 0.
 * `pop >= 3` also keeps NEEDS_COLONISTS (DS:0xa0b8) clear.
 */
static ColonizeColony* fixture_ore_colony(Fixture* f, int nation, int population) {
  ColonizeColony* c = &f->colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = 5;
  c->y = 4;
  c->population = (uint8_t)population;
  c->colonist_count = (uint8_t)population;
  c->stock[COLONIZE_CARGO_ORE] = 60;     /* surplus → specialty = Ore */
  c->stock[COLONIZE_CARGO_FOOD] = 10;    /* < pop*2*2 → not a Food surplus */
  c->stock[COLONIZE_CARGO_MUSKETS] = 5;  /* non-zero and < 20: no muskets need */
  c->stock[COLONIZE_CARGO_TOOLS] = 50;   /* non-zero: no tools need */
  c->stock[COLONIZE_CARGO_LUMBER] = 0;
  c->building_in_production = -1;
  f->colonies.colony_count = 1;
  f->colonies.next_id = 1;
  return c;
}

/* A ship parked on the Europe dock. moves_left = 0 so the dispatcher's unit
 * act (which would sail it and re-run the 20e6 band on the new hull) skips
 * it: what the hull holds after the call is exactly what 5d04 did to it. */
static int spawn_europe_ship(Fixture* f, int nation) {
  const int id = units_spawn_allow_stack(&f->units, 2, EUROPE_X, EUROPE_Y);
  ColonizeUnit* u = id >= 0 ? units_get(&f->units, id) : NULL;
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation);
  u->moves_left = 0;
  u->orders = 0;
  u->col1_origin = 0xff;
  return id;
}

static int spawn_europe_colonist(Fixture* f, int nation, int type_index, int profession) {
  const int id = units_spawn_allow_stack(&f->units, type_index, EUROPE_X, EUROPE_Y);
  ColonizeUnit* u = id >= 0 ? units_get(&f->units, id) : NULL;
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation);
  u->moves_left = 0;
  u->orders = 0;
  u->profession = (uint8_t)profession;
  return id;
}

static int ship_cargo_of(const ColonizeUnit* ship, int cargo) {
  int n = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] > 0 && ship->hold_goods_amount[h] < 255 &&
        ship->hold_goods_type[h] == cargo) {
      n += ship->hold_goods_amount[h];
    }
  }
  return n;
}

static int ship_cargo_total(const ColonizeUnit* ship) {
  int n = 0;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (ship->hold_goods_amount[h] > 0 && ship->hold_goods_amount[h] < 255) {
      n += ship->hold_goods_amount[h];
    }
  }
  return n;
}

static int europe_land_unit_count(const Fixture* f, int nation) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &f->units.units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    if (u->x < 200 && u->y < 200) {
      continue;
    }
    if (units_is_sea((ColonizeUnitPool*)&f->units, u->id)) {
      continue;
    }
    n++;
  }
  return n;
}

/*
 * Case 3a — the departing ship tops its hull up with the cargo its colonies
 * want (raw 93036-93050). Turn 5: odd, so `local_46 = DS:0x945a(0) + 1 = 1`;
 * `expand_signal` is 0 (turn % 3 != 0 and the past-the-end bVar23 read is a
 * hard 0), so the `local_46 <= demand[p]` test is the ONLY way into the buy.
 * Only Ore clears it (demand 1), so exactly 100 Ore is bought, for
 * price(Ore) = euro_price + 1 = 2 → 200 gold.
 *
 * The two-pass hire loop and the recruit-purchase loop stay inert here: the
 * Europe list holds only the ship (dispatch byte 0x0d → not "skilled", and
 * `local_22` = land units waiting = 0), and the recruit-slot swap is closed
 * by `colony_counts>>1 (0) <= a0b8(0) − free_colonists(1)` being false.
 */
static int departing_ship_buys_wanted_cargo(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation, 5, 100) != 0) {
    return 1;
  }
  fixture_ore_colony(&f, nation, 3);
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 1; /* price 2 */
  const int sid = spawn_europe_ship(&f, nation);
  if (sid < 0) {
    fixture_free(&f);
    return fail("spawn europe ship");
  }
  const uint32_t gold_before = f.col1.nation[nation].gold;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  const ColonizeUnit* ship = units_get(&f.units, sid);
  if (!ship || !ship->active) {
    fixture_free(&f);
    return fail("europe ship vanished");
  }
  const int ore = ship_cargo_of(ship, COLONIZE_CARGO_ORE);
  if (ore != 100 || ship_cargo_total(ship) != 100 ||
      f.col1.nation[nation].gold != gold_before - 200u) {
    fprintf(stderr, "ore=%d total=%d gold %u->%u\n", ore, ship_cargo_total(ship),
            (unsigned)gold_before, (unsigned)f.col1.nation[nation].gold);
    fixture_free(&f);
    return fail("departing ship did not buy the demanded cargo at the DOS price");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Case 3b — the demand table's afloat half (raw 93163: per own ship, per
 * occupied hold, `demand[cargo]--`). Identical to 3a except the ship arrives
 * on the dock already carrying 100 Ore: the colony's +1 and the hull's −1
 * cancel, demand[Ore] = 0 < local_46 = 1, and after the sell/unload loop has
 * emptied the hull nothing is bought back.
 *
 * A port that read the demand table as a plain "colonies specialising in it"
 * count (or profession-indexed, as before 2026-09-07e) would re-buy the Ore
 * it just sold.
 */
static int afloat_cargo_cancels_colony_demand(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation, 5, 100) != 0) {
    return 1;
  }
  fixture_ore_colony(&f, nation, 3);
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 1;
  const int sid = spawn_europe_ship(&f, nation);
  if (sid < 0) {
    fixture_free(&f);
    return fail("spawn europe ship");
  }
  if (units_load_goods(&f.units, sid, COLONIZE_CARGO_ORE, 100) <= 0) {
    fixture_free(&f);
    return fail("load ore");
  }

  ai_euro_dispatcher_turn(&f.ctx, nation);

  const ColonizeUnit* ship = units_get(&f.units, sid);
  if (!ship || !ship->active) {
    fixture_free(&f);
    return fail("europe ship vanished");
  }
  if (ship_cargo_total(ship) != 0) {
    fprintf(stderr, "hull carries %d (ore %d) after the pass\n", ship_cargo_total(ship),
            ship_cargo_of(ship, COLONIZE_CARGO_ORE));
    fixture_free(&f);
    return fail("afloat cargo must cancel its colony's demand cell");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Case 3c — DS:0x945a: the dock queue raises the bar (raw 92983-92988). Same
 * colony and price as 3a, but TWO Free Colonists wait on the Europe dock, so
 * `local_46 = 2 + (turn & 1) = 3` and Ore's demand of 1 no longer clears it.
 *
 * Their presence also closes the recruit-slot swap (bVar8: a non-ship in the
 * Europe list), and the recruit-purchase loop cannot afford anything —
 * `base(140) + muskets price(20) * 50 = 1140 > 1000 gold` — so bVar9 stays 0
 * and the "ship capacity == local_24" early-exhaust arm is not what makes
 * this pass. The two-pass hire loop is likewise priced out (muskets 1000,
 * tools 2000 > gold), so the colonists are still colonists at the end.
 */
static int europe_dock_queue_raises_cargo_bar(void) {
  const int nation = 1;
  Fixture f;
  if (fixture_init(&f, nation, 5, 100) != 0) {
    return 1;
  }
  fixture_ore_colony(&f, nation, 3);
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 1;      /* buy 100 = 200 */
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_MUSKETS] = 19; /* 50 = 1000 */
  f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_TOOLS] = 19;   /* 100 = 2000 */
  const int sid = spawn_europe_ship(&f, nation);
  if (sid < 0 || spawn_europe_colonist(&f, nation, 0, 0x1c) < 0 ||
      spawn_europe_colonist(&f, nation, 0, 0x1c) < 0) {
    fixture_free(&f);
    return fail("spawn europe stack");
  }
  const uint32_t gold_before = f.col1.nation[nation].gold;

  ai_euro_dispatcher_turn(&f.ctx, nation);

  const ColonizeUnit* ship = units_get(&f.units, sid);
  if (!ship || !ship->active) {
    fixture_free(&f);
    return fail("europe ship vanished");
  }
  if (ship_cargo_total(ship) != 0 || f.col1.nation[nation].gold != gold_before) {
    fprintf(stderr, "hull=%d ore=%d gold %u->%u\n", ship_cargo_total(ship),
            ship_cargo_of(ship, COLONIZE_CARGO_ORE), (unsigned)gold_before,
            (unsigned)f.col1.nation[nation].gold);
    fixture_free(&f);
    return fail("dock queue should have priced the cargo buy out (local_46 = 3 > demand 1)");
  }
  fixture_free(&f);
  return 0;
}

/*
 * Case 3d — DS:0xa0b8: the recruit-slot swap gate (raw 92592-92625). The gate
 * is `!woi && !bVar8 && !cargo_short && (!has_any_colony || (!bVar23 &&
 * !every_third_turn && colony_counts>>1 <= a0b8 − free_colonist_counts))`.
 * With one colony, free_colonist_counts = 1 and turn 5 (not a third turn) the
 * only free variable left is DS:0xa0b8 = own colonies flagged
 * NEEDS_COLONISTS:
 *
 *   flag clear → a0b8 = 0 → 0 <= −1 false → no swap, Europe dock stays empty
 *   flag set   → a0b8 = 1 → 0 <=  0 true  → gold −= base
 *                            ((recruit_count − difficulty + 7) * 20 = 140,
 *                            the crosses term is 0 at current_crosses = 0)
 *                            and FUN_38fd_0718 puts a recruit on the dock
 *
 * The flag is set on the fixture, not derived from `population < 3`: the port
 * refreshes it in ai_euro_colony_goals, which runs AFTER ai_euro_nation_
 * planning, so what 5d04 reads is the flag as the PREVIOUS beat left it —
 * DOS's own ordering (6d8e's prelude builds 0xa0b8 from the colony bytes
 * before it calls 5d04). Both runs keep population 3 so the colony's
 * specialty stays Ore at an unaffordable 201/unit: the swap is then the only
 * thing in the whole tail that can move gold or add a dock unit.
 *
 * This case also pins the "past-the-end bVar23 read is kept as 0" decision:
 * the same gate ANDs `!unit_flag_bit5`, so a port that resolved that stale
 * register read to anything truthy could never open the gate at all.
 */
static int recruit_swap_follows_colonies_wanting_colonists(void) {
  const int nation = 1;
  for (int wants = 0; wants < 2; ++wants) {
    Fixture f;
    if (fixture_init(&f, nation, 5, 100) != 0) {
      return 1;
    }
    ColonizeColony* c = fixture_ore_colony(&f, nation, 3);
    c->ai_flags = (uint8_t)(wants ? COLONIZE_COLONY_AI_NEEDS_COLONISTS : 0);
    /* Nothing affordable to buy for the hull (Ore at 201/unit), and a purse
     * that covers the 140 swap but NOT the 190 the recruit-purchase loop
     * would want for the colonist the swap just put on the dock
     * (base2 140 + muskets 50) — so the swap is the only mover here and the
     * gold delta is exact. */
    f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_ORE] = 200;
    f.col1.nation[nation].gold = 300;
    if (spawn_europe_ship(&f, nation) < 0) {
      fixture_free(&f);
      return fail("spawn europe ship");
    }
    const uint32_t gold_before = f.col1.nation[nation].gold;

    ai_euro_dispatcher_turn(&f.ctx, nation);

    const int dock = europe_land_unit_count(&f, nation);
    const uint32_t gold = f.col1.nation[nation].gold;
    if (wants && (dock != 1 || gold != gold_before - 140u)) {
      fprintf(stderr, "wants=%d dock=%d gold %u->%u (want -140)\n", wants, dock,
              (unsigned)gold_before, (unsigned)gold);
      fixture_free(&f);
      return fail("NEEDS_COLONISTS colony should open the recruit-slot swap");
    }
    if (!wants && (dock != 0 || gold != gold_before)) {
      fprintf(stderr, "wants=%d dock=%d gold %u->%u\n", wants, dock, (unsigned)gold_before,
              (unsigned)gold);
      fixture_free(&f);
      return fail("swap fired with no colony wanting colonists");
    }
    fixture_free(&f);
  }
  return 0;
}

/*
 * Case 3e — the turn > 99 Pioneer-training skip (raw 92745-92748). The arm
 * trains a dock colonist into a Pioneer (dispatch byte 2) for
 * price(Tools) * 100; past turn 99 DOS first rolls RNG(0,2) and SKIPS the arm
 * whenever the roll is <= the nation's own Pioneer count
 * (unit_type_counts[n][2]). With two Pioneers already in the field the roll
 * can never exceed 2, so past turn 99 the arm is unreachable — while at turn
 * 99 exactly (`turn > 99` false) the same fixture trains.
 *
 * The port had this inverted (and read free_colonist_counts) before
 * 2026-09-07e, which this shape catches: it sweeps the same RNG seeds through
 * both turns and requires the early turn to train at least once and the late
 * turn never to train. `has_any_colony` is 0 here (colony_counts = 0) so the
 * arm's `!has_any_colony` entry holds regardless of turn parity, and the
 * Pioneer count comes from the census row, not from live units, so no fixture
 * Pioneer competes for the dock.
 */
static int pioneer_training_skipped_past_turn_99(void) {
  const int nation = 1;
  const int k_seeds = 24;
  int trained_early = 0;
  int trained_late = 0;
  for (int late = 0; late < 2; ++late) {
    for (int s = 0; s < k_seeds; ++s) {
      Fixture f;
      if (fixture_init(&f, nation, late ? 102 : 99, (unsigned)(7 + s * 13)) != 0) {
        return 1;
      }
      /* No colonies at all: `has_any_colony` false opens the dispatch-0 arm,
       * and the treasury bump stays 0 (local_12 = 0 + (1500-1500)/50). */
      f.col1.stuff.colony_counts[nation] = 0;
      f.col1.stuff.colony_pop_totals[nation] = 0;
      f.col1.stuff.free_colonist_counts[nation] = 0;
      f.col1.stuff.unit_type_counts[nation][2] = 2; /* own Pioneers */
      f.col1.nation[nation].gold = 5000;
      /* Tools training costs price(Tools) * 100 = 100 (affordable); the
       * muskets arm just above it (LAB_521d_6454, reachable past turn 99 via
       * the 642a fall-through) costs price(Muskets) * 50 = 10000 and is
       * therefore never `handled` — without this the late runs would skip the
       * Pioneer arm for the WRONG reason and the sweep would prove nothing.
       * The same 10000 also prices the recruit-purchase loop out. */
      f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_TOOLS] = 0;
      f.col1.nation[nation].trade.euro_price[COLONIZE_CARGO_MUSKETS] = 199;
      if (spawn_europe_ship(&f, nation) < 0 ||
          spawn_europe_colonist(&f, nation, 0, 0x1c) < 0) {
        fixture_free(&f);
        return fail("spawn europe stack");
      }

      ai_euro_dispatcher_turn(&f.ctx, nation);

      int pioneers = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &f.units.units[i];
        if (!u->active || u->nation_id != nation) {
          continue;
        }
        if (strcmp(units_display_name(&f.units, u), "Pioneer") == 0) {
          pioneers++;
        }
      }
      if (pioneers > 0) {
        if (late) {
          trained_late++;
          fprintf(stderr, "seed %u trained a Pioneer at turn 102\n", (unsigned)(7 + s * 13));
        } else {
          trained_early++;
        }
      }
      fixture_free(&f);
    }
  }
  if (trained_late != 0) {
    return fail("turn > 99 with 2 own Pioneers must always skip the training arm");
  }
  if (trained_early == 0) {
    return fail("turn 99 fixture never trains — the sweep proves nothing");
  }
  printf("unit_ai_euro_5d04_hire: pioneer sweep trained %d/%d at turn 99, %d at turn 102\n",
         trained_early, k_seeds, trained_late);
  return 0;
}

/*
 * Case 4 — the hire chain's @UNIT table. `ai_euro_5d04_dos_type_of` is the
 * DOS-side view of a Linux unit type, and its rows have to be NAMES.TXT
 * @UNIT file order (COLONIZE/NAMES.TXT:301-323): 0 Colonists .. 5 Scouts,
 * then the four WoI rows 6 Regulars / 7 Cont. Cav. / 8 Cavalry / 9 Cont.
 * Army, then 10 Treasure / 11 Artillery / 12 Wagon Train. Until 2026-09-09
 * the table tested "Cav" before "Cont", which folded Cont. Cav. onto 8 and
 * Cont. Army onto 7 — disagreeing with the sibling table
 * `ai_euro_20e6_dos_type` in the same file.
 *
 * The pool below is deliberately NOT in NAMES order, so a mapper that fell
 * back on the pool index would fail every row.
 *
 * Sea rows stay coarse by design (this mapper only separates Privateer 0x10
 * from "some other ship" 0x0d, which is all the 5d04 chain looks at), so the
 * two ship rows assert that contract rather than the file index.
 */
static int dos_type_table_is_names_txt_unit_order(void) {
  static const struct {
    const char* name;
    int domain;
    int dos;
  } k_rows[] = {
    {"Cont. Cav.", COLONIZE_UNIT_DOMAIN_LAND, 7},
    {"Cavalry", COLONIZE_UNIT_DOMAIN_LAND, 8},
    {"Cont. Army", COLONIZE_UNIT_DOMAIN_LAND, 9},
    {"Regulars", COLONIZE_UNIT_DOMAIN_LAND, 6},
    {"Free Colonist", COLONIZE_UNIT_DOMAIN_LAND, 0},
    {"Soldier", COLONIZE_UNIT_DOMAIN_LAND, 1},
    {"Pioneer", COLONIZE_UNIT_DOMAIN_LAND, 2},
    {"Missionary", COLONIZE_UNIT_DOMAIN_LAND, 3},
    {"Dragoon", COLONIZE_UNIT_DOMAIN_LAND, 4},
    {"Scout", COLONIZE_UNIT_DOMAIN_LAND, 5},
    {"Treasure", COLONIZE_UNIT_DOMAIN_LAND, 0xa},
    {"Artillery", COLONIZE_UNIT_DOMAIN_LAND, 0xb},
    {"Wagon Train", COLONIZE_UNIT_DOMAIN_LAND, 0xc},
    {"Privateer", COLONIZE_UNIT_DOMAIN_SEA, 0x10},
    {"Galleon", COLONIZE_UNIT_DOMAIN_SEA, 0xd},
  };
  const int rows = (int)(sizeof(k_rows) / sizeof(k_rows[0]));

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = rows;
  for (int i = 0; i < rows; ++i) {
    snprintf(units.types[i].name, sizeof(units.types[i].name), "%s", k_rows[i].name);
    units.types[i].domain = k_rows[i].domain;
    units.types[i].movement = 1;
    units.types[i].space = 1;
  }
  for (int i = 0; i < rows; ++i) {
    const int got = ai_euro_5d04_dos_type_code(&units, i);
    if (got != k_rows[i].dos) {
      fprintf(stderr, "unit_ai_euro_5d04_hire: %s -> @UNIT %d (want %d)\n", k_rows[i].name, got,
              k_rows[i].dos);
      return fail("@UNIT code table disagrees with NAMES.TXT order");
    }
  }
  printf("unit_ai_euro_5d04_hire: @UNIT code table matches NAMES.TXT order (%d rows)\n", rows);
  return 0;
}

/*
 * Case 5 — DS:0x5238[type], the hull-space column the recruit-buy loop
 * subtracts from `local_42` (raw 92656). The loop holds a DOS @UNIT code (it
 * has just written one with `set_unit_dispatch_byte`), and DS:0x5238 is a
 * DOS-indexed table, so the code must be translated back to a Linux pool type
 * before the row is read. Until 2026-09-09 the code went straight into
 * `units_type()` as a pool index, which only happened to agree when the pool
 * was loaded from NAMES.TXT in file order.
 *
 * The pool below puts a `space` 9 row exactly at pool index 4 (the @UNIT code
 * for Dragoons) and the real Dragoons row at index 1 with `space` 1, so the
 * translated read and the raw-index read cannot be confused.
 */
static int hull_budget_space_uses_translated_type(void) {
  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 6;
  static const char* const k_names[6] = {"Free Colonist", "Dragoon", "Soldier",
                                         "Pioneer",       "Scout",   "Missionary"};
  static const int k_space[6] = {1, 1, 1, 1, 9, 1};
  for (int i = 0; i < 6; ++i) {
    snprintf(units.types[i].name, sizeof(units.types[i].name), "%s", k_names[i]);
    units.types[i].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[i].movement = 1;
    units.types[i].space = k_space[i];
  }
  const int space = ai_euro_5d04_dos_type_space(&units, 4); /* 4 = @UNIT Dragoons */
  if (space != 1) {
    fprintf(stderr, "unit_ai_euro_5d04_hire: DS:0x5238[4] = %d (want 1, the Dragoon row)\n", space);
    return fail("hull-space read used the DOS code as a pool index");
  }
  /* Scouts (@UNIT 5) is the row that actually sits at pool index 4 — proof
   * the fixture would have caught the old behaviour. */
  if (ai_euro_5d04_dos_type_space(&units, 5) != 9) {
    return fail("fixture wrong: Scouts row should carry space 9");
  }
  printf("unit_ai_euro_5d04_hire: DS:0x5238 read translates the @UNIT code\n");
  return 0;
}

/*
 * Case 6 — the colony `+0x1b` bit 0x10 (NEEDS_COLONISTS) latch, FUN_5952_035e
 * raw 555-563. It was an uncited `population < 3` until 2026-09-09; DOS is
 *
 *   pop < 0x20 && pop < wanted + 2*tier && pop - 2*tier < ring - blocked
 *
 * with `wanted` = FUN_15eb_0484 (8/12/32) and `tier` = FUN_15eb_0470 (2..4).
 * On an unfortified colony that is tier 2 / wanted 8, so the first clause is
 * `pop < 12` and the second `pop - 4 < 8 - blocked`, `blocked` counting
 * off-map / Ocean / High Seas field tiles unless the colony owns Docks.
 */
static int needs_colonists_latch_matches_5952(void) {
  Fixture f;
  if (fixture_init(&f, 1, 5, 7) != 0) {
    return 1;
  }
  /* Building table with a single Docks row, so the Docks override is live. */
  f.colonies.building_type_count = 1;
  snprintf(f.colonies.building_types[0].name, sizeof(f.colonies.building_types[0].name), "Docks");

  ColonizeColony* c = &f.colonies.colonies[0];
  c->active = true;
  c->nation_id = 1;
  c->x = 4; /* all-land ring: blocked = 0 */
  c->y = 4;

  /* Inland: `pop < 12` is the binding clause (pop − 4 < 8 is the same bar). */
  static const struct {
    int pop;
    int want;
  } k_inland[] = {{1, 1}, {3, 1}, {11, 1}, {12, 0}, {20, 0}};
  for (int i = 0; i < (int)(sizeof(k_inland) / sizeof(k_inland[0])); ++i) {
    c->population = k_inland[i].pop;
    const int got = ai_euro_colony_needs_colonists_5952(&f.colonies, &f.map, c) ? 1 : 0;
    if (got != k_inland[i].want) {
      fprintf(stderr, "inland pop=%d -> %d (want %d)\n", k_inland[i].pop, got, k_inland[i].want);
      fixture_free(&f);
      return fail("inland NEEDS_COLONISTS threshold");
    }
  }

  /* Coastal: x = 11 puts the whole x = 12 column (Ocean) in the ring, so
   * blocked = 3 and the second clause tightens to `pop < 9`. */
  c->x = 11;
  c->population = 8;
  if (!ai_euro_colony_needs_colonists_5952(&f.colonies, &f.map, c)) {
    fixture_free(&f);
    return fail("coastal pop 8 should still want colonists (blocked = 3)");
  }
  c->population = 9;
  if (ai_euro_colony_needs_colonists_5952(&f.colonies, &f.map, c)) {
    fixture_free(&f);
    return fail("water field tiles should tighten the latch to pop < 9");
  }
  /* Docks (@BUILDING index 6) zeroes `iStack_142`, restoring the pop < 12 bar. */
  c->has_building[0] = true;
  if (!ai_euro_colony_needs_colonists_5952(&f.colonies, &f.map, c)) {
    fixture_free(&f);
    return fail("Docks should discount the water field tiles");
  }

  /* A Stockade lifts the colony to tier 3 / wanted 12 → `pop < 18`. */
  c->has_building[0] = false;
  c->x = 4;
  c->population = 14;
  if (ai_euro_colony_needs_colonists_5952(&f.colonies, &f.map, c)) {
    fixture_free(&f);
    return fail("unfortified pop 14 must not want colonists");
  }
  fixture_free(&f);
  printf("unit_ai_euro_5d04_hire: NEEDS_COLONISTS latch follows the 5952 capacity test\n");
  return 0;
}

int main(void) {
  if (departing_ship_buys_wanted_cargo() != 0) {
    return 1;
  }
  if (afloat_cargo_cancels_colony_demand() != 0) {
    return 1;
  }
  if (europe_dock_queue_raises_cargo_bar() != 0) {
    return 1;
  }
  if (recruit_swap_follows_colonies_wanting_colonists() != 0) {
    return 1;
  }
  if (pioneer_training_skipped_past_turn_99() != 0) {
    return 1;
  }
  if (dos_type_table_is_names_txt_unit_order() != 0) {
    return 1;
  }
  if (hull_budget_space_uses_translated_type() != 0) {
    return 1;
  }
  if (needs_colonists_latch_matches_5952() != 0) {
    return 1;
  }
  printf("unit_ai_euro_5d04_hire: OK\n");
  return 0;
}
