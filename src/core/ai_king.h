#ifndef COLONIZE_AI_KING_H
#define COLONIZE_AI_KING_H

#include "core/turn.h"

/*
 * King / tax / REF / independence — partial structural port of FUN_43f7_*.
 * Thin map: original_sources_annotated/ai/king_ref.md
 * Replaces turn_run_king_stub body.
 */

void ai_king_nation_turn(ColonizeTurnContext* ctx);

/* FUN_43f7_0004: pop-weighted SoL percent for a European nation (0..100). */
int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id);

#endif
