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
void ai_diplo_make_peace(ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_form_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_break_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b);

/* Thin 102a/1092 status chrome (Contact/King pattern): call existing
 * declare/make_peace then write ctx->status when human is involved.
 * AI callers keep using declare_war / make_peace without status. */
void ai_diplo_declare_war_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b);
void ai_diplo_make_peace_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b);

/* FUN_5bfb_0000/00f8/312e-shaped military score (unpark #5 deepen). */
int ai_diplo_military_score(const ColonizeTurnContext* ctx, int nation_id);

/* 6d8e step 4: decrement per-rival treaty timer bytes (before planning);
 * also thin peaceful Indian relation drift when not at Euro war. */
void ai_diplo_treaty_timers(ColonizeTurnContext* ctx, int nation_id);

/* Opportunistic war/ally by military balance (5bfb_10ec/13b0; not timer slot).
 * Also thin FA ally-aid + FA gift while allied (full 3f41 PARKED);
 * at-war near-parity → make_peace_ctx (status when human; 102a/1092 chrome). */
void ai_diplo_euro_balance(ColonizeTurnContext* ctx, int nation_id);

/* Thin FA 3f41 goodwill gift: 15g from→to + both treaty timers +2 when
 * donor gold >= 100 and peer gold < donor*2. euro_balance calls when ALLY
 * and timer==1. Full FA dialog UI PARKED. */
void ai_diplo_fa_gift(ColonizeCol1Save* col1, int from, int to);

/* Alias → ai_diplo_treaty_timers (6d8e timer pass). */
void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id);

/* FUN_4cc6_00f2 / 15dc_00e0-shaped Indian relation scalar (not full 15b3). */
void ai_diplo_indian_relation_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
);

/* Thin Indian×Euro matrix cell: relation_by_indian[indian_idx] (0..7). */
uint8_t ai_diplo_indian_read(const ColonizeCol1Save* col1, int euro_nation, int indian_idx);

/* Thin stand-in: at war with Indian nation when relation < 50.
 * Fuller Indian×Euro 15b3 matrix OPEN (unpark #5); sticky unknown26[8]. */
int ai_diplo_indian_at_war(const ColonizeCol1Save* col1, int euro_nation, int indian_idx);

/* True if any of 8 Indian slots is at war (relation < 50). Contact/diplo helper. */
int ai_diplo_indian_any_at_war(const ColonizeCol1Save* col1, int euro_nation);

/* Read unknown26[8] Indian hostility sticky: 0 clear, 1 at-war, 2 very-low deepen. */
uint8_t ai_diplo_indian_hostility_sticky(const ColonizeCol1Save* col1, int euro_nation);

/* Sync sticky from relation matrix (set/clear/deepen). Call after relation hits. */
void ai_diplo_indian_hostility_sync(ColonizeCol1Save* col1, int euro_nation);

#endif
