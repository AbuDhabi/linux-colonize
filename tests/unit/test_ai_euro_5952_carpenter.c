/*
 * FUN_5952_035e carpenter-staffing arm, per-pass election (raw 94690-94740;
 * annotated colony_tick_5952_035e.md:1114-1160). Covered here:
 *   - the four passes and their order: 0x0d, 0x1c, 0x19, anyone;
 *   - DOS's profession rewrite at raw 94716-94718 (FUN_281f_0cae(slot, 0x1c)):
 *     an Indentured Servant or a Petty Criminal becomes a Free Colonist when
 *     this arm picks it, and `out_prof` still reports the profession DOS's
 *     cached aiStack_12e would have (the 0cae write never updates it);
 *   - already-placed slots are skipped.
 */
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/units.h"

#include <string.h>

#define TEST_NAME "unit_ai_euro_5952_carpenter"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

static void fx_colony(ColonizeColony* c, const int* profs, int n) {
  memset(c, 0, sizeof *c);
  c->active = true;
  c->population = (uint8_t)n;
  c->colonist_count = n;
  for (int i = 0; i < n; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].profession = (uint8_t)profs[i];
    c->colonists[i].building_type = -1;
    c->colonists[i].field_job = -1;
  }
}

/* Pass 0 takes the Master Carpenter, pass 1 the Free Colonist, pass 2 the
 * Servant, pass 3 whatever is left — here the Criminal. */
static int case_pass_order(void) {
  ColonizeColony c;
  const int profs[] = {
    COLONIZE_PROF_CRIMINAL, COLONIZE_PROF_INDENTURED, COLONIZE_PROF_FREE_COLONIST,
    COLONIZE_PROF_CARPENTER
  };
  bool placed[COLONIZE_COLONY_POP_MAX];
  memset(placed, 0, sizeof placed);

  fx_colony(&c, profs, 4);
  if (ai_euro_5952_carpenter_pick(&c, placed, 4, 0, 0, NULL) != 3) {
    return fail("pass 0 must take the Master Carpenter");
  }
  fx_colony(&c, profs, 4);
  if (ai_euro_5952_carpenter_pick(&c, placed, 4, 1, 0, NULL) != 2) {
    return fail("pass 1 must take the Free Colonist");
  }
  fx_colony(&c, profs, 4);
  if (ai_euro_5952_carpenter_pick(&c, placed, 4, 2, 0, NULL) != 1) {
    return fail("pass 2 must take the Indentured Servant");
  }
  fx_colony(&c, profs, 4);
  if (ai_euro_5952_carpenter_pick(&c, placed, 4, 3, 0, NULL) != 0) {
    return fail("pass 3 must take the first unplaced slot, whoever it is");
  }
  /* No candidate for a pass whose profession is absent. */
  const int only_convert[] = {COLONIZE_PROF_CONVERT};
  fx_colony(&c, only_convert, 1);
  if (ai_euro_5952_carpenter_pick(&c, placed, 1, 0, 0, NULL) != -1) {
    return fail("pass 0 must find nothing without a Master Carpenter");
  }
  return 0;
}

/* raw 94716-94718: 0x19/0x1a -> 0x1c on the chosen colonist; nobody else. */
static int case_servant_criminal_rewrite(void) {
  ColonizeColony c;
  bool placed[COLONIZE_COLONY_POP_MAX];
  memset(placed, 0, sizeof placed);

  const int servant[] = {COLONIZE_PROF_INDENTURED, COLONIZE_PROF_INDENTURED};
  fx_colony(&c, servant, 2);
  int prof = -1;
  if (ai_euro_5952_carpenter_pick(&c, placed, 2, 2, 0, &prof) != 0) {
    return fail("pass 2 must take slot 0");
  }
  if (prof != COLONIZE_PROF_INDENTURED) {
    return fail("out_prof must report the pre-rewrite profession");
  }
  if ((int)c.colonists[0].profession != COLONIZE_PROF_FREE_COLONIST) {
    return fail("servant must be rewritten to Free Colonist");
  }
  if ((int)c.colonists[1].profession != COLONIZE_PROF_INDENTURED) {
    return fail("only the chosen colonist is rewritten");
  }

  const int criminal[] = {COLONIZE_PROF_CRIMINAL};
  fx_colony(&c, criminal, 1);
  prof = -1;
  if (ai_euro_5952_carpenter_pick(&c, placed, 1, 3, 0, &prof) != 0) {
    return fail("pass 3 must take the criminal");
  }
  if (prof != COLONIZE_PROF_CRIMINAL ||
      (int)c.colonists[0].profession != COLONIZE_PROF_FREE_COLONIST) {
    return fail("criminal must be rewritten to Free Colonist");
  }

  /* A Free Colonist and a Master Carpenter are left alone. */
  const int others[] = {COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_CARPENTER};
  fx_colony(&c, others, 2);
  (void)ai_euro_5952_carpenter_pick(&c, placed, 2, 1, 0, NULL);
  (void)ai_euro_5952_carpenter_pick(&c, placed, 2, 0, 0, NULL);
  if ((int)c.colonists[0].profession != COLONIZE_PROF_FREE_COLONIST ||
      (int)c.colonists[1].profession != COLONIZE_PROF_CARPENTER) {
    return fail("no rewrite outside 0x19/0x1a");
  }
  return 0;
}

/* Placed slots and the `start` cursor both skip forward. */
static int case_placed_and_start_skip(void) {
  ColonizeColony c;
  const int profs[] = {
    COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_FREE_COLONIST, COLONIZE_PROF_FREE_COLONIST
  };
  bool placed[COLONIZE_COLONY_POP_MAX];
  memset(placed, 0, sizeof placed);
  placed[0] = true;
  fx_colony(&c, profs, 3);
  if (ai_euro_5952_carpenter_pick(&c, placed, 3, 1, 0, NULL) != 1) {
    return fail("a placed slot is skipped");
  }
  if (ai_euro_5952_carpenter_pick(&c, placed, 3, 1, 2, NULL) != 2) {
    return fail("the start cursor advances the scan");
  }
  if (ai_euro_5952_carpenter_pick(&c, placed, 3, 1, 3, NULL) != -1) {
    return fail("a cursor past the roster finds nothing");
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"pass_order", case_pass_order},
  {"servant_criminal_rewrite", case_servant_criminal_rewrite},
  {"placed_and_start_skip", case_placed_and_start_skip},
};

TEST_MAIN(k_cases)
