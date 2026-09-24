/*
 * FUN_5952_035e musket-surplus bank, raw 94362-94367 (bugs.md #914).
 *
 * The arm runs at the tail of the colony tick's build-preference block:
 *   if (local_76 == 0 && 199 < +0xb8 && nation[+0x49] < 0x14) {
 *     nation[+0x49]++; +0xb8 -= 0x32;
 *   }
 * It is driven here through ai_euro_5952_absorb_equip (ai_euro_internal.h),
 * the frame that owns local_76 (`labor_running`) and calls the block.
 */
#include "core/ai_euro.h"
#include "core/ai_euro_internal.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <string.h>

#define TEST_NAME "unit_ai_euro_goals_bank"
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
  ColonizeDosRng rng;
  uint32_t turn;
  ColonizeColony* col;
} Fx;

static int fx_build(Fx* f) {
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
  for (size_t i = 0; i < f->map.tile_count; ++i) {
    f->map.layer3[i] = 0xf1; /* owner none, continent 1 */
  }
  col1_save_init(&f->col1);
  for (int i = 0; i < 4; ++i) {
    f->col1.player[i].control = (i == NATION) ? 1 : 0;
  }
  f->col1.head.turn = 100;
  f->col1.head.year = 1600;
  f->col1.head.difficulty = 2;
  dos_rng_seed(&f->rng, 12345u);
  /* pop 3: under the `equip_pop > 10` gate, so local_90 stays 0 and the
   * equip arm cannot eject a colonist and spend the muskets under test. */
  f->col = fx_colony_add(&f->colonies, NATION, 8, 8, 3);
  f->col->ai_flags = 0;
  f->turn = f->col1.head.turn;
  f->ctx.turn_number = &f->turn;
  f->ctx.units = &f->units;
  f->ctx.colonies = &f->colonies;
  f->ctx.map = &f->map;
  f->ctx.col1 = &f->col1;
  f->ctx.col1_ok = true;
  f->ctx.rng = &f->rng;
  f->ctx.human_nation = 0;
  ai_euro_5952_set_ring1_threat(f->col->id, 0);
  memset(ai_euro_s_5d04_hire_scratch, 0, sizeof ai_euro_s_5d04_hire_scratch);
  return 0;
}

static void fx_done(Fx* f) {
  fx_map_free(&f->map);
  assets_msg_free(&f->names);
}

static int run_tick(Fx* f, int labor_running) {
  ai_euro_5952_absorb_equip(&f->ctx, NATION, f->col, &labor_running);
  return labor_running;
}

/* raw 94362-94367: 250 muskets, no garrison shortfall, empty bank. */
static int case_surplus_banks_one_lot(void) {
  Fx f;
  if (fx_build(&f) != 0) {
    return 1;
  }
  f.col->stock[COLONIZE_CARGO_MUSKETS] = 250;
  (void)run_tick(&f, 0);
  int rc = 0;
  if (ai_euro_s_5d04_hire_scratch[NATION].musket_bank_lots != 1) {
    rc = fail("bank not credited");
  }
  if (f.col->stock[COLONIZE_CARGO_MUSKETS] != 200) {
    rc = fail("muskets not debited by 50");
  }
  fx_done(&f);
  return rc;
}

/* `local_76 == 0` gate: a colony short of hands keeps its muskets. */
static int case_garrison_need_blocks_bank(void) {
  Fx f;
  if (fx_build(&f) != 0) {
    return 1;
  }
  f.col->stock[COLONIZE_CARGO_MUSKETS] = 250;
  (void)run_tick(&f, 1);
  int rc = 0;
  if (ai_euro_s_5d04_hire_scratch[NATION].musket_bank_lots != 0) {
    rc = fail("bank credited despite labor shortfall");
  }
  if (f.col->stock[COLONIZE_CARGO_MUSKETS] != 250) {
    rc = fail("muskets debited despite labor shortfall");
  }
  fx_done(&f);
  return rc;
}

/* `nation[+0x49] < 0x14` cap. */
static int case_full_bank_blocks(void) {
  Fx f;
  if (fx_build(&f) != 0) {
    return 1;
  }
  f.col->stock[COLONIZE_CARGO_MUSKETS] = 250;
  ai_euro_s_5d04_hire_scratch[NATION].musket_bank_lots = 20;
  (void)run_tick(&f, 0);
  int rc = 0;
  if (ai_euro_s_5d04_hire_scratch[NATION].musket_bank_lots != 20) {
    rc = fail("bank grew past 0x14");
  }
  if (f.col->stock[COLONIZE_CARGO_MUSKETS] != 250) {
    rc = fail("muskets debited with a full bank");
  }
  fx_done(&f);
  return rc;
}

/* `199 < +0xb8` gate is strict. */
static int case_exactly_199_blocks(void) {
  Fx f;
  if (fx_build(&f) != 0) {
    return 1;
  }
  f.col->stock[COLONIZE_CARGO_MUSKETS] = 199;
  (void)run_tick(&f, 0);
  int rc = 0;
  if (ai_euro_s_5d04_hire_scratch[NATION].musket_bank_lots != 0) {
    rc = fail("bank credited at 199 muskets");
  }
  if (f.col->stock[COLONIZE_CARGO_MUSKETS] != 199) {
    rc = fail("muskets debited at 199");
  }
  fx_done(&f);
  return rc;
}

static const TestCase k_cases[] = {
  {"case_surplus_banks_one_lot", case_surplus_banks_one_lot},
  {"case_garrison_need_blocks_bank", case_garrison_need_blocks_bank},
  {"case_full_bank_blocks", case_full_bank_blocks},
  {"case_exactly_199_blocks", case_exactly_199_blocks},
};

TEST_MAIN(k_cases)
