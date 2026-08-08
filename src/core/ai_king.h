#ifndef COLONIZE_AI_KING_H
#define COLONIZE_AI_KING_H

#include <stdint.h>

#include "core/dos_rng.h"
#include "core/turn.h"

/*
 * King / tax / REF / independence — partial structural port of FUN_43f7_*.
 * Thin map: original_sources_annotated/ai/king_ref.md
 * Replaces turn_run_king_stub body.
 */

void ai_king_nation_turn(ColonizeTurnContext* ctx);

/*
 * Apply human choice from map AI popup:
 *   KING_AUDIENCE Accept/Refuse (38fd_5be8; Refuse → Sugar follow-up OK),
 *   KING_MERC Hire/Decline (2244; Hire → success follow-up OK;
 *     Decline → declined follow-up OK),
 *   KING_CONGRESS Confirm (2564/1a26 → rename + WoI-begins OK chain).
 * No-op if tag mismatch / cancelled.
 */
void ai_king_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup);

/* FUN_43f7_0004: pop-weighted SoL percent for a European nation (0..100). */
int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_38fd_3dc8 dump-goods cargo pick (thin API for AI / tax refuse callers).
 *
 * Among @CARGO indices 0..15 whose bit is set in candidate_mask and clear in
 * boycott_bitmap, pick one uniformly via dos_rng. Returns cargo index, or -1
 * if none / null rng.
 *
 * Cite: viceroy_unpacked.c FUN_38fd_3dc8 — builds eligible list from cargos
 * not already in nation boycott_bitmap (local_a6), then RNG-picks; Europe
 * price weighting (local_7a) stays PARKED here (uniform among eligible).
 * docs/fandom_col1994.md Boycott: throw "named goods" (RNG unnamed list —
 * not a fixed Tobacco second cargo). Does not mutate boycott_bitmap; caller
 * ORs (1u << idx) when applying. King refuse path still freezes Sugar only.
 */
int ai_king_pick_dump_goods_cargo(
  uint16_t boycott_bitmap,
  uint16_t candidate_mask,
  ColonizeDosRng* rng
);

#endif
