#ifndef COLONIZE_TEST_AI_KING_COMMON_H
#define COLONIZE_TEST_AI_KING_COMMON_H

/* Smoke: King/REF SoL, tax→REF, boycott audience + Fugger sync, tax SoL≥30 gate
 * (+ SoL-low hike assert), SoL chrome, declare+160a/1528/congress,
 * 0982 wave composition (MoW-per-beat / pool caps / Regular-heavy),
 * 10f0 intervention (dual + small pools + nation pick), 2244 merc hire /
 * Decline, 1eca Continental mobilization (colony-SoL bias, SoL=50 band edge,
 * Cont. Army abbrev skip, Veteran/own-tile gates), congress market_demand_pool_raw[5] on
 * declare, WoI market_demand_pool_raw[0] only when SoL≥50, Refuse→@TEAPARTY OK (thin 3dc8
 * stock dump), revolution end ladder (@LOSING1/2/3, @WARN1/2/3, @WINNING,
 * @RETIRING2, @SCORED, @SOONRETIRING0/1), 20e6 MoW sail-home, 0a22 bell-pool
 * spend. 160a letter cinematic Done (declaration.c). PARK: dump-goods CHOICE
 * prompt invent English (picker + Europe bid>0 Done).
 */
/*
 * 2026-09-07g (D1) — the King/REF MOVEMENT scenarios in this file are RETIRED.
 * ai_king_war_act lost its per-unit loop: DOS's 43f7 king beat never writes
 * orders/goto/act-state, so the crown slot now runs the ordinary Euro turn
 * (turn.c calls the new ai_king_ref_pre_euro_beat at the crown slot's EURO
 * step, then ai_euro_nation_turn) and every REF hunt / march / capture /
 * fortify / MoW unload is euro-side code. Replacement owner: REF movement now
 * flows through ai_euro_nation_turn (D1 2026-09-07g); movement behavior is
 * covered by golden_woi_ref01. Per docs/port_plan.md "Method notes", a
 * fidelity fix invalidates the test that encoded the old behaviour — the same
 * treatment test_ai_euro_expand.c's 31 retired 5d04 scenarios got.
 *
 * Deleted with the code they tested:
 *   - "REF land hunt + colony capture + fortify Regular" (hunt goto/step,
 *     colonies_capture from ai_king, capture status chrome, fortify cap-1/cap-2,
 *     REF stack extras hunt, after-capture next-colony hunt);
 *   - "REF idle fortify" (crown-colony garrison, cap-2 extras, third hunts,
 *     captured-capital garrison, already-FORTIFIED stays);
 *   - "Dragoon / Cont. Cav garrison fallback" (cavalry fortify cap-2);
 *   - "Artillery after capture / idle on crown colony FORTIFY";
 *   - "Artillery hunt prefer fortified" + the adjacent-unfortified tighten
 *     (split out of the 0982 Artillery-pool block, whose wave/pool asserts stay);
 *   - "Dragoon open-land bias" and "Cont. Cav open-land bias" (+ the Cont. Army
 *     stays-nearest negative);
 *   - the whole MoW band: cargo AI_SAIL→coast, coastal unload (adjacent-first),
 *     multi-unload ≤moves, full-unload→next-coast sail, post-sail unload,
 *     Dragoon-only unload seize, idle empty MoW coastal patrol;
 *   - "REF Cavalry should be a land hunter" (the NAMES.TXT `Cavalry` spelling
 *     probe — ai_king_is_ref_land_hunter itself is gone);
 *   - "REF capital MD hunt bias" and "Artillery siege capital MD slack".
 *
 * Kept and retargeted: every WAVE / SPAWN / POOL / MOBILIZATION / MERC /
 * INTERVENTION / REVOLUTION-END / LATCH / popup block. Where a block used to
 * reach the wave or the 2022 bookkeeping through ai_king_nation_turn it now
 * calls ai_king_ref_pre_euro_beat first (the pipeline's own per-slot order),
 * followed by ai_king_nation_turn where the block also reads the king slice's
 * chrome or end check. The 20e6 sail-home beat survives as a direct
 * ai_king_mow_sail_home_20e6 call — that function stayed public and is driven
 * from the euro ship band.
 */
#include "../common/test_catalogs.h"
#include "core/ai_king.h"
#include "core/ai_king_internal.h"
#include "core/ai_diplo.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units_move.h"
#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_king: FAIL %s\n", msg);
  return 1;
}

#endif /* COLONIZE_TEST_AI_KING_COMMON_H */
