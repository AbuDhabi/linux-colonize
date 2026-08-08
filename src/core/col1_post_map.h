#ifndef COLONIZE_COL1_POST_MAP_H
#define COLONIZE_COL1_POST_MAP_H

#include <stdbool.h>

#include "core/col1_save.h"
#include "core/map.h"

/*
 * FUN_67f4_0088 — rebuild sea/land connectivity planes + continent tallies.
 * Does not touch the 10-byte post_map tail (unknown_post_604 / 8d80 / 190).
 *
 * Requires live terrain + layer3 continent IDs (FUN_67bf_0000 /
 * map_gen_assign_continents). Standard 58×72 maps only (15×18 ÷4 grid).
 */
bool col1_post_map_is_blank(const ColonizeCol1PostMap* pm);

void col1_post_map_rebuild_connectivity(
  ColonizeCol1PostMap* out,
  const ColonizeWorldMap* map
);

#endif /* COLONIZE_COL1_POST_MAP_H */
