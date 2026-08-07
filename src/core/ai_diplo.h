#ifndef COLONIZE_AI_DIPLO_H
#define COLONIZE_AI_DIPLO_H

#include <stdint.h>

#include "core/col1_save.h"
#include "core/turn.h"

/*
 * Euro diplomacy — partial structural port of FUN_15b3_* + FUN_5bfb war/ally.
 * Thin map: original_sources_annotated/ai/euro_diplo.md
 */

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

/* 6d8e step 4: decrement per-rival treaty timer bytes (before planning). */
void ai_diplo_treaty_timers(ColonizeTurnContext* ctx, int nation_id);

/* Opportunistic war/ally by military balance (5bfb_10ec/13b0; not timer slot). */
void ai_diplo_euro_balance(ColonizeTurnContext* ctx, int nation_id);

/* Alias → ai_diplo_treaty_timers (6d8e timer pass). */
void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id);

/* FUN_4cc6_00f2 / 15dc_00e0-shaped Indian relation scalar (not full 15b3). */
void ai_diplo_indian_relation_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
);

#endif
