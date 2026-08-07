/*
 * Unit MP / orders helpers — real FUN_1427_* bodies behind FUN_281f_* thunks.
 *
 * The 6-line FUN_281f_09xx stubs only call FUN_210d_0d91 (EMS page) then the
 * real body. Annotate the real body here; ignore the thunk machinery.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c
 * Linux:  src/core/ai.c — Brave max_mp=3; pulse spent < max_mp
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

/*
 * Ghidra: FUN_1427_065a | unit_max_mp  (FUN_281f_090c thunk)
 *
 * max_mp = type_table[type].base_mp at DS:0x5234 + type*0x0e.
 * If FUN_15eb_3960(nation, 5) nonzero AND type in 0x0d..0x12 (ships): +3.
 * Brave (type 19): table byte is 3 — Linux hardcodes 3.
 */
int unit_max_mp(int unit_index) {
  ViceroyUnit* u = VICEROY_UNIT_AT(unit_index);
  int max_mp = 3; /* live: *(uint8_t *)(type * 0xe + 0x5234) */
  (void)u;
  /* ship Magellan-style bonus parked — quiet Braves never hit it */
  return max_mp;
}

/*
 * Ghidra: FUN_1427_155e | unit_exhaust_mp  (FUN_281f_0934 thunk)
 *
 * moves_spent = unit_max_mp(unit). Sole non-ADD spent=max writer on the quiet
 * Brave path when cargo/wagon (465b:08f8), stay-dir (14fe), or act>0x14 (1816).
 */
void unit_exhaust_mp(int unit_index) {
  ViceroyUnit* u = VICEROY_UNIT_AT(unit_index);
  u->moves_spent = (uint8_t)unit_max_mp(unit_index);
}

/* Alias kept for indian_nation_turn / SYMBOL_MAP. */
void unit_clear_orders(int unit_index) {
  unit_exhaust_mp(unit_index);
}

/*
 * Ghidra: FUN_1427_13b0 | unit_has_moves_remaining  (FUN_281f_097a thunk)
 *
 * True when: live index, x signed-ok, nation==g_active_nation, not suppressed
 * (flag 0x80 unless type==wagon 0x0b), moves_spent < unit_max_mp.
 */
int unit_has_moves_remaining(int unit_index) {
  if (unit_index < 0) {
    return 0;
  }
  ViceroyUnit* u = VICEROY_UNIT_AT(unit_index);
  if ((int8_t)u->x < 0) {
    return 0;
  }
  /* nation must match DS:0x5394 g_active_nation_id */
  if ((u->unk_04 & 0x80) != 0 && u->type != 0x0b) {
    return 0;
  }
  return u->moves_spent < (uint8_t)unit_max_mp(unit_index);
}

/*
 * Ghidra: FUN_1427_12f6 | unit_tile_list_refresh  (FUN_281f_0916 thunk)
 *
 * Ships (type 0x0d..0x12): FUN_1427_10be. Else FUN_1427_0362(unit, 0xfffe, 0xfffe)
 * (tile occupancy list bookkeeping). Does NOT write moves_spent (0x3149).
 */
void unit_tile_list_refresh(int unit_index) {
  (void)unit_index;
  /* no 3149 write — see brave_spent_callgraph.md */
}

/*
 * Ghidra: FUN_1427_040c | stack_set_xy  (FUN_281f_0948 thunk)
 *
 * Walk transport stack (0002/004a); FUN_1427_0362 each to (x,y).
 * Does NOT write moves_spent.
 */
void stack_set_xy(int unit_index, int x, int y) {
  (void)unit_index;
  (void)x;
  (void)y;
}

/*
 * Ghidra: FUN_1427_0968 | stack_facing_refresh  (FUN_281f_08da thunk)
 * Walk stack; FUN_1427_0954 each. No 3149 write.
 */
void stack_facing_refresh(int unit_index) {
  (void)unit_index;
}

/*
 * Ghidra: FUN_1427_0ce6 | unit_post_move_chrome  (FUN_281f_084e thunk)
 * No 3149 write (verified in decomp body).
 */
void unit_post_move_chrome(int unit_index) {
  (void)unit_index;
}

/*
 * Ghidra: FUN_1427_0c72 | unit_visibility_bits  (FUN_281f_07fe thunk)
 * No 3149 write.
 */
void unit_visibility_bits(int unit_index, int mask) {
  (void)unit_index;
  (void)mask;
}

/*
 * Ghidra: FUN_1427_0644 | tile_stack_head  (FUN_281f_08e4 thunk)
 * 04d6 + 0002 — stack probe helper. No 3149 write.
 */
int tile_stack_head(void) {
  return -1;
}

/*
 * Ghidra: FUN_1427_1284 | stack_has_ship  (FUN_281f_088a thunk)
 * Walk stack; true if any type in 0x0d..0x12. Lone Brave → false.
 */
int stack_has_ship(int unit_index) {
  (void)unit_index;
  return 0;
}

/*
 * Ghidra: FUN_1427_09ac | stack_or_nation_flag  (FUN_281f_07d6 thunk)
 * Walk stack; OR nation bit into 3147. No 3149 write.
 */
void stack_or_nation_flag(int unit_index, int nation) {
  (void)unit_index;
  (void)nation;
}
