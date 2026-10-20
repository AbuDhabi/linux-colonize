#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/colony.h"
#include "core/units.h"

#include "tests/common/col1_compare.h"
#include "tests/common/golden_fixture.h"

#include "../common/test_runner.h"

/*
 * Colony production golden: COLONY00_no-transports.SAV -> one turn_end() ->
 * compare against COLONY01_no-transports.SAV, a save produced by running
 * the *real* one turn in original DOS (not OpenCol-derived). Both fixtures
 * had every European ship / wagon train stripped first (see
 * original_saves/colony-prod-tests/), so this is Euro-unit-free and
 * isolates colony field production math.
 *
 * Human-controlled Dutch (nation_id 3) colonies only: the player made no
 * moves that turn, so any Dutch colony drift is either engine production
 * math or a bridge apply/capture bug. AI nations are not checked.
 *
 * The map comes straight from the save (col1_bridge_apply). Until
 * 2026-10-18 this file carried ~750 lines of hand-picked terrain/worker
 * patches from the days before the bridge loaded real terrain; the real
 * tiles reproduce every Dutch colony, so they are gone.
 */

#define COLONY_PROD01_RNG_SEED 100u
#define COLONY_PROD01_HUMAN_NATION 3 /* Netherlands */

static int run_pair(const char* path_in, const char* path_exp, const char* label) {
  GoldenFixture fx;
  if (!golden_open(path_in, path_exp, COLONY_PROD01_RNG_SEED, &fx)) {
    golden_close(&fx);
    return 1;
  }
  /* This fixture has no known VR_SEED, so the native RNG stream is not the
   * DOS one. In the port's stream the Brave at (46,63) attacks St. Louis and
   * runs off with 36 Horses DOS kept (St. Louis then misses Food 29 / Horses
   * 100 and gains a Horses produced bit); DOS made no such attack that turn.
   * Removing that one Brave is the only edit: everything else is the save
   * as captured (port_bugs.md #1126; a VR_SEED re-capture would retire it). */
  for (int ui = 0; ui < units_slot_end(&fx.units); ++ui) {
    ColonizeUnit* u = &fx.units.units[ui];
    if (u->active && u->nation_id == 6 && u->x == 46 && u->y == 63) {
      units_despawn(&fx.units, u->id);
    }
  }
  if (!golden_turn(&fx)) {
    golden_close(&fx);
    return 1;
  }
  const bool ok = col1_compare_nation_colonies(
    &fx.orig, &fx.start, &fx.expect, COLONY_PROD01_HUMAN_NATION, COL1_CMP_ALL, label
  );
  golden_close(&fx);
  if (!ok) {
    fprintf(stderr, "%s FAILED\n", label);
    return 1;
  }
  printf("%s ok\n", label);
  return 0;
}

static int case_colony_prod01_dutch(void) {
  const int rc = run_pair(
    "original_saves/colony-prod-tests/COLONY00_no-transports.SAV",
    "original_saves/colony-prod-tests/COLONY01_no-transports.SAV",
    "colony_prod01 COLONY00->01 (Dutch)"
  );
  if (rc != 0) {
    return rc;
  }
  printf("golden_colony_prod01: Dutch colony production ok\n");
  return 0;
}

static const TestCase k_cases[] = {
  {"colony_prod01_dutch", case_colony_prod01_dutch},
};

TEST_MAIN(k_cases)
