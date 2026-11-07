#ifndef COLONIZE_CORE_MAP_GEN_INTERNAL_H
#define COLONIZE_CORE_MAP_GEN_INTERNAL_H

/*
 * Map-gen pass seams for tests (see core/internal.h). `layer2` is the land
 * coverage plane (map_generate's `mask`), which DOS's river pass reaches past
 * the end of the terrain plane (#1559).
 */

#include <stdint.h>

#include "core/dos_rng.h"
#include "core/internal.h"

/* FUN_684c_04a6 rivers on a w*h terrain plane; climate / land_mass 0..3. */
COLONIZE_INTERNAL void map_gen_rivers_pass(
  uint8_t* terrain,
  uint8_t* layer2,
  int w,
  int h,
  ColonizeDosRng* rng,
  int climate,
  int land_mass
);

#endif /* COLONIZE_CORE_MAP_GEN_INTERNAL_H */
