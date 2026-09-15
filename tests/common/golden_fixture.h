#ifndef COLONIZE_TESTS_COMMON_GOLDEN_FIXTURE_H
#define COLONIZE_TESTS_COMMON_GOLDEN_FIXTURE_H

/*
 * The "load a start/expect .SAV pair, bring up the live pools, run one
 * turn_end(), capture back" prologue the golden turn tests each hand-rolled
 * (duplication audit TT-9; TT-10 is the `orig` leak on the error paths,
 * fixed here by construction — golden_close() is the single teardown and is
 * safe to call at any stage).
 *
 * Usage:
 *   GoldenFixture fx;
 *   if (!golden_open(path_in, path_exp, seed, &fx)) { golden_close(&fx); return 1; }
 *   ...optional per-test map/colony patching on fx.map / fx.colonies...
 *   if (!golden_turn(&fx)) { golden_close(&fx); return 1; }
 *   ...compare fx.start (post-turn) against fx.expect, fx.orig = pre-turn...
 *   golden_close(&fx);
 */

#include <stdbool.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/units.h"

typedef struct {
  /* start: bridged in, then turn_end()+capture write the post-turn state
   * back into it. expect: the real-DOS post-turn save. orig: an untouched
   * second read of the start file, for ownership-stability checks. */
  ColonizeCol1Save start;
  ColonizeCol1Save expect;
  ColonizeCol1Save orig;
  ColonizeMsgCatalog names;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  ColonizeWorldMap map;
  EuropeScreen europe;
  ColonizeCol1BridgeResult br;
  ColonizeDosRng rng;
  uint32_t turn_number;
  uint16_t year;
  uint16_t autumn;
  uint32_t seed;
  bool names_loaded;
  bool bridged;
} GoldenFixture;

bool golden_open(const char* path_in, const char* path_exp, uint32_t rng_seed, GoldenFixture* fx);
bool golden_turn(GoldenFixture* fx);
void golden_close(GoldenFixture* fx);

#endif /* COLONIZE_TESTS_COMMON_GOLDEN_FIXTURE_H */
