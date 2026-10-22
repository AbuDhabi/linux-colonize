/* FUN_647e_0094 (destination-picker reachability): travel_dest_ok on a small
 * synthetic map: the Atlantic (region 1) down the east side, a lake (region 3)
 * in the west, land continents 2 and 4. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/colony.h"
#include "core/map.h"
#include "core/travel_dest.h"
#include "../common/test_runner.h"

#define W 14
#define H 10

enum { ATLANTIC = 1, CONT_EAST = 2, LAKE = 3, CONT_WEST = 4 };
enum { A_COAST = 1, B_LAKESIDE, C_INLAND, D_BOTH, E_FOREIGN };

typedef struct Fixture {
  ColonizeWorldMap map;
  ColonizeColonyPool* col;
} Fixture;

static void set_tile(ColonizeWorldMap* m, int x, int y, int terrain, int region) {
  m->terrain[y * W + x] = (uint8_t)terrain;
  m->layer3[y * W + x] = (uint8_t)(0xf0 | region);
}

static void add_colony(Fixture* f, int id, int x, int y, int nation) {
  ColonizeColony* c = &f->col->colonies[id];
  memset(c, 0, sizeof(*c));
  c->id = id;
  c->x = x;
  c->y = y;
  c->nation_id = nation;
  c->active = true;
}

/*
 * East (x >= 8): Atlantic water, region 1 (x = W-1 high seas). West: land,
 * continent 4 for x 0..2 and continent 2 for x 3..7, with a 3x3 lake (region 3)
 * at x 1..3, y 6..8 and one more lake tile at (6,7).
 *   A (7,3): coast, only Atlantic neighbours -> water region 1
 *   B (2,5): lakeside, only lake neighbours  -> water region 3
 *   C (5,3): inland, no water neighbour
 *   D (7,6): touches the Atlantic (8,y) and the lake tile (6,7) -> lowest region 1
 *   E (7,2): another nation's colony
 */
static int fixture_open(Fixture* f) {
  memset(f, 0, sizeof(*f));
  char err[128];
  if (!map_alloc(&f->map, W, H, err, sizeof(err))) {
    fprintf(stderr, "map_alloc: %s\n", err);
    return 1;
  }
  f->col = (ColonizeColonyPool*)calloc(1, sizeof(*f->col));
  if (!f->col) {
    return 1;
  }
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (x >= 8) {
        set_tile(&f->map, x, y, x == W - 1 ? 0x1a : 0x19, ATLANTIC);
      } else {
        set_tile(&f->map, x, y, 3 /* land */, x <= 2 ? CONT_WEST : CONT_EAST);
      }
    }
  }
  for (int y = 6; y <= 8; ++y) {
    for (int x = 1; x <= 3; ++x) {
      set_tile(&f->map, x, y, 0x19, LAKE);
    }
  }
  set_tile(&f->map, 6, 7, 0x19, LAKE); /* a lake tile touching D (7,6) */
  add_colony(f, A_COAST, 7, 3, 1);
  add_colony(f, B_LAKESIDE, 2, 5, 1);
  add_colony(f, C_INLAND, 5, 3, 1);
  add_colony(f, D_BOTH, 7, 6, 1);
  add_colony(f, E_FOREIGN, 7, 2, 2);
  return 0;
}

static void fixture_close(Fixture* f) {
  map_free(&f->map);
  free(f->col);
}

#define OK(f, dest, who) travel_dest_ok(&(f)->map, (f)->col, 1, (dest), (who))

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);       \
      fixture_close(&f);                                               \
      return 1;                                                        \
    }                                                                  \
  } while (0)

/* A ship lying in coastal colony A sees the Atlantic: the Europe row, A, and D
 * (its lowest-region water neighbour is region 1 although a lake tile touches it); not the lake colony B,
 * the inland colony C, nor a foreign colony. */
static int ship_at_sea_colony(void) {
  Fixture f;
  if (fixture_open(&f)) {
    return 1;
  }
  const TravelDestWho ship = {true, 7, 3};
  CHECK(OK(&f, TRAVEL_DEST_EUROPE, &ship));
  CHECK(OK(&f, A_COAST, &ship));
  CHECK(OK(&f, D_BOTH, &ship));
  CHECK(!OK(&f, B_LAKESIDE, &ship));
  CHECK(!OK(&f, C_INLAND, &ship));
  CHECK(!OK(&f, E_FOREIGN, &ship));
  CHECK(!OK(&f, 40, &ship)); /* no such colony */
  fixture_close(&f);
  return 0;
}

/* A ship on the lake reaches only lake-region colonies, never Europe, and not
 * D: DOS compares the REGION of D's lowest-region water neighbour (1), so a
 * colony touching both waters belongs to the ocean only. */
static int ship_on_lake(void) {
  Fixture f;
  if (fixture_open(&f)) {
    return 1;
  }
  const TravelDestWho ship = {true, 2, 7};
  CHECK(!OK(&f, TRAVEL_DEST_EUROPE, &ship));
  CHECK(OK(&f, B_LAKESIDE, &ship));
  CHECK(!OK(&f, A_COAST, &ship));
  CHECK(!OK(&f, D_BOTH, &ship));
  fixture_close(&f);
  return 0;
}

/* Ship docked at the lake colony: no Europe row. Ship off the interior (the
 * trade editor's Europe-first phantom, a Europe-dock pseudo tile): the Atlantic. */
static int ship_lake_dock_and_off_map(void) {
  Fixture f;
  if (fixture_open(&f)) {
    return 1;
  }
  const TravelDestWho lake_dock = {true, 2, 5};
  CHECK(!OK(&f, TRAVEL_DEST_EUROPE, &lake_dock));
  CHECK(OK(&f, B_LAKESIDE, &lake_dock));
  CHECK(!OK(&f, A_COAST, &lake_dock));
  const TravelDestWho phantom = {true, -1, -1};
  CHECK(OK(&f, TRAVEL_DEST_EUROPE, &phantom));
  CHECK(OK(&f, A_COAST, &phantom));
  CHECK(!OK(&f, B_LAKESIDE, &phantom));
  fixture_close(&f);
  return 0;
}

/* Land unit (the TRAVELPLACE list / a land route's phantom): same continent
 * only, no Europe row, no water test (inland C is fine). */
static int land_unit_continent(void) {
  Fixture f;
  if (fixture_open(&f)) {
    return 1;
  }
  const TravelDestWho walker = {false, 6, 4};
  CHECK(OK(&f, A_COAST, &walker));
  CHECK(OK(&f, C_INLAND, &walker));
  CHECK(OK(&f, D_BOTH, &walker));
  CHECK(!OK(&f, B_LAKESIDE, &walker)); /* continent 4 */
  CHECK(!OK(&f, TRAVEL_DEST_EUROPE, &walker));
  CHECK(!OK(&f, E_FOREIGN, &walker));
  fixture_close(&f);
  return 0;
}

/* The create wizard asks with unit -1 (no traveller): every own colony, lake
 * and inland included, never a foreign one. */
static int no_traveller_only_owner(void) {
  Fixture f;
  if (fixture_open(&f)) {
    return 1;
  }
  CHECK(OK(&f, A_COAST, NULL));
  CHECK(OK(&f, B_LAKESIDE, NULL));
  CHECK(OK(&f, C_INLAND, NULL));
  CHECK(!OK(&f, E_FOREIGN, NULL));
  fixture_close(&f);
  return 0;
}

/* FUN_15eb_00a2: lowest-region water neighbour over INSET neighbours only. */
static int water_region_probe(void) {
  Fixture f;
  if (fixture_open(&f)) {
    return 1;
  }
  CHECK(travel_dest_water_region(&f.map, 7, 3) == ATLANTIC);
  CHECK(travel_dest_water_region(&f.map, 2, 5) == LAKE);
  CHECK(travel_dest_water_region(&f.map, 7, 6) == ATLANTIC); /* 1 beats the lake's 3 */
  CHECK(travel_dest_water_region(&f.map, 5, 3) == -1);
  /* A tile on the rim: its off-map neighbours are not water. Column 0 land. */
  CHECK(travel_dest_water_region(&f.map, 0, 4) == -1);
  /* Region 0 ranks last (as 0x10): a 0-region water neighbour only wins when
   * it is the only one, and reads back as 0. */
  set_tile(&f.map, 5, 4, 0x19, 0);
  CHECK(travel_dest_water_region(&f.map, 5, 3) == 0);
  set_tile(&f.map, 4, 4, 0x19, LAKE);
  CHECK(travel_dest_water_region(&f.map, 5, 3) == LAKE);
  fixture_close(&f);
  return 0;
}

static const TestCase k_cases[] = {
  {"ship_at_sea_colony", ship_at_sea_colony},
  {"ship_on_lake", ship_on_lake},
  {"ship_lake_dock_and_off_map", ship_lake_dock_and_off_map},
  {"land_unit_continent", land_unit_continent},
  {"no_traveller_only_owner", no_traveller_only_owner},
  {"water_region_probe", water_region_probe},
};

TEST_MAIN(k_cases)
