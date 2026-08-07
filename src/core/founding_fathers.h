#ifndef COLONIZE_FOUNDING_FATHERS_H
#define COLONIZE_FOUNDING_FATHERS_H

#include "core/turn.h"

/*
 * Rough founding-father election from liberty bells (human nation only).
 *
 * Cost curve (gate, not spend): elect when
 *   liberty_bells_total >= 40 * (founding_father_count + 1)
 * Bells are never decremented — DOS threshold/spend recovery PARKED.
 *
 * Effects: status line + tiny stand-ins for 1–2 famous FFs. Full table PARKED.
 * Congress debate UI / AI-nation election PARKED.
 */

/* Bells required to elect the next FF given how many already elected. */
unsigned founding_fathers_bells_needed(unsigned elected_count);

/* Elect at most one FF for the human nation when the bells threshold is met. */
void founding_fathers_tick(ColonizeTurnContext* ctx);

#endif
