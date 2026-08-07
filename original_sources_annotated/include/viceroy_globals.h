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

/*
 * Coarse fog / tribe-region plane (FUN_1d1d_0dae memset 0x10e @ entry of
 * FUN_6a09_0006 and FUN_521d_0a60).
 *
 * Ghidra often prints DS:-0x6056; unsigned near offset is DS:0x9faa
 * (0x10000-0x6056). Size 270 = 15×18, pitch VICEROY_COARSE_FOG_PITCH.
 *
 * Dual index into the same buffer (both confirmed in .asm):
 *   Explore / +8 (FUN_521d_20e6): (x>>2) + (y>>2)*pitch   — SAR 2
 *   Tribe spacing (FUN_6a09):     (y/5) + (x/5)*pitch     — IDIV 5
 * Tribe /5 marks do NOT clear explore >>2 cells; do not conflate them.
 */
#define VICEROY_DS_COARSE_FOG       0x9faa
#define VICEROY_DS_COARSE_FOG_GHIDRA_NEG 0x6056 /* printed as DS:-0x6056 */
#define VICEROY_COARSE_FOG_PITCH    0x12
#define VICEROY_COARSE_FOG_SIZE     0x10e /* 270; memset arg */

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

/*
 * Euro AI goal tables (FUN_521d_* helpers; written by 0a60, consumed by act).
 * Ghidra often prints signed DS negatives; unsigned near offsets below.
 *
 * Primary:  nation × 64 slots × 4 bytes  @ DS:0x98b0  (Ghidra −0x6750)
 * Secondary:nation × 16 slots × 4 bytes  @ DS:0x9eaa  (Ghidra −0x6156)
 * Work queue:        16 slots × 6 bytes  @ DS:0xa0dc  (Ghidra −0x5f24)
 *
 * Slot layout (primary/secondary): x, y, code (0xff empty), priority.
 * Work-queue slot: int16 id, int16 score, uint8 flag_a, uint8 flag_b.
 */
#define VICEROY_DS_AI_PRIMARY_GOALS   0x98b0
#define VICEROY_DS_AI_SECONDARY_GOALS 0x9eaa
#define VICEROY_DS_AI_WORK_QUEUE      0xa0dc
#define VICEROY_AI_PRIMARY_SLOTS      0x40
#define VICEROY_AI_SECONDARY_SLOTS    0x10
#define VICEROY_AI_WORK_QUEUE_SLOTS   0x10
#define VICEROY_AI_GOAL_STRIDE        4
#define VICEROY_AI_WORK_STRIDE        6
#define VICEROY_AI_GOAL_EMPTY         0xff

/* Soft goal-code labels from 0a60 upsert call sites (param code byte). */
#define VICEROY_AI_GOAL_CONTACT       0 /* foreign ship / presence claim */
#define VICEROY_AI_GOAL_FOUND         1 /* founding / settle-adjacent */
#define VICEROY_AI_GOAL_LABOR         3 /* colony tools/labor demand */
#define VICEROY_AI_GOAL_MILITARY      4 /* military pressure on colony */
#define VICEROY_AI_GOAL_COLONY_BUILD  5 /* colony cargo/build (else of 8) */
#define VICEROY_AI_GOAL_MIL_EXPAND    7 /* mil-expand peer of FOUND */
#define VICEROY_AI_GOAL_COLONY_CARGO  8 /* colony +0x1b bit1 set → code 8 */

/* ---- Helpers for annotated C (documentation macros) -------------------- */

/*
 * In annotated .c files we write `g_units[i].type` instead of
 * `*(byte *)(i * 0x1c + 0x3146)`. These macros document the mapping; they are
 * not meant to compile against a real DS image.
 */
#define VICEROY_UNIT_AT(i)   ((ViceroyUnit *)(uintptr_t)(VICEROY_DS_UNITS_BASE + (i) * VICEROY_UNIT_STRIDE))
#define VICEROY_TRIBE_AT(i)  ((ViceroyTribe *)(uintptr_t)(VICEROY_DS_TRIBES_BASE + (i) * VICEROY_TRIBE_STRIDE))

#endif /* VICEROY_GLOBALS_H */
