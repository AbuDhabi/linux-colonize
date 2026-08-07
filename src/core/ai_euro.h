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

#endif
