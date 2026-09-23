#ifndef COLONIZE_TEST_AI_EURO_WAR_COMMON_H
#define COLONIZE_TEST_AI_EURO_WAR_COMMON_H

/* Smoke: at-war Euro mid-hire / MILITARY bind + G stance + thin naval hunt. */
/*
 * 2026-09-07e — 6 scenarios RETIRED with the Linux-shaped 5d04 hire matrix
 * they were written against: `unit_mid_hire_mil_colonies_ge6`,
 * `unit_mid_hire_dragoon_prefer`, `unit_mid_hire_veteran_prefer`,
 * `unit_at_war_tools_prefer_soldier`, `unit_mid_hire_artillery`,
 * `unit_artillery_treasury_fallback`. FUN_521d_5d04 has no war/peace hire
 * fork, no Dragoon-over-Soldier or Veteran-over-Soldier preference and no
 * `units_find_type("Artillery")` gold gate — those were Linux inventions,
 * deleted from ai_euro.c when the DOS hire matrix (raw 92568-93070,
 * `ai_euro_5d04_hire_ladder_tail`) became the only Europe hire economy.
 * What DOS actually does: Colonist + 50 Muskets -> Soldier, then + 50
 * Horses -> Dragoon, gated on the DS:0xa0db colony muskets-need tally and
 * per-unit RNG, with Artillery bought from the 5c3c purchase table only
 * when Europe holds none. `unit_mid_hire_mil` is KEPT — it accepts the
 * MILITARY bind path, which is unaffected.
 */
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_euro_internal.h"
#include "core/ai_goals.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NAME "unit_ai_euro_war"
#include "../common/ai_fixture.h"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

#endif /* COLONIZE_TEST_AI_EURO_WAR_COMMON_H */
