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
 * FUN_43f7_2244 — peacetime AI-nation-only self/ally-funded troop gift.
 * Called once per AI-controlled Euro nation's own turn (never the human —
 * see ai_king.c's header comment on the function body for the full
 * derivation). No-op post-WoI.
 */
void ai_king_ai_peacetime_gift(ColonizeTurnContext* ctx, int nation_id);

/*
 * Apply human choice from map AI popup:
 *   KING_AUDIENCE Accept/Refuse (38fd_5be8; Refuse → @TEAPARTY OK /
 *     KING_DUMP_GOODS then @TEAPARTY),
 *   KING_MERC Hire/Decline (2244; Hire → success follow-up OK;
 *     Decline → declined follow-up OK),
 *   KING_CONGRESS Confirm (2564/1a26 → rename + WoI-begins OK chain).
 * No-op if tag mismatch / cancelled.
 */
void ai_king_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup);

/* FUN_43f7_0004: pop-weighted SoL percent for a European nation (0..100). */
int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id);

/* True once WoI is declared (head.unknown46[0] stand-in). */
int ai_king_independence_declared(const ColonizeCol1Save* col1);

/* Crown nation-slot stand-in (nation 1 if human is 0, else nation 0) — no
 * real 5th DOS crown identity in Linux, this reuses an existing slot. */
int ai_king_crown_nation(int human_nation);

/*
 * FUN_38fd_3dc8 dump-goods cargo pick (thin API for AI / tax refuse callers).
 *
 * Among @CARGO indices 0..15 whose bit is set in candidate_mask and clear in
 * boycott_bitmap, pick one via dos_rng. When @cargo_bid is NULL, pick
 * uniformly among that set (europe unavailable / prior behavior). When
 * non-NULL, eligible further requires cargo_bid[c] > 0 (live Europe bid;
 * zero-price goods are not dumped), then roulette by bid[c] (Europe local_7a
 * stand-in). Returns cargo index, or -1 if none / null rng.
 *
 * Cite: viceroy_unpacked.c FUN_38fd_3dc8 — builds eligible list from cargos
 * not already in nation boycott_bitmap (local_a6), then RNG-picks weighted by
 * Europe prices (local_7a). docs/fandom_col1994.md Boycott: throw "named
 * goods" (RNG unnamed list — not a fixed Tobacco second cargo). Does not
 * mutate boycott_bitmap; caller ORs (1u << idx) when applying. King refuse
 * path still freezes Sugar only.
 */
int ai_king_pick_dump_goods_cargo(
  uint16_t boycott_bitmap,
  uint16_t candidate_mask,
  ColonizeDosRng* rng,
  const int* cargo_bid /* COLONIZE_CARGO_COUNT, or NULL → uniform */
);

#endif
