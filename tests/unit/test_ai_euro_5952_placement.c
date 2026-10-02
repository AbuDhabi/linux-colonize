/*
 * FUN_5952_035e colonist-placement section gate (raw 94570-94584, bugs.md
 * #1028): local_80 = pop < ring*2, local_1e = (stock[HORSES] >= 2 &&
 * warehouse_capacity > stock[HORSES]), local_7e = local_80 || local_1e.
 * ai_euro_5952_placement_gate() is the extracted DOS-literal body; this
 * exercises it directly against a bare colony (no map/tick needed) since
 * the full AI tick's unconditional indoor pass (raw 94784+) means "nobody
 * got a field_job" is not an observable the gate alone controls end to end.
 */
#include "core/ai_euro_internal.h"
#include "core/colony.h"

#include <string.h>

#define TEST_NAME "unit_ai_euro_5952_placement"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

static void fx_colony(ColonizeColonyPool* pool, ColonizeColony* col, int pop) {
  memset(pool, 0, sizeof *pool);
  memset(col, 0, sizeof *col);
  col->population = pop;
}

/* Well-fed, well-stocked colony: ring(no buildings)=8, pop=16 => local_80
 * false; horses=0 => local_1e false (horses < 2). local_7e must be false. */
static int case_fed_colony_gate_closed(void) {
  ColonizeColonyPool pool;
  ColonizeColony col;
  fx_colony(&pool, &col, 16);
  col.stock[COLONIZE_CARGO_HORSES] = 0;
  bool local_80 = true;
  bool local_1e = true;
  const bool local_7e = ai_euro_5952_placement_gate(&pool, &col, &local_80, &local_1e);
  if (local_80 || local_1e || local_7e) {
    return fail("pop>=ring*2 and horses<2 should close the gate (bugs.md #1028)");
  }
  return 0;
}

/* Small colony: pop(4) < ring(8)*2 => local_80 true => local_7e true. */
static int case_small_colony_opens_on_local_80(void) {
  ColonizeColonyPool pool;
  ColonizeColony col;
  fx_colony(&pool, &col, 4);
  bool local_80 = false;
  bool local_1e = true;
  const bool local_7e = ai_euro_5952_placement_gate(&pool, &col, &local_80, &local_1e);
  if (!local_80 || local_1e || !local_7e) {
    return fail("pop < ring*2 should set local_80 and open the gate");
  }
  return 0;
}

/* Big colony with horse-breeding headroom: pop>=ring*2 keeps local_80
 * false, but 2 <= horses < warehouse_capacity (100) sets local_1e => gate
 * opens through local_1e alone. */
static int case_horse_headroom_opens_on_local_1e(void) {
  ColonizeColonyPool pool;
  ColonizeColony col;
  fx_colony(&pool, &col, 16);
  col.stock[COLONIZE_CARGO_HORSES] = 50; /* 2 <= 50 < 100 (base capacity) */
  bool local_80 = true;
  bool local_1e = false;
  const bool local_7e = ai_euro_5952_placement_gate(&pool, &col, &local_80, &local_1e);
  if (local_80 || !local_1e || !local_7e) {
    return fail("2 <= horses < warehouse_capacity should set local_1e and open the gate");
  }
  return 0;
}

/* Horse stock at/above warehouse capacity closes local_1e again. */
static int case_horse_stock_at_capacity_closes_local_1e(void) {
  ColonizeColonyPool pool;
  ColonizeColony col;
  fx_colony(&pool, &col, 16);
  col.stock[COLONIZE_CARGO_HORSES] = 100; /* == base warehouse capacity */
  bool local_80 = true;
  bool local_1e = true;
  const bool local_7e = ai_euro_5952_placement_gate(&pool, &col, &local_80, &local_1e);
  if (local_80 || local_1e || local_7e) {
    return fail("horses >= warehouse_capacity should close local_1e too");
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"case_fed_colony_gate_closed", case_fed_colony_gate_closed},
  {"case_small_colony_opens_on_local_80", case_small_colony_opens_on_local_80},
  {"case_horse_headroom_opens_on_local_1e", case_horse_headroom_opens_on_local_1e},
  {"case_horse_stock_at_capacity_closes_local_1e", case_horse_stock_at_capacity_closes_local_1e},
};

TEST_MAIN(k_cases)
