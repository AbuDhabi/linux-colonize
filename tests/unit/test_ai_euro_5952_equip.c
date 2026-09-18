/*
 * FUN_5952_035e equip-arm candidate scorer (raw 94318-94345; annotated
 * colony_tick_5952_035e.md:667-691). The cases below are the DOS score
 * arms read off that dump, driven through ai_euro_5952_equip_pick:
 *   prof == target -> 4, non-expert -> +1, expert -> -99 unless +0x1b & 0x40,
 *   Indentured Servant +1, Petty Criminal +2, Indian Convert never picked,
 *   best starts at -1 and `<=` lets a later tie win.
 */
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/units.h"

#include <string.h>

#define TEST_NAME "unit_ai_euro_5952_equip"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

static void fx_colony(ColonizeColony* c, const int* profs, int n, unsigned ai_flags) {
  memset(c, 0, sizeof *c);
  c->active = true;
  c->ai_flags = ai_flags;
  c->population = (uint8_t)n;
  c->colonist_count = n;
  for (int i = 0; i < n; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].profession = (uint8_t)profs[i];
    c->colonists[i].building_type = -1;
    c->colonists[i].field_job = -1;
  }
}

/* Criminal (1+2=3) beats Servant (1+1=2) beats Free Colonist (1). */
static int case_criminal_over_servant_over_colonist(void) {
  ColonizeColony c;
  const int profs[] = {COLONIZE_PROF_FREE_COLONIST, UNITS_JOB_SERVANT, UNITS_JOB_CRIMINAL};
  fx_colony(&c, profs, 3, 0);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 2) {
    return fail("criminal should win");
  }
  const int profs2[] = {UNITS_JOB_CRIMINAL, UNITS_JOB_SERVANT, COLONIZE_PROF_FREE_COLONIST};
  fx_colony(&c, profs2, 3, 0);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 0) {
    return fail("criminal should win from slot 0 too");
  }
  const int profs3[] = {COLONIZE_PROF_FREE_COLONIST, UNITS_JOB_SERVANT};
  fx_colony(&c, profs3, 2, 0);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 1) {
    return fail("servant should beat free colonist");
  }
  return 0;
}

/* An Indian Convert is never taken, whatever it scores. */
static int case_convert_never_picked(void) {
  ColonizeColony c;
  const int profs[] = {COLONIZE_PROF_FREE_COLONIST, UNITS_JOB_CONVERT};
  fx_colony(&c, profs, 2, 0);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 0) {
    return fail("convert must not be picked");
  }
  const int only[] = {UNITS_JOB_CONVERT, UNITS_JOB_CONVERT};
  fx_colony(&c, only, 2, 0);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != -1) {
    return fail("all-convert colony has no candidate");
  }
  return 0;
}

/* Expert = -99 without +0x1b & 0x40 (NEEDS_GARRISON), plain +1 with it. */
static int case_expert_penalty_needs_garrison(void) {
  ColonizeColony c;
  /* Elder Statesman (an expert) in slot 0, Free Colonist in slot 1. */
  const int profs[] = {0, COLONIZE_PROF_FREE_COLONIST};
  fx_colony(&c, profs, 2, 0);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 1) {
    return fail("expert must lose without flag 0x40");
  }
  fx_colony(&c, profs, 2, COLONIZE_COLONY_AI_NEEDS_GARRISON);
  /* No penalty now: expert scores 0, colonist 1 — the colonist still wins. */
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 1) {
    return fail("colonist (1) still beats unpenalised expert (0)");
  }
  const int expert_only[] = {0};
  fx_colony(&c, expert_only, 1, COLONIZE_COLONY_AI_NEEDS_GARRISON);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 0) {
    return fail("lone expert is taken when 0x40 is set");
  }
  return 0;
}

/* prof == target scores 4 — a Veteran Soldier colonist outranks everyone. */
static int case_target_profession_wins(void) {
  ColonizeColony c;
  const int profs[] = {UNITS_JOB_SOLDIER, UNITS_JOB_CRIMINAL};
  fx_colony(&c, profs, 2, 0);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 0) {
    return fail("veteran soldier (4) should win");
  }
  return 0;
}

/* `best <= score`: equal scores let the LATER slot win. */
static int case_later_tie_wins(void) {
  ColonizeColony c;
  const int profs[] = {UNITS_JOB_CRIMINAL, UNITS_JOB_CRIMINAL, UNITS_JOB_CRIMINAL};
  fx_colony(&c, profs, 3, 0);
  if (ai_euro_5952_equip_pick(&c, UNITS_JOB_SOLDIER) != 2) {
    return fail("later tie must win");
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"case_criminal_over_servant_over_colonist", case_criminal_over_servant_over_colonist},
  {"case_convert_never_picked", case_convert_never_picked},
  {"case_expert_penalty_needs_garrison", case_expert_penalty_needs_garrison},
  {"case_target_profession_wins", case_target_profession_wins},
  {"case_later_tie_wins", case_later_tie_wins},
};

TEST_MAIN(k_cases)
