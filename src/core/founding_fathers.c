#include "core/ai_king.h"
#include "core/founding_fathers.h"

#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/popup_msg.h"
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

/* King tax-refuse stand-in byte (ai_king unknown46[2]). */

/* DOS nation+0xc — bells since last FF elect; not stored in ColonizeCol1Nation. */
static uint16_t s_ff_bells_since_elect[COLONIZE_COL1_NATION_COUNT];
/* Score line: +1 per WoI bell-pool spend on foreign intervention. */
static uint16_t s_intervention_bells[COLONIZE_COL1_NATION_COUNT];
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

void founding_fathers_reset(void) {
  memset(s_ff_bells_since_elect, 0, sizeof(s_ff_bells_since_elect));
  memset(s_intervention_bells, 0, sizeof(s_intervention_bells));
  s_ff_pools_initialized = false;
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
    if (last > 0u && last <= need) {
      s_ff_bells_since_elect[n] = (uint16_t)last;
    }
  }
}

/*
 * Test helper: treat liberty_bells_total as the since-last-elect pool (legacy
 * unit-test convention). Live play uses turn accrual + sync_from_col1_after_load.
 */
void founding_fathers_force_pool_from_total(const ColonizeCol1Save* col1) {
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

unsigned founding_fathers_intervention_bells(int nation_id) {
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0u;
  }
  return (unsigned)s_intervention_bells[nation_id];
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

#define FF_CORONADO_REVEAL_RADIUS 2
#define FF_DESOTO_REVEAL_RADIUS 1
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

bool founding_fathers_nation_has(const ColonizeCol1Save* col1, int nation, int ff_index) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }
  if (ff_index < 0 || ff_index >= (int)COLONIZE_COL1_FF_COUNT) {
    return false;
  }
  if (col1->head.founding_father[ff_index] == (int8_t)nation) {
    return true;
  }
  const uint8_t byte = col1->nation[nation].founding_fathers[ff_index / 8];
  return (byte & (uint8_t)(1u << (ff_index % 8))) != 0;
}

bool founding_fathers_franklin_keeps_nw_peace(const ColonizeCol1Save* col1, int nation) {
  /*
   * docs/fandom_col1994.md Benjamin Franklin — NW peer peace gate.
   * Ownership via founding_fathers_nation_has (head owner or nation bitmask).
   * Requires head.founding_father[i]==-1 when unclaimed (col1_save_init).
   */
  return founding_fathers_nation_has(col1, nation, FF_BENJAMIN_FRANKLIN);
}

bool founding_fathers_brebeuf_missionaries_are_experts(
  const ColonizeCol1Save* col1,
  int nation
) {
  /*
   * docs/fandom_col1994.md Father Jean de Brebeuf — all missionaries function
   * as experts. Ownership via founding_fathers_nation_has (head or bitmask).
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
   * yield more treasure. Ownership gate; amount via units_cortes_conquest_treasure_gold
   * (FUN_5fef_31ea peel) when fallout gold_amount<=0.
   */
  return founding_fathers_nation_has(col1, nation, FF_HERNAN_CORTES);
}

bool founding_fathers_cortes_free_king_galleon(const ColonizeCol1Save* col1, int nation) {
  /*
   * docs/fandom_col1994.md Hernan Cortes — king's galleons transport treasure
   * free. GAME.TXT @KINGGALLEON3: Crown share = current tax rate (already
   * europe_cash_treasure); "for no extra charge" — do NOT invent KINGGALLEON2
   * non-Cortes royal-galleon extra %. AI/human stand-in: coastal own-colony
   * Treasure → europe_cash_treasure via units_cortes_cash_coastal_treasures.
   * KINGGALLEON2 resolved 2026-08-27: FUN_5fef_1908 builds "KINGGALLEON"+"2"/"3"
   * at runtime (DS 0x1bed/0x1bfb/0x1bf9); non-Cortes share =
   * max((difficulty+10)*5, 2*tax) cap 90 — see units_king_galleon_share_pct.
   * This Cortes-only auto-cash stays for the AI callers; the human path is
   * units_king_galleon_offer_coastal_treasures (CHOICE, DOS-shaped).
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

int founding_fathers_revere_auto_arm(
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int colony_id
) {
  ColonizeColony* col = colonies_get_mut(colonies, colony_id);
  if (!col || !col->active || !units) {
    return -1;
  }
  if (col->colonist_count <= 0 || col->stock[COLONIZE_CARGO_MUSKETS] < UNITS_EQUIP_MUSKETS) {
    return -1;
  }
  /* First active colonist takes up muskets (PEDIA auto-arm). */
  int idx = -1;
  for (int i = 0; i < col->colonist_count; ++i) {
    if (col->colonists[i].active) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    return -1;
  }
  return colonies_eject_colonist(colonies, colony_id, idx, units, COLONIZE_EJECT_SOLDIER);
}

/*
 * Franklin elect: clear Euro×Euro WAR with all New World peers (make_peace).
 * Source: docs/fandom_col1994.md — king's European wars no longer affect NW
 * relations; ongoing gate lives in ai_diplo declare / euro_balance.
 */
static void effect_franklin_nw_peace(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation_id) {
      continue;
    }
    if (ai_diplo_at_war(col1, nation_id, peer)) {
      ai_diplo_make_peace(col1, nation_id, peer);
    }
  }
}

/* Pocahontas elect: all native tension → content for this European nation. */
static void effect_pocahontas_reset_alarm(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id > 3) {
    return;
  }
  if (col1->tribe) {
    for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
      col1->tribe[i].alarm[nation_id].friction = 0;
      col1->tribe[i].alarm[nation_id].attacks = 0;
    }
  }
  for (int ind = 0; ind < 8; ++ind) {
    col1->indian[ind].alarm_by_player[nation_id] = 0;
  }
}

static bool ff_unclaimed(const ColonizeCol1Save* col1, int idx) {
  return col1->head.founding_father[idx] < 0;
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

static const char* k_ff_short_names[COLONIZE_COL1_FF_COUNT] = {
  "Adam Smith",
  "Jakob Fugger",
  "Peter Minuit",
  "Peter Stuyvesant",
  "Jan de Witt",
  "Ferdinand Magellan",
  "Francisco Coronado",
  "Hernando de Soto",
  "Henry Hudson",
  "Sieur De La Salle",
  "Hernan Cortes",
  "George Washington",
  "Paul Revere",
  "Francis Drake",
  "John Paul Jones",
  "Thomas Jefferson",
  "Pocahontas",
  "Thomas Paine",
  "Simon Bolivar",
  "Benjamin Franklin",
  "William Brewster",
  "William Penn",
  "Jean de Brebeuf",
  "Juan de Sepulveda",
  "Bartolome de las Casas"
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

/* FUN_4345_0080: count unclaimed FFs of type with non-zero weight this century. */
static int ff_count_eligible_of_type(const ColonizeCol1Save* col1, int type) {
  const int band = ff_century_band(col1);
  int count = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if ((int)k_ff_type[i] != type || !ff_unclaimed(col1, i)) {
      continue;
    }
    if (k_ff_weight[i][band] > 0) {
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
  int type,
  ColonizeDosRng* rng
) {
  const int band = ff_century_band(col1);
  int total = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if ((int)k_ff_type[i] != type || !ff_unclaimed(col1, i)) {
      continue;
    }
    total += (int)k_ff_weight[i][band];
  }
  if (total <= 0) {
    return -1;
  }
  if (!rng) {
    /* Deterministic fallback when no RNG context (should not happen in tick). */
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      if ((int)k_ff_type[i] == type && ff_unclaimed(col1, i) &&
          k_ff_weight[i][band] > 0) {
        return i;
      }
    }
    return -1;
  }
  int roll = dos_rng_range(rng, 1, total);
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if ((int)k_ff_type[i] != type || !ff_unclaimed(col1, i)) {
      continue;
    }
    const int w = (int)k_ff_weight[i][band];
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
static int ff_pick_strongest_category(const ColonizeCol1Save* col1) {
  int best_type = -1;
  int best_count = -1;
  for (int type = 0; type < 5; ++type) {
    const int n = ff_count_eligible_of_type(col1, type);
    if (n > best_count) {
      best_count = n;
      best_type = type;
    }
  }
  return best_type;
}

static void ff_record_intervention_spend(int nation_id) {
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  if (s_intervention_bells[nation_id] < 65535u) {
    s_intervention_bells[nation_id]++;
  }
}

void founding_fathers_consume_woi_bell_pool(int nation_id) {
  founding_fathers_reset_bells_pool(nation_id);
  ff_record_intervention_spend(nation_id);
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

static int pick_candidate(ColonizeTurnContext* ctx, const ColonizeCol1Save* col1,
                          const ColonizeCol1Nation* nat) {
  const int next = (int)nat->next_founding_father;
  if (next >= 0 && next < (int)COLONIZE_COL1_FF_COUNT && ff_unclaimed(col1, next)) {
    return next;
  }
  const int type = ff_pick_strongest_category(col1);
  if (type < 0) {
    return -1;
  }
  return ff_pick_weighted_of_type(col1, type, ctx ? ctx->rng : NULL);
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
  if (next >= 0 && next < (int)COLONIZE_COL1_FF_COUNT && ff_unclaimed(col1, next)) {
    return;
  }
  nat->next_founding_father = -1;

  if (nation_id == ctx->human_nation && ctx->ai_popups) {
    if (ff_debate_pending(ctx->ai_popups)) {
      return;
    }
    char labels[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    int ids[AI_POPUP_CHOICE_MAX];
    int n = 0;
    for (int type = 0; type < 5 && n < AI_POPUP_CHOICE_MAX; ++type) {
      const int idx = ff_pick_weighted_of_type(col1, type, ctx->rng);
      if (idx < 0) {
        continue;
      }
      snprintf(labels[n], sizeof(labels[n]), "%s", k_ff_short_names[idx]);
      ids[n] = idx;
      n++;
    }
    if (n >= 2) {
      const char* choice_ptrs[AI_POPUP_CHOICE_MAX];
      for (int i = 0; i < n; ++i) {
        choice_ptrs[i] = labels[i];
      }
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        ctx->messages,
        "WHICHFREEDOM",
        NULL,
        "The Continental Congress will expand during its next session. Which Founding Father shall we appoint as its next member?",
        body,
        sizeof(body)
      );
      (void)ai_popup_enqueue_choice_ctx(
        ctx->ai_popups,
        AI_POPUP_TAG_FF_CONGRESS,
        nation_id,
        -1,
        1, /* payload >0: debate apply sets next (not announce OK) */
        NULL,
        body,
        choice_ptrs,
        ids,
        n
      );
      if (ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "Congress debates founding fathers.");
      }
      return;
    }
    if (n == 1) {
      nat->next_founding_father = (int16_t)ids[0];
      return;
    }
    return;
  }

  const int idx = pick_candidate(ctx, col1, nat);
  if (idx >= 0) {
    nat->next_founding_father = (int16_t)idx;
  }
}

/* Coronado: reveal radius around each owned colony. Returns colonies touched. */
static int effect_coronado_reveal(
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  int nation_id
) {
  if (!map || !map->seen || !colonies) {
    return 0;
  }
  int touched = 0;
  for (int i = 0; i < colonies->colony_count; ++i) {
    ColonizeColony* col = &colonies->colonies[i];
    if (!col->active || col->nation_id != nation_id) {
      continue;
    }
    map_reveal_radius(map, col->x, col->y, nation_id, FF_CORONADO_REVEAL_RADIUS);
    touched++;
  }
  return touched;
}

/* de Soto partial: extended sight stand-in via land-unit reveal.
 * LCR always-positive: wired via units_resolve_lcr_rumour (reveal radius). */
static int effect_desoto_reveal(
  ColonizeWorldMap* map,
  ColonizeUnitPool* units,
  int nation_id
) {
  if (!map || !map->seen || !units) {
    return 0;
  }
  int touched = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || !units_is_on_map(u)) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      continue;
    }
    map_reveal_radius(map, u->x, u->y, nation_id, FF_DESOTO_REVEAL_RADIUS);
    touched++;
  }
  return touched;
}

/*
 * Magellan: permanent naval +1 — bump current sea moves_left once on elect;
 * turn_refresh_moves_for_nation adds +1 each turn while owned (see turn.c).
 * FUN_48d3_0002 also gates landfall goto duration 2 (RNG>89, docks>2) on
 * Magellan ownership — wired in turn.c rare immigrant spawn. Wiki "west-edge
 * Europe sail shortcut" not found in viceroy (only 48d3:77575 07b4 FF#5); PARK.
 */
static int effect_magellan_sea_moves(ColonizeUnitPool* units, int nation_id) {
  if (!units) {
    return 0;
  }
  int bumped = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
      continue;
    }
    if (u->moves_left < 0x7fffffff) {
      u->moves_left++;
    }
    bumped++;
  }
  return bumped;
}

/* La Salle: Stockade when colony population >= 3 (wiki / manual). */
static int effect_la_salle_stockades(ColonizeColonyPool* colonies, int nation_id) {
  if (!colonies) {
    return 0;
  }
  const int stock_idx = colonies_find_building(colonies, "Stockade");
  if (stock_idx < 0 || stock_idx >= COLONIZE_BUILDING_TYPES_MAX) {
    return 0;
  }
  int touched = 0;
  for (int i = 0; i < colonies->colony_count; ++i) {
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
 * or dock. (Player pick among pool→dock UI still PARKED.)
 */
static void effect_brewster_filter_pool(EuropeScreen* europe) {
  if (!europe) {
    return;
  }
  europe->brewster_no_criminals = true;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (!europe->pool[i].filled) {
      continue;
    }
    if (europe->pool[i].profession == COLONIZE_PROF_CRIMINAL ||
        europe->pool[i].profession == COLONIZE_PROF_INDENTURED ||
        strstr(europe->pool[i].name, "Criminal") != NULL ||
        strstr(europe->pool[i].name, "Indentured") != NULL ||
        strstr(europe->pool[i].name, "Servant") != NULL) {
      europe_refill_pool_slot(europe, i, NULL);
    }
  }
  /* Dock starters may still be Indentured — clear/refill to Free Colonists. */
  for (int i = 0; i < europe->dock_count; ++i) {
    if (!europe->dock[i].present) {
      continue;
    }
    if (europe->dock[i].profession == COLONIZE_PROF_CRIMINAL ||
        europe->dock[i].profession == COLONIZE_PROF_INDENTURED ||
        strstr(europe->dock[i].name, "Criminal") != NULL ||
        strstr(europe->dock[i].name, "Indentured") != NULL ||
        strstr(europe->dock[i].name, "Servant") != NULL) {
      snprintf(europe->dock[i].name, sizeof(europe->dock[i].name), "Free Colonists");
      europe->dock[i].profession = COLONIZE_PROF_FREE_COLONIST;
    }
  }
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
    int free_ty = units_find_type(units, "Free Colonist");
    if (free_ty < 0) {
      free_ty = units_find_type(units, "Colonists");
    }
    if (free_ty < 0) {
      free_ty = units_find_type(units, "Colonist");
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      bool changed = false;
      if (u->profession == COLONIZE_PROF_CONVERT) {
        u->profession = COLONIZE_PROF_FREE_COLONIST;
        changed = true;
      }
      /* Name-based type swap when a Convert/@JOB display type was used. */
      const ColonizeUnitType* ut = units_type(units, u->type_index);
      if (ut && free_ty >= 0 &&
          (strstr(ut->name, "Indian Convert") != NULL ||
           strcmp(ut->name, "Convert") == 0 ||
           strcmp(ut->name, "Converts") == 0)) {
        u->type_index = free_ty;
        if (u->profession == COLONIZE_PROF_CONVERT) {
          u->profession = COLONIZE_PROF_FREE_COLONIST;
        }
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
    for (int i = 0; i < colonies->colony_count; ++i) {
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

/* John Paul Jones: free Frigate (Man-O-War fallback). Returns true on spawn. */
static bool effect_jones_frigate(
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int nation_id
) {
  if (!units || !map) {
    return false;
  }
  int ship_ty = units_find_type(units, "Frigate");
  if (ship_ty < 0) {
    ship_ty = units_find_type(units, "Man-O-War");
  }
  if (ship_ty < 0) {
    return false;
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
      /* Manual/wiki: clear all Europe boycotts (no back taxes). */
      nat->boycott_bitmap = 0;
      if (nation_id == human_nation && col1) {
        ai_king_latch_set(col1, AI_KING_BOYCOTT_BYTE, 0);
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
       * FA detailed strength already peeks head.founding_father[4] (reports).
       * Cargo: colonies_de_witt_transfer_* + ai_euro de Witt wagon/ship act
       * (stock only; no gold invent). */
      break;
    case FF_FERDINAND_MAGELLAN:
      /* Manual/wiki: all naval vessels +1 movement (permanent). */
      (void)effect_magellan_sea_moves(units, nation_id);
      break;
    case FF_FRANCISCO_CORONADO:
      /* Manual/wiki: reveal owned colonies and surroundings. */
      (void)effect_coronado_reveal(map, colonies, nation_id);
      break;
    case FF_HERNANDO_DE_SOTO:
      /* docs/fandom_col1994.md: LCR always positive + extended sight.
       * Partial: extended sight via land-unit reveal on elect.
       * LCR gate: units_resolve_lcr_rumour ← FUN_65dd_0004 via FUN_2a1f_0178
       * (465b move-spent path); FF bit 7 reroll loop at 65dd:00a6.
       * Full 65dd outcome weights / burial / FoY table PARKED — see
       * original_sources_annotated/ai/indian_contact.md. */
      (void)effect_desoto_reveal(map, units, nation_id);
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
       * units_cortes_conquest_treasure_gold (FUN_5fef_31ea peel) +
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
       * attacked. Ownership bit; gate + eject via founding_fathers_revere_*;
       * combat spawn wired in units_try_move when FF col1 context is set
       * (turn_refresh_moves_for_nation → units_set_ff_col1). */
      break;
    case FF_FRANCIS_DRAKE:
      /* PEDIA/wiki: Privateer combat strength +50%.
       * Ownership bit; multiplier in units_resolve_naval_combat_ff (*3/2).
       * AI/king path: units_resolve_naval_combat passes g_units_ff_col1
       * (units_set_ff_col1 from turn_refresh_moves_for_nation). */
      break;
    case FF_JOHN_PAUL_JONES:
      /* Manual/wiki: free Frigate. No gold fallback. */
      (void)effect_jones_frigate(map, colonies, units, nation_id);
      break;
    case FF_THOMAS_JEFFERSON:
      /* Wiki: liberty bell production of statesmen +50%.
       * Ownership bit; applied in colony_prod_colony_bells_ff via turn nation ticks. */
      break;
    case FF_POCAHONTAS:
      /* Wiki/fandom: all native tension → content; Indian alarm half as fast.
       * Elect: zero this nation's tribe friction/attacks + alarm_by_player.
       * Half-rate ongoing growth: ai_contact_alarm_bump_amount (encroachment /
       * prelude escalate / raid bump) — not PARKED. */
      effect_pocahontas_reset_alarm(col1, nation_id);
      break;
    case FF_THOMAS_PAINE:
      /* Wiki: liberty bell production in all colonies + current tax rate %.
       * Ownership bit; applied in colony_prod_colony_bells_ff via turn nation ticks. */
      break;
    case FF_SIMON_BOLIVAR:
      /* FUN_15eb_0274: SoL +20% on every read while owned (human).
       * Display-time via founding_fathers_bolivar_sol_bonus — no storage bump. */
      break;
    case FF_BENJAMIN_FRANKLIN:
      /* docs/fandom_col1994.md: king's European wars no longer affect NW
       * relations; Europeans in the New World always offer peace.
       * Elect: make_peace with all Euro peers. Ongoing: ownership gate via
       * founding_fathers_franklin_keeps_nw_peace → ai_diplo declare /
       * euro_balance / war-hit (no gold fiction). FA 3f41 UI PARKED. */
      effect_franklin_nw_peace(col1, nation_id);
      break;
    case FF_WILLIAM_BREWSTER:
      /* Manual/wiki: no criminals/servants on docks + recruit pool.
       * effect_brewster_filter_pool (pool refill + Indentured dock→Free).
       * Pick-among-pool UI PARKED. */
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
       * Ownership bit only — no elect crosses fiction. Ongoing gate:
       * founding_fathers_brebeuf_missionaries_are_experts → ai_contact
       * Jesuit-grade mid convert for plain Missionary. */
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
  if (!ff_unclaimed(ctx->col1, idx)) {
    return false;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  col1->head.founding_father[idx] = (int8_t)nation_id;
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
  founding_fathers_reset_bells_pool(nation_id);

  if (ctx->ai_popups && nation_id == ctx->human_nation) {
    char body[AI_POPUP_BODY_LEN];
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = k_ff_short_names[idx];
    tok.string1 = "The";
    popup_msg_fill(
      ctx->messages,
      "FREEDOM",
      &tok,
      "Founding Fathers announce that a new member has joined the Continental Congress!",
      body,
      sizeof(body)
    );
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_FF_CONGRESS,
      nation_id,
      -1,
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
  if (pool == 0u && nat->liberty_bells_last_turn == 0) {
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
  if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT || !ff_unclaimed(col1, idx)) {
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
  if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT || !ff_unclaimed(ctx->col1, idx)) {
    return;
  }
  /* Lock candidate first (choose → accumulate → join). Elect if already funded. */
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation];
  nat->next_founding_father = (int16_t)idx;
  const unsigned needed = founding_fathers_bells_needed(ctx->col1, nation);
  const unsigned pool = founding_fathers_bells_since_last_elect(nation);
  if (pool >= needed) {
    (void)elect_commit(ctx, nation, idx);
  } else if (ctx->status && ctx->status_size > 0 && nation == ctx->human_nation) {
    snprintf(
      ctx->status,
      ctx->status_size,
      "Congress seeks %s (%u/%u bells).",
      k_ff_short_names[idx],
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

  /* Human first (one elect max). */
  try_elect_nation(ctx, ctx->human_nation);

  /* Then each AI Euro nation (control==1), one elect each max. */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (n == ctx->human_nation) {
      continue;
    }
    if (col1->player[n].control != 1) {
      continue;
    }
    try_elect_nation(ctx, n);
  }

  /* Las Casas ownership tick: re-assimilate Convert→Free Colonist while owned
   * (PEDIA elect is one-shot; tick catches late joins without inventing join). */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (!founding_fathers_nation_has(col1, n, FF_BARTOLOME_DE_LAS_CASAS)) {
      continue;
    }
    (void)effect_las_casas_assimilate(ctx->colonies, ctx->units, n);
  }

  /* La Salle ownership tick: PEDIA says "existing AND future" colonies get a
   * Stockade at population 3+. The elect-time sweep (apply_effect) only
   * catches colonies that already exist at election; re-sweep every turn
   * while owned so a colony founded later, or one that grows into pop 3
   * later, still gets the free Stockade (same re-tick shape as Las Casas
   * above — docs/founding_fathers.md Open item 2). */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (!founding_fathers_nation_has(col1, n, FF_SIEUR_DE_LA_SALLE)) {
      continue;
    }
    (void)effect_la_salle_stockades(ctx->colonies, n);
  }
}
