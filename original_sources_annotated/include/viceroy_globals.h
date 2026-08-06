/*
 * Named DS / near-pointer globals used by AI nation turns.
 *
 * Values are absolute offsets as written in the Ghidra C export
 * (`*(int *)0x5394`, etc.). The Linux port does not share this address space;
 * names exist so annotated AI bodies read as simulation code.
 *
 * Reference only — not compiled into the Linux binary.
 */
#ifndef VICEROY_GLOBALS_H
#define VICEROY_GLOBALS_H

#include <stdint.h>

#include "viceroy_types.h"

/* ---- Map plane bases / pitch ------------------------------------------- */

/* Map width (pitch) word used by FUN_137f_010e / 0142 / 01ac. */
#define VICEROY_DS_MAP_PITCH        0x853a
/* Terrain / layer2 / layer3 base pointers. */
#define VICEROY_DS_MAP_TERRAIN_PTR  0x015c
#define VICEROY_DS_MAP_LAYER2_PTR   0x0160
#define VICEROY_DS_MAP_LAYER3_PTR   0x0164

/* Terrain class cost table base (byte[32] ×3 in move_spent_add). */
#define VICEROY_DS_TERR_COST_TABLE  0x2f76

/* ---- Unit / tribe pools ------------------------------------------------ */

#define VICEROY_DS_UNITS_BASE       0x3144  /* first unit.x */
#define VICEROY_DS_UNIT_COUNT       0x539c  /* int: live unit count */
#define VICEROY_DS_TRIBES_BASE      0x54ee  /* first tribe.x */
#define VICEROY_DS_TRIBE_COUNT      0x539a  /* int: live tribe count */
#define VICEROY_DS_COLONY_COUNT     0x539e  /* int: live colony count */

/* ---- Nation-turn bookkeeping ------------------------------------------- */

/*
 * g_active_nation_id:
 *   Euro 6d8e: set to param_1 (0..3 EN/FR/SP/DU).
 *   Indian 1816: set to param_1 + 4 (Indian nations 4..11).
 */
#define VICEROY_DS_ACTIVE_NATION    0x5394

/* Human / focus nation and related turn flags (Euro path). */
#define VICEROY_DS_HUMAN_NATION     0x5396
#define VICEROY_DS_FOCUS_NATION     0x5398

/* Difficulty / AI aggression byte used in alarm prelude RNG. */
#define VICEROY_DS_DIFFICULTY       0x53a6

/* NEW WORLD vs AMERICA / scenario flag byte (bit0 checked in 1816 alarm). */
#define VICEROY_DS_GAME_FLAGS       0x5382

/* Timer word reseeding LCG via FUN_281f_04ca (VR_SEED locks to 100). */
#define VICEROY_DS_TIMER_WORD       0x83a6

/*
 * Per-Indian-nation state block pointer (relations / alarm / accumulators).
 * 1816 reads flags at +3, signed bytes at +7/+8, word at +10, words at +0xe….
 */
#define VICEROY_DS_INDIAN_STATE_PTR 0x8d4e

/* Current tribe / village pointers used around growth + alarm. */
#define VICEROY_DS_CUR_TRIBE_PTR    0x8d4a
#define VICEROY_DS_CUR_TRIBE_ALT    0x8d4c
#define VICEROY_DS_CUR_INDIAN_PTR   0x8d50
#define VICEROY_DS_CUR_INDIAN_ALT   0x8d52

/* Euro unit-act sticky index (anti-spin in 6d8e unit loop). */
#define VICEROY_DS_EURO_STICKY_UNIT 0x2d12
#define VICEROY_DS_EURO_STICKY_CNT  0x2d14

/* ---- Helpers for annotated C (documentation macros) -------------------- */

/*
 * In annotated .c files we write `g_units[i].type` instead of
 * `*(byte *)(i * 0x1c + 0x3146)`. These macros document the mapping; they are
 * not meant to compile against a real DS image.
 */
#define VICEROY_UNIT_AT(i)   ((ViceroyUnit *)(uintptr_t)(VICEROY_DS_UNITS_BASE + (i) * VICEROY_UNIT_STRIDE))
#define VICEROY_TRIBE_AT(i)  ((ViceroyTribe *)(uintptr_t)(VICEROY_DS_TRIBES_BASE + (i) * VICEROY_TRIBE_STRIDE))

#endif /* VICEROY_GLOBALS_H */
