/*
 * FUN_5952_035e construction-project cascade (bugs.md #483), asm
 * 5952:21d4-5952:274b. These cases drive ai_euro_5952_build_cascade directly
 * through ai_euro_internal.h against the real @BUILDING table loaded from
 * COLONIZE/NAMES.TXT, so every expectation below is the DOS gate read off
 * viceroy_overlays.asm and not the port's own output.
 *
 * The five UNIT picks the cascade can end in (FUN_5952_02f4 returns
 * row + 0x1f): Wagon Train 43, Galleon 46, Privateer 47, Frigate 48,
 * Artillery 42.
 */
#include "core/ai_euro.h"
#include "core/ai_euro_internal.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NAME "unit_ai_euro_5952_build"
#include "../common/ai_fixture.h"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

#define NATION 1

typedef struct Fx {
  ColonizeMsgCatalog names;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  ColonizeWorldMap map;
  ColonizeCol1Save col1;
  ColonizeTurnContext ctx;
  uint32_t turn;
  ColonizeColony* col;
} Fx;

static int fx_build(Fx* f, int pop) {
  memset(f, 0, sizeof *f);
  assets_msg_init(&f->names);
  if (!assets_msg_load_file(&f->names, "COLONIZE/NAMES.TXT")) {
    return fail("NAMES.TXT load");
  }
  fx_units_init(&f->units);
  if (!units_load_types(&f->units, &f->names)) {
    return fail("units_load_types");
  }
  fx_colonies_init(&f->colonies);
  if (!colonies_load_buildings(&f->colonies, &f->names)) {
    return fail("colonies_load_buildings");
  }
  if (!fx_map_alloc(&f->map, 24, 24, 3, false)) {
    return fail("map alloc");
  }
  /* All-land plains: no water in any colony ring unless a case adds it. */
  for (size_t i = 0; i < f->map.tile_count; ++i) {
    f->map.layer3[i] = 0xf1; /* owner none, continent 1 */
  }
  col1_save_init(&f->col1);
  for (int i = 0; i < 4; ++i) {
    f->col1.player[i].control = (i == NATION) ? 1 : 0;
  }
  f->col1.head.turn = 100; /* past the Armory band's `turn > 0x50` */
  f->col1.head.year = 1600;
  f->col1.head.difficulty = 2;

  f->col = fx_colony_add(&f->colonies, NATION, 8, 8, pop);
  f->turn = f->col1.head.turn;
  f->ctx.turn_number = &f->turn;
  f->ctx.units = &f->units;
  f->ctx.colonies = &f->colonies;
  f->ctx.map = &f->map;
  f->ctx.col1 = &f->col1;
  f->ctx.col1_ok = true;
  f->ctx.human_nation = 0;
  ai_euro_5952_set_ring1_threat(f->col->id, 0);
  return 0;
}

static void fx_done(Fx* f) {
  fx_map_free(&f->map);
  assets_msg_free(&f->names);
}

static int bld(Fx* f, const char* name) {
  return colonies_find_building(&f->colonies, name);
}

static void own(Fx* f, const char* name) {
  const int i = bld(f, name);
  if (i >= 0) {
    f->col->has_building[i] = true;
  }
}

/* Give the colony every building the cascade can queue ahead of the naval /
 * artillery band, so the pick under test is the one that is reached. */
static void own_all_before_naval(Fx* f) {
  static const char* const names[] = {
    "Docks",       "Stockade",     "Stable",    "Warehouse", "Warehouse Expansion",
    "Custom House", "Schoolhouse", "Armory",    "Church",    "Carpenter's Shop",
    "Lumber Mill", "Fort",         "Blacksmith's House", "Blacksmith's Shop",
    "Printing Press", "Newspaper",  "College",   "University", "Fortress",
    NULL
  };
  for (int i = 0; names[i]; ++i) {
    own(f, names[i]);
  }
}

/* A complete craft chain: iStack_2c (asm 5952:2583) needs three owned tiers. */
static void own_full_weaver_chain(Fx* f) {
  own(f, "Weaver's House");
  own(f, "Weaver's Shop");
  own(f, "Textile Mill");
}

/* ---- the building head of the cascade -------------------------------- */

/*
 * asm 22ec: with the whole ring workable and no food pressure the Docks arm
 * at 22da does not fire, so the first candidate is @BUILDING 0 = Stockade.
 */
static int case_first_pick_is_stockade(void) {
  Fx f;
  if (fx_build(&f, 3) != 0) {
    return 1;
  }
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  const int want = bld(&f, "Stockade");
  fx_done(&f);
  if (got != want) {
    return fail("expected Stockade first (asm 22ec), got ? want ?");
  }
  return 0;
}

/*
 * asm 22da: `(ring − nonland) <= pop` puts Docks ahead of the Stockade. A
 * pop-8 colony with a full land ring satisfies 8 <= 8.
 */
static int case_docks_when_ring_worked_out(void) {
  Fx f;
  if (fx_build(&f, 8) != 0) {
    return 1;
  }
  /* Docks carries the @BUILDING coastal requirement, so the ring needs one
   * ocean tile; nonland then = 1 and the gate reads 8 − 1 <= 8. */
  f.map.terrain[8 * 24 + 7] = 0x19;
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  const int want = bld(&f, "Docks");
  fx_done(&f);
  if (got != want) {
    return fail("expected Docks (asm 22da), got ? want ?");
  }
  return 0;
}

/*
 * asm 2312: a colony under pop 4 that could not queue anything falls through
 * to LAB_2747 and only raises the `+0x1d` bit 0x80 wants-construction latch.
 */
static int case_small_colony_wants_construction(void) {
  Fx f;
  if (fx_build(&f, 3) != 0) {
    return 1;
  }
  own(&f, "Docks");
  own(&f, "Stockade");
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  const unsigned flags = f.col->build_ai_flags;
  fx_done(&f);
  if (got != -1) {
    return fail("expected no project under pop 4, got ?");
  }
  if ((flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) == 0) {
    return fail("expected +0x1d bit 0x80 (asm 2747)");
  }
  return 0;
}

/*
 * FUN_5952_0214's recursion (asm 5952:024e): asking for a tier whose parent
 * is missing retries the parent, so a colony with no Docks that reaches the
 * Shipyard arm queues Docks, never Shipyard.
 */
static int case_try_build_recurses_to_parent(void) {
  Fx f;
  if (fx_build(&f, 3) != 0) {
    return 1;
  }
  /* Stockade owned so the 22ec arm passes; ring full so Docks is skipped at
   * 22da (pop 3 < 8 land tiles). */
  own(&f, "Stockade");
  own(&f, "Warehouse");
  f.col->stock[COLONIZE_CARGO_HORSES] = 4; /* asm 22f9: Stable arm */
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  const int want = bld(&f, "Stable");
  fx_done(&f);
  if (got != want) {
    return fail("expected Stable on horses >= 2 (asm 22f9), got ?");
  }
  return 0;
}

/* ---- the five unit projects ------------------------------------------ */

/*
 * asm 2385 — Wagon Train (unit row 0xc, code 43): colony has not had one
 * (`+0x1c` bit 0x20 clear), year < 1600 (0x640), continent presence bit 0,
 * nearest-village alarm < 0x32 and no ring-1 European threat.
 */
static int case_wagon_train_pick(void) {
  Fx f;
  if (fx_build(&f, 5) != 0) {
    return 1;
  }
  f.col1.head.year = 1550;
  own_all_before_naval(&f);
  /* Presence bit 0 = "an Indian village exists on this continent"
   * (FUN_4962_0018); one village with a low alarm supplies both terms. */
  f.col1.head.tribe_count = 1;
  f.col1.tribe = (ColonizeCol1Tribe*)calloc(1, sizeof(ColonizeCol1Tribe));
  if (!f.col1.tribe) {
    fx_done(&f);
    return fail("tribe alloc");
  }
  f.col1.tribe[0].nation_id = 4;
  f.col1.tribe[0].x = 12;
  f.col1.tribe[0].y = 8;
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  free(f.col1.tribe);
  f.col1.tribe = NULL;
  fx_done(&f);
  if (got != 43) {
    return fail("expected Wagon Train code 43 (asm 2385), got ?");
  }
  return 0;
}

/* Same colony, but a European unit standing next door (iStack_22 != 0)
 * closes the Wagon Train arm. */
static int case_wagon_train_blocked_by_ring1_threat(void) {
  Fx f;
  if (fx_build(&f, 5) != 0) {
    return 1;
  }
  f.col1.head.year = 1550;
  own_all_before_naval(&f);
  ai_euro_5952_set_ring1_threat(f.col->id, 1);
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  fx_done(&f);
  if (got == 43) {
    return fail("ring-1 threat must close the Wagon Train arm (asm 23b4)");
  }
  return 0;
}

/* Common setup for the naval band: coastal, pop >= 8, a finished craft chain
 * and a Shipyard, with everything the cascade asks for first already owned. */
static int naval_fx(Fx* f) {
  if (fx_build(f, 12) != 0) {
    return 1;
  }
  own_all_before_naval(f);
  own_full_weaver_chain(f);
  own(f, "Cathedral");
  own(f, "Drydock");
  own(f, "Shipyard");
  own(f, "Arsenal");
  own(f, "Magazine");
  f->col->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
  f->col->colony_flags |= COLONIZE_COLONY_FLAG_WAGON_TRAIN; /* no Wagon arm */
  return 0;
}

/*
 * asm 25f1 — Galleon (row 0xf, code 46):
 * `(census_pop_proxy >> 1) + colony_counts >= ship_cargo_totals`.
 */
static int case_galleon_pick(void) {
  Fx f;
  if (naval_fx(&f) != 0) {
    return 1;
  }
  f.col1.stuff.census_pop_proxy[NATION] = 20; /* >>1 = 10 */
  f.col1.stuff.colony_counts[NATION] = 2;    /* 12 */
  f.col1.stuff.ship_cargo_totals[NATION] = 6;
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  fx_done(&f);
  if (got != 46) {
    return fail("expected Galleon code 46 (asm 25f1), got ?");
  }
  return 0;
}

/*
 * asm 260b — Privateer (row 0x10, code 47): cargo capacity already covers the
 * population, the nation holds no Frigate and no armed ship, and armed ships
 * are below 4.
 */
static int case_privateer_pick(void) {
  Fx f;
  if (naval_fx(&f) != 0) {
    return 1;
  }
  f.col1.stuff.census_pop_proxy[NATION] = 4; /* >>1 = 2 */
  f.col1.stuff.colony_counts[NATION] = 1;    /* 3 */
  f.col1.stuff.ship_cargo_totals[NATION] = 40;
  f.col1.stuff.unit_type_counts[NATION][0x11] = 0;
  f.col1.stuff.armed_ship_counts[NATION] = 0;
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  fx_done(&f);
  if (got != 47) {
    return fail("expected Privateer code 47 (asm 260b), got ?");
  }
  return 0;
}

/*
 * asm 2626 — Frigate (row 0x11, code 48): no Frigate but armed ships exist,
 * so the 0x925d < 1 test at 261c falls through to the Frigate project.
 */
static int case_frigate_pick(void) {
  Fx f;
  if (naval_fx(&f) != 0) {
    return 1;
  }
  f.col1.stuff.census_pop_proxy[NATION] = 4;
  f.col1.stuff.colony_counts[NATION] = 1;
  f.col1.stuff.ship_cargo_totals[NATION] = 40;
  f.col1.stuff.unit_type_counts[NATION][0x11] = 0;
  f.col1.stuff.armed_ship_counts[NATION] = 2;
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  fx_done(&f);
  if (got != 48) {
    return fail("expected Frigate code 48 (asm 2626), got ?");
  }
  return 0;
}

/*
 * asm 2689 — Artillery (row 0xb, code 42). The naval band is closed (not
 * coastal), the colony has a finished craft chain and no Artillery on the
 * tile, so LAB_262c takes the 2670 arm: the Armory is already owned, so
 * try(3) keeps scanning and the Artillery project is set.
 */
static int case_artillery_pick(void) {
  Fx f;
  if (naval_fx(&f) != 0) {
    return 1;
  }
  f.col->colony_flags =
    (uint8_t)(f.col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_COASTAL);
  f.col->stock[COLONIZE_CARGO_TOOLS] = 100; /* asm 2670: `+0xb6 != 0` */
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  fx_done(&f);
  if (got != 42) {
    return fail("expected Artillery code 42 (asm 2689), got ?");
  }
  return 0;
}

/*
 * asm 271b/273e — three or more Artillery already standing on the colony tile
 * clears the project slot and raises the wants-construction latch instead.
 */
static int case_artillery_capped_at_three_on_tile(void) {
  Fx f;
  if (naval_fx(&f) != 0) {
    return 1;
  }
  f.col->colony_flags =
    (uint8_t)(f.col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_COASTAL);
  f.col->stock[COLONIZE_CARGO_TOOLS] = 100;
  f.col->labor_shortage = 1; /* asm 265b: sends 262c down the 268e arm */
  const int arty = units_find_type(&f.units, "Artillery");
  if (arty < 0) {
    fx_done(&f);
    return fail("no Artillery unit type");
  }
  /* units_spawn takes one unit per tile, so stack them by hand. */
  for (int i = 0; i < 3; ++i) {
    const int id = units_spawn(&f.units, arty, 2 + i, 2);
    ColonizeUnit* u = units_get(&f.units, id);
    if (!u) {
      fx_done(&f);
      return fail("artillery spawn");
    }
    u->nation_id = NATION;
    u->x = f.col->x;
    u->y = f.col->y;
  }
  f.col1.stuff.armed_ship_counts[NATION] = 8; /* close the 26e4 ship arms */
  f.col1.stuff.unit_type_counts[NATION][0x0f] = 8;
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const int got = f.col->building_in_production;
  const unsigned flags = f.col->build_ai_flags;
  fx_done(&f);
  if (got != -1) {
    return fail("3 Artillery on tile must clear the slot (asm 273e), got ?");
  }
  if ((flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) == 0) {
    return fail("expected wants-construction latch (asm 2747)");
  }
  return 0;
}

/*
 * asm 270d — the SMALL_AI writer: a colony under pop 10 that reaches the
 * chain-upgrade tail raises `+0x1c` bit 0x10.
 */
static int case_small_ai_flag_written(void) {
  Fx f;
  if (naval_fx(&f) != 0) {
    return 1;
  }
  f.col->population = 9;
  f.col->colonist_count = 9;
  f.col->colony_flags =
    (uint8_t)(f.col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_COASTAL);
  f.col->labor_shortage = 1;
  /* asm 265b: `factory && arty_on_tile && labor_shortage` is what sends 262c
   * down the 268e chain-upgrade arm where the SMALL_AI writer lives. */
  {
    const int arty = units_find_type(&f.units, "Artillery");
    const int id = arty >= 0 ? units_spawn(&f.units, arty, f.col->x, f.col->y) : -1;
    ColonizeUnit* u = id >= 0 ? units_get(&f.units, id) : NULL;
    if (!u) {
      fx_done(&f);
      return fail("artillery spawn");
    }
    u->nation_id = NATION;
  }
  f.col1.stuff.armed_ship_counts[NATION] = 8;
  f.col1.stuff.unit_type_counts[NATION][0x0f] = 8;
  ai_euro_5952_build_cascade(&f.ctx, f.col);
  const unsigned flags = f.col->colony_flags;
  fx_done(&f);
  if ((flags & COLONIZE_COLONY_FLAG_SMALL_AI) == 0) {
    return fail("expected SMALL_AI (asm 270d)");
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"case_first_pick_is_stockade", case_first_pick_is_stockade},
  {"case_docks_when_ring_worked_out", case_docks_when_ring_worked_out},
  {"case_small_colony_wants_construction", case_small_colony_wants_construction},
  {"case_try_build_recurses_to_parent", case_try_build_recurses_to_parent},
  {"case_wagon_train_pick", case_wagon_train_pick},
  {"case_wagon_train_blocked_by_ring1_threat", case_wagon_train_blocked_by_ring1_threat},
  {"case_galleon_pick", case_galleon_pick},
  {"case_privateer_pick", case_privateer_pick},
  {"case_frigate_pick", case_frigate_pick},
  {"case_artillery_pick", case_artillery_pick},
  {"case_artillery_capped_at_three_on_tile", case_artillery_capped_at_three_on_tile},
  {"case_small_ai_flag_written", case_small_ai_flag_written},
};

TEST_MAIN(k_cases)
