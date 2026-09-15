#include "tests/common/col1_compare.h"

#include <stdio.h>
#include <string.h>

#include "core/reports.h"

/* One mismatch line, in the format all three goldens printed. */
#define CMP_U(bit, field)                                                              \
  do {                                                                                 \
    if ((field_mask & (bit)) != 0 && got->field != expect->field) {                    \
      fprintf(                                                                         \
        stderr, "%s %s " #field " got %u expected %u\n", step_label, expect->name,     \
        (unsigned)got->field, (unsigned)expect->field                                  \
      );                                                                               \
      ok = false;                                                                      \
    }                                                                                  \
  } while (0)

bool col1_compare_colony_production(
  const ColonizeCol1Colony* got,
  const ColonizeCol1Colony* expect,
  unsigned field_mask,
  const char* step_label
) {
  bool ok = true;
  CMP_U(COL1_CMP_POPULATION, population);
  CMP_U(COL1_CMP_BUILDING_IN_PRODUCTION, building_in_production);
  CMP_U(COL1_CMP_HAMMERS, hammers);
  CMP_U(COL1_CMP_HAMMERS_PURCHASED, hammers_purchased);
  CMP_U(COL1_CMP_WAREHOUSE_LEVEL, warehouse_level);
  CMP_U(COL1_CMP_CAPITOL_LEVEL, capitol_level);
  CMP_U(COL1_CMP_DEPLETION_COUNTER, depletion_counter);
  CMP_U(COL1_CMP_SPECIALTY_CARGO, specialty_cargo);
  CMP_U(COL1_CMP_LABOR_SHORTAGE, labor_shortage);
  CMP_U(COL1_CMP_CARGO_IDLE_TURNS, cargo_idle_turns);
  CMP_U(COL1_CMP_IMPROVE_TIMER, improve_timer);
  if ((field_mask & COL1_CMP_CARGO_PRODUCED_MASK) != 0 &&
      got->cargo_produced_mask != expect->cargo_produced_mask) {
    fprintf(
      stderr, "%s %s cargo_produced_mask got 0x%04x expected 0x%04x\n", step_label,
      expect->name, (unsigned)got->cargo_produced_mask, (unsigned)expect->cargo_produced_mask
    );
    ok = false;
  }
  if ((field_mask & COL1_CMP_STOCK) != 0) {
    for (unsigned c = 0; c < COLONIZE_COL1_CARGO_TYPES; ++c) {
      if (got->stock[c] != expect->stock[c]) {
        fprintf(
          stderr, "%s %s stock[%s] got %u expected %u\n", step_label, expect->name,
          reports_cargo_display_name((int)c), (unsigned)got->stock[c],
          (unsigned)expect->stock[c]
        );
        ok = false;
      }
    }
  }
  return ok;
}

#undef CMP_U

bool col1_compare_nation_colonies(
  const ColonizeCol1Save* orig,
  const ColonizeCol1Save* got,
  const ColonizeCol1Save* exp,
  int nation,
  unsigned field_mask,
  const char* step_label
) {
  bool ok = true;
  int checked = 0;
  int excluded = 0;
  for (unsigned i = 0; i < exp->head.colony_count; ++i) {
    const ColonizeCol1Colony* e = &exp->colony[i];
    if ((int)e->nation_id != nation) {
      continue;
    }
    const int oi = col1_save_colony_at(orig, nation, (int)e->x, (int)e->y);
    const int gi = col1_save_colony_at(got, nation, (int)e->x, (int)e->y);
    if (oi < 0 || gi < 0) {
      ++excluded;
      fprintf(
        stderr, "%s excluded '%s' at (%u,%u): our sim changed its ownership (AI/RNG, out of scope)\n",
        step_label, e->name, e->x, e->y
      );
      continue;
    }
    const ColonizeCol1Colony* g = &got->colony[gi];
    if (strncmp(g->name, e->name, sizeof(g->name)) != 0) {
      fprintf(
        stderr, "%s colony at (%u,%u) got name '%s' expected '%s'\n", step_label, e->x, e->y,
        g->name, e->name
      );
      ok = false;
      continue;
    }
    ++checked;
    if (!col1_compare_colony_production(g, e, field_mask, step_label)) {
      ok = false;
    }
  }
  fprintf(
    stderr, "%s checked %d colonies of nation %d (%d excluded, ownership changed)\n", step_label,
    checked, nation, excluded
  );
  if (checked == 0) {
    fprintf(stderr, "%s checked no colonies\n", step_label);
    return false;
  }
  return ok;
}
