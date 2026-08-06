/*
 * Move spent / apply step — FUN_465b_0000 (phase 12–14).
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c ~75417–75843
 * ASM:    CODE_115:465b (viceroy_unpacked.asm)
 * Linux:  src/core/ai.c — ai_dos_move_spent + ocean force-to-max in
 *         ai_native_nation_pulse.
 *
 * Quiet NEW WORLD Braves only exercise the cost head + friendly ADD path
 * (LAB_465b_05ca with foreign_tile=false). Euro combat / diplomacy / colony
 * contact tails are labeled PARKED.
 *
 * Reference only — not compiled into the Linux binary.
 *
 * =====================================================================
 * Phase 14 — every local_40 / moves_spent (0x3149) mutation (ASM-cited)
 * =====================================================================
 *
 * local_40 assignments (cost before ADD):
 *   465b:0051  table[class(dest)]*3     (078c → 2f76 stride 0x10)
 *   465b:0078  := 1  if both FA (0754) &0x0a
 *   465b:00b1  := 1  if both river (072c) &0x40 and axis-aligned
 *   465b:00e4  := min(local_40, 3) if 06be(dest) >= 0   (tribe owner)
 *   (no further local_40 writes before LAB_465b_05ca on any path)
 *
 * moves_spent (unit+0x05 / DS 0x3149) on friendly path (!foreign):
 *   465b:05f0  ADD  AL=low(local_40)
 *   465b:0628  MOV  AL=090c(unit)  if ocean(from)!=ocean(dest)
 *              AND 0696(from)<0 AND 0696(dest)<0   (euro settlement gate)
 *   465b:08f8  CALL 0934 (→1427_155e spent=max_mp) if (088a(unit) OR type==wagon)
 *              AND 07be(dest)>=0 — Brave (type 19) skips 088a cargo walk
 *
 * Foreign path skips the ADD block (JZ at 05de). Gamble fail after ADD does
 * not undo the ADD (JMP 0bcc).
 *
 * Phase 15: dump_b465r3 Sioux ADD AL=9 (= Linux class*3). T1/T2 land pairs
 * share presence→unowned shape yet golden spent 9 vs 3 — do not invent
 * exhaust caps from tile contrast. VR_B465F probes stock force-max entry.
 * Apache on r3 already spent=3 with force stubbed ⇒ AL was 3 at ADD.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

/* From accessors.c / indian_nation_turn.c */
extern int terrain_class_at(int x, int y);
extern int tile_fa_flags(int x, int y);
extern int tile_has_minor_river(int x, int y);
extern int tile_tribe_owner(int x, int y); /* FUN_281f_06be → FUN_137f_03e4 */
extern int ocean_or_high_seas(int x, int y); /* FUN_281f_0768 */
extern int unit_index_on_tile(int x, int y); /* FUN_281f_07e0 */
extern int unit_max_mp(int unit_index); /* FUN_281f_090c */
extern int rng_range(int lo, int hi_inclusive);

/*
 * Ghidra: FUN_281f_0696 → FUN_137f_0358 | euro_settlement_owner
 *
 * If dest has layer2 tribe bit and owner nibble is a European nation (0..3),
 * return that owner; else −1. Used as the "no settlement on tile" gate for
 * ocean/HS force-to-max (both tiles must return < 0).
 *
 * Older Linux comments called this "colony index"; the decomp returns a
 * nation id or −1, not a colony slot.
 */
int euro_settlement_owner(int x, int y) {
  /* Annotated: same bit test as tile_tribe_owner, then clamp Indians to −1. */
  int own = tile_tribe_owner(x, y);
  if (own > 3) {
    return -1;
  }
  return own; /* −1 if no tribe bit; 0..3 if Euro-owned settlement tile */
}

/* ====================================================================== */
/* Section 1 — Cost head (quiet Brave path)                               */
/* ====================================================================== */

/*
 * Ghidra: FUN_465b_0000 opening | move_spent_cost_head
 *
 * local_40 step cost before foreign/combat branches:
 *   spent = terr_cost_table[class(dest)] * 3          (DS:0x2f76, stride 0x10)
 *   if both tiles fa-mask & 0x0a → spent = 1
 *   if both minor-river & cardinal (from_x==to_x || from_y==to_y) → spent = 1
 *   if tile_tribe_owner(dest) >= 0 && spent > 3 → spent = 3   (06be)
 *
 * Phase 14 tile contrast (TURN2 pulse start): Sioux (49,40)→(49,39) and
 * Apache (45,52)→(46,53) have the same cost-head inputs as TURN1 Sioux
 * (49,41)→(49,40) spent=9 — FROM presence (l2&1), DEST no tribe, no FA/river
 * pair — yet goldens spent=3. From-presence caps break TURN1.
 * dump_b465r3: Sioux ADD AL=9 (cost head matches Linux); golden 3 is post-ADD.
 * Apache on same dump spent=3 with force-max stubbed ⇒ AL was 3 at ADD.
 *
 * Linux: ai_dos_move_spent.
 */
int move_spent_cost_head(int from_x, int from_y, int to_x, int to_y, int dir) {
  int terr_class = terrain_class_at(to_x, to_y);
  int spent = /* g_terr_cost[terr_class & 31] */ 0 * 3;
  (void)terr_class;

  if (tile_fa_flags(from_x, from_y) != 0 && tile_fa_flags(to_x, to_y) != 0) {
    spent = 1;
  }
  if (tile_has_minor_river(from_x, from_y) && tile_has_minor_river(to_x, to_y) &&
      (from_x == to_x || from_y == to_y)) {
    (void)dir;
    spent = 1;
  }
  if (tile_tribe_owner(to_x, to_y) >= 0 && spent > 3) {
    spent = 3;
  }
  /*
   * Sioux t2: hang AL=9 — do not invent cost-head caps. Chase post-ADD.
   * Apache t2: AL likely 3 at ADD (midturn_465b.md dump_b465r3).
   */
  if (spent > 100) {
    spent = 1;
  }
  return spent;
}

/* Legacy name — cost head only (phase 1 accessors). */
int move_spent_cost_only(
  int unit_index,
  int from_x,
  int from_y,
  int to_x,
  int to_y,
  int dir
) {
  (void)unit_index;
  return move_spent_cost_head(from_x, from_y, to_x, to_y, dir);
}

/* ====================================================================== */
/* Section 2 — Foreign-tile gate                                          */
/* ====================================================================== */

/*
 * After cost head, 465b resolves dest ownership:
 *   local_4 = tile_tribe_owner(dest)
 *   if unit_on_dest >= 0: local_4 = that unit's nation_lo
 *   foreign_tile = (local_4 >= 0 && local_4 != self_nation)
 *
 * Quiet Braves on own/unowned land: foreign_tile=false → skip combat body,
 * fall through to LAB_465b_05ca ADD path.
 */
int move_spent_dest_occupant_nation(int dest_x, int dest_y) {
  int own = tile_tribe_owner(dest_x, dest_y);
  int u = unit_index_on_tile(dest_x, dest_y);
  if (u >= 0) {
    own = (int)(VICEROY_UNIT_AT(u)->nation_id);
  }
  return own; /* may be −1 */
}

int move_spent_is_foreign_tile(int self_nation, int dest_occupant_nation) {
  if (dest_occupant_nation < 0) {
    return 0;
  }
  return dest_occupant_nation != self_nation;
}

/* ====================================================================== */
/* Section 3 — Euro combat / diplomacy (PARKED)                           */
/* ====================================================================== */

/*
 * When foreign_tile && self is Euro (nation < 4), 465b runs a large body:
 *   colony enter, war flags, dialogs (FUN_281f_0652), diplomacy OR bits,
 *   Indian raid contact (FUN_281f_0a42 / 0d6c), early exits to LAB_465b_0bd1.
 *
 * Also: unit-type combat table at DS:0x5236; ship types 0x0d..0x12 special.
 * Quiet Brave NEW WORLD never enters this with foreign_tile=false.
 *
 * PARKED — do not port into ai_dos_move_spent for R0 spent debt.
 */
void move_spent_foreign_combat_parked(int unit_index, int to_x, int to_y) {
  (void)unit_index;
  (void)to_x;
  (void)to_y;
  /* parked — see SYMBOL_MAP / docs/ai_transcription.md R3 */
}

/* ====================================================================== */
/* Section 4 — Ocean / HS transition force-to-max                         */
/* ====================================================================== */

/*
 * LAB_465b_05ca (friendly path, !foreign_tile):
 *   moves_spent += local_40
 *   if ocean_or_hs(from) != ocean_or_hs(dest)
 *      && euro_settlement_owner(from) < 0
 *      && euro_settlement_owner(dest) < 0:
 *        moves_spent = unit_max_mp(unit)   // FUN_281f_090c
 *
 * Quiet land Braves: dump_b465f3 — force-max body not entered for Sioux T2,
 * yet end spent=3 after ADD AL=9. Ocean gate is not the mechanism; chase other
 * 3149 writers / post-465b act path. Apache AL was 3 at ADD (dump_b465r3).
 */
int move_spent_ocean_force_max(
  int from_x,
  int from_y,
  int to_x,
  int to_y
) {
  if (ocean_or_high_seas(from_x, from_y) == ocean_or_high_seas(to_x, to_y)) {
    return 0;
  }
  if (euro_settlement_owner(from_x, from_y) >= 0) {
    return 0;
  }
  if (euro_settlement_owner(to_x, to_y) >= 0) {
    return 0;
  }
  return 1;
}

/* ====================================================================== */
/* Section 5 — ADD + partial-overspend gamble                             */
/* ====================================================================== */

/*
 * After computing local_40 and remaining = max_mp - moves_spent:
 *
 * Friendly ADD (!foreign):
 *   moves_spent += local_40
 *   maybe force max (ocean gate above)
 *
 * Then (shared with foreign path bookkeeping):
 *   if local_40 <= remaining OR moves_spent_was_0 OR foreign:
 *     commit move / chrome (LAB_465b_0673)
 *   else:
 *     roll = rng_range(1, local_40)   // FUN_281f_04d4
 *     if roll <= remaining: commit (LAB_465b_0673)
 *     else: fail — set failed flag, leave unit in place (LAB_465b_0bd1 path)
 *
 * AI Brave pulse in Linux always commits (no gamble); human pathfinder uses
 * the range(1,cost) rule in units.c. 097a loop allows spent to exceed max
 * after a successful ADD (force-to-max or large cost).
 *
 * Exit bookkeeping (act_counter bump, clear orders after 0x14) is PARKED for
 * quiet pulse — Linux uses turns_worked++ per step instead.
 */
int move_spent_partial_overspend_ok(int remaining, int step_cost, int moves_spent_was_zero) {
  if (step_cost <= remaining || moves_spent_was_zero) {
    return 1;
  }
  int roll = rng_range(1, step_cost);
  return roll <= remaining;
}

/* ====================================================================== */
/* Top-level skeleton                                                     */
/* ====================================================================== */

/*
 * Ghidra: FUN_465b_0000 | move_spent_add
 *
 * param_1 = unit index; param_2/param_3 = dest x/y.
 * Reads unit xy from DS unit record as from.
 *
 * Returns nothing; mutates moves_spent / xy on success (full body).
 * Annotated skeleton shows Brave-relevant control flow only.
 */
void move_spent_add(int unit_index, int to_x, int to_y) {
  ViceroyUnit* u = VICEROY_UNIT_AT(unit_index);
  int from_x = (int)u->x;
  int from_y = (int)u->y;
  int self = (int)u->nation_id;
  int dir = 0; /* caller supplies facing; river test uses axis align */

  int step_cost = move_spent_cost_head(from_x, from_y, to_x, to_y, dir);
  int dest_nation = move_spent_dest_occupant_nation(to_x, to_y);
  int foreign = move_spent_is_foreign_tile(self, dest_nation);

  if (foreign) {
    /* Section 3 — Euro/Indian combat contact. Quiet Braves skip. */
    move_spent_foreign_combat_parked(unit_index, to_x, to_y);
    return;
  }

  /* Section 5 — ADD */
  int spent_before = (int)u->moves_spent;
  int max_mp = unit_max_mp(unit_index);
  int remaining = max_mp - spent_before;
  u->moves_spent = (uint8_t)(spent_before + step_cost);

  /* Section 4 — ocean / HS force-to-max */
  if (move_spent_ocean_force_max(from_x, from_y, to_x, to_y)) {
    u->moves_spent = (uint8_t)max_mp;
  }

  if (!move_spent_partial_overspend_ok(remaining, step_cost, spent_before == 0)) {
    /* failed gamble — unit stays; moves_spent may still reflect attempt in DOS */
    return;
  }

  /* Commit xy / owner / chrome: PARKED detail — Linux quiet_brave_apply_step */
  u->x = (uint8_t)to_x;
  u->y = (uint8_t)to_y;
}
