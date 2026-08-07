#ifndef COLONIZE_FOUNDING_FATHERS_H
#define COLONIZE_FOUNDING_FATHERS_H

#include "core/turn.h"

/*
 * Rough founding-father election from liberty bells.
 *
 * Cost curve (gate, not spend): elect when
 *   liberty_bells_total >= 40 * (founding_father_count + 1)
 * Bells are never decremented — DOS threshold/spend recovery PARKED.
 *
 * founding_fathers_tick: at most one elect per nation per call —
 * human first, then each AI Euro nation (player.control==1).
 *
 * Effects: status line (human) + tiny stand-ins for many famous FFs
 * (gold / crosses / tax / bells / REF / Fugger boycott forgive).
 * Full wiki/decomp effect table + Congress debate UI PARKED.
 */

/* Bells required to elect the next FF given how many already elected. */
unsigned founding_fathers_bells_needed(unsigned elected_count);

/* Elect at most one FF per eligible nation when the bells threshold is met. */
void founding_fathers_tick(ColonizeTurnContext* ctx);

#endif
