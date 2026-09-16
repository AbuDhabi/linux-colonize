#ifndef COLONIZE_CORE_WOODCUT_PRESENT_H
#define COLONIZE_CORE_WOODCUT_PRESENT_H

/*
 * Internal seam between woodcut.c (SIM: once-only bits + pending queue) and
 * woodcut_present.c (UI: plate load / draw / input), created by the
 * 2026-09-16 sim/UI split. Public prototypes stay in woodcut.h.
 */

#include "core/woodcut.h"

extern ColonizeWoodcutSoundFn g_woodcut_play;
extern ColonizeWoodcutSoundFn g_woodcut_set_bgm;

#endif /* COLONIZE_CORE_WOODCUT_PRESENT_H */
