#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/common/col1_compare.h"
#include "tests/common/golden_fixture.h"

/*
 * Colony production golden #2: COLONY00-dutch2-t0.SAV -> one turn_end() ->
 * compare against COLONY01-dutch2-t1.SAV, a save produced by running the
 * *real* one turn in original DOS (not Linux-derived). A later, larger
 * capture than colony_prod01 (turn 169, 17 colonies, established Dutch
 * economy with indoor manufacturing specialists), same idea: the player
 * made no moves that turn, so any Dutch colony field/building drift is
 * either engine production math or a bridge apply/capture bug.
 *
 * Unlike colony_prod01, this pair has no hand-reconstructed map-tile
 * patches: col1_bridge_apply builds the map straight from this save's own
 * embedded tile data, and that was enough here (see colony_prod01's
 * comment for *why* it needed patches — a different, small fixture whose
 * terrain round-trip needed hand correction; this larger capture didn't
 * hit that gap).
 *
 * Only human-controlled Dutch (nation_id 3) colonies are checked; colonies
 * that change hands either side of the turn are excluded, not failed (see
 * compare_dutch_colonies). AI nations are not checked here.
 *
 * Status 2026-08-18: 2/7 Dutch colonies (Guadeloupe, Fort Orange) match
 * exactly; the other 5 have real remaining drift. (Later the same day:
 * town-commons secondary's asm-confirmed formula fix — see colony_yield.c
 * — cleared Curacao's furs and New Holland's sugar/lumber outright, both
 * exactly, since town commons was each colony's sole source. New
 * Amsterdam/New Holland horses and Curacao/Recife food are still open —
 * see the dated note below.)
 *
 * 2026-08-18 fix #1: New Amsterdam and Fort Orange were wrongly decoded as
 * owning Iron Works (factory-tier Blacksmith) — neither Dutch nor any
 * other nation in this save owns Adam Smith, so that was impossible from
 * the start. Root cause: `col1_apply_building_level`/`col1_encode_
 * building_level` treated a chain's stored level as the tier count N
 * directly, but DOS actually stores `(1 << N) - 1` (player-confirmed:
 * every 2-or-3-tier chain field across both colony_prod01 and
 * colony_prod02's ~30 colonies reads only 0/1/3[/7], the value 2 never
 * once appears — a plain tier-count encoding would produce 2 constantly).
 * Fixed in col1_bridge.c; both colonies now correctly read Blacksmith's
 * Shop, and their tools/muskets numbers now match the real save exactly.
 *
 * 2026-08-18 fix #2: Hills-Ore base is 4, not 3 — player-confirmed via
 * this same Fort Orange colonist (single expert Ore Miner, Hills, no
 * road/river/resource, sentiment +2, no other ore consumer/producer in
 * the colony -> 12 ore = (4+2)x2), paired with its non-specialist
 * Blacksmith's Shop worker (8 tools from 8 ore, confirming the shop-tier
 * manufacturing side unchanged by fix #1). Fort Orange now matches
 * exactly. base=3 had come from golden_colony_prod01's Bahia fixture,
 * whose whole map is hand-synthesized (real terrain was never loaded for
 * it, unlike this save) and carried an unconfirmed "+road" guess that
 * only "worked" by cancelling the wrong base's error; fixed by re-picking
 * that fixture's synthetic terrain instead (see test_colony_prod01.c).
 * New Amsterdam's ore now matches too; its remaining food/horses drift is
 * unrelated (see below).
 *
 * 2026-08-18 fix #3: town-commons secondary's real formula (asm-read from
 * FUN_15eb_1f72: base + river + SoL latch bits, no plow, no flat road —
 * see colony_yield.c/docs/terrain_yields.md) cleared New Holland's sugar
 * (was way under, real +11 vs ours ~+0) and Curacao's furs (each colony's
 * only source of that cargo) exactly, plus Guadeloupe/Bahia/Quebec/
 * St. Louis's synthetic fixtures in colony_prod01. Along the way this also
 * caught a genuine data bug: k_forested's Rain row had Food/Sugar 2/2
 * where NAMES.TXT (and Paramaribo's real capture) both say 1/1 — fixed
 * (colony_yield.c).
 *
 * Still open, unrelated to the above:
 * - New Amsterdam, New Holland: horses off by 1 — a food-surplus/2
 *   breeding-rounding cascade, gross food is off by ~1 pre-breed even
 *   though *net* food lands exactly right (the rounding hides it). New
 *   Amsterdam's likely source: the "expert Fisherman + Fishery resource
 *   -> +4 (not the table's +3), yielding 14 before doubling" override in
 *   colony_yield_pipeline — added early this project with no dedicated
 *   test, may be an unverified guess; New Holland's expert Fisherman has
 *   *no* resource yet is *also* off (opposite direction), so a single
 *   fix likely doesn't cover both — not chased down further this pass.
 * - Curacao, Recife: food off by 1 / several — not investigated.
 *
 * One real, unrelated bug *was* found and fixed via this save:
 * `colonies_try_complete_building` used to clear `building_in_production`
 * on completion; DOS never does (Vlissingen's Lumber Mill completing this
 * turn proved it) — fixed in colony.c, regression-tested in test_turn.c /
 * test_colony_screen.c.
 */

#define COLONY_PROD02_RNG_SEED 100u
#define COLONY_PROD02_HUMAN_NATION 3 /* Netherlands */

static int run_pair(const char* path_in, const char* path_exp, const char* label) {
  GoldenFixture fx;
  if (!golden_open(path_in, path_exp, COLONY_PROD02_RNG_SEED, &fx)) {
    golden_close(&fx);
    return 1;
  }
  if (!golden_turn(&fx)) {
    golden_close(&fx);
    return 1;
  }

  /*
   * Full field set, cargo_produced_mask included (2026-09-15): New
   * Amsterdam's Custom House ships 70 lumber the same tick, and DOS
   * subtracts the sale from the net before setting the produced bit (raw
   * 57273/57343), so the old "extra lumber bit" was the port testing the
   * pre-sale net. depletion_counter and cargo_idle_turns, which the old
   * per-file comparator never checked, are compared too.
   */
  const bool ok = col1_compare_nation_colonies(
    &fx.orig, &fx.start, &fx.expect, COLONY_PROD02_HUMAN_NATION,
    COL1_CMP_ALL, label
  );
  golden_close(&fx);
  if (!ok) {
    fprintf(stderr, "%s FAILED\n", label);
    return 1;
  }
  printf("%s ok\n", label);
  return 0;
}

int main(void) {
  const int rc = run_pair(
    "original_saves/colony-prod-tests/COLONY00-dutch2-t0.SAV",
    "original_saves/colony-prod-tests/COLONY01-dutch2-t1.SAV",
    "colony_prod02 COLONY00->01 (Dutch)"
  );
  if (rc != 0) {
    return rc;
  }
  printf("golden_colony_prod02: Dutch colony production ok\n");
  return 0;
}
