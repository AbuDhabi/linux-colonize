#ifndef COLONIZE_TEST_AI_EURO_EXPAND_COMMON_H
#define COLONIZE_TEST_AI_EURO_EXPAND_COMMON_H

/* Smoke: Euro second-wave settle + CONTACT scout rings + tools delivery. */
/*
 * 2026-09-07e — 31 scenarios RETIRED with the Linux-shaped 5d04 hire matrix
 * they were written against (`unit_tools/lumber/food/horses/muskets_cargo_hire`,
 * `unit_tools_mid_threshold_hire`, the seven `unit_wagon_hire_*_once`, and the
 * seventeen `unit_dock_*_hire` Europe-dock expert cases). None of that
 * behaviour exists in FUN_521d_5d04: the invented `hire_cost = 200 +
 * 25*difficulty` gate, the NAMES-display-string dock-expert ladder and the
 * `inv->*_short` wagon/cargo ladders were all Linux stand-ins, deleted from
 * ai_euro.c when the DOS hire matrix (raw 92568-93070,
 * `ai_euro_5d04_hire_ladder_tail`) became the only Europe hire economy.
 * Per docs/port_plan.md "Method notes": a fidelity fix invalidates the test
 * that encoded the old behaviour. DOS-side coverage of the replacement lives
 * in the goldens (golden_ai_turns / golden_ai_joint / golden_ai_mid01 /
 * golden_ai_late01), which stayed byte-green across the swap.
 */
#include "core/ai_diplo.h"
#include "core/ai.h"
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/colony.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NAME "unit_ai_euro_expand"
#include "../common/ai_fixture.h"
#include "../common/test_fail.h"
#include "../common/test_runner.h"

#endif /* COLONIZE_TEST_AI_EURO_EXPAND_COMMON_H */
