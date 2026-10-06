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

/*
 * DOS-LITERAL FUN_4962_0018(nation) (raw 78111-78328, saturating adds per
 * FUN_4962_0006): zero and refill `nation`'s slice of every census table plus
 * the whole DS:0x95f2 presence byte row (which therefore describes the last
 * nation censused). DOS calls it from each nation's FUN_3844_00f2 (raw 58390),
 * so a save carries per-nation snapshots of different ages. Not written: the
 * colony +0x1b ship probe (ai_euro_colony_ship_probe_4962), the non-saved
 * lane counters 0x9456/0x945a and the 0x9650 tally.
 */
void col1_stuff_census_4962_w(const ColonizeWorld* w, ColonizeCol1Stuff* stuff, int nation);

#endif
