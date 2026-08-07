#ifndef COLONIZE_AI_CONTACT_H
#define COLONIZE_AI_CONTACT_H

#include "core/turn.h"

/*
 * Indian contact / missions / trade / raids (T0).
 * FUN_5bfb_022e meet, FUN_4cc6 missions, FUN_4d56_2154/2820/4528 raids.
 */

/* Alarm prelude + relation ticks + mission clear for one indian nation. */
void ai_contact_indian_prelude(ColonizeTurnContext* ctx, int nation_id);

/* Meet / teach / trade opportunities adjacent to Braves. */
void ai_contact_indian_meet_trade(ColonizeTurnContext* ctx, int nation_id);

/* Friction-gated raids against nearby Euro units/colonies. */
void ai_contact_indian_raids(ColonizeTurnContext* ctx, int nation_id);

#endif
