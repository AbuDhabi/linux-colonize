#ifndef COLONIZE_FOUNDING_FATHERS_H
#define COLONIZE_FOUNDING_FATHERS_H

#include <stdbool.h>

#include "core/col1_save.h"
#include "core/turn.h"

/*
 * Founding-father election from liberty bells (manual / wiki effect table).
 *
 * Cost curve (gate, not spend): elect when
 *   liberty_bells_total >= 40 * (founding_father_count + 1)
 * Bells are never decremented — DOS threshold/spend recovery still PARKED.
 *
 * founding_fathers_tick: at most one elect per nation per call —
 * human first, then each AI Euro nation (player.control==1).
 *
 * Effects follow Colonization.pdf + docs/fandom_col1994.md (+ NAMES/decomp).
 * "Rough" means incomplete UI/wiring — not invented gold/crosses stand-ins.
 * Missing hooks: elect ownership only + PARKED comment naming the real effect.
 * Congress debate UI PARKED.
 */

/* FF indices (NAMES / COL1 order). */
#define FF_ADAM_SMITH 0
#define FF_JAKOB_FUGGER 1
#define FF_PETER_MINUIT 2
#define FF_PETER_STUYVESANT 3
#define FF_JAN_DE_WITT 4
#define FF_FERDINAND_MAGELLAN 5
#define FF_FRANCISCO_CORONADO 6
#define FF_HERNANDO_DE_SOTO 7
#define FF_HENRY_HUDSON 8
#define FF_SIEUR_DE_LA_SALLE 9
#define FF_HERNAN_CORTES 10
#define FF_GEORGE_WASHINGTON 11
#define FF_PAUL_REVERE 12
#define FF_FRANCIS_DRAKE 13
#define FF_JOHN_PAUL_JONES 14
#define FF_THOMAS_JEFFERSON 15
#define FF_POCAHONTAS 16
#define FF_THOMAS_PAINE 17
#define FF_SIMON_BOLIVAR 18
#define FF_BENJAMIN_FRANKLIN 19
#define FF_WILLIAM_BREWSTER 20
#define FF_WILLIAM_PENN 21
#define FF_JEAN_DE_BREBEUF 22
#define FF_JUAN_DE_SEPULVEDA 23
#define FF_BARTOLOME_DE_LAS_CASAS 24

/* Bells required to elect the next FF given how many already elected. */
unsigned founding_fathers_bells_needed(unsigned elected_count);

/* True if nation owns FF index (head owner or nation bitmask). */
bool founding_fathers_nation_has(const ColonizeCol1Save* col1, int nation, int ff_index);

/* Elect at most one FF per eligible nation when the bells threshold is met. */
void founding_fathers_tick(ColonizeTurnContext* ctx);

#endif
