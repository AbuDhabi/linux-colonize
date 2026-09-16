#ifndef COLONIZE_COL1_STUFF_CENSUS_H
#define COLONIZE_COL1_STUFF_CENSUS_H

#include <stdbool.h>

#include "core/col1_save.h"
#include "core/colony.h"
#include "core/units.h"
#include "core/world.h"

/*
 * FUN_4962_0018 census window fill for blank templates only.
 * Never call on mid-campaign RMW — DOS leaves lag intentional.
 */
bool col1_stuff_census_window_is_blank(const ColonizeCol1Stuff* stuff);

void col1_stuff_census_fill_blank_w(
  const ColonizeWorld* w,
  ColonizeCol1Stuff* stuff
);
/*
 * FUN_4962_0018 thin live peel: colony_counts + pop/mean, and when units!=NULL
 * also unit_type / ship / combat tallies (DOS EOT freshen). Cite: census_tally.md.
 */
void col1_stuff_census_refresh_colony_counts_w(
  const ColonizeWorld* w,
  ColonizeCol1Stuff* stuff
);

#endif
