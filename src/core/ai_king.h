#ifndef COLONIZE_AI_KING_H
#define COLONIZE_AI_KING_H

#include "core/turn.h"

/*
 * King / tax / REF / independence (T0) — FUN_43f7_* cluster.
 * Replaces turn_run_king_stub.
 */

void ai_king_nation_turn(ColonizeTurnContext* ctx);

/* Pop-weighted SoL percent for a European nation (0..100). */
int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id);

#endif
