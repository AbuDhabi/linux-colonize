/*
 * FUN_5952_035e "train an expert in this colony" pick, raw 95926-95944
 * (asm viceroy_overlays.asm OVL15 0x2940-0x29c7; bugs.md #571). The arm
 * scans the roster for the LAST non-expert, non-Convert colonist and buys
 * him an Expert Fisherman (when the colony has no Fisherman and owns Docks)
 * else an Expert Farmer (when it has no Farmer); 0x1c means "buy nothing".
 */
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/units.h"

#include <string.h>

#define TEST_NAME "unit_ai_euro_5952_train"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

static void fx_colony(ColonizeColony* c, const int* profs, int n) {
  memset(c, 0, sizeof *c);
  c->active = true;
  c->population = (uint8_t)n;
  c->colonist_count = n;
  for (int i = 0; i < n; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].profession = profs[i];
    c->colonists[i].building_type = -1;
    c->colonists[i].field_job = -1;
  }
}

/* No Farmer, no Docks: the last unskilled slot becomes an Expert Farmer. */
static int case_default_is_expert_farmer(void) {
  ColonizeColony c;
  const int profs[] = {COLONIZE_PROF_WEAVER, COLONIZE_PROF_FREE_COLONIST,
                       COLONIZE_PROF_FREE_COLONIST};
  fx_colony(&c, profs, 3);
  int slot = -1;
  if (ai_euro_5952_train_pick(&c, 3, false, &slot) != COLONIZE_PROF_FARMER) {
    return fail("no Farmer, no Docks -> Expert Farmer");
  }
  if (slot != 2) {
    return fail("DOS keeps the LAST non-expert slot");
  }
  return 0;
}

/* Docks and no Fisherman wins over the Farmer arm (DOS tests bit 1 first). */
static int case_docks_prefers_fisherman(void) {
  ColonizeColony c;
  const int profs[] = {COLONIZE_PROF_FREE_COLONIST};
  fx_colony(&c, profs, 1);
  int slot = -1;
  if (ai_euro_5952_train_pick(&c, 1, true, &slot) != COLONIZE_PROF_FISHERMAN) {
    return fail("Docks + no Fisherman -> Expert Fisherman");
  }
  /* With a Fisherman already present the Farmer arm takes over. */
  const int profs2[] = {COLONIZE_PROF_FISHERMAN, COLONIZE_PROF_FREE_COLONIST};
  fx_colony(&c, profs2, 2);
  if (ai_euro_5952_train_pick(&c, 2, true, &slot) != COLONIZE_PROF_FARMER) {
    return fail("Fisherman present -> Expert Farmer");
  }
  return 0;
}

/* Converts and experts are not candidates; both specialties present = no buy. */
static int case_no_candidate_and_no_need(void) {
  ColonizeColony c;
  const int profs[] = {COLONIZE_PROF_CONVERT, COLONIZE_PROF_WEAVER};
  fx_colony(&c, profs, 2);
  int slot = 5;
  if (ai_euro_5952_train_pick(&c, 2, true, &slot) != COLONIZE_PROF_FREE_COLONIST) {
    return fail("Convert + expert only -> buy nothing");
  }
  if (slot != -1) {
    return fail("no candidate slot");
  }
  const int profs2[] = {COLONIZE_PROF_FARMER, COLONIZE_PROF_FISHERMAN,
                        COLONIZE_PROF_FREE_COLONIST};
  fx_colony(&c, profs2, 3);
  if (ai_euro_5952_train_pick(&c, 3, true, &slot) != COLONIZE_PROF_FREE_COLONIST) {
    return fail("both specialties present -> buy nothing");
  }
  if (slot != 2) {
    return fail("candidate is still found");
  }
  /* Indentured Servants and Petty Criminals are non-experts, so they are
   * candidates (FUN_15eb_0002 returns 0 for 0x19/0x1a). */
  const int profs3[] = {COLONIZE_PROF_FREE_COLONIST, UNITS_JOB_CRIMINAL};
  fx_colony(&c, profs3, 2);
  if (ai_euro_5952_train_pick(&c, 2, false, &slot) != COLONIZE_PROF_FARMER || slot != 1) {
    return fail("criminal is a candidate and is the last one");
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"default_is_expert_farmer", case_default_is_expert_farmer},
  {"docks_prefers_fisherman", case_docks_prefers_fisherman},
  {"no_candidate_and_no_need", case_no_candidate_and_no_need},
};

TEST_MAIN(k_cases)
