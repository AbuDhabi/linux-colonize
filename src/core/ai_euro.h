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
 * (docs/port_plan.md T1.17 / docs/port_plan.md W1.7). See ai_euro.c's own
 * header comment above the definition, and
 * original_sources_annotated/turn/colonist_work_plot_28c8.md, for scope and
 * fidelity notes. The structural entry point scores with plain (non-expert)
 * tile yields so tests/unit/test_ai_euro_28c8_job_score.c stays
 * hand-auditable; the live AI colony tick (ai_euro_colony_tick_28c8_reassign,
 * W3.1 2026-08-29, DOS FUN_5952_035e) uses the same scorer with the
 * colonist's real profession.
 */
typedef struct AiEuro28c8JobCandidate {
  int job;   /* COLONIZE_JOB_*, or -1 if nothing scored */
  int tile;  /* 0..COLONIZE_COLONY_FIELD_TILES-1 */
  int score;
  int yield; /* winning tile's raw field yield (DOS DS:0x8dbe) */
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
 * war with B. Continent tallies via the 20e6 accessors; the −0x6a9a
 * belligerence term is real since 2026-09-06d (`ai_diplo_leader_trait`),
 * and the DS:0xa153 byte gate is skipped (never resolved).
 */
int ai_euro_10ec_war_worthy(const ColonizeTurnContext* ctx, int a, int b);

/*
 * Wagon Train village-errand latch — DOS byte DS:(unit_index * 0x1c + 0x3158).
 * The unit array base is DS:0x3144 (stride 0x1c = sizeof COL1 unit record),
 * so this is COL1 unit record +0x14 = ColonizeCol1Unit.cargo_hold[4]
 * (record map: holds_occupied +0x0c, cargo_item nibbles +0x0d..+0x0f,
 * cargo_hold[6] +0x10..+0x15 — src/core/col1_save.h ColonizeCol1Unit;
 * cross-check: +0x315b = record +0x17 = `profession`, the Treasure gold/100
 * byte, and +0x314a = record +0x06 = `origin`).
 *
 * DOS touches the byte for type 0x0c (Wagon Train) only:
 *   set 1  — FUN_521d_20e6 load matrix, land arm (viceroy_unpacked.c:84817,
 *            :90364 in the second entry point);
 *   read   — FUN_521d_20e6 dead-end band, guarded by `type == '\f'`
 *            (viceroy_unpacked.c:85154/:89943);
 *   clear  — FUN_4d56_2820 trade entry for non-person types
 *            (viceroy_unpacked.c:82121), and unit spawn init only when
 *            `type == '\f'` (viceroy_unpacked.c:7752).
 * For every sea type +0x14 is a real cargo hold, so the bridge must never
 * reinterpret it there.
 *
 * These accessors let col1_bridge round-trip the latch through the save.
 */
unsigned char ai_euro_wagon_errand_get(int unit_id);
void ai_euro_wagon_errand_set(int unit_id, unsigned char value);
void ai_euro_wagon_errand_clear_all(void);

#endif
