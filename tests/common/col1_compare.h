#ifndef COLONIZE_TESTS_COMMON_COL1_COMPARE_H
#define COLONIZE_TESTS_COMMON_COL1_COMPARE_H

/*
 * Shared colony-production comparator for the golden colony tests
 * (duplication audit TT-6 / TT-7 / TT-8). test_colony_prod01/02/03 each
 * carried their own divergent copy; this is the union, with a field mask so
 * a golden can document (in one place) which fields it is not yet able to
 * pin.
 *
 * The colony-by-(x,y) lookup the three tests also retyped is not here: it is
 * col1_save_colony_at(save, -1, x, y) in core/col1_save.h.
 */

#include <stdbool.h>

#include "core/col1_save.h"

enum {
  COL1_CMP_POPULATION = 1u << 0,
  COL1_CMP_BUILDING_IN_PRODUCTION = 1u << 1,
  COL1_CMP_HAMMERS = 1u << 2,
  COL1_CMP_HAMMERS_PURCHASED = 1u << 3,
  COL1_CMP_WAREHOUSE_LEVEL = 1u << 4,
  COL1_CMP_CAPITOL_LEVEL = 1u << 5,
  COL1_CMP_DEPLETION_COUNTER = 1u << 6,
  COL1_CMP_SPECIALTY_CARGO = 1u << 7,
  COL1_CMP_LABOR_SHORTAGE = 1u << 8,
  COL1_CMP_CARGO_IDLE_TURNS = 1u << 9,
  COL1_CMP_CARGO_PRODUCED_MASK = 1u << 10,
  COL1_CMP_IMPROVE_TIMER = 1u << 11,
  COL1_CMP_STOCK = 1u << 12,
  COL1_CMP_ALL = 0x1fffu
};

/* Compare one colony record field by field; prints every mismatch to stderr
 * prefixed with step_label and the colony name. Returns false on any
 * mismatch in a field the mask selects. */
bool col1_compare_colony_production(
  const ColonizeCol1Colony* got,
  const ColonizeCol1Colony* expect,
  unsigned field_mask,
  const char* step_label
);

/*
 * orig = pre-turn save (ground truth start), untouched by turn_end/capture.
 * got = post-turn save (our simulated end state); exp = real-DOS post-turn.
 *
 * A colony that changes hands (either side of the real DOS turn, or only in
 * our own simulation) had combat/AI decide its fate this turn — that's
 * explicitly out of scope for these goldens (AI behavior + RNG stream aren't
 * checked by them). Only colonies owned by `nation` in orig, in exp, AND
 * still in got get a production comparison; everything else is reported as
 * excluded, not failed. Comparing zero colonies is a failure.
 */
bool col1_compare_nation_colonies(
  const ColonizeCol1Save* orig,
  const ColonizeCol1Save* got,
  const ColonizeCol1Save* exp,
  int nation,
  unsigned field_mask,
  const char* step_label
);

#endif /* COLONIZE_TESTS_COMMON_COL1_COMPARE_H */
