#ifndef COLONIZE_AI_DIPLO_H
#define COLONIZE_AI_DIPLO_H

#include <stdint.h>

#include "core/col1_save.h"
#include "core/turn.h"

/* T0 diplomacy — FUN_15b3_* bytes + FUN_5bfb war/ally helpers. */

#define AI_DIPLO_WAR 0x01
#define AI_DIPLO_PEACE 0x02
#define AI_DIPLO_ALLY 0x04
#define AI_DIPLO_MET 0x40

uint8_t ai_diplo_read(const ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_write(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t value);
void ai_diplo_or_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits);
void ai_diplo_clear_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits);

int ai_diplo_at_war(const ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_declare_war(ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_form_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_break_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b);

/* Euro dispatcher diplo-timer pass (T0): war/ally by military balance. */
void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id);

/* Indian↔Euro relation delta on Col1Nation.relation_by_indian. */
void ai_diplo_indian_relation_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
);

#endif
