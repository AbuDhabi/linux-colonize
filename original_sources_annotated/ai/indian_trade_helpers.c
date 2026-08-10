/*
 * Indian trade nest — FUN_4d56_2820 helpers (annotated stubs only).
 *
 * Decomp: viceroy_unpacked.c ~82064–83476
 * Section map: ai/indian_trade_2820.md
 * Linux: ai_contact_* thin auto-trade / gift / hard-bargain; deep PARKED.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

/* Ghidra: FUN_4d56_2820 | indian_trade_meet_shell
 * Thunk 2a1f_044c. param: unit, ?, euro_nation (−1 abort).
 */
void indian_trade_meet_shell(int unit_index, int unused, int euro_nation) {
  (void)unit_index;
  (void)unused;
  (void)euro_nation;
  /* human gate → chrome; price tables; cargo pick → indian_trade_dispatch */
}

/* Ghidra: FUN_4d56_2aac | indian_trade_dispatch */
void indian_trade_dispatch(void) {
  /* selected_good<0 → no_deal; AI → buy_ai; else player buy or refuse 0x1561 */
}

/* Ghidra: FUN_4d56_2af6 | indian_trade_clear_last_goods */
void indian_trade_clear_last_goods(void) {
  /* clear tribe last-good slots; refuse dialog 0x1561 → 3582 */
}

/* Ghidra: FUN_4d56_2b92 | indian_trade_buy_player */
void indian_trade_buy_player(void) {
  /* price loop; accept / haggle(2f96) / hard(306c) / demand(311e) */
}

/* Ghidra: FUN_4d56_2bbc | indian_trade_buy_ai */
void indian_trade_buy_ai(void) {
  /* same pricing; auto choices */
}

/* Ghidra: FUN_4d56_2e92 | indian_trade_no_deal */
void indian_trade_no_deal(void) {
  /* invalid good/capacity → 311e or 3582 */
}

/* Ghidra: FUN_4d56_2f96 | indian_trade_haggle */
void indian_trade_haggle(void) {
  /* choice 2: bump offer/tension; resume buy loop */
}

/* Ghidra: FUN_4d56_306c | indian_trade_hard_bargain */
void indian_trade_hard_bargain(void) {
  /* choice 3: worse terms + tension; resume */
}

/* Ghidra: FUN_4d56_311e | indian_trade_counter_demand */
void indian_trade_counter_demand(void) {
  /* tribute goods + priced buy-back dialog */
}

/* Ghidra: FUN_4d56_3582 | indian_trade_close_friction */
void indian_trade_close_friction(void) {
  /* post-2820 friction / alarm floor */
}
