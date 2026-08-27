#ifndef COLONIZE_AI_EURO_H
#define COLONIZE_AI_EURO_H

#include "core/turn.h"

/*
 * Euro AI dispatcher T0 — FUN_521d_6d8e / 0a60 / 5d04 / 5b66 / 20e6.
 * Called from ai_euro_nation_turn after crosses (and optionally after early fixture).
 */

void ai_euro_dispatcher_turn(ColonizeTurnContext* ctx, int nation_id);

/* True when full dispatcher should run (non-fixture path). */
int ai_euro_use_full_dispatch(const ColonizeTurnContext* ctx);

/*
 * FUN_15eb_28c8 — colonist work-plot job scoring, structural reference port
 * (docs/ai_port_plan.md T1.17 / docs/port_plan.md W1.7). See ai_euro.c's own
 * header comment above the definition, and
 * original_sources_annotated/turn/colonist_work_plot_28c8.md, for scope and
 * fidelity notes. Reference-only / not wired into any live AI path (Tier 3
 * per docs/port_plan.md); declared here only so
 * tests/unit/test_ai_euro_28c8_job_score.c can verify it directly.
 */
typedef struct AiEuro28c8JobCandidate {
  int job;   /* COLONIZE_JOB_*, or -1 if nothing scored */
  int tile;  /* 0..COLONIZE_COLONY_FIELD_TILES-1 */
  int score;
} AiEuro28c8JobCandidate;

int ai_euro_28c8_colonist_job_score_structural(
  const ColonizeTurnContext* ctx,
  int colony_id,
  int colonist_slot,
  AiEuro28c8JobCandidate* out_best
);


/*
 * FUN_5bfb_10ec — Euro A↔B "war-worthy" eligibility by military balance
 * (static port 2026-08-27, T1.20). Returns 1 when A may reasonably go to
 * war with B. Continent tallies via the 20e6 accessors; the unknown
 * per-nation ×3 byte table at −0x6a9a (Linux `unknown34_pad`, filed dead)
 * reads as 0, and the DS:0xa153 byte gate is skipped (never resolved).
 */
int ai_euro_10ec_war_worthy(const ColonizeTurnContext* ctx, int a, int b);

#endif
