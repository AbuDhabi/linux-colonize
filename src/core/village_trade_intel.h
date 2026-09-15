#ifndef COLONIZE_CORE_VILLAGE_TRADE_INTEL_H
#define COLONIZE_CORE_VILLAGE_TRADE_INTEL_H

#include <stdbool.h>

/*
 * Linux-only convenience: what a native settlement has TOLD a European
 * player it buys and sells, so the map sidebar can list it under the
 * settlement (View Pieces, "Buys:" / "Sells:" icon rows).
 *
 * DOS has no such memory, so this is deliberately runtime-only: it is never
 * written to a save, and ai_contact_reset (new game and Load) wipes it.
 *
 * The settlement record itself (ColonizeCol1Tribe) is the raw 18-byte DOS
 * blob read/written verbatim by col1_save.c, so the knowledge lives in this
 * side table instead, keyed by the settlement's tile (settlements never move;
 * tribe[] indices do shift when one is destroyed).
 *
 * Fed by the three "what we need" dialogs (@BRING, @BADCARGO, @CHIEFHOWDY —
 * each names the three most-wanted goods) and by @BUYWHICH (the goods the
 * settlement offers). Goods are cargo ids 0..15.
 */

#define VILLAGE_TRADE_INTEL_GOODS 3

void village_trade_intel_reset(void);

/* Record the goods a settlement said it wants. `n` ≤ 3; ids outside 0..15 are skipped. */
void village_trade_intel_note_buys(int euro_nation, int x, int y, const int* goods, int n);

/* Record the goods a settlement said it has for sale. */
void village_trade_intel_note_sells(int euro_nation, int x, int y, const int* goods, int n);

/*
 * Fetch what `euro_nation` has been told about the settlement at (x, y).
 * `out_*_n` = 0 when that half is unknown. Returns true when either half is known.
 */
bool village_trade_intel_get(
  int euro_nation, int x, int y, int buys[VILLAGE_TRADE_INTEL_GOODS], int* out_buys_n,
  int sells[VILLAGE_TRADE_INTEL_GOODS], int* out_sells_n
);

/* Settlement at (x, y) is gone — drop everything known about it. */
void village_trade_intel_forget_tile(int x, int y);

#endif
