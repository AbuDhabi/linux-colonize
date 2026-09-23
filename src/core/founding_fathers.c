#include "core/ai_king.h"
#include "core/founding_fathers.h"

#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "platform/diagnostics.h"
#include "core/units.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * FF election (FUN_4345_0a22 / 0982 / 0342 stand-ins for control flow).
 * head.founding_father[i]: -1 unclaimed; 0..3 = owning European nation.
 * nation.founding_fathers[4]: bit i set when nation elected FF i.
 *
 * Effect authority: Colonization.pdf (FF ~pp. 83–94) + docs/fandom_col1994.md.
 * No invented treasury/crosses/tools fiction when the real rule is known.
 */

/* King tax-refuse stand-in byte (ai_king market_demand_pool_raw[2]). */

/* DOS nation+0xc — bells since last FF elect; not stored in ColonizeCol1Nation. */
static uint16_t s_ff_bells_since_elect[COLONIZE_COL1_NATION_COUNT];
static bool s_ff_pools_initialized;

/*
 * Sentinel written to nation.unknown21_pad (dead DOS byte, col1_save.h) when
 * our own writer stashes the pool into liberty_bells_last_turn. Lets
 * after_load tell that apart from a genuine/untouched DOS last_turn value.
 */
#define FF_POOL_STASH_MARKER ((uint8_t)0xc1)

static unsigned ff_bells_threshold_at_elect_count(
  const ColonizeCol1Save* col1,
  int nation,
  unsigned elected_count
) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 40u;
  }
  ColonizeCol1Save snap = *col1;
  snap.nation[nation].founding_father_count = (uint16_t)elected_count;
  return founding_fathers_bells_needed(&snap, nation);
}

/*
 * bugs.md: the Congress debate is persistent in DOS — escaping the popup
 * re-presents it immediately with the SAME candidates, so the pseudorandom
 * slate cannot be rerolled by cancelling (or by waiting a turn: the old
 * code rolled a fresh slate on every enqueue). The rolled slate is kept
 * here until a candidate is actually chosen.
 */
static int s_ff_debate_slate[AI_POPUP_CHOICE_MAX];
static int s_ff_debate_slate_n;
static int s_ff_debate_slate_nation = -1;

void founding_fathers_reset(void) {
  memset(s_ff_bells_since_elect, 0, sizeof(s_ff_bells_since_elect));
  s_ff_pools_initialized = false;
  s_ff_debate_slate_n = 0;
  s_ff_debate_slate_nation = -1;
}

void founding_fathers_stash_pools_into_col1(
  ColonizeCol1Save* col1,
  uint16_t restore_last_turn[COLONIZE_COL1_NATION_COUNT],
  uint8_t restore_pad21[COLONIZE_COL1_NATION_COUNT]
) {
  if (!col1) {
    return;
  }
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (restore_last_turn) {
      restore_last_turn[n] = col1->nation[n].liberty_bells_last_turn;
    }
    if (restore_pad21) {
      restore_pad21[n] = col1->nation[n].unknown21_pad;
    }
    if (!s_ff_pools_initialized) {
      continue;
    }
    col1->nation[n].liberty_bells_last_turn = s_ff_bells_since_elect[n];
    col1->nation[n].unknown21_pad = FF_POOL_STASH_MARKER;
  }
}

void founding_fathers_restore_col1_last_turn(
  ColonizeCol1Save* col1,
  const uint16_t restore_last_turn[COLONIZE_COL1_NATION_COUNT],
  const uint8_t restore_pad21[COLONIZE_COL1_NATION_COUNT]
) {
  if (!col1) {
    return;
  }
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (restore_last_turn) {
      col1->nation[n].liberty_bells_last_turn = restore_last_turn[n];
    }
    if (restore_pad21) {
      col1->nation[n].unknown21_pad = restore_pad21[n];
    }
  }
}

bool founding_fathers_col1_last_turn_is_stash(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }
  return col1->nation[nation_id].unknown21_pad == FF_POOL_STASH_MARKER;
}

void founding_fathers_sync_from_col1(const ColonizeCol1Save* col1) {
  if (!col1) {
    founding_fathers_reset();
    return;
  }
  s_ff_pools_initialized = true;
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    const ColonizeCol1Nation* nat = &col1->nation[n];
    const unsigned total = (unsigned)nat->liberty_bells_total;
    const unsigned count = (unsigned)nat->founding_father_count;
    const unsigned need = founding_fathers_bells_needed(col1, n);

    if (count == 0u) {
      s_ff_bells_since_elect[n] = (uint16_t)total;
      continue;
    }

    if (total <= need) {
      /* Authentic DOS: +0xc is the live pool, not lifetime cumulative — check
       * this first. A save with several FFs already elected has a "spent"
       * sum (below) that grows past any plausible live pool almost
       * immediately (thresholds compound with each election), so testing
       * total<=spent before this would zero out a perfectly good live pool
       * on nearly every multi-FF save. Only fall through to the cumulative
       * interpretation once total can't possibly be a live pool on its own. */
      s_ff_bells_since_elect[n] = (uint16_t)total;
    } else {
      unsigned spent = 0u;
      for (unsigned c = 0u; c < count; ++c) {
        spent += ff_bells_threshold_at_elect_count(col1, n, c);
      }
      /* Linux cumulative minus thresholds consumed at past elects. */
      s_ff_bells_since_elect[n] = (total <= spent) ? 0 : (uint16_t)(total - spent);
    }
  }
}

void founding_fathers_sync_from_col1_after_load(const ColonizeCol1Save* col1) {
  founding_fathers_sync_from_col1(col1);
  if (!col1) {
    return;
  }
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    const ColonizeCol1Nation* nat = &col1->nation[n];
    const unsigned count = (unsigned)nat->founding_father_count;
    if (count == 0u) {
      continue;
    }
    if (nat->unknown21_pad != FF_POOL_STASH_MARKER) {
      /* Not one of our own stashed saves (fresh DOS import, or a save this
       * engine never wrote) — liberty_bells_last_turn is genuine EOT bell
       * production here, not our pool. Keep the total-derived estimate. */
      continue;
    }
    const unsigned need = founding_fathers_bells_needed(col1, n);
    const unsigned last = (unsigned)nat->liberty_bells_last_turn;
    /* Adopt a stashed 0 too (smell #84): right after an election the pool is
     * legitimately zero; the old `last > 0` guard rejected it and kept the
     * total-minus-thresholds estimate, refunding a large phantom pool on
     * reload (enough to fund an instant second election). */
    if (last <= need) {
      s_ff_bells_since_elect[n] = (uint16_t)last;
    }
  }
}

/*
 * Test helper: treat liberty_bells_total as the since-last-elect pool (legacy
 * unit-test convention). Live play uses turn accrual + sync_from_col1_after_load.
 */
void founding_fathers_test_force_pool_from_total(const ColonizeCol1Save* col1) {
  if (!col1) {
    return;
  }
  s_ff_pools_initialized = true;
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    s_ff_bells_since_elect[n] = col1->nation[n].liberty_bells_total;
  }
}

unsigned founding_fathers_bells_since_last_elect(int nation_id) {
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0u;
  }
  return (unsigned)s_ff_bells_since_elect[nation_id];
}

void founding_fathers_accrue_bells(int nation_id, unsigned delta) {
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT || delta == 0u) {
    return;
  }
  s_ff_pools_initialized = true;
  unsigned total = (unsigned)s_ff_bells_since_elect[nation_id] + delta;
  if (total > 65535u) {
    total = 65535u;
  }
  s_ff_bells_since_elect[nation_id] = (uint16_t)total;
}

static void founding_fathers_reset_bells_pool(int nation_id) {
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  s_ff_bells_since_elect[nation_id] = 0;
}

#define FF_BOLIVAR_SOL_BONUS 20
#define FF_LA_SALLE_STOCKADE_POP 3

unsigned founding_fathers_bells_needed(const ColonizeCol1Save* col1, int nation) {
  /*
   * FUN_4345_0982 — next liberty-bell threshold.
   * Human (control==0): base=(diff+3)*2; else AI: base=14-diff; then *8.
   * Year >1599/1649/1699/1749 each add +50%. Threshold (count+1)*base+1,
   * halved when count==0. WoI (0x5382&1): diff*0x5dc+2000.
   */
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 40u;
  }
  const unsigned elected_count = col1->nation[nation].founding_father_count;
  if (col1->head.game_options.woi) {
    const unsigned diff = (unsigned)col1->head.difficulty;
    return diff * 0x5dcu + 2000u;
  }
  const int human = (nation < 4 && col1->player[nation].control == 0);
  const unsigned diff = (unsigned)col1->head.difficulty;
  unsigned base = human ? (diff + 3u) * 2u : (14u - diff);
  base *= 8u;
  const unsigned year = (unsigned)col1->head.year;
  if (year > 0x63fu) {
    base += base >> 1;
  }
  if (year > 0x671u) {
    base += base >> 1;
  }
  if (year > 0x6a3u) {
    base += base >> 1;
  }
  if (year > 0x6d5u) {
    base += base >> 1;
  }
  unsigned need = (elected_count + 1u) * base + 1u;
  if (elected_count == 0u) {
    need >>= 1;
  }
  return need;
}

int founding_fathers_bolivar_sol_bonus(const ColonizeCol1Save* col1, int nation) {
  /* FUN_15eb_0274: FF 0x12 + owner < 4 + player[owner].control == 0 → +20. */
  if (!col1 || nation < 0 || nation >= 4) {
    return 0;
  }
  if (col1->player[nation].control != 0) {
    return 0;
  }
  if (!founding_fathers_nation_has(col1, nation, FF_SIMON_BOLIVAR)) {
    return 0;
  }
  return FF_BOLIVAR_SOL_BONUS;
}

/*
 * DOS ownership test = FUN_15eb_3960 (reached as FUN_281f_07b4):
 *
 *   if (idx < 0) return 1;
 *   if (nation > 3) return 0;
 *   return *(byte *)((idx >> 3) + nation * 0x13c + -0x77f1) & (1 << (idx & 7));
 *
 * i.e. the per-nation bitmask and nothing else — the exact inverse of
 * ff_available_to, and the gate behind all ~90 effect call sites. The
 * head.founding_father[] array (DS:0x53a9) is written once by FUN_4345_0342
 * (`if (entry < 0) entry = nation`), filled with -1 at init, and never read
 * back anywhere in the binary. Smell audit #83: reading head equality here
 * granted effects to a nation that only happened to be the first claimer
 * (and, with zero-filled heads from bugs 288, to nation 0 wholesale) while
 * ff_available_to still listed the father as electable — a second election
 * re-ran apply_effect (Jones's Frigate twice, Magellan re-bump, Coronado
 * re-reveal). reports.c:717 reached the same conclusion from dutch-reports.SAV.
 */
bool founding_fathers_nation_has(const ColonizeCol1Save* col1, int nation, int ff_index) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }
  if (ff_index < 0 || ff_index >= (int)COLONIZE_COL1_FF_COUNT) {
    return false;
  }
  const uint8_t byte = col1->nation[nation].founding_fathers[ff_index / 8];
  return (byte & (uint8_t)(1u << (ff_index % 8))) != 0;
}

bool founding_fathers_franklin_keeps_nw_peace(const ColonizeCol1Save* col1, int nation) {
  /*
   * docs/fandom_col1994.md Benjamin Franklin — NW peer peace gate.
   * Ownership via founding_fathers_nation_has (per-nation bitmask).
   */
  return founding_fathers_nation_has(col1, nation, FF_BENJAMIN_FRANKLIN);
}

bool founding_fathers_brebeuf_missionaries_are_experts(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Father Jean de Brebeuf — all missionaries function
   * as experts. Ownership via founding_fathers_nation_has (nation bitmask).
   * No elect crosses; ai_contact Jesuit-grade mid convert consumes the gate.
   */
  return founding_fathers_nation_has(col1, nation, FF_JEAN_DE_BREBEUF);
}

bool founding_fathers_sepulveda_convert_join_bonus(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md / PEDIA @FATHER23 — higher convert-join chance.
   * Wired: units_try_native_settlement_fallout FUN_5fef_31ea threshold +4.
   */
  return founding_fathers_nation_has(col1, nation, FF_JUAN_DE_SEPULVEDA);
}

bool founding_fathers_de_soto_lcr_always_positive(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Hernando de Soto; decomp FUN_65dd_0004 FF index 7.
   * Ownership gate; resolve via units_resolve_lcr_rumour (thin positive-only).
   */
  return founding_fathers_nation_has(col1, nation, FF_HERNANDO_DE_SOTO);
}

bool founding_fathers_de_witt_allows_foreign_colony_trade(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Jan de Witt — foreign-colony trade allowed.
   * Ownership gate; cargo via colonies_de_witt_transfer_* (no gold invent).
   */
  return founding_fathers_nation_has(col1, nation, FF_JAN_DE_WITT);
}

bool founding_fathers_cortes_guarantees_conquest_treasure(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Hernan Cortes — conquered native settlements always
   * yield more treasure. Ownership gate; amount via units_conquest_treasure_gold
   * (FUN_5fef_31ea peel) when fallout gold_amount<=0.
   */
  return founding_fathers_nation_has(col1, nation, FF_HERNAN_CORTES);
}

bool founding_fathers_cortes_free_king_galleon(const ColonizeCol1Save* col1, int nation) {
  /*
   * docs/fandom_col1994.md Hernan Cortes — king's galleons transport treasure
   * free. GAME.TXT @KINGGALLEON3: Crown share = current tax rate (already
   * europe_cash_treasure); "for no extra charge" — do NOT invent KINGGALLEON2
   * non-Cortes royal-galleon extra %.
   * KINGGALLEON2 resolved 2026-08-27: FUN_5fef_1908 builds "KINGGALLEON"+"2"/"3"
   * at runtime (DS 0x1bed/0x1bfb/0x1bf9); non-Cortes share =
   * max((difficulty+10)*5, 2*tax) cap 90 — see units_king_galleon_share_pct.
   * Human-only: `units_king_galleon_offer_coastal_treasures` (CHOICE,
   * DOS-shaped) is the sole caller — FUN_465b_0000 raw 75798 gates the whole
   * colony-arrival King-galleon/Cortes offer on the mover's nation being
   * human-controlled (`nation*0x34-0x543f == 0`), so AI never reaches it; an
   * AI-side "auto-cash via Cortes" stand-in (removed 2026-09-17,
   * units_cortes_cash_coastal_treasures) had no DOS counterpart and shorted
   * AI treasuries by the tax rate on coastal colonies for nothing — AI
   * treasure cash-in is the unconditional, untaxed FUN_521d_20e6 in-colony
   * band (units_ai_treasure_cash_in_colony).
   */
  return founding_fathers_nation_has(col1, nation, FF_HERNAN_CORTES);
}

bool founding_fathers_revere_should_auto_arm(
  const ColonizeCol1Save* col1,
  int nation,
  bool colony_has_soldier_defender,
  int muskets_stock
) {
  /* PEDIA: "When a colony with no standing soldiers is attacked, a colonist
   * automatically takes up any stockpiled muskets in defense of the colony."
   * Equip step matches colony arm path (UNITS_EQUIP_MUSKETS = 50). */
  if (!founding_fathers_nation_has(col1, nation, FF_PAUL_REVERE)) {
    return false;
  }
  if (colony_has_soldier_defender) {
    return false;
  }
  return muskets_stock >= UNITS_EQUIP_MUSKETS;
}

/*
 * There is deliberately no `founding_fathers_revere_auto_arm` any more.
 * DOS never ejects a real Soldier for Revere: FUN_5fef_1b0e's undefended-
 * colony arm (viceroy_unpacked.c 100417-100432) spawns the SAME phantom
 * defender it spawns without Revere, and the Father only overrides that
 * phantom's graphic (0x4b) and base combat (+1). The colonist stays at work,
 * the warehouse muskets are never debited (1b0e reads colony +0xb8 twice and
 * writes it never), and the phantom is deleted after the roll by
 * FUN_291f_0a06. See units_spawn_colony_temp_defender /
 * units_revere_defend_colony_tile in src/core/units.c — the gate above is
 * still the only piece of Revere that lives here.
 */

/*
 * Pocahontas elect: all native tension → content for this European nation.
 * DOS FUN_4345_0342 case 0x10 (raw 73093-73113) is TWO separate loops, not
 * one uniform zero:
 *   - 8 ColonizeCol1Indian records (0a42 selects DS:0x8d4e base): delta =
 *     -(alarm_by_player[nation]); when negative, routed through
 *     FUN_281f_0d6c -> FUN_4cc6_00f2 (ai_diplo_indian_alarm_delta) rather
 *     than stored directly, so the war/attack-confirmed bit clears and the
 *     tension-tier update runs exactly as any other alarm cooling would.
 *     (ai_diplo_indian_alarm_delta only halves POSITIVE deltas — see its own
 *     comment — so this negative reset always lands the field at exactly 0
 *     regardless of whether Pocahontas's own FF bit is already set, which it
 *     is here since elect_commit writes it before apply_effect runs.)
 *   - tribe_count ColonizeCol1Tribe records (0a4c selects DS:0x8d4a base,
 *     field +10 = alarm[nation]): unconditional hard zero, no delta call.
 */
static void effect_pocahontas_reset_alarm(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id > 3) {
    return;
  }
  for (int ind = 0; ind < 8; ++ind) {
    const int old_v = (int)col1->indian[ind].alarm_by_player[nation_id];
    if (old_v > 0) {
      /* ai_diplo_indian_alarm_delta's `indian_nation` is the DOS player-slot
       * id (4..11), not the 0..7 record index — ai_diplo_indian_slot
       * subtracts 4 back off. */
      ai_diplo_indian_alarm_delta(col1, ind + 4, nation_id, -old_v);
    }
  }
  if (col1->tribe) {
    for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
      col1->tribe[i].alarm[nation_id].friction = 0;
      col1->tribe[i].alarm[nation_id].attacks = 0;
    }
  }
}

/*
 * DOS FUN_0000_9810 (reached as FUN_281f_07b4): can this NATION still elect
 * Father idx? The only test is that nation's own +7..+0xa bitfield.
 *
 * bugs.md ("fewer than one Founding Father per category"): this used to read
 * head.founding_father[idx] < 0, i.e. "nobody anywhere has him". Fathers are
 * not globally exclusive in DOS — every power runs its own Congress, and
 * head.founding_father[] is only a first-claimer record (FUN_4345_0342 writes
 * it once, `if (entry < 0) entry = nation`, and nothing ever reads it back as
 * a gate; reports.c had already noticed the same thing against
 * dutch-reports.SAV). Reading it as a lock deleted a candidate from the human's
 * slate every time an AI elected someone, which is exactly how categories went
 * missing.
 */
static bool ff_available_to(const ColonizeCol1Save* col1, int nation, int idx) {
  if (!col1 || idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT) {
    return false;
  }
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }
  /* The one per-nation bitmask read (audit SC-4): "available" is exactly
   * "not already elected by this nation". */
  return !founding_fathers_nation_has(col1, nation, idx);
}

/* NAMES.TXT @FATHERS type column (0=Trade … 4=Religious). */
static const uint8_t k_ff_type[COLONIZE_COL1_FF_COUNT] = {
  0, 0, 0, 0, 0, /* Trade 0–4 */
  1, 1, 1, 1, 1, /* Exploration 5–9 */
  2, 2, 2, 2, 2, /* Military 10–14 (Cortes…JPJ) */
  3, 3, 3, 3, 3, /* Political 15–19 */
  4, 4, 4, 4, 4  /* Religious 20–24 */
};

/*
 * NAMES.TXT @FATHERS century weights (cols 3–5 after type).
 * Century band from FUN_4345_005a: 0 ≤1599, 1 1600–1699, 2 ≥1700.
 */
static const uint8_t k_ff_weight[COLONIZE_COL1_FF_COUNT][3] = {
  {2, 8, 6},
  {0, 5, 8},
  {9, 1, 0},
  {2, 4, 8},
  {2, 6, 10},
  {2, 10, 10},
  {3, 5, 7},
  {5, 10, 5},
  {10, 1, 0},
  {7, 5, 3},
  {6, 5, 1},
  {0, 4, 10},
  {10, 2, 1},
  {4, 8, 6},
  {0, 6, 7},
  {4, 5, 6},
  {7, 5, 3},
  {1, 2, 8},
  {0, 4, 6},
  {5, 5, 5},
  {7, 4, 1},
  {8, 5, 2},
  {6, 6, 1},
  {3, 8, 3},
  {0, 5, 10}
};

/* FUN_4345_005a: century band for @FATHERS weight column. */
static int ff_century_band(const ColonizeCol1Save* col1) {
  const unsigned year = col1 ? (unsigned)col1->head.year : 1600u;
  if (year <= 1599u) {
    return 0;
  }
  if (year <= 1699u) {
    return 1;
  }
  return 2;
}

/*
 * This century's @FATHERS weight for candidate `i`, or 0 when it is not an
 * eligible pick for `nation` at all (wrong category, or already elected).
 * The single predicate FUN_4345_0080 and FUN_4345_06d2 share (audit SC-43,
 * where it was spelled out four times).
 */
static int ff_eligible_weight(
  const ColonizeCol1Save* col1, int nation, int i, int type, int band
) {
  if ((int)k_ff_type[i] != type || !ff_available_to(col1, nation, i)) {
    return 0;
  }
  return (int)k_ff_weight[i][band];
}

/* FUN_4345_0080: count unclaimed FFs of type with non-zero weight this century. */
static int ff_count_eligible_of_type(const ColonizeCol1Save* col1, int nation, int type) {
  const int band = ff_century_band(col1);
  int count = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if (ff_eligible_weight(col1, nation, i, type, band) > 0) {
      count++;
    }
  }
  return count;
}

/*
 * FUN_4345_06d2 weighted pick within one @FATHERS category (RNG(1, sum)).
 * Returns FF index or -1 when none eligible.
 */
static int ff_pick_weighted_of_type(
  const ColonizeCol1Save* col1,
  int nation,
  int type,
  ColonizeDosRng* rng
) {
  const int band = ff_century_band(col1);
  int total = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    total += ff_eligible_weight(col1, nation, i, type, band);
  }
  if (total <= 0) {
    return -1;
  }
  if (!rng) {
    /* Deterministic fallback when no RNG context (should not happen in tick). */
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      if (ff_eligible_weight(col1, nation, i, type, band) > 0) {
        return i;
      }
    }
    return -1;
  }
  int roll = dos_rng_range(rng, 1, total);
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    const int w = ff_eligible_weight(col1, nation, i, type, band);
    if (w <= 0) {
      continue;
    }
    roll -= w;
    if (roll < 1) {
      return i;
    }
  }
  return -1;
}

/* FUN_4345_015a: @FATHERS category with the most eligible unclaimed Fathers. */
static int ff_pick_strongest_category(const ColonizeCol1Save* col1, int nation) {
  int best_type = -1;
  int best_count = -1;
  for (int type = 0; type < 5; ++type) {
    const int n = ff_count_eligible_of_type(col1, nation, type);
    if (n > best_count) {
      best_count = n;
      best_type = type;
    }
  }
  return best_type;
}

void founding_fathers_consume_woi_bell_pool(int nation_id) {
  founding_fathers_reset_bells_pool(nation_id);
}

/*
 * FUN_4345_0a22 phase 3 (thin): status while WoI bell pool grows toward
 * intervention threshold. VGA-identical chrome remains PARKED.
 */
void founding_fathers_woi_intervention_chrome(
  ColonizeTurnContext* ctx,
  int nation_id,
  unsigned pool,
  unsigned needed
) {
  if (!ctx || nation_id != ctx->human_nation) {
    return;
  }
  if (pool == 0u || pool >= needed) {
    return;
  }
  if (ctx->status && ctx->status_size > 0 && pool >= needed / 2u) {
    snprintf(
      ctx->status,
      ctx->status_size,
      "Liberty bells rally foreign intervention (%u/%u).",
      pool,
      needed
    );
  }
}

static int ff_debate_pending(const AiPopupState* p) {
  if (!p) {
    return 0;
  }
  if (p->open && p->current.tag == AI_POPUP_TAG_FF_CONGRESS &&
      p->current.kind == AI_POPUP_KIND_CHOICE) {
    return 1;
  }
  for (int i = 0; i < p->queue_count; ++i) {
    if (p->queue[i].tag == AI_POPUP_TAG_FF_CONGRESS &&
        p->queue[i].kind == AI_POPUP_KIND_CHOICE) {
      return 1;
    }
  }
  return 0;
}

/*
 * DOS Congress-debate option row (FUN_4345_06d2, OVL07_L0040 0xc72..0xcfb —
 * Ghidra drops the pushed string pointers, ndisasm recovers them):
 *
 *   buf[0] = 0
 *   281f_016e(buf, DS:0x9652 + ff*6)   @FATHERS name
 *   281f_0178(buf)                     strcat DS:0x50 = " "
 *   281f_011e(buf)                     strcat DS:0x5e = "("
 *   281f_016e(buf, DS:0x96e8 + type*2) @FOUNDING category word
 *   281f_0178(buf)                     " "
 *   281f_016e(buf, DS:0x2e88)          @MISC 103 = "Adviser"
 *   281f_0128(buf)                     strcat DS:0x60 = ")"
 *
 * i.e. "Adam Smith (Trade Adviser)". The DS word at 0x2e88 is @MISC index
 * (0x2e88 − 0x2dba)/2 = 103 under the pointer base in docs/popup_tag_ids.md.
 * Each live lookup returns reports.c's shared scratch buffer, so the parts
 * are copied out before they are composed.
 */
static void ff_debate_row_label(int idx, char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT) {
    out[0] = '\0';
    return;
  }
  char name[48];
  char category[32];
  char adviser[32];
  snprintf(name, sizeof(name), "%s", reports_ff_display_name(idx));
  snprintf(category, sizeof(category), "%s", reports_ff_category_display_name(k_ff_type[idx]));
  snprintf(adviser, sizeof(adviser), "%s", reports_misc_display_word(103, ""));
  if (category[0] == '\0') {
    snprintf(out, out_size, "%s", name);
    return;
  }
  snprintf(out, out_size, "%s (%s %s)", name, category, adviser);
}

/* Enqueue the Congress debate CHOICE from the stored slate (dropping any
 * candidate elected elsewhere since). False when no usable slate remains. */
static bool ff_enqueue_debate_from_slate(
  ColonizeTurnContext* ctx,
  AiPopupState* popups,
  int nation_id
) {
  if (!ctx || !ctx->col1 || !popups || s_ff_debate_slate_nation != nation_id) {
    return false;
  }
  char labels[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const char* choice_ptrs[AI_POPUP_CHOICE_MAX];
  int ids[AI_POPUP_CHOICE_MAX];
  int n = 0;
  for (int i = 0; i < s_ff_debate_slate_n && n < AI_POPUP_CHOICE_MAX; ++i) {
    const int idx = s_ff_debate_slate[i];
    if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT ||
        !ff_available_to(ctx->col1, nation_id, idx)) {
      continue;
    }
    ff_debate_row_label(idx, labels[n], sizeof(labels[n]));
    choice_ptrs[n] = labels[n];
    ids[n] = idx;
    n++;
  }
  if (n < 1) {
    s_ff_debate_slate_n = 0;
    s_ff_debate_slate_nation = -1;
    return false;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    ctx->messages,
    "WHICHFREEDOM",
    NULL,
    /* GAME.TXT @WHICHFREEDOM verbatim (fallback only — popup_msg_fill uses
     * the live section, including its @width=190, whenever assets load). */
    "",
    body,
    sizeof(body)
  );
  return ai_popup_enqueue_choice_ctx(
    popups, AI_POPUP_TAG_FF_CONGRESS, nation_id, -1, 1, NULL, body, choice_ptrs, ids, n
  );
}

static int pick_candidate(ColonizeTurnContext* ctx, const ColonizeCol1Save* col1,
                          const ColonizeCol1Nation* nat, int nation_id) {
  const int next = (int)nat->next_founding_father;
  if (next >= 0 && next < (int)COLONIZE_COL1_FF_COUNT &&
      ff_available_to(col1, nation_id, next)) {
    return next;
  }
  const int type = ff_pick_strongest_category(col1, nation_id);
  if (type < 0) {
    return -1;
  }
  return ff_pick_weighted_of_type(col1, nation_id, type, ctx ? ctx->rng : NULL);
}

static int16_t advance_next_candidate(const ColonizeCol1Save* col1, int elected_idx) {
  (void)col1;
  (void)elected_idx;
  /* DOS FUN_4345_0342: after elect, next_founding_father = -1 (re-debate). */
  return -1;
}

/*
 * DOS FUN_4345_0a22 / 06d2: when next_founding_father < 0, open Congress debate
 * (one unclaimed candidate per @FATHERS type) or AI auto-pick into next.
 * Does not elect — elect waits until bells >= threshold with next locked in.
 */
static void ensure_next_candidate(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  /* FUN_4345_0a22 phase 2: Congress nominate only in peacetime. */
  if (col1->head.game_options.woi) {
    return;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  const int next = (int)nat->next_founding_father;
  if (next >= 0 && next < (int)COLONIZE_COL1_FF_COUNT &&
      ff_available_to(col1, nation_id, next)) {
    return;
  }
  nat->next_founding_father = -1;

  if (nation_id == ctx->human_nation && ctx->ai_popups) {
    if (ff_debate_pending(ctx->ai_popups)) {
      return;
    }
    /* A slate already rolled (and escaped, or carried over) re-presents
     * verbatim — cancelling or waiting a turn must not reroll it. */
    if (ff_enqueue_debate_from_slate(ctx, ctx->ai_popups, nation_id)) {
      if (ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "Congress debates founding fathers.");
      }
      return;
    }
    int ids[AI_POPUP_CHOICE_MAX];
    int n = 0;
    for (int type = 0; type < 5 && n < AI_POPUP_CHOICE_MAX; ++type) {
      const int idx = ff_pick_weighted_of_type(col1, nation_id, type, ctx->rng);
      if (idx < 0) {
        continue;
      }
      ids[n] = idx;
      n++;
    }
    /* DOS FUN_4345_06d2 opens the debate whenever ANY category still has a
     * candidate (`local_4 != 0`) — a one-row Congress is a real DOS state, not
     * a reason to skip the ask and pick for the player. */
    if (n >= 1) {
      memcpy(s_ff_debate_slate, ids, sizeof(ids[0]) * (size_t)n);
      s_ff_debate_slate_n = n;
      s_ff_debate_slate_nation = nation_id;
      if (ff_enqueue_debate_from_slate(ctx, ctx->ai_popups, nation_id) &&
          ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "Congress debates founding fathers.");
      }
    }
    return;
  }

  const int idx = pick_candidate(ctx, col1, nat, nation_id);
  if (idx >= 0) {
    nat->next_founding_father = (int16_t)idx;
  }
}

/*
 * Coronado: DOS FUN_4345_0342 case `param_2 == 6` (viceroy_unpacked.c
 * 73155-73159) sweeps EVERY colony on the board — the loop runs over
 * 0..colony_count (DS:0x539e) with no owner test — and calls FUN_13f1_00a6
 * (viceroy_unpacked.c 7096), the ±5 square reveal that also seeds
 * pop_on_map=1 / fort_on_map=0 on colonies inside it. Delegated to
 * colonies_reveal_all_for_nation so both call sites share the one sweep.
 */
static void effect_coronado_reveal(
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  int nation_id
) {
  if (!map || !map->seen || !colonies) {
    return;
  }
  colonies_reveal_all_for_nation(map, colonies, nation_id);
}

/* La Salle: Stockade when colony population >= 3 (wiki / manual). */
static int effect_la_salle_stockades(ColonizeColonyPool* colonies, int nation_id) {
  if (!colonies) {
    return 0;
  }
  const int stock_idx = colonies_building_row(colonies, COLONY_BUILDING_STOCKADE);
  if (stock_idx < 0 || stock_idx >= COLONIZE_BUILDING_TYPES_MAX) {
    return 0;
  }
  int touched = 0;
  /* Pool bound (colonies_abandon leaves holes and shrinks colony_count). */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* col = &colonies->colonies[i];
    if (!col->active || col->nation_id != nation_id) {
      continue;
    }
    if (col->population < FF_LA_SALLE_STOCKADE_POP) {
      continue;
    }
    if (!col->has_building[stock_idx]) {
      col->has_building[stock_idx] = true;
      touched++;
    }
  }
  return touched;
}

/*
 * Brebeuf elect: DOS FUN_4345_0342 case 0x16 (raw 73113-73121) sweeps every
 * village record (0a4c/DS:0x8d4a, tribe_count) and, where mission byte +5 is
 * present (>=0, i.e. not COL1_TRIBE_MISSION_NONE) and its low nibble equals
 * this nation, ORs in bit 0x10 (Jesuit-grade). Existing missions upgrade in
 * place at election; the ongoing per-establish gate
 * (founding_fathers_brebeuf_missionaries_are_experts, consumed by
 * ai_contact_is_jesuit_grade) is separately real — confirmed at the mission-
 * establish site itself, viceroy_overlays.c raw ~75863-75867:
 * `if (profession==0x18 (Jesuit) || FUN_1000_89a4(nation, 0x16) != 0)` sets
 * the +5 bit0x10, i.e. DOS tests FF 0x16 (Brebeuf) right there. Both sites
 * are real and independent; neither substitutes for the other.
 */
static void effect_brebeuf_grade_missions(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || !col1->tribe || nation_id < 0 || nation_id > 3) {
    return;
  }
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    ColonizeCol1Tribe* t = &col1->tribe[i];
    if ((int8_t)t->mission < 0) {
      continue;
    }
    if ((int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK) == nation_id) {
      t->mission |= COL1_TRIBE_MISSION_JESUIT_BIT;
    }
  }
}

/*
 * Public immediate-effect hook: call right when a colony's population
 * changes (join/admit/birth), not just once per turn tick. PEDIA says
 * "gives... a stockade when the population of the colony reaches 3" —
 * from the player's seat that reads as instant, not "next turn," so this
 * runs the same sweep `founding_fathers_tick` uses but on demand. Cheap
 * (single ownership check + a pass over one nation's colonies).
 */
int founding_fathers_la_salle_check(
  ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id
) {
  if (!col1 || !founding_fathers_nation_has(col1, nation_id, FF_SIEUR_DE_LA_SALLE)) {
    return 0;
  }
  return effect_la_salle_stockades(colonies, nation_id);
}

/* Bolivar: SoL +20% is display-time via founding_fathers_bolivar_sol_bonus
 * (FUN_15eb_0274). Elect records FF only — no rebel_dividend mutation. */

/*
 * Brewster: no Petty Criminals / Indentured Servants in Europe recruit pool
 * or dock. (Pick-among-pool arrival lives in europe.c / units.c.)
 */
/*
 * DOS FUN_4345_0342 case 0x14 (raw 73160-73168): nation+2..+4 (recruit[3])
 * bytes 0x19 (Indentured Servant) / 0x1a (Petty Criminal) -> 0x1c, for
 * WHICHEVER nation elects Brewster -- no human gate. Keep the save-side
 * recruit[] live for every nation; the human EuropeScreen mirror is a
 * separate write (effect_brewster_filter_pool below).
 */
static void effect_brewster_filter_recruit(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  for (int i = 0; i < 3; ++i) {
    if (nat->recruit[i] == 0x19 || nat->recruit[i] == 0x1a) {
      nat->recruit[i] = 0x1c;
    }
  }
}

static void effect_brewster_filter_pool(EuropeScreen* europe) {
  if (!europe) {
    return;
  }
  /* Pool substitution shared with the immigration tick (DOS 4345_0342 case
   * 0x14: servant/criminal slot bytes overwritten with 0x1c in place). */
  europe_apply_brewster(europe, 1);
  /* bugs.md #728: FUN_4345_0342 case 0x14 (raw 73160-73168) rewrites ONLY the
   * three recruit slots at nation*0x13c-0x77f6 (0x19/0x1a -> 0x1c). It does
   * not walk the docks, so colonists who already emigrated keep being
   * Indentured Servants / Petty Criminals. The dock sweep that used to sit
   * here was invented. */
}

/*
 * Las Casas: existing Indian converts → free colonists (profession only).
 * Cite: COLONIZE/PEDIA.TXT @FATHER24; docs/fandom_col1994.md Religious table.
 * Representation: NAMES @JOB Convert (27) / Free Colonists (19) — no separate
 * @UNIT; map units use Colonists type + convert profession.
 */
static int effect_las_casas_assimilate(
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int nation_id
) {
  int touched = 0;
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }

  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* col = &colonies->colonies[i];
      if (!col->active || col->nation_id != nation_id) {
        continue;
      }
      for (int c = 0; c < col->colonist_count; ++c) {
        ColonizeColonist* person = &col->colonists[c];
        if (!person->active) {
          continue;
        }
        if (person->profession == COLONIZE_PROF_CONVERT) {
          person->profession = COLONIZE_PROF_FREE_COLONIST;
          touched++;
        }
      }
    }
  }

  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      /*
       * DOS FUN_4345_0342 case 0x18 map-unit half (raw 73123-73125) gates on
       * THREE tests together: owner nibble (+0x3147 & 0xf) == nation,
       * unit TYPE byte (+0x3146) == 0 (the base "Colonists" @UNIT code —
       * an actual Indian Convert on the map is still type 0, just with
       * profession 0x1b; a Soldier/Dragoon/etc carrying profession 0x1b as
       * a former-Convert veteran is NOT touched), and profession (+0x315b)
       * == 0x1b. Without the type==0 gate this loop reassigned profession
       * on any unit kind that happened to carry job 27.
       */
      const ColonizeUnitType* ut = units_type(units, u->type_index);
      bool changed = false;
      if (u->profession == COLONIZE_PROF_CONVERT && units_type_is_colonist(ut)) {
        u->profession = COLONIZE_PROF_FREE_COLONIST;
        changed = true;
      }
      if (changed) {
        touched++;
      }
    }
  }
  return touched;
}

static bool ff_find_coastal_water(
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int nation_id,
  int* out_x,
  int* out_y
) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  if (!map || !out_x || !out_y) {
    return false;
  }

  if (colonies) {
    /* Pool bound (colonies_abandon leaves holes and shrinks colony_count). */
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* col = &colonies->colonies[i];
      if (!col->active || col->nation_id != nation_id) {
        continue;
      }
      if (!map_tile_is_coastal(map, col->x, col->y)) {
        continue;
      }
      for (int d = 0; d < 8; ++d) {
        const int nx = col->x + dx[d];
        const int ny = col->y + dy[d];
        if (map_tile_is_water(map, nx, ny)) {
          *out_x = nx;
          *out_y = ny;
          return true;
        }
      }
    }
  }

  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id || !units_is_on_map(u)) {
        continue;
      }
      if (!units_is_sea(units, u->id)) {
        continue;
      }
      *out_x = u->x;
      *out_y = u->y;
      return true;
    }
  }
  return false;
}

/*
 * John Paul Jones: free Frigate (Man-O-War fallback). Returns true on spawn.
 *
 * bugs.md #467 — DOS FUN_4345_0342 case 0xe (raw 73044ff):
 *   FUN_281f_095c(0x11, nation, nation - 0x18, nation - 0x18)  spawn a
 *   Frigate at the nation's OFF-MAP EUROPE slot; then order (+0x314c) = 0,
 *   goto (+0x314d/e) = the nation's stored Europe landfall tile
 *   (nation*0x13c - 0x77c6/-0x77c5), counter16 (+0x315a) = 0.
 * So the ship starts docked in Europe and sails over like any purchase —
 * it never materialises beside a colony. For the human that is the Europe
 * harbor; an AI nation keeps the coastal-water stand-in (the port has no
 * AI Europe model).
 */
static bool effect_jones_frigate(
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  EuropeScreen* europe,
  int nation_id
) {
  if (!units || !map) {
    return false;
  }
  int ship_ty = units_kind_type_index(units, UNITS_KIND_FRIGATE);
  if (ship_ty < 0) {
    ship_ty = units_kind_type_index(units, UNITS_KIND_MAN_O_WAR);
  }
  if (ship_ty < 0) {
    return false;
  }
  if (europe) {
    const ColonizeUnitType* ty = units_type(units, ship_ty);
    return europe_harbor_push(
      europe, ship_ty, ty && ty->name[0] ? ty->name : "", NULL, 0, NULL, NULL
    );
  }
  int sx = 0;
  int sy = 0;
  if (!ff_find_coastal_water(map, colonies, units, nation_id, &sx, &sy)) {
    return false;
  }
  const int sid = units_spawn_allow_stack(units, ship_ty, sx, sy);
  if (sid < 0) {
    return false;
  }
  ColonizeUnit* ship = units_get(units, sid);
  if (ship) {
    units_set_nation(ship, nation_id);
  }
  return true;
}

/*
 * Apply manual/wiki FF effect. Prefer real gates/hooks; PARK when missing —
 * never invent gold/crosses/tools as a substitute power.
 */
static void apply_effect(
  ColonizeTurnContext* ctx,
  ColonizeCol1Save* col1,
  ColonizeCol1Nation* nat,
  EuropeScreen* europe,
  int nation_id,
  int human_nation,
  int ff_index
) {
  ColonizeWorldMap* map = ctx ? ctx->map : NULL;
  ColonizeColonyPool* colonies = ctx ? ctx->colonies : NULL;
  ColonizeUnitPool* units = ctx ? ctx->units : NULL;
  (void)nat;

  switch (ff_index) {
    case FF_ADAM_SMITH:
      /* Manual/wiki: unlock factory-tier + 1.5× factory throughput.
       * Gate already via game_nation_has_ff → ColoniesBuildableOpts.has_adam_smith;
       * factory 6→9 in colony_production. No elect treasury fiction. */
      break;
    case FF_JAKOB_FUGGER:
      /*
       * Manual/wiki: clear all Europe boycotts (no back taxes).
       *
       * nation+0x20 is the authoritative word every trade path now reads
       * (europe_cargo_boycotted_ex, smell audit G6); europe->boycott_bitmap is
       * only the Europe-screen render mirror, so drop it too for the human —
       * same pair europe_buyback_boycott clears, and it keeps the market strip
       * from painting red rows for one frame after the elect.
       */
      nat->boycott_bitmap = 0;
      if (nation_id == human_nation) {
        if (europe) {
          europe->boycott_bitmap = 0;
        }
        /* No DOS write ties the king's tax-refuse latch (AI_KING_BOYCOTT_BYTE,
         * a Linux-only bit) to Fugger's elect case — FUN_4345_0342 case 1
         * (raw 73079-73081) only ever zeroes nation+0x20. The latch
         * self-clears next king tick via ai_king_sync_boycott_refuse once it
         * observes boycott_bitmap==0 (ai_king.c), so an explicit clear here
         * would only race that, not port real behaviour. */
      }
      break;
    case FF_PETER_MINUIT:
      /* Manual/wiki: Indians no longer demand payment for land.
       * Decomp FUN_4cc6_07c2 zeros land-buy gold when FF 2 owned.
       * Wired via founding_fathers_nation_has → colonies_indian_land_purchase_gold
       * / colonies_found_with_indian_land (**Done**). */
      break;
    case FF_PETER_STUYVESANT:
      /* Manual/wiki: unlock Custom House — gated via has_peter_stuyvesant.
       * Auto-sell: europe_custom_house_autosell from turn_produce (FUN_364b_0688
       * stock>99 leave 50; FUN_364b_0636 denylist). Per-cargo UI PARKED. */
      break;
    case FF_JAN_DE_WITT:
      /* docs/fandom_col1994.md: trade with foreign colonies; FA more revealing.
       * Ownership gate: founding_fathers_de_witt_allows_foreign_colony_trade.
       * FA detailed strength gate: reports.c:3184 tests the per-nation bitmask
     * via reports_ff_owned_by_nation(&nation[human], DE_WITT) (or the
     * show-entire-map cheat) — head.founding_father[] is the first-claimer
     * stamp only, never an ownership read (audit #82/#83 unification).
       * Cargo: colonies_de_witt_transfer_* + ai_euro de Witt wagon/ship act
       * (stock only; no gold invent). */
      break;
    case FF_FERDINAND_MAGELLAN:
      /* Manual/wiki: all naval vessels +1 movement (permanent). DOS
       * FUN_4345_0342 (raw 73044-73160) has no `param_2 == 5` case — electing
       * Magellan bumps nothing by itself. The ongoing per-turn refresh
       * (turn_refresh_moves_for_nation, turn.c) is the only real site; the
       * one-shot elect-time bump here was a fandom-sourced invention and is
       * removed (matches the Franklin/de Soto pattern above). */
      break;
    case FF_FRANCISCO_CORONADO:
      /* Manual/wiki: "all existing colonies and the area around them become
       * visible" — DOS FUN_4345_0342 case 6 (viceroy_unpacked.c 73155-73159):
       * ±5 sweep (FUN_13f1_00a6) around EVERY colony, foreign ones included. */
      effect_coronado_reveal(map, colonies, nation_id);
      break;
    case FF_HERNANDO_DE_SOTO:
      /* PEDIA @FATHER7: LCR always positive + extended sight. Both halves are
       * ONGOING, not elect-time: FUN_4345_0342 (viceroy_unpacked.c
       * 73044-73160) has no `param_2 == 7` case at all, so electing de Soto
       * reveals nothing by itself.
       *   Sight: FUN_13f1_02f8 (viceroy_unpacked.c, units_sight_radius) gives
       *   every non-ship unit radius 2 while owned — each subsequent unit
       *   reveal picks it up (13f1:0321).
       *   LCR: units_resolve_lcr_rumour ← FUN_65dd_0004 (full port, reroll
       *   loop at 65dd:00a6 on FF bit 7). */
      break;
    case FF_HENRY_HUDSON:
      /* Manual/wiki: fur trapper output +100% — applied in turn harvest
       * when founding_fathers_nation_has(..., FF_HENRY_HUDSON). */
      break;
    case FF_SIEUR_DE_LA_SALLE:
      /* Manual/wiki: Stockade at population >= 3 (existing + future via elect). */
      (void)effect_la_salle_stockades(colonies, nation_id);
      break;
    case FF_HERNAN_CORTES:
      /* API ready (docs/fandom_col1994.md Hernan Cortes; Colonization.pdf FF):
       * founding_fathers_cortes_guarantees_conquest_treasure +
       * units_conquest_treasure_gold (FUN_5fef_31ea peel) +
       * units_spawn_treasure_train; free king-galleon via
       * founding_fathers_cortes_free_king_galleon. Fallout wired from
       * units_resolve_land_combat_ff when fallout context set. rich_capital
       * (-0xcc) ← tribe.state.capital. */
      break;
    case FF_GEORGE_WASHINGTON:
      /* PEDIA/wiki: non-veteran soldiers/dragoons who win combat always upgrade.
       * Ownership bit; promote-on-win in units_resolve_land_combat_ff.
       * AI/contact path: units_resolve_land_combat passes g_units_ff_col1
       * (units_set_ff_col1 from turn_refresh_moves_for_nation). */
      break;
    case FF_PAUL_REVERE:
      /* PEDIA/wiki: colony with no soldiers auto-arms from musket stock when
       * attacked. Ownership bit only; the gate is
       * founding_fathers_revere_should_auto_arm and it merely upgrades the
       * phantom militia defender units_try_move already spawns when FF col1
       * context is set (turn_refresh_moves_for_nation → units_set_ff_col1).
       * The stock is a threshold, not a cost — DOS never spends it. */
      break;
    case FF_FRANCIS_DRAKE:
      /* PEDIA/wiki: Privateer combat strength +50%.
       * Ownership bit; multiplier in units_resolve_naval_combat_ff (*3/2).
       * AI/king path: units_resolve_naval_combat passes g_units_ff_col1
       * (units_set_ff_col1 from turn_refresh_moves_for_nation). */
      break;
    case FF_JOHN_PAUL_JONES:
      /* Manual/wiki: free Frigate. No gold fallback. */
      (void)effect_jones_frigate(map, colonies, units, europe, nation_id);
      break;
    case FF_THOMAS_JEFFERSON:
      /* Wiki: liberty bell production of statesmen +50%.
       * Ownership bit; applied in colony_prod_colony_bells_ff via turn nation ticks. */
      break;
    case FF_POCAHONTAS:
      /* Wiki/fandom: all native tension → content; Indian alarm half as fast.
       * Elect: zero this nation's tribe friction/attacks + alarm_by_player.
       * Half-rate ongoing growth: ai_diplo_indian_alarm_delta, which halves
       * inside the delta exactly as DOS FUN_4cc6_00f2 does (viceroy
       * 80844-80850). (The old pre-call ai_contact_alarm_bump_amount helper
       * went with its last caller, smell #72.) */
      effect_pocahontas_reset_alarm(col1, nation_id);
      break;
    case FF_THOMAS_PAINE:
      /* Wiki: liberty bell production in all colonies + current tax rate %.
       * Ownership bit; applied in colony_prod_colony_bells_ff via turn nation ticks. */
      break;
    case FF_SIMON_BOLIVAR:
      /* FUN_15eb_0274: SoL +20% on every read while owned (human) — display-
       * time via founding_fathers_bolivar_sol_bonus, no storage bump there.
       * DOS FUN_4345_0342 case 0x12 (raw 73101-73108) is a SEPARATE elect
       * write: nation<4 && human-controlled (nation*0x34+0x543f)=='\0' ->
       * rebel_sentiment_report (DS:0x53d0) += 20, clamp <=100. Both are real
       * and additive (this is a global report field, not per-nation SoL). */
      if (col1 && nation_id >= 0 && nation_id < 4 &&
          col1->player[nation_id].control == 0) {
        int v = (int)col1->head.rebel_sentiment_report + 20;
        if (v > 100) {
          v = 100;
        }
        col1->head.rebel_sentiment_report = (int16_t)v;
      }
      break;
    case FF_BENJAMIN_FRANKLIN:
      /* docs/fandom_col1994.md: king's European wars no longer affect NW
       * relations; Europeans in the New World always offer peace.
       * DOS FUN_4345_0342 (raw 73044-73160) has no `param_2 == 19` case at
       * all — the elect-time make_peace sweep was a fandom-sourced
       * invention and is removed. Ongoing: ownership gate via
       * founding_fathers_franklin_keeps_nw_peace → ai_diplo declare /
       * euro_balance / war-hit (no gold fiction). FA 3f41 UI PARKED. */
      break;
    case FF_WILLIAM_BREWSTER:
      /* PEDIA @FATHER20: no criminals/servants on docks + recruit pool.
       * DOS FUN_4345_0342 case 0x14 (raw 73160-73168) rewrites the
       * ELECTING nation's own recruit[3] bytes (0x19/0x1a -> 0x1c) with no
       * human gate — real for AI electors too. effect_brewster_filter_recruit
       * ports that for any nation_id. The human EuropeScreen (dock/pool
       * mirror + @RECRUITCHOOSE pick, europe_tick_immigration_pressure()==2
       * + units_brewster_enqueue_pick) is a separate, human-only UI model —
       * no per-nation EuropeScreen exists (docs/architecture.md) — so that
       * mirror still applies only when nation_id == human_nation. */
      effect_brewster_filter_recruit(col1, nation_id);
      if (nation_id == human_nation) {
        effect_brewster_filter_pool(europe);
      }
      break;
    case FF_WILLIAM_PENN:
      /* Wiki: cross production in all colonies +50%.
       * Ownership bit; applied in colony_prod_colony_crosses_ff via turn nation ticks. */
      break;
    case FF_JEAN_DE_BREBEUF:
      /* docs/fandom_col1994.md: all missionaries function as experts.
       * DOS FUN_4345_0342 case 0x16: elect-time sweep upgrades every
       * existing mission of this nation to Jesuit-grade (bit 0x10) —
       * effect_brebeuf_grade_missions. Ongoing gate (real, confirmed at the
       * mission-establish site, see that function's comment):
       * founding_fathers_brebeuf_missionaries_are_experts → ai_contact
       * Jesuit-grade mid convert for plain Missionary. */
      effect_brebeuf_grade_missions(col1, nation_id);
      break;
    case FF_JUAN_DE_SEPULVEDA:
      /* PEDIA @FATHER23 / fandom: higher chance subjugated Indians convert/join.
       * Ownership gate + FUN_5fef_31ea peel in units_try_native_settlement_fallout
       * (threshold +4). Missionary convert pulse is a different path. */
      break;
    case FF_BARTOLOME_DE_LAS_CASAS:
      /* PEDIA @FATHER24 / fandom_col1994.md: existing Indian converts
       * assimilate as free colonists. Elect: profession Convert→Free
       * Colonist on owned colony colonists + map units. Ownership tick
       * in founding_fathers_tick re-runs for late converts. No gold/crosses. */
      (void)effect_las_casas_assimilate(colonies, units, nation_id);
      break;
    default:
      break;
  }
}

/* Returns true if a founding father was elected for this nation. */
static bool elect_commit(
  ColonizeTurnContext* ctx,
  int nation_id,
  int idx
) {
  if (!ctx || !ctx->col1 || idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT) {
    return false;
  }
  if (!ff_available_to(ctx->col1, nation_id, idx)) {
    return false;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  /* Write-once first-claimer record: DOS FUN_4345_0342 does `if (entry < 0)
   * entry = nation` (verified vs dutch-reports.SAV); smell audit #82. The
   * per-nation bitmask below is the real ownership. */
  if (col1->head.founding_father[idx] < 0) {
    col1->head.founding_father[idx] = (int8_t)nation_id;
  }
  nat->founding_fathers[idx / 8] |= (uint8_t)(1u << (idx % 8));
  if (nat->founding_father_count < 65535u) {
    nat->founding_father_count++;
  }
  nat->next_founding_father = advance_next_candidate(col1, idx);

  EuropeScreen* europe = (nation_id == ctx->human_nation) ? ctx->europe : NULL;
  apply_effect(ctx, col1, nat, europe, nation_id, ctx->human_nation, idx);

  if (ctx->status && ctx->status_size > 0 && nation_id == ctx->human_nation) {
    snprintf(ctx->status, ctx->status_size, "Founding Father elected (#%d)", idx);
  }
  diag_info(
    "FF nation %d elected #%d %s (count=%u)",
    nation_id, idx, reports_ff_display_name(idx), (unsigned)nat->founding_father_count
  );
  founding_fathers_reset_bells_pool(nation_id);

  if (ctx->ai_popups && nation_id == ctx->human_nation) {
    char body[AI_POPUP_BODY_LEN];
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    /* Audit SC-3: the player-visible @FREEDOM popup used the hardcoded
     * English name while every other FF string went through NAMES.TXT.
     * Copied out: reports_ff_display_name hands back a shared scratch
     * buffer, and `tok` outlives this statement. */
    char ff_name[48];
    snprintf(ff_name, sizeof(ff_name), "%s", reports_ff_display_name(idx));
    tok.string0 = ff_name;
    tok.string1 = "The";
    popup_msg_fill(
      ctx->messages,
      "FREEDOM",
      &tok,
      "",
      body,
      sizeof(body)
    );
    /* nation_b carries the elected FF index so the shell can chain the
     * Continental Congress page 2 + Colonizopedia entry after the OK
     * (game_apply_ai_popup_result). payload -1 = announce, not debate. */
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_FF_CONGRESS,
      nation_id,
      idx,
      -1, /* payload -1: announce OK, not debate apply */
      NULL,
      body
    );
  }
  return true;
}

/* Returns true if a founding father was elected. */
static bool try_elect_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }

  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Nation* nat = &col1->nation[nation_id];

  /*
   * FUN_4345_0a22 order: after any liberty bells exist, ensure a locked-in
   * candidate (debate if next < 0), then elect only when bells >= threshold
   * and next >= 0. Wiki: choice after first bells, then accumulate to join.
   */
  const unsigned pool = founding_fathers_bells_since_last_elect(nation_id);
  /* liberty_bells_last_turn is only genuine EOT bell production while the
   * save is NOT carrying our own pool stash in it (smell audit #85/#94):
   * founding_fathers_stash_pools_into_col1 overwrites the field with the FF
   * pool and marks it via unknown21_pad. Reading the stash back here would
   * let a save-time artefact stand in for "this nation has produced bells". */
  const unsigned last_turn_bells =
    founding_fathers_col1_last_turn_is_stash(col1, nation_id)
      ? 0u
      : (unsigned)nat->liberty_bells_last_turn;
  if (pool == 0u && last_turn_bells == 0u) {
    return false;
  }

  /* Peacetime only — WoI bell spend runs from turn.c (ai_king hook). */
  if (col1->head.game_options.woi) {
    return false;
  }

  ensure_next_candidate(ctx, nation_id);

  const unsigned needed = founding_fathers_bells_needed(col1, nation_id);
  if (pool < needed) {
    return false;
  }

  const int idx = (int)nat->next_founding_father;
  if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT ||
      !ff_available_to(col1, nation_id, idx)) {
    return false; /* waiting on debate CHOICE, or no candidates left */
  }
  return elect_commit(ctx, nation_id, idx);
}

void founding_fathers_apply_popup_result(ColonizeTurnContext* ctx, AiPopupState* popups) {
  if (!ctx || !popups || !popups->has_result) {
    return;
  }
  if (popups->result_tag != AI_POPUP_TAG_FF_CONGRESS) {
    return;
  }
  if (popups->result_cancelled) {
    /* DOS: escaping the debate re-presents it at once, same candidates —
     * the choice is persistent and cannot be rerolled (bugs.md). */
    if (popups->result_payload > 0) {
      const int nation =
        popups->result_nation_a >= 0 ? popups->result_nation_a : ctx->human_nation;
      (void)ff_enqueue_debate_from_slate(ctx, popups, nation);
    }
    return;
  }
  /* Peacetime only — Congress debate is gated in ensure_next_candidate. */
  if (ctx->col1 && ctx->col1->head.game_options.woi) {
    return;
  }
  /* Debate CHOICE stores payload >0. Announce OK uses -1. */
  if (popups->result_payload <= 0) {
    return;
  }
  const int idx = popups->result_choice_id;
  const int nation = popups->result_nation_a >= 0 ? popups->result_nation_a : ctx->human_nation;
  if (!ctx->col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT ||
      !ff_available_to(ctx->col1, nation, idx)) {
    return;
  }
  /* Lock candidate first (choose → accumulate → join). Elect if already funded. */
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation];
  nat->next_founding_father = (int16_t)idx;
  /* Debate answered — the persistent slate is spent. */
  s_ff_debate_slate_n = 0;
  s_ff_debate_slate_nation = -1;
  const unsigned needed = founding_fathers_bells_needed(ctx->col1, nation);
  const unsigned pool = founding_fathers_bells_since_last_elect(nation);
  if (pool >= needed) {
    (void)elect_commit(ctx, nation, idx);
  } else if (ctx->status && ctx->status_size > 0 && nation == ctx->human_nation) {
    snprintf(
      ctx->status,
      ctx->status_size,
      "Congress seeks %s (%u/%u bells).",
      reports_ff_display_name(idx),
      pool,
      needed
    );
  }
}

void founding_fathers_tick(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (ctx->human_nation < 0 || ctx->human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }

  ColonizeCol1Save* col1 = ctx->col1;

  /* bugs.md #434: the HUMAN's election is NOT run here. DOS checks it inside
   * the human's own FUN_3844_00f2 pass, which 130d runs immediately before
   * that nation's Move Pieces — so the Congress debate belongs to the start
   * of the player's turn (TURN_PROC_FINISH calls
   * founding_fathers_tick_human_elect), not the moment End Turn is pressed.
   */

  /* Each AI Euro nation (control==1), one elect each max. */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (n == ctx->human_nation) {
      continue;
    }
    if (col1->player[n].control != 1) {
      continue;
    }
    try_elect_nation(ctx, n);
  }

  /*
   * No Las Casas ownership tick. It used to re-assimilate Convert→Free
   * Colonist every turn while the father was owned, and DOS's own saves say
   * that never happens: in original_saves/colony-prod-tests the Dutch already
   * hold Las Casas (father 24) in COLONY00, yet Montreal's first three
   * colonists are Converts (profession 27) in COLONY00 *and* still Converts in
   * COLONY01 one full turn later. Since a Convert outproduces a Free Colonist
   * on every outdoor job but Lumberjack (colony_yield.c), the re-tick was also
   * quietly costing those colonies yield. The elect-time one-shot stays.
   */

  /*
   * No La Salle ownership tick either. DOS FUN_4345_0342's `param_2 == 9`
   * sweep runs only once, at elect (raw 73084-73092: owner+pop>2 -> stockade
   * over every existing colony). Future growth across pop 3, or a colony
   * founded after election, gets the grant from the admit-time body instead
   * (FUN_15eb, raw ~11312: pop>2 && !WoI && FF 9 on a Join/birth admit) — see
   * founding_fathers_la_salle_check's callers in colony.c and turn.c. There
   * is no per-turn re-sweep site in the decomp; the old one here was a
   * PEDIA-wording invention (same defect class as the retired Las Casas tick
   * above) and is removed.
   */
}

void founding_fathers_tick_human_elect(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (ctx->human_nation < 0 || ctx->human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  try_elect_nation(ctx, ctx->human_nation);
}
