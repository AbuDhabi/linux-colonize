/*
 * FUN_5952_035e forced-lumberjack pass (raw 94659-94679) and the AI's
 * emergency lumber purchase (raw 94680-94689); annotated
 * colony_tick_5952_035e.md:1074-1112. Covered here:
 *   - the three passes and their order: profession 5, any non-expert, anyone;
 *   - that the arm does NOT rewrite a Servant/Criminal profession (unlike the
 *     carpenter arm's 0cae call one arm later);
 *   - the purchase's three gates (no lumberjack placed, stock < 2,
 *     turn & 7 == 0) and its DOS asymmetry: the 100 lumber is credited
 *     unconditionally, the 200 gold only comes off a purse that holds it.
 */
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/europe.h"
#include "core/col1_save.h"

#include <string.h>

#define TEST_NAME "unit_ai_euro_5952_lumber"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

static void fx_colony(ColonizeColony* c, const int* profs, int n) {
  memset(c, 0, sizeof *c);
  c->active = true;
  c->population = (uint8_t)n;
  c->colonist_count = n;
  c->nation_id = 0;
  for (int i = 0; i < n; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].profession = (uint8_t)profs[i];
    c->colonists[i].building_type = -1;
    c->colonists[i].field_job = -1;
  }
}

/* Pass 0 takes the Expert Lumberjack, pass 1 the first non-expert, pass 2
 * the first unplaced slot whoever it is. */
static int case_pass_order(void) {
  ColonizeColony c;
  bool placed[COLONIZE_COLONY_POP_MAX];
  memset(placed, 0, sizeof placed);

  /* 0 Master Carpenter (expert), 1 Indentured Servant (not), 2 Lumberjack. */
  const int profs[] = {
    COLONIZE_PROF_CARPENTER, COLONIZE_PROF_INDENTURED, COLONIZE_PROF_LUMBERJACK
  };
  fx_colony(&c, profs, 3);
  if (ai_euro_5952_lumberjack_pick(&c, placed, 3, 0, 0) != 2) {
    return fail("pass 0 must take the Expert Lumberjack");
  }
  if (ai_euro_5952_lumberjack_pick(&c, placed, 3, 1, 0) != 1) {
    return fail("pass 1 must take the first non-expert");
  }
  if (ai_euro_5952_lumberjack_pick(&c, placed, 3, 2, 0) != 0) {
    return fail("pass 2 must take the first unplaced slot");
  }

  /* No Expert Lumberjack in the colony -> pass 0 finds nothing. */
  const int no_lj[] = {COLONIZE_PROF_CARPENTER, COLONIZE_PROF_FREE_COLONIST};
  fx_colony(&c, no_lj, 2);
  if (ai_euro_5952_lumberjack_pick(&c, placed, 2, 0, 0) != -1) {
    return fail("pass 0 must find nothing without an Expert Lumberjack");
  }
  /* All-expert colony -> pass 1 finds nothing. */
  const int all_expert[] = {COLONIZE_PROF_CARPENTER, COLONIZE_PROF_LUMBERJACK};
  fx_colony(&c, all_expert, 2);
  if (ai_euro_5952_lumberjack_pick(&c, placed, 2, 1, 0) != -1) {
    return fail("pass 1 must find nothing when everyone is an expert");
  }
  return 0;
}

/* No 0cae rewrite in this arm: the picked colonist keeps its identity. */
static int case_no_profession_rewrite(void) {
  ColonizeColony c;
  bool placed[COLONIZE_COLONY_POP_MAX];
  memset(placed, 0, sizeof placed);

  const int profs[] = {COLONIZE_PROF_CRIMINAL, COLONIZE_PROF_INDENTURED};
  fx_colony(&c, profs, 2);
  if (ai_euro_5952_lumberjack_pick(&c, placed, 2, 1, 0) != 0) {
    return fail("pass 1 must take the Petty Criminal");
  }
  if ((int)c.colonists[0].profession != COLONIZE_PROF_CRIMINAL) {
    return fail("the forced-lumberjack arm must not rewrite a Criminal");
  }
  if (ai_euro_5952_lumberjack_pick(&c, placed, 2, 1, 1) != 1) {
    return fail("the start cursor advances the scan");
  }
  if ((int)c.colonists[1].profession != COLONIZE_PROF_INDENTURED) {
    return fail("the forced-lumberjack arm must not rewrite a Servant");
  }
  if (ai_euro_5952_lumberjack_pick(&c, placed, 2, 1, 2) != -1) {
    return fail("a cursor past the roster finds nothing");
  }
  placed[0] = true;
  if (ai_euro_5952_lumberjack_pick(&c, placed, 2, 2, 0) != 1) {
    return fail("an already-placed slot must be skipped");
  }
  return 0;
}

/* raw 94680-94689: the three gates and the two effects. */
static int case_purchase_gates(void) {
  ColonizeColony c;
  ColonizeCol1Save sv;
  const int profs[] = {COLONIZE_PROF_FREE_COLONIST};

  /* All gates open, purse rich: +100 lumber, -200 gold. */
  fx_colony(&c, profs, 1);
  memset(&sv, 0, sizeof sv);
  sv.nation[0].gold = 1000;
  c.stock[COLONIZE_CARGO_LUMBER] = 1;
  ai_euro_5952_lumber_purchase(NULL, &sv, &c, 8, false);
  if (c.stock[COLONIZE_CARGO_LUMBER] != 101) {
    return fail("the colony must be credited exactly 100 lumber");
  }
  if (sv.nation[0].gold != 800) {
    return fail("the purse must be debited exactly 200 gold");
  }

  /* Broke purse: DOS still credits the lumber, and takes nothing. */
  fx_colony(&c, profs, 1);
  memset(&sv, 0, sizeof sv);
  sv.nation[0].gold = 199;
  c.stock[COLONIZE_CARGO_LUMBER] = 0;
  ai_euro_5952_lumber_purchase(NULL, &sv, &c, 0, false);
  if (c.stock[COLONIZE_CARGO_LUMBER] != 100 || sv.nation[0].gold != 199) {
    return fail("under 200 gold the lumber still arrives and nothing is paid");
  }
  /* Exactly 200 pays. */
  fx_colony(&c, profs, 1);
  memset(&sv, 0, sizeof sv);
  sv.nation[0].gold = 200;
  ai_euro_5952_lumber_purchase(NULL, &sv, &c, 16, false);
  if (sv.nation[0].gold != 0) {
    return fail("200 gold is exactly affordable");
  }

  /* Gate: stock >= 2. */
  fx_colony(&c, profs, 1);
  memset(&sv, 0, sizeof sv);
  sv.nation[0].gold = 1000;
  c.stock[COLONIZE_CARGO_LUMBER] = 2;
  ai_euro_5952_lumber_purchase(NULL, &sv, &c, 8, false);
  if (c.stock[COLONIZE_CARGO_LUMBER] != 2 || sv.nation[0].gold != 1000) {
    return fail("2 lumber in stock closes the gate");
  }

  /* Gate: turn & 7. */
  c.stock[COLONIZE_CARGO_LUMBER] = 0;
  ai_euro_5952_lumber_purchase(NULL, &sv, &c, 9, false);
  if (c.stock[COLONIZE_CARGO_LUMBER] != 0 || sv.nation[0].gold != 1000) {
    return fail("the arm runs one turn in eight only");
  }

  /* Gate: a lumberjack was placed this tick. */
  ai_euro_5952_lumber_purchase(NULL, &sv, &c, 8, true);
  if (c.stock[COLONIZE_CARGO_LUMBER] != 0 || sv.nation[0].gold != 1000) {
    return fail("a placed lumberjack closes the gate");
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"pass_order", case_pass_order},
  {"no_profession_rewrite", case_no_profession_rewrite},
  {"purchase_gates", case_purchase_gates},
};

TEST_MAIN(k_cases)
