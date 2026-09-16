#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/common/col1_compare.h"
#include "tests/common/golden_fixture.h"
#include "tests/common/test_runner.h"

/*
 * Colony production golden #3: farmer skill/river arithmetic, from two
 * purpose-made DOS captures (2026-09-03). Each pair is a real DOS save plus
 * the save after one real DOS turn with no player moves; the colony under
 * test is New Amsterdam (Dutch), population 3: one expert Farmer, one Petty
 * Criminal (both on field tiles), one Town Hall worker.
 *
 * - case1: criminal on Broadleaf (base 2, no river) = 3 food; expert on
 *   Broadleaf with a MAJOR river = 6 food. 6 only reconciles if the
 *   Farmer's river bonus is flat +2 with no major-river doubling (the bug
 *   this pair caught: the port doubled major to +4, giving 8) — see
 *   colony_yield_river_bonus's Farmer case and docs/terrain_yields.md
 *   "Plow / road / river stacking" for the matching FUN_15eb_18ec asm
 *   reading (job<4 river tiles fire two +u signals, so the term==u
 *   major-river add never triggers for a Farmer).
 * - case2: same colony, workers moved; expert on Rain forest (base 1,
 *   minor river) = 5 food, criminal on riverless Rain forest = 2. Pins the
 *   expert flat +2 (not x2) and the non-expert unconditional +1 again.
 *
 * Only Dutch (nation_id 3) colonies are compared, same scope rules as
 * golden_colony_prod02.
 */

#define COLONY_PROD03_RNG_SEED 100u
#define COLONY_PROD03_HUMAN_NATION 3 /* Netherlands */

static int run_pair(const char* path_in, const char* path_exp, const char* label) {
  GoldenFixture fx;
  if (!golden_open(path_in, path_exp, COLONY_PROD03_RNG_SEED, &fx)) {
    golden_close(&fx);
    return 1;
  }
  if (!golden_turn(&fx)) {
    golden_close(&fx);
    return 1;
  }

  const bool ok = col1_compare_nation_colonies(
    &fx.orig, &fx.start, &fx.expect, COLONY_PROD03_HUMAN_NATION, COL1_CMP_ALL, label
  );
  golden_close(&fx);
  if (!ok) {
    fprintf(stderr, "%s FAILED\n", label);
    return 1;
  }
  printf("%s ok\n", label);
  return 0;
}

static int case_farming1(void) {
  return run_pair(
    "original_saves/colony-prod-tests/farming/case1-turn1.SAV",
    "original_saves/colony-prod-tests/farming/case1-turn2.SAV",
    "colony_prod03 farming case1 (expert Farmer, major river)"
  );
}

static int case_farming2(void) {
  return run_pair(
    "original_saves/colony-prod-tests/farming/case2-turn1.SAV",
    "original_saves/colony-prod-tests/farming/case2-turn2.SAV",
    "colony_prod03 farming case2 (expert Farmer, minor river)"
  );
}

/*
 * case3 (2026-09-03): Fort Orange, expert Farmer on Broadleaf + Game
 * (8 food; the player then moved him to a bare Hill for 4). Pinned
 * three things at once: the unconditional skill-blind farmer +1 (asm
 * 15eb:1c32-1c40), Hills farmer base = NAMES.TXT's 1 (the old
 * "player-confirmed 2" had absorbed that +1), and — via the
 * golden_colony_prod02 reconciliation it forced — that commons food
 * has no river term. Only turn1->turn2 runs as a golden pair: the
 * farmer was moved to the Hill BETWEEN turn2 and turn3 (turn2's save
 * still has him on the forest), so turn2->turn3 is not a no-move turn
 * and can't be simulated blind; the Hill value (4) is asserted
 * statically in unit_colony_yield instead.
 */
static int case_farming3_turn1_2(void) {
  return run_pair(
    "original_saves/colony-prod-tests/farming/case3-turn1.SAV",
    "original_saves/colony-prod-tests/farming/case3-turn2.SAV",
    "colony_prod03 farming case3 turn1->2 (expert Farmer, forest+Game)"
  );
}

/*
 * case4 (2026-09-03): Fort Nassau, pop 1, expert Farmer on Mixed forest
 * with a Beaver resource = 5 food (base 2 + expert 2 + farmer 1 —
 * Beaver pairs only with Fur Trapper in FUN_15eb_17fa, no farmer
 * bonus), then on Mountains = 0 food regardless of expertise (table
 * base 0; DOS gates the SoL fold and the expert branch on a nonzero
 * yield, so nothing ever accrues — the ungated port invented 3).
 * turn1->2 is simulable even though the farmer moved to the mountain
 * between the saves: DOS ran that turn's production off the turn1
 * (forest) position, and the comparison checks stocks, not tile
 * assignments. turn2->3 is a plain no-move mountain turn.
 */
static int case_farming4_turn1_2(void) {
  return run_pair(
    "original_saves/colony-prod-tests/farming/case4-turn1.SAV",
    "original_saves/colony-prod-tests/farming/case4-turn2.SAV",
    "colony_prod03 farming case4 turn1->2 (expert Farmer, Mixed+Beaver)"
  );
}

static int case_farming4_turn2_3(void) {
  return run_pair(
    "original_saves/colony-prod-tests/farming/case4-turn2.SAV",
    "original_saves/colony-prod-tests/farming/case4-turn3.SAV",
    "colony_prod03 farming case4 turn2->3 (expert Farmer, Mountains = 0)"
  );
}

static const TestCase k_cases[] = {
    {"case_farming1", case_farming1},
    {"case_farming2", case_farming2},
    {"case_farming3_turn1_2", case_farming3_turn1_2},
    {"case_farming4_turn1_2", case_farming4_turn1_2},
    {"case_farming4_turn2_3", case_farming4_turn2_3},
};

TEST_MAIN(k_cases)
