#include "core/ai_king.h"
#include "core/ai_diplo.h"
#include "core/sound.h"

#include "core/assets.h"
#include "core/colony.h"
#include "core/combat_strength.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/strutil.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * FUN_43f7_* King/REF/independence — partial structural port.
 * Thin map: original_sources_annotated/ai/king_ref.md
 *
 * WoI: primary latch is head.game_options.woi (DOS 0x5382 bit0, mapped in col1_save.h).
 *   unknown46[AI_KING_WOI_BYTE] is kept in sync on declare for legacy Linux saves only;
 *   reads use game_options.woi — unknown46[0..5] alias price_group_state on DOS saves.
 * REF-present: head.unknown46[1] stand-in for 0x5382 bit1.
 * Tax audience (ported 2026-08-19, real formula — see ai_king_audience_roll /
 *   ai_king_audience_apply_delta / ai_king_tax_event): FUN_38fd_5be8 rolls a
 *   signed delta off a turn-interval-gated favor-score ladder (cut, +1, +2,
 *   +3-4, +5-8) and FUN_38fd_3dc8 applies it UNCONDITIONALLY, clamped to
 *   0..75%. Only a genuine positive applied delta can trigger the
 *   village-goods popup (Accept "kiss the ring" keeps it / Refuse "tea
 *   party" REVERTS the just-applied hike + boycotts one roulette-picked
 *   cargo) — there is no DOS gate on whether the hike itself happens.
 *   head.unknown46[2] is now presentation-only (boycott-active flag +
 *   Fugger sync), no longer gates the audience interval.
 *   Follow-up OK is GAME.TXT @TEAPARTY (KING_TAX; thin 3dc8 stock dump +
 *   tokens). Cargo freeze: nation.boycott_bitmap. Fugger/diplo bitmap
 *   clear → drop unknown46[2] when bitmap==0 (king sync; do not touch FF).
 * Rebel troop-gift purchase (FUN_43f7_2022 rebel branch, real port
 *   2026-08-14): recurring per-turn 1-in-3 roll while REF absent or
 *   Artillery backup pool empty; ai_popup CHOICE Hire/Decline when
 *   ctx->ai_popups (auto-accept when NULL); unaffordable → silently
 *   skipped (no DOS status/dialog). No once-per-war flag — head.unknown46[3]
 *   is unused for this now (was an invented gate, see king_ref.md).
 * 160a: signing cinematic only (core/declaration.c, armed from game_loop on
 *   the KING_LETTER popup). No country rename — "United Colonies" was a port
 *   invention (bugs.md 245); WoI faction labels are Rebels/Tory via
 *   units_combat_nation_label. unknown46[4] endgame latch: 0/1 won/2 lost.
 *   @HOWTOWIN fires at first rebel recapture (units.c), not at declare
 *   (bugs.md 242).
 * Congress confirm: head.unknown46[5] + thin 2564 (ai_popup CHOICE from
 *   GAME.TXT @DECLARE Never/Yes when ctx->ai_popups; auto-declare when NULL;
 *   same-turn 1528 may overwrite status).
 * Mid-war @WARN%d: ONE digit-patch selector per turn (raw 58506-58534, twin
 *   of @LOSING%d), precedence colonies<3 (2) > share ≥80% (3) > ports<3 (1);
 *   the selected warn keeps a port-side episode latch — unknown46[6]/[7]/[10]
 *   for @WARN1/2/3 — each cleared when its own band is left, so a relapse
 *   re-fires. @LOSING3 takes the turn instead when share ≥90%.
 * Calendar @SOONRETIRING0 (1790 spring peacetime): head.unknown46[8] once.
 * Calendar @SOONRETIRING1 (1840 WoI): head.unknown46[9] once.
 * Revolution end (raw 58470-58556): lose on one @LOSING%d selector, DOS
 *   precedence 0 colonies (2) > share ≥90% (3) > 0 coastal ports (1); win on
 *   the C1 triple with NO year gate; @RETIRING2 on year 1850 alone.
 * SoL restless chrome (40..49): status only (no invented wood OK).
 * backup_force: DOS 0x53e2…0x53e8 foreign-intervention pools, seeded on
 *   declare by the ported FUN_43f7_1a26 body and drained by 10f0.
 * Crown nation_id: non-human Euro slot (1 if human==0 else 0).
 */

#define AI_KING_YEAR_CAP 1850
#define AI_KING_PEACE_YEAR_CAP 1800
#define AI_KING_SOONRETIRE0_YEAR 1790
#define AI_KING_SOONRETIRE1_YEAR 1840
/* DOS 3844_0442: warn at >=80% crown pop share (0x4f < pct), lose at >=90. */
#define AI_KING_WARN3_PCT_MIN 80
#define AI_KING_LOSING3_PCT 90

/*
 * Auto-path (no ai_popups attached) tea-party stand-in thresholds. DOS's
 * Accept/tea-party choice is inherently player-interactive (a single
 * FUN_291f_0182 dialog inside FUN_38fd_3dc8, no NPC/auto answer exists) —
 * this heuristic is invented for the no-UI auto path only, unrelated to
 * the real 38fd_5be8 delta formula it now follows. See ai_king_tax_event.
 */
#define AI_KING_BOYCOTT_TAX_MIN 20
#define AI_KING_BOYCOTT_SOL_MIN 30
#define AI_KING_BOYCOTT_BELLS_MIN 80
/* Village-goods cargo pick is FUN_38fd_3dc8's roulette (stock×price weight)
 * over non-boycotted, Europe-bid-eligible cargos — use
 * ai_king_pick_dump_goods_cargo; do not invent a fixed Sugar/Tobacco pick.
 * Cite: docs/fandom_col1994.md Boycott; viceroy FUN_38fd_3dc8. */
/*
 * FUN_43f7_2022 rebel-branch self-funded troop-gift purchase — real port
 * (2026-08-14, replaces an earlier SoL/300-gold invented stand-in; see
 * king_ref.md "2244/2022 — corrected"). Recurring per-turn 1-in-3 roll
 * while REF is not present or the Artillery backup pool is empty; price
 * = (qty_regular+2) * ((difficulty+3)*2 + roll(0,6)) * 100, paid from the
 * rebel (human) nation's own gold. AI_KING_MERC_COST kept only as the
 * cannot-afford-path fallback display value, not a real DOS constant.
 */
#define AI_KING_MERC_COST 300
#define AI_KING_MERC_ROLL_CHANCE 3 /* 1-in-3 per turn, dos_rng_range(0,2)==0 */
/*
 * FUN_43f7_2564 / fandom Independence: declare when nation SoL ≥ 50%.
 * Human + ai_popups → CHOICE; else auto-declare.
 */
#define AI_KING_DECLARE_SOL_MIN 50
/* Restless chrome band immediately below declare (SoL 40..49 when min=50). */
#define AI_KING_RESTLESS_SOL_MIN 40
/*
 * MoW hold fill uses real ship capacity (units_ship_capacity / type->cargo,
 * capped at COLONIZE_UNIT_CARGO_MAX=6). Cite: fandom REF “man-o-war with 6
 * units”; units_board_stacked. Coastal unload dumps multiple cargo_ids per
 * war_act beat up to min(moves_left, capacity) (1 MP/pax); full unload with
 * moves left → AI_SAIL next human coast; after that sail step, if still
 * carrying and now adjacent to the next colony → unload same beat.
 * PARK: full embark UI chrome; dump-goods boycott modal
 * CHOICE Done (pick API + Europe bid>0 weight for auto; KING_DUMP_GOODS for
 * human; VGA PARKED).
 */
/* 10f0 (re-read 2026-08-28): the intervention force is the HUMAN's — one
 * Man-O-War on the best water tile by the colony + Cont. Cav. ≤2 /
 * Artillery ≤2 / Cont. Army = 6 − those, pool-capped, Veteran 0x15. The old
 * "dual/third landing by difficulty" shape was a stand-in and is gone. */
/* 0982: second MoW same beat when difficulty ≥ 2 and force[2] still > 0. */
#define AI_KING_SECOND_MOW_DIFF 2
/*
 * REF idle hunt capital bias: when founding-capital MD is within this slack of
 * the nearest other human colony MD, prefer the capital (fandom REF pressure
 * on main ports; FUN_521d_20e6 multi-step combat×8 siege scoring PARKED).
 */
#define AI_KING_CAPITAL_MD_SLACK 2

/* ai_popup choice_ids (FUN_43f7_38fd_5be8 / 2244 / 2564). */
#define AI_KING_CHOICE_ACCEPT 1
#define AI_KING_CHOICE_REFUSE 2
#define AI_KING_CHOICE_HIRE 1
#define AI_KING_CHOICE_DECLINE 2
#define AI_KING_CHOICE_CONFIRM 1
#define AI_KING_CHOICE_NOT_YET 2
#define AI_KING_CHOICE_THATS_ALL 0
#define AI_KING_CHOICE_KEEP_PLAYING 1

int ai_king_crown_nation(int human_nation) {
  return (human_nation == 0) ? 1 : 0;
}

int ai_king_pick_dump_goods_cargo(
  uint16_t boycott_bitmap,
  uint16_t candidate_mask,
  ColonizeDosRng* rng,
  const int* cargo_bid
) {
  /*
   * Eligible = candidate_mask & ~boycott_bitmap (FUN_38fd_3dc8 skips bits
   * already set in nation boycott_bitmap / local_a6). When cargo_bid non-NULL,
   * also require bid[c] > 0 (live Europe local_7a — do not dump zero-price
   * goods), then roulette by bid. When cargo_bid NULL → uniform among mask.
   * Cite: FUN_38fd_3dc8 / king_ref dump-goods.
   */
  if (!rng) {
    return -1;
  }
  const uint16_t eligible = (uint16_t)(candidate_mask & (uint16_t)~boycott_bitmap);
  if (eligible == 0) {
    return -1;
  }
  int idxs[COLONIZE_CARGO_COUNT];
  int n = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if ((eligible & (uint16_t)(1u << c)) == 0) {
      continue;
    }
    if (cargo_bid && cargo_bid[c] <= 0) {
      continue;
    }
    idxs[n++] = c;
  }
  if (n <= 0) {
    return -1;
  }
  if (!cargo_bid) {
    const int pick = dos_rng_range(rng, 0, n - 1);
    if (pick < 0 || pick >= n) {
      return -1;
    }
    return idxs[pick];
  }
  int total = 0;
  int weights[COLONIZE_CARGO_COUNT];
  for (int i = 0; i < n; ++i) {
    const int c = idxs[i];
    const int w = cargo_bid[c];
    weights[i] = w;
    total += w;
  }
  if (total <= 0) {
    return -1;
  }
  const int roll = dos_rng_range(rng, 1, total);
  int cum = 0;
  for (int i = 0; i < n; ++i) {
    cum += weights[i];
    if (roll <= cum) {
      return idxs[i];
    }
  }
  return idxs[n - 1];
}

/* @CARGO display names (colony.h / NAMES.TXT / reports.c) for boycott chrome. */
static const char* ai_king_cargo_name(int cargo_idx) {
  static const char* const names[COLONIZE_CARGO_COUNT] = {
    "Food",        "Sugar",  "Tobacco", "Cotton", "Furs",  "Lumber",
    "Ore",         "Silver", "Horses",  "Rum",    "Cigars", "Cloth",
    "Coats",       "Trade Goods", "Tools", "Muskets"
  };
  if (cargo_idx < 0 || cargo_idx >= COLONIZE_CARGO_COUNT) {
    return "cargo";
  }
  return names[cargo_idx];
}

/*
 * Comma-separated @CARGO names set in boycott_bitmap (presentation only).
 * Returns 1 if any bit set. Cite: king_ref refuse/holds chrome; Fugger partial
 * clear may leave a subset of bits while unknown46[2] still holds.
 */
static int ai_king_format_boycott_cargos(char* buf, size_t buf_size, uint16_t bitmap) {
  if (!buf || buf_size == 0) {
    return 0;
  }
  buf[0] = '\0';
  size_t pos = 0;
  int any = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if ((bitmap & (uint16_t)(1u << c)) == 0) {
      continue;
    }
    if (any) {
      if (pos + 2 >= buf_size) {
        break;
      }
      pos += (size_t)snprintf(buf + pos, buf_size - pos, ", ");
    }
    pos += (size_t)snprintf(buf + pos, buf_size - pos, "%s", ai_king_cargo_name(c));
    any = 1;
  }
  return any;
}

/* Human-facing map popup queue attached (game_loop); AI/auto path when NULL. */
static int ai_king_human_popups(const ColonizeTurnContext* ctx) {
  return (ctx && ctx->ai_popups) ? 1 : 0;
}

/*
 * FUN_38fd_3dc8 aiStack_a4[]: for each cargo, the human colony holding the most
 * of it (DOS also requires the colony flag 0x40). Used both to name the party
 * and to seize the stock.
 */
static ColonizeColony* ai_king_teaparty_colony(
  const ColonizeTurnContext* ctx,
  int human,
  int cargo
) {
  if (!ctx || !ctx->colonies || human < 0 || human >= 4) {
    return NULL;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return NULL;
  }
  ColonizeColony* best = NULL;
  int best_stock = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    if (c->stock[cargo] > best_stock) {
      best_stock = c->stock[cargo];
      best = c;
    }
  }
  return best;
}

/*
 * GAME.TXT %STRING3 for @TAXOPTIONS / @TEAPARTY: DOS builds it at
 * FUN_38fd_3dc8 (strcpy colony name, strcat " ", strcat cargo name), so the
 * rendered headline is "<Colony> <Cargo> Party!" — a literal "Tea Party" never
 * occurs, since Colonization has no Tea cargo.
 */
static void ai_king_teaparty_party_name(
  char* buf,
  size_t buf_size,
  const ColonizeColony* colony,
  int cargo
) {
  if (!buf || buf_size == 0) {
    return;
  }
  const char* cargo_nm = ai_king_cargo_name(cargo);
  if (colony && colony->name[0]) {
    snprintf(buf, buf_size, "%s %s", colony->name, cargo_nm);
  } else {
    snprintf(buf, buf_size, "%s", cargo_nm);
  }
}

/*
 * GAME.TXT @TEAPARTY follow-up OK after refuse / dump-goods apply.
 * Thin FUN_38fd_3dc8: dump min(100, stock) from richest human colony of cargo,
 * then enqueue KING_TAX OK with authentic tokens. VGA chrome PARKED.
 * Cite: GAME.TXT @TEAPARTY; popup_audit MissingWire → Done thin.
 */
static void ai_king_enqueue_teaparty_ok(ColonizeTurnContext* ctx, int human, int cargo) {
  if (!ctx || !ai_king_human_popups(ctx) || human < 0 || human >= 4) {
    return;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }

  ColonizeColony* best = ai_king_teaparty_colony(ctx, human, cargo);

  int tons = 0;
  if (best) {
    tons = best->stock[cargo] > 100 ? 100 : best->stock[cargo];
    best->stock[cargo] -= tons;
  }

  const char* cargo_nm = ai_king_cargo_name(cargo);
  const char* colony_nm =
    (best && best->name[0]) ? best->name : "the colonies";
  char party[96];
  ai_king_teaparty_party_name(party, sizeof(party), best, cargo);

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cargo_nm;
  tok.string1 = colony_nm;
  tok.string2 = "Europe";
  tok.string3 = party;
  tok.number0 = tons;
  tok.has_number0 = true;

  char fallback[AI_POPUP_BODY_LEN];
  snprintf(
    fallback,
    sizeof(fallback),
    "%s Party! Sons of Liberty throw %d tons of %s into the sea at %s! "
    "Colonists refuse to pay new tax. Parliament announces boycott of %s. "
    "%s cannot be traded in Europe until boycott is lifted.",
    party,
    tons,
    cargo_nm,
    colony_nm,
    cargo_nm,
    cargo_nm
  );

  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "TEAPARTY", &tok, fallback, body, sizeof(body));
  sound_play(0x56); /* FUN_38fd_3dc8 tea party (COLDIG 9 cheering) */
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups,
    AI_POPUP_TAG_KING_TAX,
    human,
    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
    ctx->col1 && ctx->col1_ok ? (int)ctx->col1->nation[human].tax_rate : 0,
    NULL,
    body
  );
}

/* Active colony count for a Euro nation (10f0 intervene nation pick). */
static int ai_king_colony_count(const ColonizeColonyPool* colonies, int nation_id) {
  if (!colonies || nation_id < 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (c->active && c->nation_id == nation_id) {
      n++;
    }
  }
  return n;
}

/*
 * Crown-hostile Euro slot for 10f0 landings (not human, not crown).
 * After declare, prefer head.rival_nation_slot_1 cached at 1a26 (DOS 0x53d4);
 * else most colonies + land-unit tie-break.
 */
static bool ai_king_valid_intervention_slot(
  const ColonizeCol1Save* col1, int human_nation, int slot
) {
  if (!col1 || slot < 0 || slot >= 4) {
    return false;
  }
  const int crown = ai_king_crown_nation_col1(col1, human_nation);
  /* WoI foreign landings: eliminated Euros (control==2) still intervene via 10f0. */
  return slot != human_nation && slot != crown;
}

static int ai_king_intervention_nation(const ColonizeTurnContext* ctx, int human_nation) {
  if (ctx && ctx->col1_ok && ctx->col1 &&
      ai_king_independence_declared(ctx->col1)) {
    const int slot = (int)ctx->col1->head.rival_nation_slot_1;
    if (ai_king_valid_intervention_slot(ctx->col1, human_nation, slot)) {
      return slot;
    }
  }
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human_nation);
  int best = -1;
  int best_colonies = -1;
  int best_force = -1;
  for (int n = 0; n < 4; ++n) {
    if (n == human_nation || n == crown) {
      continue;
    }
    const int cols = ctx && ctx->colonies ? ai_king_colony_count(ctx->colonies, n) : 0;
    int force = 0;
    if (ctx && ctx->units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != n) {
          continue;
        }
        if (units_is_sea(ctx->units, u->id)) {
          continue;
        }
        force++;
      }
    }
    if (best < 0 || cols > best_colonies ||
        (cols == best_colonies && force > best_force)) {
      best = n;
      best_colonies = cols;
      best_force = force;
    }
  }
  return best >= 0 ? best : crown;
}

static int ai_king_intervention_nation_slot(
  const ColonizeTurnContext* ctx, int human_nation, int slot_idx
) {
  if (ctx && ctx->col1_ok && ctx->col1 &&
      ai_king_independence_declared(ctx->col1)) {
    const int slot = slot_idx == 0 ? (int)ctx->col1->head.rival_nation_slot_1
                                   : (int)ctx->col1->head.rival_nation_slot_2;
    if (ai_king_valid_intervention_slot(ctx->col1, human_nation, slot)) {
      return slot;
    }
  }
  return ai_king_intervention_nation(ctx, human_nation);
}

/*
 * FUN_43f7_0218 / FUN_43f7_1a26 shared nation rank (both build the same
 * table before FUN_291f_0ed0's ascending insertion sort):
 *   score[n] = ship_counts[n]*3 + colony_counts[n]*2 + census_pop_proxy[n]
 * (DS:-0x6be8 = 0x9418 ship_counts, -0x6d68 = 0x9298 colony_counts,
 * -0x6bf0 = 0x9410 census_pop_proxy — save_format_map.md rows 241/243/247;
 * the port's live FUN_4962_0018 census keeps all three fresh per turn).
 * Was an invented per-colony pop score before 2026-09-06.
 */
static void ai_king_rank_nations_0218(const ColonizeCol1Save* col1, int order[4]) {
  int score[4] = {0, 0, 0, 0};
  for (int n = 0; n < 4; ++n) {
    order[n] = n;
    if (col1) {
      score[n] = 3 * (int)col1->stuff.ship_counts[n] + 2 * (int)col1->stuff.colony_counts[n] +
                 (int)col1->stuff.census_pop_proxy[n];
    }
  }
  for (int a = 1; a < 4; ++a) {
    for (int b = a; b > 0 && score[order[b]] < score[order[b - 1]]; --b) {
      const int t = order[b];
      order[b] = order[b - 1];
      order[b - 1] = t;
    }
  }
}

/*
 * FUN_43f7_1a26: cache two non-human/non-crown Euro slots (DOS 0x53d4/0x53d6).
 * DOS ranks all four by ships*3 + colonies*2 + census (see
 * ai_king_rank_nations_0218) and the sort is ASCENDING — so the intervention
 * ally (0x53d4) is the WEAKER of the two remaining powers, the second
 * (0x53d6) the stronger. (Same sorted list 43f7_0218 picks the crown from:
 * first non-human = the weakest power, which is withdrawn-by-merger to free
 * its slot for the King.)
 */
static void ai_king_write_rival_nation_slots(ColonizeCol1Save* col1, int human) {
  if (!col1 || human < 0 || human >= 4) {
    return;
  }
  const int crown = ai_king_crown_nation_col1(col1, human);
  int order[4];
  ai_king_rank_nations_0218(col1, order);
  col1->head.rival_nation_slot_1 = -1;
  col1->head.rival_nation_slot_2 = -1;
  int w = 0;
  for (int i = 0; i < 4 && w < 2; ++i) {
    const int n = order[i];
    if (n == human || n == crown) {
      continue;
    }
    if (w == 0) {
      col1->head.rival_nation_slot_1 = (int16_t)n;
    } else {
      col1->head.rival_nation_slot_2 = (int16_t)n;
    }
    w++;
  }
}

/*
 * FUN_43f7_1a26 foreign-intervention pool seed (DOS 0x53e2…0x53e8 → backup_force).
 * Uses intervention-nation stats where DOS nation bytes are mapped to Col1 fields.
 *
 * Index is a literal mirror of the DOS addresses (backup_force[i] ↔
 * 0x53e2+i*2), with the formula-to-address mapping read off
 * viceroy_unpacked.c:74765-74799 and the unit types off :74424
 * (`thunk_FUN_2a1f_0070` type lookup,
 * FUN_43f7_0082 @73519: param_1==2 → unit type 0x12 Man-O-War, param_1==3 →
 * type 0xb Artillery — both cross-checked against NAMES.TXT @UNIT row order):
 *   [0] founding-father-count-based (0x53e2) — Regular land-troop pool
 *   [1] rebel-sentiment-based       (0x53e4) — Dragoon land-troop pool
 *   [2] colony-count-based          (0x53e6) — Man-O-War pool; FUN_43f7_2022's
 *       rebel-gift gate reads this address directly (line 75007) — gates
 *       ai_king_merc_offer, NOT spent as a land unit (FUN_43f7_10f0 skips
 *       `local_52==2` in its land-troop loop, line 74418/74448)
 *   [3] liberty-bells-based         (0x53e8) — Artillery land-troop pool
 * (was previously stored index-swapped — [2]<->[3] — which fed the wrong
 * formula into ai_king_merc_offer's gate and into the Artillery land-troop
 * drain; fixed together with that call site below.)
 */
static void ai_king_seed_backup_force_1a26(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int diff = (int)ctx->col1->head.difficulty;
  const int ally = ai_king_intervention_nation(ctx, human);
  if (ally < 0 || ally >= 4) {
    return;
  }
  /*
   * Real DOS operands resolved 2026-09-06 (were mapped to unrelated nation
   * fields before): all four are FUN_4962_0018 census tables for the ally
   * (DOS 0x53d4 rival slot):
   *   -0x6bf0 = 0x9410 census_pop_proxy;
   *   -0x6bd4 = 0x942c field_combat_totals (land combat units in the field);
   *   -0x6be4 word = 0x941c land_combat_strength (u16);
   *   local_4 = unit_type_counts[ally][0x10] + [0x11] (DS:-0x6da4/-0x6da3,
   *     stride 0x13 = Privateer + Frigate counts — the ally's warships).
   */
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int local_4 =
    (int)stuff->unit_type_counts[ally][16] + (int)stuff->unit_type_counts[ally][17];
  const int n6bf0 = (int)stuff->census_pop_proxy[ally];
  const int n6bd4 = (int)stuff->field_combat_totals[ally];
  const int n6be4 = (int)stuff->land_combat_strength[ally];

  /*
   * DOS halves the PRE-store accumulators, not the stored pool (raw
   * 74769-74784): `iVar4 = pop/10 - diff; *0x53e2 = iVar4 + 8; …
   * iVar5 = (iVar4 + 9) / 2; *0x53e2 = iVar5`. The +8/+1/+3/+3 stores are
   * dead — overwritten by the halved value before anything reads them — so
   * folding them into the halving inflates every pool (pool0 by +4 Regulars,
   * and a fatter pool2 widens the x2/x6 clamps below and the merc-offer gate).
   */
  const int iVar4 = (n6bf0 / 10) - diff;         /* 0x53e2 accumulator */
  const int iVar7 = (4 - diff) / 2;
  const int iVar9 = ((n6bd4 + 1) >> 4) + iVar7;  /* 0x53e4 accumulator */
  const int iVar8 = iVar7 + ((n6be4 + 1) >> 5);  /* 0x53e8 accumulator */

  int pool0 = (iVar4 + 9) / 2;
  int pool1 = (iVar9 + 2) / 2;
  int pool3 = (iVar8 + 4) / 2;
  int pool2 = (iVar7 + local_4 + 4) / 2;

  const int cap2x = pool2 * 2;
  if (pool3 > cap2x) {
    pool3 = cap2x;
  }
  if (pool1 > cap2x) {
    pool1 = cap2x;
  }
  {
    const int rem = pool2 * 6 - pool3 - pool1;
    if (pool0 > rem) {
      pool0 = rem;
    }
  }

  ctx->col1->head.backup_force[0] = (uint16_t)(pool0 > 0 ? pool0 : 0);
  ctx->col1->head.backup_force[1] = (uint16_t)(pool1 > 0 ? pool1 : 0);
  ctx->col1->head.backup_force[2] = (uint16_t)(pool2 > 0 ? pool2 : 0); /* 0x53e6 MoW pool */
  ctx->col1->head.backup_force[3] = (uint16_t)(pool3 > 0 ? pool3 : 0); /* 0x53e8 Artillery pool */
}

/* True if name looks like a Man-O-War (Galleon fallback used as REF ship). */
static int ai_king_is_mow(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u || !units_is_sea(units, u->id)) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  if (tname && (strstr(tname, "Man-O-War") || strstr(tname, "Man-o-War") ||
                strstr(tname, "Galleon"))) {
    return 1;
  }
  return 0;
}

static int ai_king_force_total(const uint16_t force[4]) {
  if (!force) {
    return 0;
  }
  return (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];
}

/*
 * The bare `ai_king_spawn_landing` helper that used to sit here (spawn one
 * land unit at a caller-chosen tile, no terrain test) is GONE as of
 * 2026-09-10: audit D6 folded the paid mercenary hire into
 * ai_king_10f0_land, and third-wave lead 2 folded FUN_43f7_2244's twin in
 * after it. Every King landing now goes through 10f0's own colony
 * roulette + water-tile scoring + Man-O-War transport, which is what DOS
 * does; a caller-chosen `(hx, hy+1)` could drop troops in the ocean
 * (bugs.md 261). Do not reintroduce it.
 */

static void ai_king_set_ref_present(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  ai_king_latch_set(col1, AI_KING_REF_PRESENT_BYTE, on ? 1 : 0);
  col1->head.game_options.ref_present = on ? 1 : 0;
}

static void ai_king_set_boycott(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  ai_king_latch_set(col1, AI_KING_BOYCOTT_BYTE, on ? 1 : 0);
}

/*
 * Sync tax-refuse stand-in when cargo boycotts were cleared externally
 * (Jakob Fugger / diplo peace lift — do not touch FF here).
 * Source: fandom Jakob Fugger “all boycotts forgiven”; king_ref refuse +
 * nation.boycott_bitmap. When bitmap==0, clear unknown46[2] so tax may resume.
 */
static void ai_king_sync_boycott_refuse(ColonizeCol1Save* col1, int human) {
  if (!col1 || human < 0 || human >= 4) {
    return;
  }
  if (ai_king_latch_get(col1, AI_KING_BOYCOTT_BYTE) == 0) {
    return;
  }
  if (col1->nation[human].boycott_bitmap == 0) {
    ai_king_latch_set(col1, AI_KING_BOYCOTT_BYTE, 0);
  }
}

/*
 * FUN_43f7_1d42 — the real royal REF budget tick (viceroy_unpacked.c
 * 74846-74907 + OVL07 asm at file offset 0x2c62, re-read 2026-09-06).
 * Runs each PEACETIME turn from FUN_43f7_2424 for the human slot; the asm's
 * first test (`TEST [0x5382],1 -> JMP 2de6` = RETF) makes the whole function
 * a no-op once WoI is declared — the Ghidra-visible @KINGMOBILIZE (0x1320)
 * "wartime" arm at OVL07:2d8a is DEAD CODE in the shipped binary (its only
 * inbound jump sits after that early return; the two (*) xrefs into
 * LAB_2da8 are data refs, not calls).
 *
 *   growth = difficulty*8 + 10, doubled at year>=1600, >=1700, >=1750;
 *   nation.royal_money += growth  (nation+0x22 — the same 32-bit purse the
 *     Europe sales tax, Custom House tax and treasure Crown-share already
 *     credit in this port; 1d42 is its consumer);
 *   when >= 0x708 (1800): buy exactly ONE pool unit this turn:
 *     k=0 Regulars; k=1 if (reg+2)/3 > cavalry; k=3 if reg/4 > artillery;
 *     k=2 if (reg+cavalry+artillery+5)/10 > man-o-war (last write wins);
 *   expeditionary_force[k]++ (0x53da/dc/de/e0);
 *   @KINGBUY (0x1318) with %STRING0 = FUN_43f7_0082(k, crown) unit name —
 *     the crown's 0x543f control byte is never 0, so the crown map applies:
 *     6 Regulars / 8 Cavalry / 0x12 Man-O-War / 0xb Artillery;
 *   royal_money -= 1800.
 *
 * DOS tail (asm 43f7:1e58-1e61, `MOV SI,[BP+param]; MOV AL,[SI+0x9408];
 * SUB AH,AH; ADD [BX+0xe],AX`) — ported 2026-09-06d. `nation+0xe` decoded:
 * the nation record is `n*0x13c + DS:-0x77f8` (nation_flags +0, tax_rate +1,
 * recruit[3] +2, tax_hike_count +5, recruit_count +6, founding_fathers[4] +7,
 * unknown21_pad +0xb, liberty_bells_total +0xc) so +0xe is
 * `liberty_bells_last_turn` — and the DOS write set proves the name: zeroed
 * at each nation's own turn start (FUN_3844_00f2, :58382), zeroed by new-game
 * init (FUN_38fd_6024, :68684), and ADDed alongside +0xc by the bell-spend
 * FUN_4345_0a22 (:73343). Those five sites are the ONLY touches of +0xe in
 * all three decompiled exports (scan of every `<var> = *(int*)0x84fc` alias
 * plus the array form `n*0x13c-0x77ea`, zero hits) — there is no in-game
 * reader, so the bump is observable only in the saved word.
 * `DS:0x9408` = `stuff.free_colonist_counts` (save_format_map row 242,
 * type==0 units). Phase order matches DOS: turn_run_nation_ticks (bells,
 * TURN_PROC_SETUP) runs before turn_run_king_stub (TURN_PROC_KING), so the
 * bump lands last, exactly as 2424's EOT position does in DOS.
 * Caveat kept: founding_fathers_stash_pools_into_col1 overwrites this word
 * with the FF pool while writing our own .SAV, so the bumped value survives
 * to disk only for saves this engine did not stash — a separate, pre-existing
 * divergence (col1_save.h FF_POOL_STASH_MARKER), not this one.
 *
 * (The pre-2026-09-06 stand-in grew pools by tax band on every audience
 * event and set ref_present from a peacetime pool — both invented; removed.)
 */
static void ai_king_1d42_royal_purse(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(col1)) {
    return; /* asm 43f7:1d54: wartime = full no-op */
  }
  ColonizeCol1Nation* nat = &col1->nation[human];
  const int year = (int)col1->head.year;
  int growth = (int)col1->head.difficulty * 8 + 10;
  if (year >= 1600) {
    growth <<= 1;
  }
  if (year >= 1700) {
    growth <<= 1;
  }
  if (year >= 1750) {
    growth <<= 1;
  }
  nat->royal_money += growth;
  if (nat->royal_money < 0x708) {
    return;
  }
  uint16_t* f = col1->head.expeditionary_force;
  int k = 0;
  if (((int)f[0] + 2) / 3 > (int)f[1]) {
    k = 1; /* Cavalry short of a third of the Regulars */
  }
  if ((int)f[0] / 4 > (int)f[3]) {
    k = 3; /* Artillery short of a quarter */
  }
  if (((int)f[0] + (int)f[1] + (int)f[3] + 5) / 10 > (int)f[2]) {
    k = 2; /* Man-O-War short of a tenth of the land force */
  }
  f[k]++;
  static const char* k_pool_name[4] = {"Regulars", "Cavalry", "Man-O-War", "Artillery"};
  if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
    snprintf(ctx->status, ctx->status_size,
             "King increases military spending. %s added to royal expeditionary force.",
             k_pool_name[k]);
  }
  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = k_pool_name[k];
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "KINGBUY",
      &tok,
      "King increases military spending.  %STRING0 added to royal "
      "expeditionary force.  Colonial leaders express alarm.",
      body,
      sizeof(body)
    );
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_INFO, human,
      ai_king_crown_nation_col1(col1, human), k, NULL, body
    );
  }
  nat->royal_money -= 0x708;
  /* asm 43f7:1e58 tail — 16-bit ADD, wraps like DOS. */
  nat->liberty_bells_last_turn =
    (uint16_t)(nat->liberty_bells_last_turn + (uint16_t)col1->stuff.free_colonist_counts[human]);
}

int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  /*
   * FUN_43f7_0004 (viceroy_unpacked_2.c:72202-72239) verbatim: walk every
   * colony record, and for the ones owned by `nation_id` accumulate
   * `pop` (colony +0x1f) and `pop * FUN_15eb_0274()` (the colony SoL%), then
   * divide the second by the first. No colonies (or a zero pop sum) returns
   * the untouched accumulator, i.e. 0.
   *
   * NO `liberty_bells_total / 4` STAND-IN (bugs.md 430, "rival monarchs
   * considering granting independence ... in 1530"). That number is not a
   * percentage at all — it is the nation's lifetime bell total, which real
   * DOS saves carry in the MILLIONS (original_saves/valid-lategame-saves/
   * COLONY00.SAV: nation 3 = 34,605,631), so every hit of the old fallback
   * clamped straight to 100% rebel sentiment. Section D of the year end then
   * read `rebel_sentiment * census_pop_proxy / 100` as the rival's rebel
   * count and fired @OTHERMIGHT as soon as the census passed the band floor.
   * DOS's FUN_15eb_0274 (viceroy_unpacked_2.c:8167-8190) returns 0 when the
   * rebel divisor is 0 ("nothing has accumulated"), and 43f7_0004 has no
   * nation-level fallback whatsoever; colony_prod_sol_percent already
   * deleted the identical stand-in from the colony-screen path.
   *
   * DOS calibration for the same expression, from the shipped late-game
   * saves (difficulty 2, threshold 60, band floor 40): 1505/1550 fixtures
   * read rebel_sentiment 0-1 with census 2-3 (v = 0); the 1680 save reads
   * 23/51/82 with census 51/85/128 (v = 11/43/100) and carries
   * rebellion_pct_last_notified = 43 on the nation that is inside the band.
   * The popup belongs to a mature AI empire, not to 1530.
   */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    uint64_t pop_sum = 0;
    uint64_t sol_sum = 0;
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != nation_id) {
        continue;
      }
      /* DOS adds colony +0x1f as-is (a 0-pop record weighs 0), no min-1. */
      const uint64_t pop = (uint64_t)c->population;
      int sol = 0;
      if (c->rebel_divisor > 0) {
        sol = (int)((c->rebel_dividend * 100u) / c->rebel_divisor);
      }
      if (sol < 0) {
        sol = 0;
      }
      sol += founding_fathers_bolivar_sol_bonus(ctx->col1, nation_id);
      if (sol > 100) {
        sol = 100;
      }
      sol_sum += (uint64_t)sol * pop;
      pop_sum += pop;
    }
    if (pop_sum > 0) {
      return (int)(sol_sum / pop_sum);
    }
  }
  return 0;
}

/*
 * FUN_43f7_1eca colony-SoL bias (catalog: promote when colony SoL>50%).
 * Prefer Col1 rebel_dividend/divisor at the unit tile; else nation SoL (0004).
 * King promote path only — not FF Washington mass-promote / combat upgrade.
 */
static int ai_king_colony_sol_at(const ColonizeTurnContext* ctx, int nation_id, int x, int y) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != nation_id) {
        continue;
      }
      if ((int)c->x != x || (int)c->y != y) {
        continue;
      }
      /* FUN_15eb_0274: divisor 0 means nothing has accumulated = 0%. The old
       * `div = divisor ? divisor : 1` guard turned a fresh record into
       * `dividend * 100` instead. */
      int sol = 0;
      if (c->rebel_divisor > 0) {
        sol = (int)(((uint64_t)c->rebel_dividend * 100ull) / (uint64_t)c->rebel_divisor);
      }
      if (sol < 0) {
        sol = 0;
      }
      /* FUN_15eb_0274 Bolivar display boost (same as colony_prod_sol_percent). */
      sol += founding_fathers_bolivar_sol_bonus(ctx->col1, nation_id);
      if (sol > 100) {
        sol = 100;
      }
      return sol;
    }
  }
  return ai_king_sol_percent(ctx, nation_id);
}

/*
 * DOS 0x5382 bit0 (head.game_options.woi) is the real WoI latch — read that,
 * not the unknown46[0] stand-in: unknown46[0..5] alias DOS price_group_state
 * words 0–2 (col1_save.h), so on a real DOS-authored save unknown46[0] holds
 * live price data, not a war flag, and is nonzero almost every game. A save
 * never touched by ai_king_set_independence (i.e. every save that didn't
 * come out of this port's own turn_end) would misreport WoI as declared.
 * Set only on declare (ai_king_try_declare / ai_king_set_independence) when
 * SoL≥AI_KING_DECLARE_SOL_MIN — never by restless chrome.
 */
int ai_king_independence_declared(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  return col1->head.game_options.woi != 0;
}

static void ai_king_set_independence(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  /* Legacy Linux mirror; authoritative latch is game_options.woi. */
  ai_king_latch_set(col1, AI_KING_WOI_BYTE, on ? 1 : 0);
  col1->head.game_options.woi = on ? 1 : 0;
  if (on) {
    col1->head.event.colony_burning = 1; /* chrome hint */
  }
}

/*
 * Pack/unpack the KING_AUDIENCE popup payload: the tax delta that was
 * actually applied (1..8, always positive — only raises ever reach the
 * tea-party choice) and the roulette-picked cargo (0..15) that a tea party
 * would boycott/confiscate. Small ints, trivially reversible.
 */
static int ai_king_teaparty_payload(int applied, int cargo) {
  return applied * 100 + cargo;
}
static void ai_king_teaparty_payload_parts(int payload, int* out_applied, int* out_cargo) {
  if (out_applied) {
    *out_applied = payload / 100;
  }
  if (out_cargo) {
    *out_cargo = payload % 100;
  }
}

/* Defined with the WoI counters below; used by the audience colony gate. */
static int ai_king_human_colonies(const ColonizeTurnContext* ctx, int human);

/*
 * FUN_38fd_5be8: King-audience favor-score ladder → signed tax-rate delta.
 * Real DOS gating/formula (no invented Accept/Refuse-whether-it-happens
 * gate here — see divergence note above ai_king_tax_event for history).
 * Source: original_sources_decompiled/viceroy_unpacked.c:68420.
 *
 * Gate 0 (DOS's FIRST line, raw 68433-68435: `if (*(char *)(iVar1 + -0x6d68)
 * == '\0') return 0;`): the audience nation owns at least one colony.
 * −0x6d68 = DS:0x9298 = stuff.colony_counts[] (save_format_map.md row 241),
 * the same table ai_king_rank_nations_0218 reads. Ported 2026-09-10 (audit
 * D9) — a human with zero colonies used to keep drawing tax audiences.
 * Gate: turn counter (DS:0x538e, Linux ctx->turn_number) >= 30; interval
 * base 18/15/12/9 by year band (>1600/>1700/>1750), narrowed by
 * difficulty (DOS: only when the audience's own nation *is* the human —
 * this port only ever rolls the audience for the human's nation, so that
 * gate is always true here); modulo turn counter; skip if tax_rate > 85.
 *
 * Score = RNG(1,1000) + (rebel_sentiment_report*2 − tax_rate)*5
 *       + treasury/100 + this-nation SoL% + turn/30.
 * DOS reads a cached per-nation SoL% table at DS:(nation−0x6bf0) for the
 * last term; this port has no such cache, so it recomputes the same value
 * live via ai_king_sol_percent — a documented substitution, not a guess
 * (see docs pointer in the file header).
 *
 * Ladder: score<100 → cut = −min(RNG(2,5), tax_rate), but no event at all
 * if that cut would be 0 (tax already 0%); 100≤score<650 and streak<30 →
 * +1 (streak++); score>949 → +3/+4 (score<1100) or +5..+8; else → +2
 * (covers 650..949, and the streak≥30 fallback out of the +1 band).
 * Returns 1 and writes king_audience_tax_delta + *out_delta when an event
 * fires; 0 (no state touched) when the gate fails or the cut degenerates.
 */
static int ai_king_audience_roll(ColonizeTurnContext* ctx, int human, int* out_delta) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->rng || human < 0 || human >= 4) {
    return 0;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Nation* nat = &col1->nation[human];
  /*
   * DOS's first gate: colony_counts[audience nation] != 0. The census mirror
   * is refreshed every turn (turn.c → col1_stuff_census_refresh_colony_counts),
   * but a synthetic fixture can leave the whole window blank, so a zero row
   * falls back to the live colony count before the early-out bites.
   */
  {
    int owned = (int)col1->stuff.colony_counts[human];
    if (owned == 0) {
      owned = ai_king_human_colonies(ctx, human);
    }
    if (owned == 0) {
      return 0;
    }
  }
  const uint32_t turn = ctx->turn_number ? *ctx->turn_number : 0u;
  if (turn < 30) {
    return 0;
  }
  const int year = ctx->game_year ? (int)*ctx->game_year : 1492;
  int interval_base = 18;
  if (year > 1600) {
    interval_base = 15;
  }
  if (year > 1700) {
    interval_base -= 3;
  }
  if (year > 1750) {
    interval_base -= 3;
  }
  const int diff = col1->head.difficulty;
  const int crown_adjust = diff - 2; /* "is human" branch — always true here */
  const int interval = interval_base - 2 * crown_adjust;
  if (interval <= 0 || (int)(turn % (uint32_t)interval) != 0) {
    return 0;
  }
  if (nat->tax_rate > 85) {
    return 0;
  }

  /*
   * 38fd_5be8 score (viceroy_unpacked.c:68460-68467). The 4th term reads
   * DS:nation-0x6bf0 — that is 0x9410 census_pop_proxy (population proxy),
   * NOT a per-nation SoL cache as this port assumed until 2026-09-06 (the
   * older king_ref.md "same value, no stored cache" note was wrong; the
   * live FUN_4962_0018 census now backs the real field).
   */
  const int score =
    dos_rng_range(ctx->rng, 1, 1000) +
    (col1->head.rebel_sentiment_report * 2 - (int)nat->tax_rate) * 5 +
    (int)(europe_nation_gold(ctx->europe, col1, human) / 100u) +
    (int)col1->stuff.census_pop_proxy[human] +
    (int)(turn / 30);

  int delta;
  if (score < 100) {
    const int roll = dos_rng_range(ctx->rng, 2, 5);
    int cut = (roll < (int)nat->tax_rate) ? roll : (int)nat->tax_rate;
    if (cut < 1) {
      return 0; /* DOS: no audience event when tax is already 0% */
    }
    delta = -cut;
  } else if (score < 650 && col1->head.king_audience_streak < 30) {
    delta = 1;
    if (col1->head.king_audience_streak < 255) {
      col1->head.king_audience_streak++;
    }
  } else if (score > 949) {
    delta = (score < 1100) ? dos_rng_range(ctx->rng, 3, 4) : dos_rng_range(ctx->rng, 5, 8);
  } else {
    delta = 2;
    /* Narrative-line reroll only (avoid repeating the last text pick);
     * no numeric effect on delta. */
    int pick;
    do {
      pick = dos_rng_range(ctx->rng, 1, 8);
    } while (pick == col1->head.king_audience_last_pick);
    col1->head.king_audience_last_pick = (uint8_t)pick;
  }

  nat->king_audience_tax_delta = (int16_t)delta;
  if (out_delta) {
    *out_delta = delta;
  }
  return 1;
}

/*
 * FUN_38fd_3dc8 core clamp: tax_rate += delta, floored so it can never go
 * below 0%, ceiled at 75% (excess trimmed back out of the applied delta).
 * *out_applied receives the delta actually applied post-clamp — the value
 * the village-goods/tea-party branch below reverts on a "hold a tea
 * party" choice.
 */
/*
 * Same clamp as ai_king_audience_apply_delta, without writing: the tax
 * audience needs to know what a delta WOULD come to before the player has
 * answered (bugs.md — the raise must not be in the save while the popup that
 * proposes it is still on screen).
 */
static int ai_king_audience_preview_delta(const ColonizeCol1Nation* nat, int delta) {
  int applied = delta;
  if (applied < 0) {
    const int mag = -applied;
    if (mag > (int)nat->tax_rate) {
      applied = -(int)nat->tax_rate;
    }
  }
  const int new_tax = (int)nat->tax_rate + applied;
  if (new_tax > 75) {
    applied -= (new_tax - 75);
  }
  return applied;
}

static void ai_king_audience_apply_delta(ColonizeCol1Nation* nat, int delta, int* out_applied) {
  int applied = delta;
  if (applied < 0) {
    const int mag = -applied;
    if (mag > (int)nat->tax_rate) {
      applied = -(int)nat->tax_rate;
    }
  }
  int new_tax = (int)nat->tax_rate + applied;
  if (new_tax > 75) {
    applied -= (new_tax - 75);
    new_tax = 75;
  }
  if (new_tax < 0) {
    new_tax = 0; /* safety net; the floor clamp above already prevents this */
  }
  nat->tax_rate = (uint8_t)new_tax;
  if (out_applied) {
    *out_applied = applied;
  }
}

/*
 * Build the Europe bid-eligible cargo mask for the village-goods pick
 * (FUN_38fd_3dc8's local_7a price weighting stand-in — see
 * ai_king_pick_dump_goods_cargo). *out_bids, when set, points at a
 * COLONIZE_CARGO_COUNT-sized caller-owned buffer that stays valid only as
 * long as bid_buf does.
 */
static uint16_t ai_king_teaparty_candidate_mask(
  const ColonizeTurnContext* ctx,
  int human,
  int bid_buf[COLONIZE_CARGO_COUNT],
  const int** out_bids
) {
  uint16_t candidate_mask = 0;
  *out_bids = NULL;
  if (!ctx) {
    return 0;
  }
  /*
   * bugs.md: a cargo only enters the roulette if one of this nation's colonies
   * actually holds some of it — you cannot dump 0 tons of anything in protest.
   * That is DOS's own rule: FUN_38fd_3dc8 fills aiStack_cc[c] with the largest
   * stock of c across the human's colonies and skips every cargo whose entry
   * stayed 0 (`... && aiStack_cc[local_ac] != 0`), both when summing the
   * roulette weights and when walking them. The port had keyed the mask off
   * Europe's bid instead, which let it name a good no colony was storing.
   */
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    bid_buf[c] = 0;
  }
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* col = &ctx->colonies->colonies[i];
      if (!col->active || col->nation_id != human) {
        continue;
      }
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        if (col->stock[c] > 0) {
          candidate_mask = (uint16_t)(candidate_mask | (uint16_t)(1u << c));
        }
      }
    }
  }
  /* Weights stay the Europe price (DOS's local_7a price roll stand-in). */
  if (ctx->europe) {
    const EuropeScreen* eu = ctx->europe;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      bid_buf[c] = (c < eu->cargo_count) ? eu->cargo[c].bid : 0;
      if (bid_buf[c] < 1) {
        bid_buf[c] = 1; /* stocked but unsellable: eligible, just least likely */
      }
    }
    *out_bids = bid_buf;
  }
  return candidate_mask;
}

/*
 * Tea-party choice apply: a human answered the KING_AUDIENCE CHOICE with
 * "hold a tea party" after a real tax raise. FUN_38fd_3dc8: revert the
 * just-applied hike, boycott the roulette-picked cargo, confiscate its
 * stock (ai_king_enqueue_teaparty_ok dumps up to 100 tons from the
 * richest human colony — thin stand-in for the colony-array seize into
 * DOS's own royal-stock pile, real field unresolved, see file header).
 * Used by both the human CHOICE-apply path and the no-popups auto path.
 */
static void ai_king_tax_teaparty(ColonizeTurnContext* ctx, int human, int cargo) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  /*
   * No revert to do any more: ai_king_tax_hike_apply defers the raise until
   * Accept, so refusing simply never commits it (bugs.md). DOS's
   * apply-then-revert reaches the same rate.
   */
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | (uint16_t)(1u << cargo));
  ai_king_set_boycott(ctx->col1, 1);

  if (ctx->status && ctx->status_size) {
    snprintf(
      ctx->status,
      ctx->status_size,
      "Audience: tea party! Tax stays at %u%%. %s boycotted in Europe.",
      nat->tax_rate,
      ai_king_cargo_name(cargo)
    );
  }
  if (ai_king_human_popups(ctx)) {
    ai_king_enqueue_teaparty_ok(ctx, human, cargo);
  }
}

static void ai_king_apply_dump_goods_choice(ColonizeTurnContext* ctx, int human, int cargo) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | (uint16_t)(1u << cargo));
  if (ctx->status && ctx->status_size) {
    char cargos[96];
    if (ai_king_format_boycott_cargos(cargos, sizeof(cargos), nat->boycott_bitmap)) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Dump goods: boycotted in Europe: %s.",
        cargos
      );
    }
  }
  /* GAME.TXT @TEAPARTY for the chosen dump cargo (thin 3dc8 stock dump). */
  if (ai_king_human_popups(ctx)) {
    ai_king_enqueue_teaparty_ok(ctx, human, cargo);
  }
}

/*
 * FUN_38fd_5be8 + FUN_38fd_3dc8: real King-audience tax-rate-change event.
 * Ported 2026-08-19, replacing the earlier invented "Accept/Refuse gates
 * whether the hike happens" design (see docs/archive/mysteries_catalog.md,
 * king_audience_tax_delta, for the divergence this replaces).
 *
 * Real DOS shape: the audience fires on a turn-counter interval (no
 * spring-only restriction — that was also invented; see
 * original_sources_decompiled/viceroy_unpacked.c:68539 FUN_38fd_5e52, the
 * caller, which has no season gate either). A delta is always rolled and
 * applied unconditionally (ai_king_audience_roll + ai_king_audience_apply_
 * delta) — cuts and clamped-away deltas are never asked about. Only a
 * genuine positive applied delta (a real raise) can lead to a village-
 * goods popup, and only when an eligible cargo/colony candidate exists;
 * that popup's real semantics are "keep it" vs. "hold a tea party", which
 * REVERTS the raise just applied and boycotts the picked cargo — it does
 * not gate whether the raise happens in the first place.
 *
 * The per-cargo boycott-holds-future-hikes behavior from the old design
 * (unknown46[2] gating this function) is not real DOS (5be8/3dc8 never
 * check it) and has been dropped; nation.boycott_bitmap / the tea-party
 * flag are still set/read for presentation and for the Fugger-clears-
 * boycotts sync, just no longer block the audience interval gate.
 */
static void ai_king_tax_hike_apply(ColonizeTurnContext* ctx, int human, int delta);

static void ai_king_tax_event(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  /* Fugger / external bitmap clear → drop the boycott presentation flag. */
  ai_king_sync_boycott_refuse(ctx->col1, human);

  int delta = 0;
  if (!ai_king_audience_roll(ctx, human, &delta)) {
    return; /* no audience this turn: interval gate, or degenerate 0% cut */
  }
  ai_king_tax_hike_apply(ctx, human, delta);
}

/*
 * FUN_38fd_3dc8 body for an explicit delta — the audience roll above and
 * the @KINGFRIGATE acceptance (3844_00f2 → 3dc8(KINGTAX, 10)) both land
 * here: clamp + apply, then the Kiss-the-ring / Tea-party dialog.
 */
/*
 * Write a settled tax delta: rate, the Europe mirror, and the REF-growth
 * approximation. Split out of ai_king_tax_hike_apply so the CHOICE path can
 * defer all three until the player has answered (bugs.md).
 */
static void ai_king_tax_commit(ColonizeTurnContext* ctx, int human, int delta) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4 || delta == 0) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  int applied = 0;
  ai_king_audience_apply_delta(nat, delta, &applied);
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  /* (2026-09-06) The invented "grow REF pools on every audience event"
   * stand-in was removed: FUN_43f7_1d42's real mechanic — royal_money
   * stipend + 1800-gold pool buys — now runs per peacetime turn from
   * ai_king_nation_turn (ai_king_1d42_royal_purse), independent of tax
   * audiences. */
}

static void ai_king_tax_hike_apply(ColonizeTurnContext* ctx, int human, int delta) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  /*
   * bugs.md: DOS's 3dc8 applies the delta and then offers keep-vs-revert, so
   * the raise is briefly real while the dialog asks about it — the port
   * showed the new rate in Europe/status before the player had answered.
   * The arithmetic outcome of both orderings is identical (accept = raised,
   * tea party = not raised), so the hike is now previewed here and committed
   * in ai_king_apply_popup_result on Accept. Everything DOS never asks about
   * (cuts, fully clamped deltas, the no-cargo fallback) still commits at
   * once.
   */
  const int applied = ai_king_audience_preview_delta(nat, delta);

  if (applied < 0) {
    ai_king_tax_commit(ctx, human, delta);
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "Audience: the King lowers taxes to %u%%.", nat->tax_rate);
    }
    if (ai_king_human_popups(ctx)) {
      char body[AI_POPUP_BODY_LEN];
      snprintf(body, sizeof(body),
               "The King, moved by your poverty, lowers taxes to %u%%.", nat->tax_rate);
      if (ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human,
                                  ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human), (int)nat->tax_rate,
                                  NULL, body)) {
        /* DOS 3dc8's message-only arm (38fd:402a `MOV word [0x1f5c],0x8`,
         * then FUN_281f_03fe) wears the King flair too — the port had it on
         * the @KINGTAX choice only, so a tax CUT lost the portrait. */
        ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
      }
    }
    return;
  }

  if (applied == 0) {
    return; /* delta rolled but fully clamped away (already at 75%) */
  }

  /* applied > 0: a real hike is on the table. */
  const int proposed = (int)nat->tax_rate + applied;

  int bid_buf[COLONIZE_CARGO_COUNT];
  const int* bids = NULL;
  const uint16_t candidate_mask = ai_king_teaparty_candidate_mask(ctx, human, bid_buf, &bids);
  int picked = ctx->rng
    ? ai_king_pick_dump_goods_cargo(nat->boycott_bitmap, candidate_mask, ctx->rng, bids)
    : -1;
  if (picked < 0 && ctx->rng && ai_king_human_popups(ctx) && candidate_mask != 0) {
    /*
     * Every stocked cargo is already boycotted: drop only the boycott
     * exclusion, never the "a colony actually holds some" one — re-threatening
     * a good already under boycott is harmless, naming one nobody stores is
     * not (bugs.md: you cannot dump 0 tons in protest).
     */
    picked = ai_king_pick_dump_goods_cargo(0, candidate_mask, ctx->rng, bids);
  }

  if (picked < 0) {
    /* Only reachable with no RNG (tests) or popups disabled for this
     * nation — DOS's own choice UI has nothing to drive here either, so
     * there is nothing to defer: the hike stands. */
    ai_king_tax_commit(ctx, human, delta);
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "Audience: the King raises taxes to %u%%.", nat->tax_rate);
    }
    if (ai_king_human_popups(ctx)) {
      char body[AI_POPUP_BODY_LEN];
      snprintf(body, sizeof(body), "The King raises taxes to %u%%.", nat->tax_rate);
      sound_play(0x56); /* FUN_38fd_3dc8 tax raise (COLDIG 9 cheering) */
      if (ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human,
                                  ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human), (int)nat->tax_rate,
                                  NULL, body)) {
        /* Same 38fd:402a arm — DOS reaches it when no cargo is eligible for
         * the tea party, and still stands the King beside the message. */
        ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
      }
    }
    return;
  }

  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    /* DOS 3dc8: NUMBER0 = |applied delta| (FUN_281f_09ae(0, abs(param_2))
     * after both clamps), not a constant 1 — bugs.md: popup claimed "raise
     * by 1%" for a 7% hike. */
    tok.number0 = applied;
    tok.has_number0 = true;
    /* @KINGTAX "The tax rate is now {%NUMBER1%%}" — the total the raise would
     * come to. The rate itself is still the old one until Accept. */
    tok.number1 = proposed;
    tok.has_number1 = true;
    tok.string0 = ai_king_cargo_name(picked);
    /* @TAXOPTIONS "Hold '{%STRING3 Party}.'" — DOS names it after the colony
     * that will be raided plus the boycotted cargo, not "Tea". */
    char party[96];
    ai_king_teaparty_party_name(
      party, sizeof(party), ai_king_teaparty_colony(ctx, human, picked), picked);
    tok.string3 = party;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "KINGTAX",
      &tok,
      "The King raises taxes. Kiss pinky ring, or hold a tea party and boycott a good?",
      body,
      sizeof(body)
    );
    char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const ColonizeMsgSection* taxopt = assets_msg_find(ctx->messages, "TAXOPTIONS");
    int nch = 0;
    if (taxopt) {
      char raw_choices[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
      nch = popup_msg_choices(taxopt, raw_choices, AI_POPUP_CHOICE_MAX);
      for (int i = 0; i < nch; ++i) {
        popup_msg_apply_tokens(choice_buf[i], sizeof(choice_buf[i]), raw_choices[i], &tok);
      }
    }
    const char* labels[2];
    const int ids[] = {AI_KING_CHOICE_ACCEPT, AI_KING_CHOICE_REFUSE};
    if (nch >= 2) {
      labels[0] = choice_buf[0];
      labels[1] = choice_buf[1];
    } else {
      labels[0] = "Kiss pinky ring.";
      /* %.49s bounds the party name: the literal costs 14 chars, leaving 49
       * plus NUL of the 64-byte choice slot. */
      snprintf(choice_buf[1], sizeof(choice_buf[1]), "Hold '%.49s Party.'", party);
      labels[1] = choice_buf[1];
    }
    sound_play(0x3e); /* FUN_38fd_3dc8 38fd:4022/4068: royal-audience tune */
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_AUDIENCE, human,
                                    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
                                    ai_king_teaparty_payload(applied, picked),
                                    NULL, body, labels, ids, 2)) {
      /* bugs.md: DOS's 3dc8 dialog sets DS:0x1f5c = 8 — the animated King
       * flair stands beside the tax audience. */
      ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
      return; /* effect deferred to ai_king_apply_popup_result */
    }
    /* Queue full — fall through to auto resolve. */
  }

  /*
   * Auto path (no popups attached): DOS's tea-party choice is inherently
   * player-interactive — there is no documented AI/auto answer. Stand-in
   * heuristic (invented, not a decode): tea-party when the hike pushes tax
   * to a high band and SoL/liberty bells suggest the colonies would balk.
   */
  const int sol = ai_king_sol_percent(ctx, human);
  const int auto_teaparty =
      (proposed >= AI_KING_BOYCOTT_TAX_MIN) &&
      (sol >= AI_KING_BOYCOTT_SOL_MIN || nat->liberty_bells_total >= AI_KING_BOYCOTT_BELLS_MIN);
  if (auto_teaparty) {
    ai_king_tax_teaparty(ctx, human, picked);
    return;
  }
  ai_king_tax_commit(ctx, human, delta);
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size,
             "Audience: the King raises taxes to %u%%.", nat->tax_rate);
  }
}

/*
 * FUN_43f7_0218 — War of the Spanish Succession (viceroy_unpacked.c
 * 73601-73712). Fires PRE-WoI, the first time the human's SoL passes 49%
 * (caller gate `0x31 < SoL && *0x53d2 < 0`), and 1a26 falls back to it at
 * declare. It frees the slot the King will borrow:
 *   - rank the 4 powers ascending by ship_counts*3 + colony_counts*2 +
 *     census_pop_proxy (raw 73627-73629, ai_king_rank_nations_0218);
 *   - weakest AI (local_c) is merged INTO the next-weakest AI (local_a):
 *     its colonies change owner (rebel accumulators +0xc2/+0xc4 zeroed),
 *     its units transfer when standing in a colony and are DESPAWNED in
 *     the field (`0302(x,y)==0 → 0808`), map owner/vis and tribe-alarm
 *     nibbles are remapped (thin: skipped here);
 *   - @SUCCESSION (0x128c) announces the Treaty of Utrecht;
 *   - merged slot control=2, DS:0x53d2 = the vacated slot.
 * THIS is why the King must never inherit a live nation's estate (bugs.md
 * follow-up: the port's fixed slot borrow handed the King Quebec + 5
 * Caravels that were simply France's).
 */
static void ai_king_succession(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  if (col1->head.crown_nation_id >= 0) {
    return; /* slot already vacated */
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  /* FUN_43f7_0218: rank ascending by ships*3 + colonies*2 + census — the
   * real DOS table (was an invented per-colony pop score; see
   * ai_king_rank_nations_0218). */
  int order[4];
  ai_king_rank_nations_0218(col1, order);
  int merged = -1;  /* DOS local_c: weakest AI — the slot the King takes */
  int heir = -1;    /* DOS local_a: next-weakest AI — receives the estate */
  for (int i = 0; i < 4; ++i) {
    const int n = order[i];
    if (n == human) {
      continue;
    }
    if (merged < 0) {
      merged = n;
    } else if (heir < 0) {
      heir = n;
    }
  }
  if (merged < 0 || heir < 0) {
    return;
  }
  /* Colonies: owner swap + rebel accumulators zeroed (DOS +0xc2/+0xc4). */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == merged) {
        c->nation_id = heir;
      }
    }
  }
  if (col1->colony) {
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      ColonizeCol1Colony* c = &col1->colony[i];
      if ((int)c->nation_id == merged) {
        c->nation_id = (uint8_t)heir;
        c->rebel_dividend = 0;
        c->rebel_divisor = 0;
      }
    }
  }
  /* Units: in a colony → transfer to the heir; in the field/at sea → gone
   * (DOS 0302==0 → 0808 despawn). */
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != merged) {
        continue;
      }
      const int cid =
        ctx->colonies ? colonies_id_at(ctx->colonies, u->x, u->y) : -1;
      if (cid >= 0) {
        units_set_nation(u, heir);
      } else {
        (void)units_despawn(ctx->units, u->id);
      }
    }
  }
  col1->player[merged].control = 2; /* withdrawn — the King's future slot */
  col1->head.crown_nation_id = (int16_t)merged;
  /* @SUCCESSION Treaty of Utrecht announcement. */
  if (ai_king_human_popups(ctx)) {
    static const char* k_country[4] = {"England", "France", "Spain", "Netherlands"};
    static const char* k_adj[4] = {"English", "French", "Spanish", "Dutch"};
    const char* ceder = k_country[merged];
    const char* domain = col1->player[merged].country_name[0]
                           ? col1->player[merged].country_name
                           : "its colonies";
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ceder;
    tok.string1 = domain;
    tok.string2 = k_adj[heir];
    tok.string3 = k_adj[merged];
    char body[AI_POPUP_BODY_LEN];
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "War of the Spanish Succession ends in Europe! %s, ravaged by war, agrees "
      "to cede %s to the %s. Treaty of Utrecht specifies that all %s possessions "
      "in the New World now fall under %s rule.",
      ceder, domain, k_adj[heir], k_adj[merged], k_adj[heir]
    );
    popup_msg_fill(ctx->messages, "SUCCESSION", &tok, fallback, body, sizeof(body));
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_INFO, human, merged, heir, NULL, body
    );
  }
  if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
    snprintf(ctx->status, ctx->status_size,
             "War of the Spanish Succession: %s possessions pass to the %s.",
             col1->player[merged].country_name[0] ? col1->player[merged].country_name
                                                  : "foreign",
             (heir >= 0 && heir < 4)
               ? (const char*[]){"English", "French", "Spanish", "Dutch"}[heir]
               : "heir");
  }
}

/*
 * FUN_43f7_1a26 declare body (after 2564 confirm / auto).
 * Fallback-seeds REF by difficulty only when it is still all zero; seeds the
 * 10f0 foreign-intervention pools via ai_king_seed_backup_force_1a26;
 * withdraws other Euros; thin 160a rename; unknown46[5] congress.
 */
static void ai_king_do_declare(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /* DOS 1a26: `if (*0x53d2 < 0) 0364` — run the succession merger now if the
   * SoL>49 trigger never fired, so the King borrows an EMPTY slot instead of
   * inheriting a live nation's colonies and ships (bugs.md follow-up). */
  ai_king_succession(ctx);
  ai_king_set_independence(ctx->col1, 1); /* WoI: unknown46[0] if not already */
  ai_king_write_rival_nation_slots(ctx->col1, human);
  /* FUN_43f7_2564 congress-confirm stand-in. */
  ai_king_latch_set(ctx->col1, AI_KING_CONGRESS_BYTE, 1);
  /*
   * DOS FUN_43f7_1a26 writes only the foreign-intervention pool
   * (0x53e2..0x53e8 backup_force) here — the Expeditionary Force itself is
   * whatever accumulated since the new-game seed (75c2:360b: regulars
   * 8*diff+15 etc.) plus the tax-event growth; re-seeding it at declare
   * (an earlier stand-in from when new games started at 0) shrank an
   * accumulated force. Fallback-seed only if it is still all zero (a save
   * from a build without the new-game seed).
   */
  if (ctx->col1->head.expeditionary_force[0] == 0 &&
      ctx->col1->head.expeditionary_force[1] == 0 &&
      ctx->col1->head.expeditionary_force[2] == 0 &&
      ctx->col1->head.expeditionary_force[3] == 0) {
    const int diff = ctx->col1->head.difficulty;
    ctx->col1->head.expeditionary_force[0] = (uint16_t)(8 * diff + 15);
    ctx->col1->head.expeditionary_force[1] = (uint16_t)(5 * (diff + 1));
    ctx->col1->head.expeditionary_force[2] = (uint16_t)(3 * diff + 2);
    ctx->col1->head.expeditionary_force[3] = (uint16_t)(6 * diff + 2);
  }
  ai_king_seed_backup_force_1a26(ctx, human);
  /* FUN_43f7_1a26 right after the pool seed: latch the declaration year into
   * DS:0x53a7/0x53a8 (year/100, year%100 — the king-audience RNG bytes,
   * dead once the King is gone; FUN_41f2_0092's early-revolution bonus reads
   * them back) and zero the human's liberty_bells_total so bells accrue
   * "since declaring" (score's REF-present bells line). */
  ctx->col1->head.king_audience_streak = (uint8_t)(ctx->col1->head.year / 100);
  ctx->col1->head.king_audience_last_pick = (uint8_t)(ctx->col1->head.year % 100);
  ctx->col1->nation[human].liberty_bells_total = 0;
  ai_king_set_ref_present(ctx->col1, 1);
  /*
   * bugs.md: nothing ever set the WAR bit between the rebel and the
   * crown's borrowed slot, so a player Frigate (or Man-O-War/Privateer)
   * attacking a REF ship bounced with "At peace — cannot attack". Raw
   * ai_diplo_or_both, not declare_war_ctx: the WoI is its own regime — no
   * Franklin gate, no peer embargo/tax chrome.
   */
  /*
   * FUN_43f7_1a26 (viceroy_unpacked.c:74832-74833), decoded 2026-09-06d:
   *   caseD_10(human, crown, 0x22)      -> FUN_15b3_0066 = or_both
   *   FUN_281f_0a10(human, crown, 0x40) -> FUN_15b3_00d0 = clear_both
   * With the 2026-08-27 T1.19 bit map (ai_diplo.h) 0x22 is exactly
   * WAR(0x02)|MET(0x20) — the port's existing write was already DOS-exact;
   * the missing half was the PEACE(0x40) clear. king_ref's old note read
   * "0x40 MET" off the stale pre-T1.19 map, which is why this looked like a
   * conflict with the user-verified WAR|MET set. Clearing PEACE cannot
   * regress the rebel-attacks-REF fix (units.c's gate short-circuits on WAR
   * before ai_contact's @HAVETREATY prompt ever reads PEACE) and it stops the
   * crown pair from reading "treaty signed" in the F8 report mid-war.
   */
  {
    const int crown_slot = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
    ai_diplo_or_both(ctx->col1, human, crown_slot, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_MET));
    ai_diplo_clear_both(ctx->col1, human, crown_slot, (uint8_t)AI_DIPLO_PEACE);
  }
  /* bugs.md: no landing on the declaration turn itself. */
  ai_king_latch_set(ctx->col1, AI_KING_REF_WAVE_WAIT_BYTE, 1);
  /*
   * FUN_43f7_0108 (eliminate nation), called from FUN_43f7_1a26 for every
   * nation that is neither the declaring human nor the crown proxy
   * (DS:0x5398 / 0x53d2 gate) -- WoI narrows the world to rebel vs REF, so
   * the other Euro powers are fully removed: diplomatic status withdrawn
   * *and* every unit they own destroyed (colonies are untouched by 0108
   * itself -- DOS leaves them ownerless/inert once their nation's
   * status=2). Linux already set control=2 here ("withdrawn"); the
   * unit-scrub half was missing. Crown-nation units (the REF spawns below)
   * must survive.
   */
  const int crown_fold = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  /*
   * bugs.md 234: DOS 1a26 stores the crown slot in DS:0x53d2 and sets its
   * control byte to 1 (`*(0x53d2*0x34+0x543f)=1`). The port never wrote
   * head.crown_nation_id, so a save exported mid-WoI reached DOS with
   * 0x53d2 = -1; DOS then picked its own crown — sometimes the very nation
   * the port had cached as the intervention ally (0x53d4), which made the
   * intervention force render as "Tory".
   */
  ctx->col1->head.crown_nation_id = (int16_t)crown_fold;
  /*
   * bugs.md 235: DOS 1a26 zeroes the bell pool (`*(pool+0xc)=0`) — the FF
   * election in progress is cancelled and bells start accruing toward the
   * foreign intervention instead.
   */
  founding_fathers_consume_woi_bell_pool(human);
  ctx->col1->nation[human].next_founding_father = -1;
  for (int n = 0; n < 4; ++n) {
    if (n == human) {
      continue;
    }
    /* Crown slot stays a live AI combatant (DOS control 1), the other two
     * Euro powers withdraw (control 2). */
    ctx->col1->player[n].control = (uint8_t)(n == crown_fold ? 1 : 2);
    if (n != crown_fold && ctx->col1_ok) {
      /*
       * FUN_43f7_0108 diplo-clear/set (viceroy_unpacked.c:73554-73557: clear
       * 0x0b vs DS:0x5398 then vs DS:0x53d2, OR 0x60 vs each -- DOS never
       * targets the crown itself with 0108, matching the n==crown_fold skip).
       *
       * Bit map corrected 2026-09-06d: the old comment here ("0xb =
       * WAR|PEACE|unmapped-bit3; 0x60 = unmapped-bit5|MET") predates the
       * 2026-08-27 T1.19 re-derivation in ai_diplo.h. Under the live map
       * 0x0b = WAR_INTENT(0x01)|WAR(0x02)|amicable-latch(0x08) and
       * 0x60 = MET(0x20)|PEACE(0x40) -- every bit is mapped, nothing is
       * "unmapped" any more. The port had it inverted for PEACE: it CLEARED
       * 0x40 where DOS SETS it, so a withdrawn Euro power came out of the
       * declare fold not-at-peace with both the rebel and the crown.
       * (0x08 is AI_DIPLO_AMICABLE, the DOS amicable-negotiation latch --
       * clearing it is the DOS write. Renamed 2026-09-09, smell #99: it was
       * called AI_DIPLO_TREASURE_STRONGER, which wrongly implied a
       * Linux-invented second owner of the bit; DOS writes 0x08 both here,
       * at 153e's tail, and in FUN_465b_0000's treasure arm.)
       */
      const uint8_t k_0108_clear =
        (uint8_t)(AI_DIPLO_WAR_INTENT | AI_DIPLO_WAR | AI_DIPLO_AMICABLE);
      const uint8_t k_0108_set = (uint8_t)(AI_DIPLO_MET | AI_DIPLO_PEACE);
      ai_diplo_clear_both(ctx->col1, n, human, k_0108_clear);
      ai_diplo_or_both(ctx->col1, n, human, k_0108_set);
      ai_diplo_clear_both(ctx->col1, n, crown_fold, k_0108_clear);
      ai_diplo_or_both(ctx->col1, n, crown_fold, k_0108_set);
    }
    if (n == crown_fold || !ctx->units) {
      continue;
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->nation_id == n) {
        units_despawn(ctx->units, u->id);
      }
    }
  }
  /*
   * FUN_43f7_0188(human) — ported 2026-09-06: right after the eliminate
   * loop, 1a26 deletes every HUMAN unit whose coordinates fail
   * FUN_281f_0302 (map_tile_in_bounds) — i.e. everything parked on the
   * Europe/high-seas lanes (DOS x/y 228+n / 232+n / 244+n) — and pops
   * @SEIZURE (0x1284, "%STRING0 seized on the high seas by the Royal
   * Navy!") for each ship type (0xd..0x12); passengers/cargo aboard die
   * silently with their ship. In this port those ships live in the Europe
   * lane lists, not the unit pool, so the seizure clears
   * europe->harbor/bound/expected.
   */
  if (ctx->europe) {
    EuropeHarborShip* lanes[3] = {ctx->europe->harbor, ctx->europe->bound,
                                  ctx->europe->expected};
    int* counts[3] = {&ctx->europe->harbor_ships, &ctx->europe->bound_ships,
                      &ctx->europe->expected_ships};
    for (int li = 0; li < 3; ++li) {
      for (int si = 0; si < *counts[li]; ++si) {
        const EuropeHarborShip* ship = &lanes[li][si];
        if (ai_king_human_popups(ctx)) {
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = ship->name[0] ? ship->name : "Ship";
          char body[AI_POPUP_BODY_LEN];
          char fallback[AI_POPUP_BODY_LEN];
          snprintf(fallback, sizeof(fallback),
                   "%s seized on the high seas by the Royal Navy!", tok.string0);
          popup_msg_fill(ctx->messages, "SEIZURE", &tok, fallback, body, sizeof(body));
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown_fold, 0, NULL, body
          );
        }
      }
      *counts[li] = 0;
    }
    ctx->europe->selected_harbor = -1;
  }
  /*
   * FUN_43f7_1a26 tail (43f7:1c07..1c1b): every human unit's MP is spent via
   * FUN_281f_0934 (unit_exhaust_mp) — "This will end our turn" (@DECLARE).
   */
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->nation_id == human) {
        u->moves_left = 0;
      }
    }
  }
  /*
   * FUN_43f7_1a26 last writes (OVL07 asm 2c44..2c5c): ff_count_end_prob
   * (nation+0x16, DS:-0x77e2) zeroed for the human AND the crown slot, and
   * the crown's nation_flags bit 0x04 ("achieved independence from its
   * King") cleared. col1_save.h already documented the field as "cleared on
   * independence"; the write itself landed here 2026-09-06.
   */
  ctx->col1->nation[human].ff_count_end_prob = 0;
  if (crown_fold >= 0 && crown_fold < 4) {
    ctx->col1->nation[crown_fold].ff_count_end_prob = 0;
    ctx->col1->nation[crown_fold].nation_flags &= (uint8_t)~0x04u;
  }
  /*
   * Thin 160a independence rename (the letter animation itself lives in
   * core/declaration.c and is armed by game_loop off the KING_LETTER popup).
   * Writable Col1 player.country_name (and europe.nation_name if present).
   * Congress status below; same-turn 0982/1528 wave may overwrite if it spawns
   * (wave only writes status when non-empty arrival — leave congress if empty).
   * Human queue: thin rename OK + WoI-begins OK (FUN_43f7_160a / 1a26 chain).
   * Letter chrome: KING_LETTER body carries the @INDEPENDENCE wording; the
   * signing animation (DECOIND.PIK + DEC-UPP/LOW/SQIG.SS) is core/declaration.c.
   * DECLARAT.PIK is an unused leftover — no DOS executable references it.
   */
  /* bugs.md 245: no "United Colonies" rename — DOS 160a is only the signing
   * cinematic and never touches country_name. Under the WoI the player
   * faction reads "Rebels" (LABELS 84/101) via units_combat_nation_label. */
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "Congress declares independence!");
  }
  if (ai_king_human_popups(ctx)) {
    /* FUN_43f7_160a rename OK; the cinematic rides on the KING_LETTER tag. */
    const char* leader =
      (human >= 0 && human < 4 && ctx->col1->player[human].name[0] != '\0')
        ? ctx->col1->player[human].name
        : "Washington";
    PopupMsgTokens letter_tok;
    memset(&letter_tok, 0, sizeof(letter_tok));
    letter_tok.string0 = leader;
    char letter[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "INDEPENDENCE",
      &letter_tok,
      "Continental Congress signs Declaration of Independence! "
      "Abuses and usurpations cited! Ultimatum presented to King! "
      "Expeditionary force dispatched to suppress rebellion!",
      letter,
      sizeof(letter)
    );
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_KING_LETTER,
      human,
      ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
      0,
      NULL,
      letter
    );
    /* bugs.md 242: @HOWTOWIN does NOT fire at the declaration. DOS shows it
     * once at the first colony the rebel recaptures with the REF present
     * (5fef capture tail, DS:0x5386 bit0 latch) — see
     * units_try_capture_foreign_colony. */
  }
}

/*
 * FUN_43f7_2564 gate (SoL≥AI_KING_DECLARE_SOL_MIN) + 1a26 declare.
 * Human + ctx->ai_popups → CHOICE from GAME.TXT @DECLARE (Never / Yes;
 * effect in apply_popup_result). Else auto-declare when SoL past 2564/fandom
 * threshold and SoL ≥ min.
 *
 * 2026-08-24: resolved the "two-stage declare popup" question flagged by a
 * prior pass (docs/sons_of_liberty.md). Raw 2564 body
 * (viceroy_unpacked.c:75217-75253): when SoL≤49 it shows msg 0x1386 with
 * `*(int*)0x53d0` (the SoL value) as its NUMBER0 arg — that argument shape
 * matches GAME.TXT `@TOOTORY` ("Only {%NUMBER0%%} of the colonists support
 * the independence movement... until the {majority} is behind us") exactly,
 * so 0x1386 = @TOOTORY. Unreachable *here*: this function (the per-turn
 * auto-check) only runs once `sol >= AI_KING_DECLARE_SOL_MIN` already.
 * 2564 is *also* reachable from the MENU.TXT @GAME "DECLARE INDEPENDENCE"
 * command at any SoL in the original — the port models that path
 * separately as `ai_king_menu_declare_independence` (wired to
 * MAP_MENU_ACTION_DECLARE_INDEPENDENCE in game_loop.c), which is where
 * @TOOTORY actually fires below threshold.
 *
 * The `*(byte*)0x5381 & 0x80` branch gating a *first* popup (msg 0x138e)
 * ahead of the real confirm is NOT a "recommend declare" one-shot notice —
 * traced where that bit is ever *set*: only in `FUN_75c2_10ae`
 * (viceroy_unpacked.c:120544-120694), the new-game nation-select/setup
 * screen, when more than one nation slot is flagged human (`iVar5 > 1`,
 * counting bits in the per-nation "is human" mask at 0x1f54). That's a
 * hotseat/multi-human-player marker, not a per-turn SoL event — the extra
 * dialog picks *which* human player is declaring (`*(u16*)0x5398 =
 * *(u16*)0x5394` after it returns) before falling into the same @DECLARE
 * confirm every path shares. `ColonizeTurnContext` models exactly one
 * `human_nation`, so this branch has no reachable port equivalent — the
 * single Never/Yes @DECLARE popup below is the complete behavior for the
 * port's single-human model, not a missing feature. (0x138e's exact
 * wording was never recovered — not needed since the branch is
 * unreachable; the `*(byte*)0x5382&1` else-branch's msg 0x1374, shown when
 * already at war, is likewise unrecovered and likewise not required since
 * `ai_king_independence_declared` already short-circuits that case above.)
 */
/*
 * Shared @DECLARE Never/Yes confirm body — called once `sol` is already
 * known ≥ AI_KING_DECLARE_SOL_MIN, from both the automatic per-turn check
 * (ai_king_try_declare) and the menu-invoked path
 * (ai_king_menu_declare_independence).
 */
static void ai_king_show_declare_choice(ColonizeTurnContext* ctx, int human, int sol) {
  if (ai_king_human_popups(ctx)) {
    /* bugs.md 241: %STRING0 is the Crown nation ("England"), never the
     * player's new-world country_name ("New England"). NAMES.TXT @COUNTRY. */
    static const char* const k_crown[4] = {"England", "France", "Spain", "Netherlands"};
    const char* motherland = (human >= 0 && human <= 3) ? k_crown[human] : "the Crown";
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = motherland;
    char body[AI_POPUP_BODY_LEN];
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "Shall we declare our independence from %s, Your Excellency? "
      "This will end our turn and place us at war with our King!",
      motherland
    );
    popup_msg_fill(ctx->messages, "DECLARE", &tok, fallback, body, sizeof(body));
    char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "DECLARE");
    int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
    for (int i = 0; i < nch; ++i) {
      char filled[AI_POPUP_CHOICE_LEN];
      popup_msg_apply_tokens(filled, sizeof(filled), choice_buf[i], &tok);
      str_copy_trunc(choice_buf[i], sizeof(choice_buf[i]), filled);
    }
    /* GAME.TXT: Never… / Yes… — map to Not yet / Confirm. */
    const char* labels[2];
    const int ids[] = {AI_KING_CHOICE_NOT_YET, AI_KING_CHOICE_CONFIRM};
    if (nch >= 2) {
      labels[0] = choice_buf[0];
      labels[1] = choice_buf[1];
    } else {
      labels[0] = "Never! That would be treasonous! God save the King!";
      labels[1] = "Yes! Give me liberty or give me death!";
    }
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_CONGRESS, human,
                                    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human), sol, NULL,
                                    body, labels, ids, 2)) {
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size,
                 "Congress debates independence (SoL %d%%).", sol);
      }
      return;
    }
    /* Queue full — fall through to auto declare. */
  }
  ai_king_do_declare(ctx, human);
}

static void ai_king_try_declare(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int sol = ai_king_sol_percent(ctx, human);
  if (sol < AI_KING_DECLARE_SOL_MIN) {
    return;
  }
  /*
   * bugs.md: DOS never spawns the Never/Yes @DECLARE confirm on its own at a
   * SoL threshold. FUN_43f7_2564 has no call site in the decompile at all —
   * it is reached only from the MENU.TXT @GAME "DECLARE INDEPENDENCE" command
   * (ai_king_menu_declare_independence). Firing it per turn produced a
   * persistent popup at SoL 50 that the original does not have. Keep the
   * SoL≥min auto-declare only for the headless/AI path (no popup queue), where
   * there is nobody to answer a CHOICE.
   */
  if (ai_king_human_popups(ctx)) {
    return;
  }
  ai_king_show_declare_choice(ctx, human, sol);
}

/*
 * Menu-invoked DECLARE INDEPENDENCE (MENU.TXT @GAME item, MAP_MENU_ACTION_
 * DECLARE_INDEPENDENCE). DOS FUN_43f7_2564 reached from this same menu
 * command at any SoL, not just once auto-eligible — the per-turn
 * ai_king_try_declare check above only ever calls in once already ≥
 * AI_KING_DECLARE_SOL_MIN, so the sol<min branch (GAME.TXT @TOOTORY) was
 * unreachable there. This entry point makes it reachable: below the
 * threshold, show @TOOTORY (Congress won't back a rebellion yet) as a
 * plain OK notice; at/above threshold, show the same Never/Yes @DECLARE
 * confirm ai_king_try_declare would auto-fire on its next turn — so
 * declining "Not yet" from the auto-popup can be revisited here on demand
 * instead of only ever re-asked wholesale next turn.
 */
void ai_king_menu_declare_independence(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "We are already at war with the Crown.");
    }
    return;
  }
  const int sol = ai_king_sol_percent(ctx, human);
  if (sol < AI_KING_DECLARE_SOL_MIN) {
    if (ai_king_human_popups(ctx)) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.number0 = sol;
      tok.has_number0 = true;
      char fallback[AI_POPUP_BODY_LEN];
      snprintf(
        fallback,
        sizeof(fallback),
        "Only %d%% of the colonists support the independence movement, Your "
        "Excellency. We cannot start a rebellion against the King until the "
        "majority is behind us.",
        sol
      );
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "TOOTORY", &tok, fallback, body, sizeof(body));
      (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_INFO, human,
                                    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human), sol, NULL,
                                    body);
    }
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "Only %d%% of the colonists support independence yet.", sol);
    }
    return;
  }
  ai_king_show_declare_choice(ctx, human, sol);
}

/*
 * FUN_43f7_10f0 raw 74312-74331: pop-weighted coastal colony roulette.
 * The gather loop keeps at most TEN candidates (`local_24 < 10`, a fixed
 * 10-byte stack array) and weights each by the RAW population byte +0x1f —
 * no floor of 1, so a 0-pop port carries no weight. The roulette then walks
 * that candidate array, not the colony list, so past ten human ports the
 * eleventh onward can never be picked.
 */
#define AI_KING_10F0_CANDIDATES 10

static int ai_king_10f0_pick_colony(const ColonizeTurnContext* ctx, int human, int* out_x,
                                    int* out_y) {
  if (!ctx || !out_x || !out_y || human < 0 || human >= 4) {
    return -1;
  }
  int total_pop = 0;
  int best_i = -1;
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    int cand[AI_KING_10F0_CANDIDATES];
    int weight[AI_KING_10F0_CANDIDATES];
    int n_cand = 0;
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human || !c->flags.coastal) {
        continue;
      }
      if (n_cand >= AI_KING_10F0_CANDIDATES) {
        break; /* DOS keeps scanning but the `local_24 < 10` arm never takes */
      }
      const int pop = (int)c->population;
      cand[n_cand] = (int)i;
      weight[n_cand] = pop;
      total_pop += pop;
      n_cand++;
    }
    if (n_cand > 0 && total_pop > 0) {
      int pick = total_pop / 2 + 1;
      if (ctx->rng) {
        pick = dos_rng_range(ctx->rng, 1, total_pop);
      }
      for (int k = 0; k < n_cand; ++k) {
        pick -= weight[k];
        if (pick <= 0) {
          const ColonizeCol1Colony* c = &ctx->col1->colony[cand[k]];
          *out_x = (int)c->x;
          *out_y = (int)c->y;
          return cand[k];
        }
      }
    }
    /*
     * Weightless candidate set (every one of the first ten ports at pop 0 —
     * unreachable in a real save): DOS's roulette leaves local_56 at -1 and
     * 10f0 lands nothing. Fall through to the caller's weakest-port fallback
     * instead of stalling the whole intervention.
     */
  }
  if (ctx->colonies) {
    int best_score = 999999;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (ctx->map && !map_tile_is_coastal(ctx->map, c->x, c->y)) {
        continue;
      }
      const int pop = c->population > 0 ? c->population : 1;
      if (pop < best_score) {
        best_score = pop;
        best_i = i;
        *out_x = c->x;
        *out_y = c->y;
      }
    }
  }
  return best_i;
}

/*
 * FUN_43f7_10f0 74339–74377: Man-O-War spawn tile = the 8-neighbour of the
 * colony that is WATER (`281f_0768` = `13e4_0074`, terrain 0x19/0x1a) with
 * no unit on it or only the human's (`281f_0682` < 0 || == human), no REF
 * Man-O-War (−999), scored 1 + the number of ITS neighbours that are land
 * on the colony's continent (`0722` == colony's) without a colony (`06be`
 * < 0). DOS also wants `281f_06b4`(tile) == 1 — the layer3 low nibble, i.e.
 * the sea region the open ocean carries; test maps don't fill layer3, so a
 * region-1 tile is preferred but not required.
 */
static int ai_king_10f0_score_tile(const ColonizeTurnContext* ctx, int human, int cx, int cy,
                                   int tx, int ty) {
  static const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  static const int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  if (!ctx || !ctx->map || !map_coords_inset(ctx->map, tx, ty) ||
      !map_tile_is_water(ctx->map, tx, ty)) {
    return -1;
  }
  /* bugs.md: the ally sails a Man-O-War in — a landlocked lake tile is
   * unreachable from the ocean, yet pass 2 of the picker accepted any water
   * ("MoW in the lake deposited troops into the lake colony"). */
  if (map_tile_is_lake(ctx->map, tx, ty)) {
    return -1;
  }
  int score = 1;
  /*
   * DOS runs TWO different ownership questions here, in this order
   * (viceroy_unpacked.c:74350-74364), and the port used to collapse them
   * into one human-only test — inverting the penalty (audit D11):
   *   1. `local_34 = 07e0(tile)` (first unit of the stack); if the stack's
   *      owner nibble equals the CROWN (`*(byte*)0x53d2`), walk the stack
   *      with 02e4 and subtract 999 per Man-O-War (type 0x12). A crown MoW
   *      parked on the tile is what DOS is refusing to land beside.
   *   2. `local_4 = 0682(tile)` (unit-presence owner, map.c:1187): the tile
   *      must be empty of units or hold only the HUMAN's.
   * The human's own Man-O-War therefore costs nothing in DOS; the old code
   * charged it −999 and rejected the tile, while a crown stack was rejected
   * by the wrong test and never reached the penalty at all.
   */
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  int tile_owner = -1; /* 281f_0682 */
  for (int i = 0; i < COLONIZE_UNITS_MAX && ctx->units; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->x != tx || u->y != ty || u->aboard_ship_id >= 0) {
      continue;
    }
    if (tile_owner < 0) {
      tile_owner = u->nation_id;
    }
    if (crown >= 0 && u->nation_id == crown && ai_king_is_mow(ctx->units, u)) {
      score -= 999;
    }
  }
  if (score < 0) {
    return score;
  }
  if (tile_owner >= 0 && tile_owner != human) {
    return -1;
  }
  const int colony_region = map_continent_id_at(ctx->map, cx, cy);
  for (int d = 0; d < 8; ++d) {
    const int nx = tx + dx[d];
    const int ny = ty + dy[d];
    if (!map_coords_inset(ctx->map, nx, ny) || !map_tile_is_land(ctx->map, nx, ny)) {
      continue;
    }
    if (map_continent_id_at(ctx->map, nx, ny) != colony_region) {
      continue;
    }
    if (ctx->colonies && colonies_id_at(ctx->colonies, nx, ny) >= 0) {
      continue;
    }
    score++;
  }
  return score;
}

static bool ai_king_10f0_pick_spawn(const ColonizeTurnContext* ctx, int human, int cx, int cy,
                                    int* out_x, int* out_y) {
  static const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  static const int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  if (!ctx || !out_x || !out_y) {
    return false;
  }
  int best = 0;
  int bx = -1;
  int by = -1;
  for (int pass = 0; pass < 2 && bx < 0; ++pass) {
    for (int d = 0; d < 8; ++d) {
      const int tx = cx + dx[d];
      const int ty = cy + dy[d];
      if (pass == 0 && ctx->map && map_continent_id_at(ctx->map, tx, ty) != 1) {
        continue; /* 281f_06b4 == 1: open-ocean region first */
      }
      const int sc = ai_king_10f0_score_tile(ctx, human, cx, cy, tx, ty);
      if (sc > best) {
        best = sc;
        bx = tx;
        by = ty;
      }
    }
  }
  if (bx < 0) {
    return false;
  }
  *out_x = bx;
  *out_y = by;
  return true;
}

/* FUN_43f7_060a-shaped: weakest garrison (pop × fort). */
static int ai_king_weakest_port(ColonizeTurnContext* ctx, int nation_id, int* out_x, int* out_y) {
  if (!ctx || !ctx->colonies || !out_x || !out_y) {
    return -1;
  }
  int best = -1;
  int best_score = 999999;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    int garrison = c->population;
    if (colonies_has_fortification(ctx->colonies, c)) {
      garrison *= 2;
    }
    /* Prefer coastal ports when garrison pressure is close (REF landing sites). */
    if (ctx->map && map_tile_is_coastal(ctx->map, c->x, c->y)) {
      garrison = (garrison * 9) / 10;
    }
    if (garrison < best_score) {
      best_score = garrison;
      best = c->id;
      *out_x = c->x;
      *out_y = c->y;
    }
  }
  return best;
}

/*
 * FUN_43f7_060a: colony garrison score for the REF landing pick.
 *   (muskets + 50) / 100 + 1, + Σ land units on the tile (004a attack ×8 >> 4),
 *   ×2 with a Fortress, ×1.5 with a Fort, min 1.
 */
static int ai_king_0982_garrison_score(const ColonizeTurnContext* ctx, const ColonizeColony* c) {
  int g = (c->stock[COLONIZE_CARGO_MUSKETS] + 50) / 100 + 1;
  ColonizeCombatStrengthCtx cs;
  memset(&cs, 0, sizeof(cs));
  cs.units = ctx->units;
  cs.map = ctx->map;
  cs.colonies = ctx->colonies;
  cs.col1 = ctx->col1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->x != c->x || u->y != c->y || !units_is_on_map(u) ||
        units_is_sea(ctx->units, u->id)) {
      continue;
    }
    g += combat_unit_base_x8(&cs, u->id, 1, NULL) >> 4;
  }
  const int fortress = colonies_find_building(ctx->colonies, "Fortress");
  const int fort = colonies_find_building(ctx->colonies, "Fort");
  if (fortress >= 0 && c->has_building[fortress]) {
    g <<= 1;
  } else if (fort >= 0 && c->has_building[fort]) {
    g = (g * 3) >> 1;
  }
  return g < 1 ? 1 : g;
}

/* 08bc stack query stand-in: Σ defense (004a mode 0 ×8 >> 4) of units at (x,y). */
static int ai_king_0982_tile_strength(const ColonizeTurnContext* ctx, int x, int y) {
  ColonizeCombatStrengthCtx cs;
  memset(&cs, 0, sizeof(cs));
  cs.units = ctx->units;
  cs.map = ctx->map;
  cs.colonies = ctx->colonies;
  cs.col1 = ctx->col1;
  int s = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (u->active && u->x == x && u->y == y && units_is_on_map(u)) {
      s += combat_unit_base_x8(&cs, u->id, 0, NULL) >> 4;
    }
  }
  return s;
}

/*
 * FUN_43f7_0512: purge every non-crown unit at (x,y). Human units get the
 * @SEIZURELAND / @SEIZURESEA notice (%STRING0 = unit type name).
 */
static void ai_king_0982_purge_tile(ColonizeTurnContext* ctx, int crown, int x, int y) {
  for (int i = COLONIZE_UNITS_MAX - 1; i >= 0; --i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->x != x || u->y != y || !units_is_on_map(u) || u->nation_id == crown) {
      continue;
    }
    if (u->nation_id == ctx->human_nation && ai_king_human_popups(ctx)) {
      const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
      const bool sea = units_is_sea(ctx->units, u->id);
      if (!map_tile_is_water(ctx->map, x, y) || sea) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = t ? t->name : "unit";
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages, sea ? "SEIZURESEA" : "SEIZURELAND", &tok,
          "The Royal Expeditionary Force has seized our %STRING0!", body, sizeof(body)
        );
        (void)ai_popup_enqueue_ok_ctx(
          ctx->ai_popups, AI_POPUP_TAG_INFO, ctx->human_nation, crown, 0, NULL, body
        );
      }
    }
    if (units_is_sea(ctx->units, u->id)) {
      (void)units_despawn_ship_with_cargo(
        ctx->units, u->id, NULL, NULL, 0, NULL, NULL, 0, NULL, NULL, 0
      );
    } else {
      (void)units_despawn(ctx->units, u->id);
    }
  }
}

static int ai_king_0982_crown_mow_alive(const ColonizeTurnContext* ctx, int crown) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (u->active && u->nation_id == crown && ai_king_is_mow(ctx->units, u)) {
      n++;
    }
  }
  return n;
}

/* 0982 pool index → NAMES type (43f7_0082 crown class map). */
static int ai_king_0982_spawn_pool_unit(ColonizeTurnContext* ctx, int crown, int k, int x, int y) {
  /* bugs.md: the REF fields Regulars and CAVALRY (@UNIT 8) — not colonial
   * Dragoons; pool[1] is the Cavalry pool. */
  static const char* names[4] = {"Regulars", "Cavalry", "Man-O-War", "Artillery"};
  static const char* alts[4] = {"Soldiers", "Dragoons", "Galleon", "Cannon"};
  int ty = units_find_type(ctx->units, names[k]);
  if (ty < 0) {
    ty = units_find_type(ctx->units, alts[k]);
  }
  if (ty < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(ctx->units, ty, x, y);
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (!u) {
    return -1;
  }
  units_set_nation(u, crown);
  /* DOS creator FUN_1427_06b4 stamps orders +0x314b = 0x58 (none) and no
   * goto; 0982 never overrides it (raw 74202-74222). The landed unit must
   * arrive order-free so the euro act (5b66/20e6) decides its move next
   * turn — the old AI_MOVE+goto=self stamp read as a committed goto to its
   * own tile and froze the wave once the king-side hunt was retired (D1). */
  /* 0982: the landed unit's moves are spent this beat (02d0 animate + 0948). */
  u->moves_left = 0;
  /* bugs.md: landings are in plain sight — stamp watcher vis bits so the
   * wave draws immediately instead of after its first move. */
  if (ctx->map) {
    u->col1_vis_mask |= units_vis_mask_for_tile(ctx->map, x, y, crown);
  }
  return uid;
}

#define AI_KING_0982_MAX_LANDING 31 /* DS:0x5333 */
#define AI_KING_0982_MAX_TARGETS 10

/*
 * FUN_43f7_2022 crown branch: pools>0 → FUN_43f7_0982 invasion wave, else
 * FUN_43f7_06a6 irregulars. 0982 (viceroy_unpacked.c 73935-74266):
 *   - MoW pool (force[2]) empty → +1 only while the crown has no Man-O-War
 *     alive, and no landing this turn.
 *   - exhaust = total < 5 || total == force[2] → every pool wiped at the end.
 *   - human coastal colonies scored colonists×(125−SoL) − 75×tile strength
 *     (min 100−SoL), weakest first; garrison need = 060a − attack-capable
 *     crown units already adjacent. Three relaxing passes pick the first
 *     colony the pools can cover (Dragoon/Artillery each capped at
 *     max(1, need>>3), or 1 when Regulars ≥ Dragoons+Artillery); passes ≥1
 *     cap need at DS:0x5333 = 31.
 *   - landing water tile = the colony neighbour with the most free land
 *     neighbours on the colony's continent (a human ship stack there counts
 *     as 1). Non-crown units on it are seized (0512), the MoW spawns there
 *     (@INVASION), then max(3, need) land units land on the weakest adjacent
 *     land tiles (Chebyshev 1 from the colony, same continent, no village),
 *     seizing whatever stands there — Dragoons first up to the cap (2 max
 *     when Regulars > 1), then Artillery, then Regulars.
 * Thin: 08bc stack strength = Σ defense×8>>4.
 * The emptied-Man-O-War return home is no longer stood in for here: the real
 * DOS beat is the FUN_521d_20e6 ship-band tail, ported as
 * ai_king_mow_sail_home_20e6 and run from the MoW's own act in war_act. The
 * MoW pool regrows through this function's own opening gate below
 * (force[2] == 0 && no crown MoW on the map → force[2]++), exactly as DOS
 * does at raw 73990-73993.
 */
static void ai_king_ref_wave(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->map) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /* bugs.md: the wave waits one turn after the declaration. */
  if (ai_king_latch_get(ctx->col1, AI_KING_REF_WAVE_WAIT_BYTE) != 0) {
    ai_king_latch_set(ctx->col1, AI_KING_REF_WAVE_WAIT_BYTE, 0);
    return;
  }
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation);
  const int human = ctx->human_nation;
  uint16_t* force = ctx->col1->head.expeditionary_force;
  int total = (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];

  /*
   * FUN_43f7_2022 crown gate (viceroy_unpacked.c:74994): the wave (0982)
   * runs while `regulars + (cavalry>0) + (artillery>0) != 0` — the
   * Man-O-War pool alone does NOT sustain the invasion; once the land
   * pools are gone the crown falls back to 06a6 Tory uprisings even if
   * force[2] is nonzero (was `total <= 0`, which counted the MoW pool).
   */
  if ((int)force[0] + (force[1] > 0 ? 1 : 0) + (force[3] > 0 ? 1 : 0) == 0) {
    /*
     * bugs.md 261 — full FUN_43f7_06a6 Tory uprising (viceroy_unpacked.c
     * 73829-73932), replacing the old one-Regular-at-(hx,hy+1) stand-in
     * that could drop Regulars on a WATER tile:
     *   - roll(0, difficulty+1) != 0 to fire at all;
     *   - per human colony without the +0x1c bit1 latch (flags.ref_landing):
     *     score = pop*(100-SoL)*2/100 + difficulty+1, minus the attack
     *     strength of every unit on the colony tile; a crown unit on any
     *     adjacent land tile, or no free adjacent LAND tile, disqualifies;
     *   - the max-score colony gets latched and score crown SOLDIERS (not
     *     Regulars) spawn round-robin on free adjacent land tiles — odd
     *     picks roll Veteran profession, every 3rd rolls into a Dragoon;
     *   - @TORYUPRISING popup with the colony name.
     */
    if (dos_rng_range(ctx->rng, 0, (int)ctx->col1->head.difficulty + 1) == 0) {
      return;
    }
    static const int dx8[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
    static const int dy8[8] = {-1, -1, -1, 0, 1, 1, 1, 0};
    int best_ci = -1;
    int best_score = 0;
    for (uint16_t ci = 0; ci < ctx->col1->head.colony_count; ++ci) {
      ColonizeCol1Colony* c = ctx->col1->colony ? &ctx->col1->colony[ci] : NULL;
      if (!c || (int)c->nation_id != human || c->flags.ref_landing) {
        continue;
      }
      const int sol_p = ai_king_colony_sol_at(ctx, human, (int)c->x, (int)c->y);
      int score = ((int)c->population * (100 - sol_p) * 2) / 100 +
                  (int)ctx->col1->head.difficulty + 1;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &ctx->units->units[i];
        if (u->active && u->aboard_ship_id < 0 && u->x == (int)c->x && u->y == (int)c->y) {
          const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
          score -= ty ? ty->attack : 0;
        }
      }
      int free_land = 0;
      bool crown_adjacent = false;
      for (int e = 0; e < 8; ++e) {
        const int nx = (int)c->x + dx8[e];
        const int ny = (int)c->y + dy8[e];
        if (map_tile_is_water(ctx->map, nx, ny) ||
            map_tile_has_city(ctx->map, nx, ny)) {
          continue;
        }
        const int occ = units_id_at(ctx->units, nx, ny);
        const ColonizeUnit* ou = occ >= 0 ? units_get_const(ctx->units, occ) : NULL;
        if (!ou) {
          free_land++;
        } else if (ou->nation_id == crown) {
          crown_adjacent = true;
        }
      }
      if (crown_adjacent || free_land == 0) {
        continue;
      }
      if (score > best_score) {
        best_score = score;
        best_ci = (int)ci;
      }
    }
    if (best_ci < 0 || best_score <= 0) {
      return;
    }
    ColonizeCol1Colony* c = &ctx->col1->colony[best_ci];
    c->flags.ref_landing = 1;
    const int soldier_ty = units_find_type(ctx->units, "Soldier");
    const int dragoon_ty = units_find_type(ctx->units, "Dragoon");
    int remaining = best_score;
    int spawned = 0;
    bool any_pass = true;
    while (remaining > 0 && any_pass) {
      any_pass = false;
      for (int e = 0; e < 8 && remaining > 0; ++e) {
        const int nx = (int)c->x + dx8[e];
        const int ny = (int)c->y + dy8[e];
        if (map_tile_is_water(ctx->map, nx, ny) ||
            map_tile_has_city(ctx->map, nx, ny)) {
          continue;
        }
        const int occ = units_id_at(ctx->units, nx, ny);
        const ColonizeUnit* ou = occ >= 0 ? units_get_const(ctx->units, occ) : NULL;
        if (ou && ou->nation_id != crown) {
          continue;
        }
        const int uid = soldier_ty >= 0
          ? units_spawn_allow_stack(ctx->units, soldier_ty, nx, ny)
          : -1;
        if (uid >= 0) {
          any_pass = true;
          spawned++;
          ColonizeUnit* nu = units_get(ctx->units, uid);
          if (nu) {
            units_set_nation(nu, crown);
            /* Order-free like the 0982 wave (DOS creator default 0x58) —
             * the euro act moves them from the next turn on (D1). */
            /* bugs.md follow-up to 406: uprising irregulars spawn with the
             * turn spent — the war-act loop runs this same beat and must not
             * march them into the colony the moment they appear (the crown
             * move pass ran before the king block in DOS). */
            nu->moves_left = 0;
            if ((remaining & 1) != 0 &&
                dos_rng_range(ctx->rng, 0, (int)ctx->col1->head.difficulty + 1) != 0) {
              nu->profession = UNITS_JOB_SOLDIER; /* Veteran */
            }
            if (remaining % 3 == 0 && dragoon_ty >= 0 &&
                dos_rng_range(ctx->rng, 0, (int)ctx->col1->head.difficulty + 1) != 0) {
              nu->type_index = dragoon_ty;
              nu->horses = UNITS_EQUIP_HORSES;
            }
            if (ctx->map) {
              nu->col1_vis_mask |= units_vis_mask_for_tile(ctx->map, nx, ny, crown);
            }
          }
        }
        remaining--;
      }
    }
    if (spawned == 0) {
      c->flags.ref_landing = 0;
      return;
    }
    if (ai_king_human_popups(ctx)) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = c->name[0] ? c->name : "our colony";
      char body[AI_POPUP_BODY_LEN];
      char fallback[AI_POPUP_BODY_LEN];
      snprintf(fallback, sizeof(fallback),
               "Tory uprising near %s! Loyalist irregulars take up arms for the King!",
               tok.string0);
      popup_msg_fill(ctx->messages, "TORYUPRISING", &tok, fallback, body, sizeof(body));
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, crown, spawned, NULL, body
      );
    }
    return;
  }

  /*
   * (2026-09-07) The "emptied Man-O-War sails home" stand-in that used to sit
   * here — despawn any idle empty crown MoW with turns_worked > 0, bump
   * turns_worked otherwise — is retired. The real DOS beat is the
   * FUN_521d_20e6 ship-band tail (raw 89717-89720), ported as
   * ai_king_mow_sail_home_20e6 and run from the MoW's own act in war_act; see
   * that function's header. It also frees turns_worked, which DOS uses on AI
   * units as the 20e6 cower byte (+0x315a), from a king-side second meaning.
   */

  bool exhaust = false;
  bool landed = false;
  if (force[2] == 0) {
    if (ai_king_0982_crown_mow_alive(ctx, crown) == 0) {
      force[2]++;
    }
    return;
  }
  if (total < 5 || total == (int)force[2]) {
    exhaust = true;
  }
  if (total != (int)force[2] && ctx->colonies) {
    /* Score human coastal colonies (≤10); the list is sorted ASCENDING, and
     * the picker below walks it from the top (highest score = fattest, most
     * lightly held target). */
    int score[AI_KING_0982_MAX_TARGETS];
    int cidx[AI_KING_0982_MAX_TARGETS];
    int n = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX && n < AI_KING_0982_MAX_TARGETS; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
        continue;
      }
      const int inv = 100 - ai_king_colony_sol_at(ctx, human, c->x, c->y);
      int sc = c->colonist_count * (inv + 25) - 75 * ai_king_0982_tile_strength(ctx, c->x, c->y);
      if (sc < inv) {
        sc = inv;
      }
      score[n] = sc;
      cidx[n] = i;
      n++;
    }
    for (int a = 1; a < n; ++a) {
      for (int b = a; b > 0 && score[b] < score[b - 1]; --b) {
        int t = score[b]; score[b] = score[b - 1]; score[b - 1] = t;
        t = cidx[b]; cidx[b] = cidx[b - 1]; cidx[b - 1] = t;
      }
    }
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int garrison[AI_KING_0982_MAX_TARGETS];
    for (int i = 0; i < n; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[cidx[i]];
      int g = ai_king_0982_garrison_score(ctx, c);
      for (int d = 0; d < 8; ++d) {
        const int nx = c->x + dx[d];
        const int ny = c->y + dy[d];
        if (map_tile_is_water(ctx->map, nx, ny)) {
          continue;
        }
        for (int k = 0; k < COLONIZE_UNITS_MAX && g > 0; ++k) {
          const ColonizeUnit* u = &ctx->units->units[k];
          if (!u->active || u->nation_id != crown || u->x != nx || u->y != ny ||
              !units_is_on_map(u)) {
            continue;
          }
          const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
          if (t && t->attack > 0) {
            g--;
          }
        }
      }
      garrison[i] = g;
    }
    /*
     * Pick: three relaxing passes, each walking the ascending list BACKWARDS
     * (raw 74048-74056: `iVar4 = local_2a - local_48;
     * local_6a = local_42[iVar4 - 1]`, with local_48 zeroed at the head of
     * every pass and stepped only when a candidate is rejected — it is a
     * rejection counter, not a landing-wave cursor). So the HIGHEST score
     * goes first: many colonists, low SoL, thin garrison. Walking forwards
     * invaded the least attractive colony instead.
     */
    int pick = -1;
    int need = 0;
    for (int pass = 0; pass < 3 && pick < 0; ++pass) {
      for (int i = n - 1; i >= 0; --i) {
        int g = garrison[i] < 1 ? 1 : garrison[i];
        int cap = g >> 3;
        if (cap < 1) {
          cap = 1;
        }
        if ((int)force[1] + (int)force[3] <= (int)force[0]) {
          cap = 1;
        }
        const int cd = (int)force[1] < cap ? (int)force[1] : cap;
        const int ca = (int)force[3] < cap ? (int)force[3] : cap;
        if (pass != 0 && g > AI_KING_0982_MAX_LANDING) {
          g = AI_KING_0982_MAX_LANDING;
        }
        if (pass < 2 && (int)force[0] + cd + ca < g) {
          continue;
        }
        pick = i;
        need = g;
        break;
      }
    }
    if (pick >= 0) {
      if (need > AI_KING_0982_MAX_LANDING) {
        need = AI_KING_0982_MAX_LANDING;
      }
      const ColonizeColony* c = &ctx->colonies->colonies[cidx[pick]];
      const int continent = map_continent_id_at(ctx->map, c->x, c->y);
      /* Landing water tile: most free land neighbours on the colony continent. */
      int best = 0;
      int lx = -1;
      int ly = -1;
      for (int d = 0; d < 8; ++d) {
        const int wx = c->x + dx[d];
        const int wy = c->y + dy[d];
        if (!map_tile_is_water(ctx->map, wx, wy)) {
          continue;
        }
        int free_land = 0;
        for (int e = 0; e < 8; ++e) {
          const int nx = wx + dx[e];
          const int ny = wy + dy[e];
          if (map_tile_is_water(ctx->map, nx, ny)) {
            continue;
          }
          if (map_continent_id_at(ctx->map, nx, ny) != continent) {
            continue;
          }
          /* 06be tile_tribe_owner: settlement bit only, units do not block. */
          if (map_tile_has_city(ctx->map, nx, ny) ||
              (ctx->colonies && colonies_id_at(ctx->colonies, nx, ny) >= 0)) {
            continue;
          }
          free_land++;
        }
        if (free_land > 0) {
          const int foe = units_foreign_unit_at(ctx->units, wx, wy, -1, crown);
          if (foe >= 0) {
            free_land = 1; /* a human ship stack there: lowest priority */
          }
        }
        if (free_land > best) {
          best = free_land;
          lx = wx;
          ly = wy;
        }
      }
      if (best > 0) {
        ai_king_0982_purge_tile(ctx, crown, lx, ly);
        force[2]--;
        int ship_ty = units_find_type(ctx->units, "Man-O-War");
        if (ship_ty < 0) {
          ship_ty = units_find_type(ctx->units, "Galleon");
        }
        const int sid = ship_ty >= 0 ? units_spawn_allow_stack(ctx->units, ship_ty, lx, ly) : -1;
        ColonizeUnit* ship = units_get(ctx->units, sid);
        if (ship) {
          units_set_nation(ship, crown);
          ship->orders = UNITS_ORDER_AI_SAIL;
          ship->goto_x = lx;
          ship->goto_y = ly;
          ship->turns_worked = 0;
          /* bugs.md: the invasion fleet is in plain sight of the colony —
           * stamp watcher vis bits like a real move (the land units get
           * theirs in ai_king_0982_spawn_pool_unit). */
          if (ctx->map) {
            ship->col1_vis_mask |= units_vis_mask_for_tile(ctx->map, lx, ly, crown);
          }
          landed = true;
          exhaust = false;
          /* @INVASION (thin 1528 announce; VGA chrome PARKED). */
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = c->name[0] ? c->name : "your colony";
          char fallback[AI_POPUP_BODY_LEN];
          snprintf(fallback, sizeof(fallback), "Royal Expeditionary Force lands near %s!",
                   tok.string0);
          char body[AI_POPUP_BODY_LEN];
          popup_msg_fill(ctx->messages, "INVASION", &tok, fallback, body, sizeof(body));
          if (ctx->status && ctx->status_size) {
            snprintf(ctx->status, ctx->status_size, "%s", body);
          }
          if (ai_king_human_popups(ctx)) {
            (void)ai_popup_enqueue_ok_ctx(
              ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, crown, 0, NULL, body
            );
            /* bugs.md 243: the landing popup BLOCKS before the disembark
             * slides — popup, then animations, then the rest, in sequence. */
            units_pump_combat_popups();
          }

          /* Land units: caps recomputed from the raw garrison (74150-74162). */
          int cap = garrison[pick] >> 3;
          if (cap < 1) {
            cap = 1;
          }
          if (force[0] > 1 && cap > 2) {
            cap = 2;
          }
          if ((int)force[1] + (int)force[3] <= (int)force[0]) {
            cap = 1;
          }
          if (need < 3) {
            need = 3;
          }
          /* bugs.md: one Man-O-War carries 6 units — that is the most the
           * REF can put ashore against one colony in a turn. */
          if (need > 6) {
            need = 6;
          }
          int used_d = 0;
          int used_a = 0;
          /* Candidate land tiles around the ship, weakest stack first. */
          int cx[8];
          int cy[8];
          int cs[8];
          int nc = 0;
          for (int e = 0; e < 8; ++e) {
            const int nx = lx + dx[e];
            const int ny = ly + dy[e];
            if (map_tile_is_water(ctx->map, nx, ny) ||
                map_tile_has_city(ctx->map, nx, ny) ||
                (ctx->colonies && colonies_id_at(ctx->colonies, nx, ny) >= 0) ||
                abs(nx - c->x) > 1 || abs(ny - c->y) > 1 ||
                map_continent_id_at(ctx->map, nx, ny) != continent) {
              continue;
            }
            cx[nc] = nx;
            cy[nc] = ny;
            cs[nc] = ai_king_0982_tile_strength(ctx, nx, ny);
            nc++;
          }
          for (int a = 1; a < nc; ++a) {
            for (int b = a; b > 0 && cs[b] < cs[b - 1]; --b) {
              int t = cs[b]; cs[b] = cs[b - 1]; cs[b - 1] = t;
              t = cx[b]; cx[b] = cx[b - 1]; cx[b - 1] = t;
              t = cy[b]; cy[b] = cy[b - 1]; cy[b - 1] = t;
            }
          }
          /*
           * bugs.md (REF_bugs.SAV): if ANY candidate tile is empty, the
           * landing uses only the empty tiles — seizing the player's units
           * is the blockade-runner case, legal only when every adjacent
           * tile is held. Empty tiles sort first anyway (strength 0), so
           * restrict "usable" to them when one exists.
           */
          /*
           * bugs.md: "safe" = no HUMAN stack on the tile — empty, or held
           * by an earlier crown landing (stacking with its own army is
           * fine; the previous fix treated the old beachhead as occupied
           * and pushed the next wave onto the player's units). Partition:
           * when ANY safe tile exists, land ONLY on safe tiles; the
           * seize-what-stands-there landing remains solely for a full
           * blockade.
           */
          int usable = 0;
          {
            int sx2[8];
            int sy2[8];
            int ss2[8];
            int ns = 0;
            for (int t = 0; t < nc; ++t) {
              const int occ = units_id_at(ctx->units, cx[t], cy[t]);
              const ColonizeUnit* ou = occ >= 0 ? units_get_const(ctx->units, occ) : NULL;
              if (!ou || ou->nation_id == crown) {
                sx2[ns] = cx[t];
                sy2[ns] = cy[t];
                ss2[ns] = cs[t];
                ns++;
              }
            }
            if (ns > 0) {
              for (int t = 0; t < ns; ++t) {
                cx[t] = sx2[t];
                cy[t] = sy2[t];
                cs[t] = ss2[t];
              }
              usable = ns;
            } else {
              /* Full blockade: DOS lands on every tile no stronger than the
               * weakest, seizing what stands there. */
              while (usable < nc && cs[usable] <= cs[0]) {
                usable++;
              }
              for (int t = 0; t < usable; ++t) {
                ai_king_0982_purge_tile(ctx, crown, cx[t], cy[t]);
              }
            }
          }
          int slot = 0;
          while (need > 0 && usable > 0) {
            int k;
            if (used_d < cap && force[1] > 0) {
              k = 1;
              used_d++;
            } else if (used_a < cap && force[3] > 0) {
              k = 3;
              used_a++;
            } else if (force[0] > 0) {
              k = 0;
            } else {
              break;
            }
            /* bugs.md: show the troops DISEMBARKING — spawn on the ship's
             * tile and step ashore through units_try_move, which fires the
             * move-watch slide, so the player can see what landed. Fall
             * back to a direct beach spawn if the step is refused. */
            const int uid = ai_king_0982_spawn_pool_unit(ctx, crown, k, lx, ly);
            if (uid < 0) {
              break;
            }
            {
              /* One step's worth of MP for the walk ashore (spawn parks at 0). */
              ColonizeUnit* lu = units_get(ctx->units, uid);
              if (lu) {
                lu->moves_left = 3;
                lu->goto_x = cx[slot];
                lu->goto_y = cy[slot];
              }
            }
            if (!units_try_move(
                  ctx->units, uid, ctx->map, cx[slot], cy[slot], ctx->colonies, ctx->rng
                )) {
              ColonizeUnit* lu = units_get(ctx->units, uid);
              if (lu) {
                const int sx0 = lu->x;
                const int sy0 = lu->y;
                lu->x = cx[slot];
                lu->y = cy[slot];
                units_occupancy_notify_moved(ctx->units, sx0, sy0, lu->x, lu->y);
              }
            }
            {
              ColonizeUnit* lu = units_get(ctx->units, uid);
              if (lu) {
                lu->moves_left = 0; /* landing consumes the turn */
                if (ctx->map) {
                  lu->col1_vis_mask |=
                    units_vis_mask_for_tile(ctx->map, lu->x, lu->y, crown);
                }
              }
            }
            map_reveal_radius(ctx->map, cx[slot], cy[slot], crown, 2);
            force[k]--;
            need--;
            slot = (slot + 1) % usable;
          }
        }
      }
    }
  }
  if (landed) {
    ai_king_set_ref_present(ctx->col1, 1);
  }
  if (exhaust) {
    force[0] = 0;
    force[1] = 0;
    force[2] = 0;
    force[3] = 0;
  }
}

/*
 * FUN_43f7_10f0 land-troop loop (viceroy_unpacked.c:74417-74449) skips
 * pool index 2 (Man-O-War / 0x53e6) — that pool is spent by the single
 * Man-O-War spawn at :74378-74382 instead. The "naval type on a land tile"
 * puzzle of the earlier note is resolved: the scored tile is WATER
 * (`281f_0768` = `13e4_0074`, terrain 0x19/0x1a), so the ship placement is
 * plain — see ai_king_10f0_score_tile.
 */
/*
 * FUN_43f7_0082(pool k, nation): unit type for a 10f0 landing. For the human
 * at war: 0 → Cont. Army (9), 1 → Cont. Cav. (7), 2 → Man-O-War (0x12),
 * 3 → Artillery (0xb). Names are the NAMES.TXT @UNIT rows; the singular
 * fallbacks cover the test pools.
 */
static int ai_king_10f0_spawn_unit(ColonizeTurnContext* ctx, int human, int k, int x, int y) {
  static const char* names[4][4] = {
    {"Cont. Army", "Continental Army", "Regular", "Soldier"},
    {"Cont. Cav.", "Continental Cavalry", "Dragoon", "Scout"},
    {"Man-O-War", "Frigate", NULL, NULL},
    {"Artillery", NULL, NULL, NULL},
  };
  if (!ctx || !ctx->units || k < 0 || k > 3) {
    return -1;
  }
  int ty = -1;
  for (int i = 0; i < 4 && ty < 0 && names[k][i]; ++i) {
    ty = units_find_type(ctx->units, names[k][i]);
  }
  if (ty < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(ctx->units, ty, x, y);
  if (uid < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (u) {
    units_set_nation(u, human);
    if (k != 2) {
      u->profession = UNITS_JOB_SOLDIER; /* DOS unit+0x15 = 0x15 Veteran Soldier */
    }
    u->orders = 0; /* player-controlled: no AI orders */
  }
  return uid;
}

/*
 * FUN_43f7_1528 %STRING3 colony pick — ported 2026-09-06d (was: the port
 * named the *landing* colony). DOS (viceroy_unpacked.c:74471-74481, the loop
 * that runs before the popup is built) walks the whole colony array with
 * FUN_281f_09e6 and keeps the human's (`+0x1a == DS:0x5398`) COASTAL
 * (`i*0xca+0x5d62 & 0x40` = ColonizeCol1ColonyFlags.coastal) colony with the
 * largest population (`+0x1f`); the test is strict `<`, so the FIRST colony
 * at the maximum wins, and `iVar4` is seeded to 0 — with no coastal human
 * colony at all DOS names colony index 0 whatever it is. `FUN_281f_0416(3,
 * iVar4*0xca+0x5d48)` then splices that colony's name (+2) into %STRING3.
 * Returns NULL only when there is no colony array to read at all.
 */
static const char* ai_king_1528_announce_colony(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || human < 0 || human >= 4) {
    return NULL;
  }
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony && ctx->col1->head.colony_count > 0) {
    int best_pop = -1;
    uint16_t best_i = 0; /* DOS seeds iVar4 = 0, not -1 */
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human || !c->flags.coastal) {
        continue;
      }
      if (best_pop < (int)c->population) {
        best_pop = (int)c->population;
        best_i = i;
      }
    }
    if (ctx->col1->colony[best_i].name[0]) {
      return ctx->col1->colony[best_i].name;
    }
  }
  /* No Col1 colony array (synthetic fixtures): same rule over the live pool. */
  if (ctx->colonies) {
    int best_pop = -1;
    const ColonizeColony* best = NULL;
    const ColonizeColony* first = NULL;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (!first) {
        first = c;
      }
      if (ctx->map && !map_tile_is_coastal(ctx->map, c->x, c->y)) {
        continue;
      }
      if (best_pop < c->population) {
        best_pop = c->population;
        best = c;
      }
    }
    if (!best) {
      best = first;
    }
    if (best && best->name[0]) {
      return best->name;
    }
  }
  return NULL;
}

/*
 * FUN_43f7_10f0(param_1) — the ONE landing routine DOS uses for both the
 * free foreign intervention (`param_1 == 0`, reached per turn from
 * FUN_43f7_2022's else arm) and the PAID mercenary hire (`param_1 == 1`,
 * FUN_43f7_2022's `thunk_FUN_2a1f_010a(1)` tail at 75068). `paid` is that
 * flag; it changes four things and nothing else:
 *   - the pools are neither gated on nor decremented (74379, 74445);
 *   - the per-type counts come from the caller's mercenary array
 *     (DS:0x9e46 = −0x61ba) instead of the 6-troop cap arithmetic (74424);
 *   - the arrival popup is @MERCS naming rival slot 2, not @INTERVENE /
 *     @INTERVENTION naming slot 1, and there is no music switch (74396-406);
 *   - the Man-O-War that carried them in is despawned afterwards (74451).
 * Audit D6 (2026-09-10) folded ai_king_do_merc_hire_at's separate spawner
 * into this path; before that the paid hire had no terrain test at all.
 *
 * `backup_force` is the real mapped DOS pool array
 * (0x53e2/0x53e4/0x53e6/0x53e8), seeded on the declaration by
 * ai_king_seed_backup_force_1a26 — not a stand-in.
 *
 * Gate (no REF-empty condition; see the bugs.md note in the body): the
 * bells-threshold announce path (from_bells) runs unconditionally, while the
 * per-turn free drain (FUN_43f7_2022) needs the intervention-once latch
 * AI_KING_INTERVENE_ANNOUNCED_BYTE set AND the MoW pool backup_force[2]
 * nonzero.
 *
 * Landing (74378-74449): every unit is spawned for the HUMAN nation, so the
 * force is player-controlled. One Man-O-War (pool [2] −1) on the best water
 * tile beside the target colony, then land troops capped as DOS does —
 * Cont. Cav. ≤ 2 (pool [1]), Artillery ≤ 2 (pool [3]), Cont. Army = 6 minus
 * those (pool [0]) — each further capped by its pool, unloaded at the colony,
 * with a 5×5 reveal.
 *
 * Intervene nation: the saved rival slot (rival_nation_slot_1) when valid,
 * else the Euro with most colonies (tie-break land-unit force).
 *
 * Popups are the full 1528 pair: the "<country> declares war on <country>"
 * announcement fires once per game behind the same latch (with the @FRIEND
 * general and the human's largest coastal colony), the arrival line every
 * landing. Deep economy / mercenary chrome remains unported.
 *
 * `target` = DOS's `iVar2 = *(int *)0x5398` (74308), the nation the whole
 * force is spawned for and whose colonies the roulette walks. DOS hardcodes
 * the human there; it is a parameter here ONLY because the port's
 * FUN_43f7_2244 twin (ai_king_ai_peacetime_gift) is currently premised on an
 * AI beneficiary — see the lead filed against that premise. Every other
 * caller passes ctx->human_nation, which is byte-exact.
 *
 * NO independence gate: 10f0 itself has none in DOS (74270-74310 goes
 * straight into the colony walk). WoI state is the CALLERS' business —
 * FUN_43f7_2022 runs behind `0x5382 & 1` set, FUN_43f7_2244 behind it clear
 * (75088), and the free arm's own `backup_force` drain gate below stands in
 * for 2022's. Hoisting a shared gate up here made the peacetime paid path
 * unreachable.
 */
static void ai_king_10f0_land(
  ColonizeTurnContext* ctx, int target, int from_bells, int paid, const int merc_counts[4]
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units) {
    return;
  }
  if (target < 0 || target >= 4) {
    return;
  }
  uint16_t* backup = ctx->col1->head.backup_force;
  /*
   * bugs.md (user: "Foreign intervention isn't happening even though I've
   * gathered the required bells"): the old gate here returned while the
   * expeditionary pools were nonzero — but DOS has no such condition on
   * EITHER trigger, so intervention could never fire mid-war. The real DOS
   * gates: the bells-threshold announce (FUN_4345_0a22 → 74462 @INTERVENTION,
   * sets 0x5382 bit2) is blocked only by that bit; the per-turn free drain
   * (FUN_43f7_2022 line 75007) needs bit2 SET and the MoW pool
   * (0x53e6 = backup_force[2]) nonzero. bit2 is set ONLY by the announce
   * (74493) — it is the intervention-once latch, not "REF on the map";
   * AI_KING_INTERVENE_ANNOUNCED_BYTE models it here.
   */
  if (!paid) {
    if (!from_bells) {
      if (ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) == 0 ||
          backup[2] == 0) {
        return;
      }
    }
    if (ai_king_force_total(backup) <= 0) {
      return;
    }
  }
  /* DOS 74308 `iVar2 = *(int *)0x5398` — one nation drives the colony walk,
   * the spawn owner and the popup tokens alike. */
  const int human = target;
  int hx = 0;
  int hy = 0;
  int sx = 0;
  int sy = 0;
  if (ai_king_10f0_pick_colony(ctx, human, &hx, &hy) < 0) {
    if (ai_king_weakest_port(ctx, human, &hx, &hy) < 0) {
      return;
    }
  }
  /* Rolled a colony with no ocean-reachable water beside it (a lake port):
   * fall back to any human colony that has one instead of skipping the
   * whole landing. */
  if (!ai_king_10f0_pick_spawn(ctx, human, hx, hy, &sx, &sy) && ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (ai_king_10f0_pick_spawn(ctx, human, c->x, c->y, &sx, &sy)) {
        hx = c->x;
        hy = c->y;
        break;
      }
    }
  }
  /*
   * DOS names a different power in each mode's arrival line: the free
   * intervention splices `FUN_281f_09a4(*0x53d4)` (rival slot 1, the ally)
   * into %STRING1 for @INTERVENE (74396), the paid hire
   * `FUN_281f_09a4(*0x53d6)` (rival slot 2) for @MERCS (74403) — the same
   * 0x53d6 the @MERCENARIES offer names as the seller (75048).
   */
  const int ally1 = ai_king_intervention_nation_slot(ctx, human, paid ? 1 : 0);
  if (ally1 < 0) {
    return;
  }
  /*
   * FUN_43f7_10f0 74378–74449, resolved 2026-08-28 (was P5.5 "control"):
   * every unit is spawned for DS:0x5398 — the HUMAN's nation — so the
   * intervention force is player-controlled. A Man-O-War (type 0x12) lands
   * on the best water tile next to the colony (pool 0x53e6 −1), then the
   * land troops: Cont. Cav. ≤ 2 (0x53e4), Artillery ≤ 2 (0x53e8), Cont.
   * Army = 6 − those (0x53e2), each capped by its pool; +0x15 = Veteran;
   * unloaded at the colony (`0948`); 5×5 reveal around the colony.
   */
  if (!ai_king_10f0_pick_spawn(ctx, human, hx, hy, &sx, &sy)) {
    return; /* DOS: no scored tile → nothing lands this turn */
  }
  const int mow = ai_king_10f0_spawn_unit(ctx, human, 2, sx, sy);
  if (mow < 0) {
    return;
  }
  if (!paid && backup[2] > 0) {
    backup[2]--; /* DOS: `if (param_1 == 0) *0x53e6 -= 1` (74379) */
  }
  if (ctx->map) {
    map_reveal_tile(ctx->map, sx, sy, human);
  }
  int landings = 1;
  static const int pool_k[3] = {0, 1, 3};
  /*
   * DOS 74409-74427 computes the free-drain caps into local_50[] and mins
   * each against its pool, then — for the paid hire — throws that away:
   * `if (param_1 != 0) local_8 = *(int *)(iVar4 + -0x61ba);`, i.e. the
   * mercenary count array FUN_43f7_2022 filled (DS:0x9e46/0x9e48/0x9e4c,
   * −0x61ba == 0x9e46). Neither the pools nor 0x53e2+ are touched in that
   * mode.
   */
  int want[4] = {0, 0, 0, 0};
  if (paid) {
    for (int pi = 0; pi < 3; ++pi) {
      const int k = pool_k[pi];
      want[k] = merc_counts ? merc_counts[k] : 0;
      if (want[k] < 0) {
        want[k] = 0;
      }
    }
  } else {
    int caps[4] = {0, 0, 0, 0};
    caps[1] = backup[1] > 2 ? 2 : (int)backup[1];
    caps[3] = backup[3] > 2 ? 2 : (int)backup[3];
    caps[0] = 6 - (caps[1] + caps[3]);
    for (int pi = 0; pi < 3; ++pi) {
      const int k = pool_k[pi];
      want[k] = caps[k] > (int)backup[k] ? (int)backup[k] : caps[k];
      if (want[k] < 0) {
        want[k] = 0;
      }
    }
  }
  for (int pi = 0; pi < 3; ++pi) {
    landings += want[pool_k[pi]];
  }

  if (landings > 0) {
    /* bugs.md 258: the declaration names the PARENT countries (DOS 1528
     * passes both nations through FUN_291f_0ac8's country-name form —
     * "France declares war on England"), never the new-world colony names.
     * The arrival line uses the nationality adjective ("French Intervention
     * Force"). */
    static const char* k_euro[4] = {"English", "French", "Spanish", "Dutch"};
    static const char* k_country[4] = {"England", "France", "Spain", "Netherlands"};
    const char* ally_name = (ally1 >= 0 && ally1 < 4) ? k_euro[ally1] : "Foreign";
    const char* ally_country = (ally1 >= 0 && ally1 < 4) ? k_country[ally1] : "A foreign power";
    const char* crown_country = (human >= 0 && human < 4) ? k_country[human] : "the Crown";
    const char* colony = "the colonies";
    if (ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, hx, hy);
      const ColonizeColony* c = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
      if (c && c->name[0]) {
        colony = c->name;
      }
    }
    const char* announce_colony = ai_king_1528_announce_colony(ctx, human);
    if (!announce_colony || !announce_colony[0]) {
      announce_colony = colony;
    }
    /* @FRIEND row for the ally ("French General Lafayette", …) — DOS 1528
     * splices GAME.TXT @FRIEND[ally] into %STRING2. */
    char general[64];
    snprintf(general, sizeof(general), "%s General", ally_name);
    {
      const ColonizeMsgSection* fsec = assets_msg_find(ctx->messages, "FRIEND");
      if (fsec && ally1 >= 0 && ally1 < fsec->line_count && fsec->lines[ally1][0]) {
        str_copy_trunc(general, sizeof(general), fsec->lines[ally1]);
      }
    }

    if (ctx->status && ctx->status_size) {
      if (paid) {
        snprintf(ctx->status, ctx->status_size, "%s mercenaries arrive in %s.",
                 ally_name, colony);
      } else {
        snprintf(ctx->status, ctx->status_size, "%s Intervention Force arrives in %s!",
                 ally_name, colony);
      }
    }
    if (paid && ai_king_human_popups(ctx)) {
      /*
       * DOS 74400-74406: the paid arm shows GAME.TXT 0x12ce = @MERCS
       * ("%STRING1 mercenaries arrive in %STRING0.") with %STRING0 = the
       * landing colony name (0416(0, *0x8542+2), shared with the free arm)
       * and %STRING1 = the nationality adjective of rival slot 2. No
       * @INTERVENTION declaration, no 0498(3) music switch — those are the
       * free arm's (`param_1 == 0`) only.
       */
      PopupMsgTokens mtok;
      memset(&mtok, 0, sizeof(mtok));
      mtok.string0 = colony;
      mtok.string1 = ally_name;
      char mbody[AI_POPUP_BODY_LEN];
      char mfallback[AI_POPUP_BODY_LEN];
      snprintf(mfallback, sizeof(mfallback), "%s mercenaries arrive in %s.", ally_name, colony);
      popup_msg_fill(ctx->messages, "MERCS", &mtok, mfallback, mbody, sizeof(mbody));
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_MERC, human,
        ai_king_crown_nation_col1(ctx->col1, human), landings, NULL, mbody
      );
      units_pump_combat_popups();
    }
    if (!paid && ai_king_human_popups(ctx)) {
      char body[AI_POPUP_BODY_LEN];
      char fallback[AI_POPUP_BODY_LEN];
      /* bugs.md 258: the declares-war announcement fires ONCE per game (DOS
       * 1528 latch), the per-landing arrival popup every time. */
      if (ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) == 0) {
        ai_king_latch_set(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE, 1);
        PopupMsgTokens itok;
        memset(&itok, 0, sizeof(itok));
        itok.string0 = ally_country;
        itok.string1 = crown_country;
        itok.string2 = general;
        /* DOS 1528 names the human's largest COASTAL colony here, not the
         * tile the force happens to land on — see
         * ai_king_1528_announce_colony. */
        itok.string3 = announce_colony;
        itok.string4 = ally_name;
        snprintf(
          fallback,
          sizeof(fallback),
          "%s declares war on %s and joins the War of Independence on the Rebel side!",
          ally_country,
          crown_country
        );
        popup_msg_fill(ctx->messages, "INTERVENTION", &itok, fallback, body, sizeof(body));
        (void)ai_popup_enqueue_ok_ctx(
          ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, ally1, landings, NULL, body
        );
      }

      PopupMsgTokens atok;
      memset(&atok, 0, sizeof(atok));
      atok.string0 = colony;
      atok.string1 = ally_name;
      snprintf(
        fallback,
        sizeof(fallback),
        "%s Intervention Force arrives in %s! Local Rebel Army commander regales "
        "%s admiral.",
        ally_name,
        colony,
        ally_name
      );
      popup_msg_fill(ctx->messages, "INTERVENE", &atok, fallback, body, sizeof(body));
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, ally1, landings, NULL, body
      );
      sound_set_bgm(3); /* FUN_43f7_10f0 43f7:145b: 281f_0498(3) Independence pool… */
      sound_play(0x3f); /* …then 43f7:1465: intervention tune after @INTERVENE */
      /* bugs.md 259: the arrival popup BLOCKS before the disembark slides,
       * same sequencing as the REF landing (bugs.md 243). */
      units_pump_combat_popups();
    }
  }

  /* bugs.md 259: land troops disembark VISIBLY — spawn on the ship's tile
   * and slide into the colony through units_try_move (fires the move-watch
   * animation), like the REF landing. Unlike normal disembark rules they
   * arrive ready for action: full moves restored after the step. */
  for (int pi = 0; pi < 3; ++pi) {
    const int k = pool_k[pi];
    const int n = want[k];
    for (int s = 0; s < n; ++s) {
      const int uid = ai_king_10f0_spawn_unit(ctx, human, k, sx, sy);
      if (uid < 0) {
        break;
      }
      if (!paid && backup[k] > 0) {
        backup[k]--; /* DOS 74445: `if (param_1 == 0) *(0x53e2 + k*2) -= 1` */
      }
      ColonizeUnit* lu = units_get(ctx->units, uid);
      if (lu) {
        lu->moves_left = 3;
        lu->goto_x = hx;
        lu->goto_y = hy;
      }
      if (!units_try_move(ctx->units, uid, ctx->map, hx, hy, ctx->colonies, ctx->rng)) {
        lu = units_get(ctx->units, uid);
        if (lu) {
          const int ox = lu->x;
          const int oy = lu->y;
          lu->x = hx;
          lu->y = hy;
          units_occupancy_notify_moved(ctx->units, ox, oy, lu->x, lu->y);
        }
      }
      lu = units_get(ctx->units, uid);
      if (lu) {
        lu->moves_left = units_max_mp(ctx->units, uid);
        lu->orders = 0;
        lu->goto_x = UNITS_GOTO_NONE;
        lu->goto_y = UNITS_GOTO_NONE;
        if (ctx->map) {
          lu->col1_vis_mask |= units_vis_mask_for_tile(ctx->map, lu->x, lu->y, human);
        }
      }
    }
  }
  if (ctx->map) {
    map_reveal_radius(ctx->map, hx, hy, human, 2);
  }
  /*
   * DOS 74450-74452: `if ((param_1 != 0) && (-1 < local_1c))
   * FUN_281f_0808(local_1c);` — the paid hire's Man-O-War is the seller's
   * transport, not a gift: it drops the mercenaries and is despawned. Only
   * the free intervention leaves its hull behind for the player.
   */
  if (paid && mow >= 0) {
    (void)units_despawn(ctx->units, mow);
  }
}

static void ai_king_foreign_intervene_ex(ColonizeTurnContext* ctx, int from_bells) {
  ai_king_10f0_land(ctx, ctx->human_nation, from_bells, 0, NULL);
}

/*
 * FUN_4345_0a22 wartime spend: when the bell pool reaches the WoI threshold,
 * trigger foreign intervention / REF arrival instead of electing a Father.
 * Returns 1 when the pool should be zeroed; 0 when REF-present blocks spend.
 */
int ai_king_spend_woi_bell_pool(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return 0;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return 0;
  }
  if (nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  /*
   * DOS 0a22: `(*(byte*)0x5382 & 2) != 0 -> return` WITHOUT zeroing the pool.
   * Bit2 is the intervention-once latch (set only by the @INTERVENTION
   * announce), NOT REF presence — the old ref_present gate plus the
   * exp-pools routing below meant the bells NEVER bought the intervention
   * (they re-triggered a REF wave instead). Waves run per-turn from
   * ai_king_nation_turn on their own; the bells buy exactly one thing.
   */
  if (ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) != 0) {
    return 0; /* pool kept, same as DOS */
  }
  if (nation_id == ctx->human_nation) {
    ai_king_foreign_intervene_ex(ctx, 1);
  }
  return 1;
}

/*
 * Pack offer-time roll + landing pick into the popup payload:
 * hx(6b)<<26 | hy(6b)<<20 | qty_a(4b)<<16 | extra_flag(1b)<<15 | price(15b).
 * Since audit D6 the accept runs FUN_43f7_10f0(1), which rolls its own
 * landing colony the way DOS does, so hx/hy survive only as an offer-time
 * sanity field (a negative pair still refuses the hire). qty_a and
 * extra_flag are the load-bearing halves: they ARE the DS:0x9e46 mercenary
 * count array 2022 fills before the CHOICE. Price fits
 * 15 bits (max observed (8+2)*((4+3)*2+6)*100 = 20000 < 32768); qty_a fits
 * 4 bits (range 2-8); hx/hy fit 6 bits each (map width/height ≤ 63 in this
 * project's fixed 58×72 world).
 */

static void ai_king_foreign_intervene(ColonizeTurnContext* ctx) {
  ai_king_foreign_intervene_ex(ctx, 0);
}

static int ai_king_merc_payload(int hx, int hy, int qty_a, int extra_flag, int price) {
  return ((hx & 0x3f) << 26) | ((hy & 0x3f) << 20) | ((qty_a & 0xf) << 16) |
         ((extra_flag & 1) << 15) | (price & 0x7fff);
}

static void ai_king_merc_payload_parts(int payload, int* out_hx, int* out_hy, int* out_qty_a,
                                       int* out_extra_flag, int* out_price) {
  if (out_hx) {
    *out_hx = (payload >> 26) & 0x3f;
  }
  if (out_hy) {
    *out_hy = (payload >> 20) & 0x3f;
  }
  if (out_qty_a) {
    *out_qty_a = (payload >> 16) & 0xf;
  }
  if (out_extra_flag) {
    *out_extra_flag = (payload >> 15) & 1;
  }
  if (out_price) {
    *out_price = payload & 0x7fff;
  }
}

/*
 * FUN_43f7_2022 rebel-branch accept (viceroy_unpacked.c:75060-75070): debit
 * the rolled price, then `thunk_FUN_2a1f_010a(uVar7)` = FUN_43f7_10f0(1) —
 * the paid hire and the free intervention are the SAME landing routine, and
 * 10f0's `param_1` is exactly the paid flag. Ported 2026-09-10 (audit D6):
 * this used to be a second implementation that spawned bare "Regular"/
 * "Dragoon"/"Artillery" at (hx, hy+1) with no terrain test at all — the
 * water-spawn class bugs.md 261 fixed for 06a6 — and skipped the colony
 * roulette, the Man-O-War transport and the FUN_43f7_0082 type map (which
 * for a rebel human at war yields Cont. Army / Cont. Cav., not colonial
 * Regulars). It now hands 10f0 the mercenary counts DOS keeps at DS:0x9e46
 * (−0x61ba): slot 0 = qty_a Regulars-class, and exactly one of slot 1
 * (Cavalry) or slot 3 (Artillery) per the offer-time coin flip.
 *
 * DOS re-derives the landing colony inside 10f0 at accept time; hx/hy from
 * the payload survive only as an offer-time sanity field (see
 * ai_king_merc_payload). Returns 1 when the hire went through, 0 when the
 * treasury cannot cover the price.
 */
static int ai_king_do_merc_hire_at(ColonizeTurnContext* ctx, int human, int hx, int hy,
                                   int qty_a, int extra_flag, int price) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || human < 0 || human >= 4) {
    return 0;
  }
  if (qty_a < 1 || price < 0 || hx < 0 || hy < 0) {
    return 0;
  }
  if (europe_nation_gold(ctx->europe, ctx->col1, human) < (uint32_t)price) {
    return 0;
  }
  /* Audit G3: one debit through the accessor — the old pair read the stale
   * record and then assigned it over the live purse. DOS debits before the
   * 10f0 call (75052-75059). */
  europe_nation_gold_add(ctx->europe, ctx->col1, human, -(long)price);
  /*
   * DOS 2022 writes the count array as: 0x9e46 = qty_a; then
   * `if (rng(0,1) == 0) *0x9e4c = 1; else *0x9e48 = 1;` — 0x9e4c is slot 3
   * (Artillery), 0x9e48 slot 1 (Cavalry). extra_flag carries that raw roll.
   */
  int merc_counts[4] = {0, 0, 0, 0};
  merc_counts[0] = qty_a;
  merc_counts[extra_flag == 0 ? 3 : 1] = 1;
  /* Stand-in status first: the landing overwrites it with the @MERCS line
   * when it actually puts troops ashore. */
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size,
             "Mercenaries join the Continental cause (−%d gold).", price);
  }
  ai_king_10f0_land(ctx, human, 0, 1, merc_counts);
  return 1;
}

/*
 * Linux-invented stand-in, NOT a faithful port of FUN_43f7_2244 or
 * FUN_43f7_2022 — corrected 2026-08-14, see king_ref.md "2244/2022 —
 * corrected". Neither DOS function has an SoL/300-gold gate or a
 * once-per-war human CHOICE; 2022 is a recurring per-turn self-funded
 * roll (any Euro nation's own treasury, wartime); 2244 is 2022's
 * peacetime twin for AI nations only, unrelated to a human hire offer.
 * This function now ports 2022's rebel branch faithfully: recurring
 * per-turn 1-in-3 roll while REF is absent or the Artillery backup pool
 * is empty; on a hit, roll quantity/price and offer a CHOICE (or
 * auto-accept without ai_popups). unknown46[3] is no longer a gate —
 * DOS has no once-per-war flag here — kept only as a "pending offer
 * already queued" guard so a re-roll can't stack a second CHOICE while
 * one is unanswered.
 */
/*
 * @UNIT row name for a mercenary count slot. DOS splices the live type-table
 * pointer (`type * 0xe + 0x5230`), so a modded NAMES.TXT @UNIT block wins;
 * the DOS spelling is the fallback for the synthetic test pools.
 */
static const char* ai_king_merc_unit_name(
  const ColonizeUnitPool* units, const char* dos_name
) {
  if (units) {
    const int ty = units_find_type(units, dos_name);
    if (ty >= 0) {
      const ColonizeUnitType* t = units_type(units, ty);
      if (t && t->name[0] != '\0') {
        return t->name;
      }
    }
  }
  return dos_name;
}

static int ai_king_merc_offer_pending(const AiPopupState* st) {
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_KING_MERC) {
      return 1;
    }
  }
  return st->open && st->current.tag == AI_POPUP_TAG_KING_MERC;
}

static void ai_king_merc_offer(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->rng) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  /* Don't stack a second offer while one is already pending a response. */
  if (ai_king_human_popups(ctx) && ai_king_merc_offer_pending(ctx->ai_popups)) {
    return;
  }
  /*
   * FUN_43f7_2022 line 75007: `(*(byte*)0x5382 & 2) == 0 || *(int*)0x53e6 == 0`
   * — gate reads the Man-O-War/colony-count pool (backup_force[2], see
   * ai_king_seed_backup_force_1a26), not the Artillery pool. Bit2 is the
   * intervention-announced latch (see ai_king_foreign_intervene_ex), which
   * the port used to conflate with ref_present.
   */
  const int intervened =
    ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) != 0;
  const int mow_pool = ctx->col1->head.backup_force[2];
  if (intervened && mow_pool != 0) {
    return; /* free backup-force drain path (ai_king_foreign_intervene) covers this beat */
  }
  if (dos_rng_range(ctx->rng, 0, AI_KING_MERC_ROLL_CHANCE - 1) != 0) {
    return; /* 1-in-3 chance to even attempt this turn */
  }
  const int difficulty = ctx->col1->head.difficulty;
  const int qty_a = dos_rng_range(ctx->rng, 2, ((4 - difficulty) >> 1) + 2);
  const int extra_flag = dos_rng_range(ctx->rng, 0, 1);
  const int roll2 = dos_rng_range(ctx->rng, 0, 6);
  const int price = (qty_a + 2) * ((difficulty + 3) * 2 + roll2) * 100;
  if (europe_nation_gold(ctx->europe, ctx->col1, human) < (uint32_t)price) {
    return; /* DOS silently skips the offer when unaffordable — no status/dialog */
  }
  int hx = 0;
  int hy = 0;
  if (ai_king_weakest_port(ctx, human, &hx, &hy) < 0) {
    return;
  }
  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    /*
     * DOS 75050 `FUN_291f_0ac8(0, 0, *0x53d6)`: %STRING0 is the COUNTRY name
     * (mode 0 → FUN_15b3_0144, table DS:-0x72be) of rival slot 2 — the Euro
     * power selling the mercenaries, the same slot the @MERCS arrival line
     * names. The "Europe" stand-in that was here named nobody.
     */
    static const char* const k_country[4] = {"England", "France", "Spain", "Netherlands"};
    const int seller = ai_king_intervention_nation_slot(ctx, human, 1);
    const char* seller_name = (seller >= 0 && seller < 4) ? k_country[seller] : "Europe";
    tok.string0 = seller_name;
    /*
     * %STRING1 is a COMPOSED LIST, not one word. DOS builds it in a local
     * buffer (raw 75028-75041): the count (`FUN_281f_0182`) then DS:0x50
     * " " and the @UNIT name of type 6, then — for each set count slot —
     * DS:0x52 ", " and that slot's @UNIT name:
     *   *0x5284 = type 6  Regulars   (slot 0, the rolled quantity)
     *   *0x52a0 = type 8  Cavalry    (slot 1, 0x9e48)
     *   *0x52ca = type 11 Artillery  (slot 3, 0x9e4c)
     * (`type * 0xe + 0x5230` is the @UNIT name pointer — raw 14128.)
     * 2022's rebel roll sets exactly one extra: roll 0 → 0x9e4c Artillery,
     * roll 1 → 0x9e48 Cavalry. The port named that single extra and dropped
     * the count and the Regulars head; it also called slot 1 "Dragoons".
     */
    char merc_list[96];
    int list_n = snprintf(
      merc_list, sizeof(merc_list), "%d %s", qty_a,
      ai_king_merc_unit_name(ctx->units, "Regulars")
    );
    if (list_n < 0) {
      list_n = 0;
    }
    if ((size_t)list_n < sizeof(merc_list)) {
      snprintf(
        merc_list + list_n, sizeof(merc_list) - (size_t)list_n, ", %s",
        ai_king_merc_unit_name(ctx->units, extra_flag == 0 ? "Artillery" : "Cavalry")
      );
    }
    tok.string1 = merc_list;
    tok.number0 = price;
    tok.has_number0 = true;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "%s offers to sell us mercenaries (%s) for %d gold.",
      seller_name,
      merc_list,
      price
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "MERCENARIES", &tok, fallback, body, sizeof(body));
    char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "MERCENARIES");
    int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
    for (int i = 0; i < nch; ++i) {
      char filled[AI_POPUP_CHOICE_LEN];
      popup_msg_apply_tokens(filled, sizeof(filled), choice_buf[i], &tok);
      str_copy_trunc(choice_buf[i], sizeof(choice_buf[i]), filled);
    }
    /* GAME.TXT: No thank you. / Pay $ — map to Decline / Hire. */
    const char* labels[2];
    const int ids[] = {AI_KING_CHOICE_DECLINE, AI_KING_CHOICE_HIRE};
    if (nch >= 2) {
      labels[0] = choice_buf[0];
      labels[1] = choice_buf[1];
    } else {
      labels[0] = "No thank you.";
      labels[1] = "Pay";
    }
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_MERC, human,
                                    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
                                    ai_king_merc_payload(hx, hy, qty_a, extra_flag, price), NULL,
                                    body, labels, ids, 2)) {
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size,
                 "Mercenaries offer to join the Continental cause (−%d gold).", price);
      }
      return;
    }
  }
  (void)ai_king_do_merc_hire_at(ctx, human, hx, hy, qty_a, extra_flag, price);
}

/*
 * FUN_3844_00f2 tail (viceroy_unpacked.c:58393-58424) — @KINGFRIGATE.
 * The nation-EOT census loop above it (58239-58300) scans an 11x11 box
 * around each of the nation's colonies for foreign armed ships (@UNIT
 * table byte 0x5236+type*0xe non-zero) within path distance < 6 and sets
 * colony +0x1b bit 2 (a Frigate) / bit 1 (any other warship); DS:0xa89b
 * counts Frigate-threatened colonies, DS:0xa89a the rest. Then, when
 * (a89b != 0 || a89a > 3), the nation owns no Frigate (per-nation
 * unit-type count table -0x6db4 + 0x11 == 0), WoI is not declared
 * (0x5382 bit0) and DS:0x538e (turn) & 7 == 0: a human nation gets
 * @KINGFRIGATE Yes/No behind the 0x3e audience tune, AI nations
 * auto-accept. Yes → a Frigate spawns sailing from Europe to the nation's
 * landfall (unit flag 0x40, FUN_291f_0aee voyage roll) and, human only,
 * FUN_38fd_3dc8(KINGTAX, 10) — the ordinary +10% hike with its tea-party
 * choice. 2026-09-08d: reads the real census blockade bits (colony +0x1b
 * & 3, incl. the FUN_2a1f_027e/6662_0906 flood-cost < 6 refinement) now
 * that the 4962_0018 probe runs for the human at TURN_PROC_FINISH — DOS
 * reads DS:0xa89b/0xa89a right after the same census in 00f2. The old
 * Chebyshev-only rescan here is retired.
 */
static int ai_king_frigate_threat_counts(
  const ColonizeTurnContext* ctx, int nation, int* out_frigate, int* out_other
) {
  int frig = 0;
  int other = 0;
  if (!ctx || !ctx->colonies) {
    return 0;
  }
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0) {
      frig++;
    }
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP) != 0) {
      other++;
    }
  }
  if (out_frigate) {
    *out_frigate = frig;
  }
  if (out_other) {
    *out_other = other;
  }
  return 1;
}

static int ai_king_frigate_spawn(ColonizeTurnContext* ctx, int nation) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || nation < 0 || nation >= 4) {
    return -1;
  }
  const int ti = units_find_type(ctx->units, "Frigate");
  if (ti < 0) {
    return -1;
  }
  int x = (int)ctx->col1->nation[nation].return_from_europe_x;
  int y = (int)ctx->col1->nation[nation].return_from_europe_y;
  if (x == 0 && y == 0) {
    x = 236;
    y = 236;
  }
  const int id = units_spawn_allow_stack(ctx->units, ti, x, y);
  ColonizeUnit* u = units_get(ctx->units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation);
  u->orders = UNITS_ORDER_AI_SAIL;
  u->col1_unknown15 = (uint8_t)(u->col1_unknown15 | 0x40u);
  u->goto_x = x;
  u->goto_y = y;
  const bool magellan = founding_fathers_nation_has(ctx->col1, nation, FF_FERDINAND_MAGELLAN);
  /*
   * DOS 58419 `FUN_291f_0aee(0x281f, iVar5, x, y)` — the SAME voyage roll
   * the manual sail path uses, and it reads DS:0x9418[nation] (the
   * FUN_48d3_0002 hull tally), not a live-pool rescan. Audit second-wave
   * lead 3: this used to be a bare `units_count_sea_for_nation`, a third
   * spelling that omitted the human's EuropeScreen harbour/expected/bound
   * hulls — DOS keeps those on the Europe sentinel diagonal, inside the
   * tally. A human with his whole fleet in the harbour therefore got the
   * `< 3 hulls` fast crossing for the Crown's gift Frigate.
   */
  const int dur = europe_voyage_turns_roll(
    ctx->rng, magellan, turn_voyage_ship_count(ctx, nation)
  );
  u->turns_worked = (uint8_t)dur;
  return id;
}

static void ai_king_frigate_accept(ColonizeTurnContext* ctx, int nation) {
  if (ai_king_frigate_spawn(ctx, nation) < 0) {
    return;
  }
  if (nation == ctx->human_nation) {
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "A Royal Frigate sails for the New World.");
    }
    ai_king_tax_hike_apply(ctx, nation, 10);
  }
}

void ai_king_frigate_offer(ColonizeTurnContext* ctx, int nation) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->colonies ||
      nation < 0 || nation >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  if ((ctx->col1->head.turn & 7u) != 0) {
    return;
  }
  const int ft = units_find_type(ctx->units, "Frigate");
  /* Slot array is sparse — despawns clear a slot but decrement unit_count, so
   * bound with the array size, not the live population (as every other unit
   * sweep in this file does). Bounding by unit_count hid an existing Frigate
   * behind any hole and re-fired the offer (free ship + a real +10% tax). */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    const ColonizeUnit* u = &ctx->units->units[ui];
    if (u->active && u->nation_id == nation && u->type_index == ft) {
      return; /* per-nation Frigate count != 0 */
    }
  }
  int frig = 0;
  int other = 0;
  if (!ai_king_frigate_threat_counts(ctx, nation, &frig, &other)) {
    return;
  }
  if (frig == 0 && other <= 3) {
    return;
  }
  if (nation != ctx->human_nation || !ai_king_human_popups(ctx)) {
    ai_king_frigate_accept(ctx, nation); /* AI nations: FUN_281f_03fe skipped, local_4 = 1 */
    return;
  }
  for (int q = 0; q < ctx->ai_popups->queue_count; ++q) {
    if (ctx->ai_popups->queue[q].tag == AI_POPUP_TAG_KING_FRIGATE) {
      return;
    }
  }
  static const char* k_titles[5] = {"Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"};
  const int d = (int)ctx->col1->head.difficulty;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = k_titles[d >= 0 && d < 5 ? d : 0];
  tok.string1 = ctx->col1->player[nation].name[0] ? ctx->col1->player[nation].name
                                                    : "Your Excellency";
  tok.string2 = ctx->col1->player[nation].country_name[0]
                  ? ctx->col1->player[nation].country_name
                  : "Royal";
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    ctx->messages, "KINGFRIGATE", &tok,
    "%STRING0 %STRING1.  We note that enemy warships are preying on your undefended "
    "shipping lanes.  Shall we dispatch a frigate from the %STRING2 navy to assist you?",
    body, sizeof(body)
  );
  char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "KINGFRIGATE");
  const int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
  const char* labels[2];
  const int ids[] = {AI_KING_CHOICE_ACCEPT, AI_KING_CHOICE_REFUSE};
  if (nch >= 2) {
    labels[0] = choice_buf[0];
    labels[1] = choice_buf[1];
  } else {
    labels[0] = "Yes, I fear it is necessary.";
    labels[1] = "No. We shall attend to our own defense.";
  }
  sound_play(0x3e); /* FUN_3844_00f2 3844:0350: audience tune (281f_048e) before the CHOICE */
  if (!ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_FRIGATE, nation,
                                   ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, nation), 0, NULL, body, labels, ids, 2)) {
    ai_king_frigate_accept(ctx, nation); /* queue full: DOS has no "no answer" path */
  }
}

/*
 * FUN_43f7_2244 — peacetime twin of 2022's rebel gift, implemented
 * 2026-08-14 (see king_ref.md "2244/2022 — corrected"), ported here as an
 * AI-nation beat. Reached via FUN_281f_0668 from the generic per-Euro-nation
 * turn loop (viceroy_unpacked.c:6409-6421). The "confirmed AI-only" claim
 * this header used to carry was WRONG on the polarity of
 * `nation*0x34+0x543f` — see the PREMISE note at the end of this comment.
 *
 * Gate: WoI not yet declared, 1-in-21 roll (dos_rng_range(0,20)==0). Then
 * picks a random Euro nation 0-3 as beneficiary; eligible only if that's
 * this AI nation itself or a nation it's allied with (AI_DIPLO_ALLY bit;
 * byte-faithful DOS read — never set on Euro pairs, so in practice
 * eligibility reduces to self-only, in DOS and here alike).
 * Quantity/price shape is genuinely NOT identical to 2022's (read raw
 * bytes side by side, viceroy_unpacked.c:75098-75113 vs :75017-75028,
 * before assuming king_ref.md's "same formula" summary was byte-precise
 * — it wasn't, in the quantity roll specifically):
 *   regular = dos_rng_range(1,3)
 *   coin = dos_rng_range(0,1)
 *   coin==0: artillery = 1, then dos_rng_range(0,1)==0 → artillery += 1
 *            (so artillery ends up 1 or 2; regular stays as rolled)
 *   coin==1: regular += 1 (no Dragoon path at all in 2244 — the shared
 *            0x9e48 Dragoon slot is zeroed at entry and never written
 *            again, unlike 2022 which sometimes sets it)
 *   price = (artillery*2 + regular) * ((difficulty+4)*2 + dos_rng_range(0,6)) * 100
 * (2022's `+3` price constant becomes `+4` here, matching the doc's
 * original claim — that part *was* right).
 *
 * Paid from the ACTING nation's own gold; troops land for the BENEFICIARY
 * (self or ally) through ai_king_10f0_land's paid arm — 2244's own tail,
 * `thunk_FUN_2a1f_010a(0x281f, 1)` at 75146, which is FUN_43f7_10f0(1)
 * exactly as 2022's accept is (75068; both resolve through FUN_2a1f_010a
 * at 75377-75381). No human popup is reachable through this call chain
 * (DOS's own popup-flush call presumably auto-resolves for AI without
 * blocking, same as every other AI-context dialog in this codebase) —
 * Linux always auto-accepts when affordable, matching 2022's own no-popup
 * fallback path.
 *
 * PREMISE NOT CONFIRMED — see the lead filed 2026-09-10 (seventh wave).
 * DOS's "which nation" for the eligibility check and the landing is
 * DS:0x5398, and 0x5398 is the HUMAN nation, not the acting one; the
 * caller FUN_281f_0668 (viceroy 32150-32155) is invoked from the
 * `*(char *)(n*0x34+0x543f) == '\0'` arm of the nation loop (6409-6421),
 * and that byte is 0 for a HUMAN nation (FUN_3844_00f2's @KINGFRIGATE
 * takes the interactive CHOICE + tax-hike branch on the same test,
 * 58396-58421, and the `== '\x01'` sibling arm at 6397 is the AI turn).
 * Read literally, 2244 is the PEACETIME twin of 2022's @MERCENARIES offer
 * to the human (dialog tag 0x134c vs 2022's 0x1340, both @MERCENARIES),
 * debited from `*0x84fc` = the acting player's own record. Re-premising it
 * moves the call site off the AI loop, changes the payer and the shared
 * RNG stream, and rewrites the unit test's seed assumptions — its own
 * pass. Until then `nation_id` stands in for 0x5398 in both the
 * eligibility test and the payer, and `beneficiary` is threaded into
 * ai_king_10f0_land's `target` parameter (which DOS hardcodes to 0x5398).
 */
void ai_king_ai_peacetime_gift(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->rng) {
    return;
  }
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return; /* peacetime only */
  }
  if (dos_rng_range(ctx->rng, 0, 20) != 0) {
    return; /* 1-in-21 */
  }
  const int beneficiary = dos_rng_range(ctx->rng, 0, 3);
  int eligible = (beneficiary == nation_id);
  if (!eligible && beneficiary >= 0 && beneficiary < 4) {
    eligible = (ai_diplo_read(ctx->col1, nation_id, beneficiary) & AI_DIPLO_ALLY) != 0;
  }
  if (!eligible) {
    return;
  }

  const int difficulty = ctx->col1->head.difficulty;
  int regular = dos_rng_range(ctx->rng, 1, 3);
  int artillery = 0;
  if (dos_rng_range(ctx->rng, 0, 1) == 0) {
    artillery = 1;
    if (dos_rng_range(ctx->rng, 0, 1) == 0) {
      artillery += 1;
    }
  } else {
    regular += 1;
  }
  const int roll = dos_rng_range(ctx->rng, 0, 6);
  const int price = (artillery * 2 + regular) * ((difficulty + 4) * 2 + roll) * 100;

  ColonizeCol1Nation* payer = &ctx->col1->nation[nation_id];
  if (payer->gold < (uint32_t)price) {
    return; /* DOS silently skips when unaffordable — no status/dialog */
  }
  /*
   * DOS 75091 `*(int *)0x53d6 = iVar4` — the rolled nation is stamped into
   * rival slot 2 BEFORE the offer, and that is the slot 10f0's paid arm
   * reads for the @MERCS arrival line (74403) and the @MERCENARIES offer
   * for %STRING0 (75048). Stamp it the same way so the landing names the
   * seller instead of falling back to the colony-count heuristic.
   */
  if (beneficiary >= 0 && beneficiary < 4) {
    ctx->col1->head.rival_nation_slot_2 = (uint16_t)beneficiary;
  }
  /*
   * DOS 75136-75146: debit, then `thunk_FUN_2a1f_010a(0x281f, 1)` =
   * FUN_43f7_10f0(1) — 2244 tails into the SAME paid landing routine as
   * 2022 (viceroy_unpacked.c:75146 vs :75068, both resolving through
   * FUN_2a1f_010a at :75377-75381). Ported 2026-09-10 (third-wave lead 2):
   * this used to be a third copy of the divergent spawner audit D6 deleted
   * from the paid merc hire — bare "Regular"/"Artillery" dropped at
   * `(hx, hy+1)` with no terrain test, i.e. the water-spawn class bugs.md
   * 261 fixed for 06a6, and no Man-O-War transport, no colony roulette, no
   * FUN_43f7_0082 type map. The mercenary count array DOS fills at
   * DS:0x9e46 is `{regular, 0, -, artillery}`: 2244 never writes 0x9e48
   * (slot 1, Cavalry) — it is zeroed at entry (75096-75099) and only 2022
   * ever sets it — and puts its 1-or-2 guns in 0x9e4c (slot 3).
   *
   * DOS debits unconditionally, before 10f0 has any chance to fail its
   * colony roulette or water-tile scan, so the gold goes whether or not a
   * hull actually lands. Kept literal.
   */
  int merc_counts[4] = {0, 0, 0, 0};
  merc_counts[0] = regular;
  merc_counts[3] = artillery;
  /* Payer (nation_id) is always AI-controlled (the caller only runs this
   * for AI turns) — no ctx->europe mirror to sync, that field only
   * shadows the human's own treasury. */
  payer->gold -= (uint32_t)price;
  ai_king_10f0_land(ctx, beneficiary, 0, 1, merc_counts);
}

/*
 * FUN_38fd_5930 (viceroy_unpacked.c 68305-68415) — @KINGNEWWAR, the
 * PEACETIME crown declaration run from the Europe-EOT king slot: the King
 * cancels the human's peace with a random peer he is at peace with, then
 * pays compensation. Gates (raw 68335-68365): human slot, not Franklin
 * (FUN_281f_07b4(nation, 0x13)), `(difficulty + 2) * turn > 799`, at least
 * one peace peer, NO met-but-not-at-peace peer, peer land strength sum
 * (-0x6be4, 14 passes) ≤ own, and a `rng(0, (4 - peace_n) * 20) <=
 * difficulty` roll. Grant (raw 68372-68411): count/gold scaled by the
 * field-combat gap (-0x6bd4), capped at 6 − difficulty and (5 − difficulty)
 * * 500, Veteran Soldiers (profession 0x15) spawned in Europe, relation bits
 * 0x40 cleared / 0x10 set, war year latched at 0x53c8.
 *
 * (The FUN_43f7_2022 REF land hunt / MoW unload / capital rally / 1eca
 * promote description that used to sit here documented code deleted with D1
 * — see the closure note above ai_king_ref_pre_euro_beat.)
 */
int ai_king_new_war_event(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->rng || !ctx->turn_number) {
    return 0;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4 || col1->player[human].control != 0) {
    return 0;
  }
  if (founding_fathers_nation_has(col1, human, FF_BENJAMIN_FRANKLIN)) {
    return 0; /* FUN_281f_07b4(nation, 0x13) */
  }
  const int difficulty = (int)col1->head.difficulty;
  const int turn = (int)*ctx->turn_number;
  if ((difficulty + 2) * turn <= 799) {
    return 0;
  }
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  int peace_n = 0;
  int met_no_peace = 0;
  long strength_peers = 0;
  long strength_self = 0;
  for (int p = 0; p < 4; ++p) {
    if (p == human || p == crown || (col1->nation[p].nation_flags & 0x04) != 0) {
      continue; /* self / REF nation / independent */
    }
    /* Raw byte, not ai_diplo_read: DOS reads DS:-0x77c4 directly and an unmet
     * pair is 0 there (ai_diplo_read synthesizes PEACE|MET for unwritten pairs). */
    const uint8_t rel = col1->nation[human].euro_relation[p];
    if (rel & AI_DIPLO_PEACE) {
      peace_n++;
    }
    if ((rel & (AI_DIPLO_PEACE | AI_DIPLO_MET)) == AI_DIPLO_MET) {
      met_no_peace++;
      /* DOS sums the two -0x6be4 strengths 14x (loop 1..14, per-continent shape). */
      strength_peers += 14L * (long)col1->stuff.land_combat_strength[p];
      strength_self += 14L * (long)col1->stuff.land_combat_strength[human];
    }
  }
  if (peace_n == 0 || met_no_peace != 0 || strength_peers > strength_self) {
    return 0;
  }
  if (dos_rng_range(ctx->rng, 0, (4 - peace_n) * 20) > difficulty) {
    return 0;
  }
  int peer = -1;
  for (int tries = 0; tries < 64 && peer < 0; ++tries) {
    int cand;
    do {
      cand = dos_rng_range(ctx->rng, 0, 3);
    } while (cand == human);
    if ((col1->nation[human].euro_relation[cand] & AI_DIPLO_PEACE) != 0 &&
        (col1->nation[cand].nation_flags & 0x04) == 0) {
      peer = cand;
    }
  }
  if (peer < 0) {
    return 0;
  }
  int count = 1;
  int gold = (difficulty + 1) * 100;
  const int fc_self = (int)col1->stuff.field_combat_totals[human];
  const int fc_peer = (int)col1->stuff.field_combat_totals[peer];
  if (fc_self < fc_peer) {
    const int gap = fc_peer - fc_self;
    count = (gap >> 3) + 1;
    gold += gap * 25;
  }
  if (count > 6 - difficulty) {
    count = 6 - difficulty;
  }
  if (gold > (5 - difficulty) * 500) {
    gold = (5 - difficulty) * 500;
  }
  if (count < 0) {
    count = 0;
  }

  static const char* k_titles[5] = {"Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"};
  const char* peer_name =
    col1->player[peer].country_name[0] ? col1->player[peer].country_name : "rival";
  if (ctx->ai_popups) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = k_titles[difficulty >= 0 && difficulty < 5 ? difficulty : 0];
    tok.string1 = col1->player[human].name[0] ? col1->player[human].name : "Governor";
    tok.string2 = peer_name;
    tok.number0 = gold;
    tok.has_number0 = true;
    tok.number1 = count;
    tok.has_number1 = true;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "The Crown has declared war on the %s and cancelled your peace. It sends %d$ and %d "
      "Veteran Soldier units.",
      peer_name,
      gold,
      count
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "KINGNEWWAR", &tok, fallback, body, sizeof(body));
    if (ai_popup_enqueue_ok_ctx(
          ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human, peer, gold, NULL, body
        )) {
      /* DOS 38fd:5b6e `PUSH 0x1134` / 38fd:5b71 `CALLF 291f:0ad4` — 0ad4 is
       * the King-flair message helper (FUN_6f74_378a: DS:0x1f5c = 8, then
       * show), so @KINGNEWWAR wears KING.SS like the tax audience does. */
      ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
    }
  }
  europe_nation_gold_add(ctx->europe, col1, human, (long)gold); /* audit G3 */
  /* FUN_281f_095c(type 1 Soldier, nation, -20,-20) x count, profession 0x15 = Veteran:
   * the units appear in Europe — Linux puts them on the docks. */
  if (ctx->europe) {
    for (int i = 0; i < count; ++i) {
      if (!europe_dock_push_load(ctx->europe, "Veteran Soldier", UNITS_JOB_SOLDIER)) {
        break;
      }
    }
  }
  ai_diplo_clear_both(col1, human, peer, AI_DIPLO_PEACE);
  ai_diplo_or_both(col1, human, peer, AI_DIPLO_CROWN_ARMED);
  col1->head.nation_relation[peer] = (int16_t)turn; /* DS:0x53c8[peer] = turn */
  return 1;
}

/*
 * DS:0x9456[nation] — census FUN_4962_0018 (viceroy_unpacked.c:78146 clear,
 * :78182/:78185 the two increments): the count of that nation's SHIP units
 * (type 0x0d..0x12) parked on a Europe x-sentinel, i.e. `x - nation == -0xc`
 * (244+n, "sailing to Europe") or `== -0x10` (240+n, docked in Europe) — see
 * docs/save_format_map.md row `x`/`y`. DS:0x945a is the land-unit twin
 * (236+n, already used by the 5d04 hire tail).
 *
 * The port has no Europe dock for the crown slot, so a crown hull that
 * reaches the high seas leaves the map outright (below) and this count is
 * the number of crown ships still standing ON a high-seas tile, i.e. the
 * ones mid-crossing.
 */
static int ai_king_crown_ships_in_europe_lane(const ColonizeTurnContext* ctx, int crown) {
  if (!ctx || !ctx->units || !ctx->map) {
    return 0;
  }
  int n = 0;
  /*
   * Slot walk, `u->id` to the id-taking accessors. `units_get_const` and
   * `units_is_sea` take a unit ID; ids are handed out monotonically from 1
   * and never recycled (units.c:337), so an `i`-as-id walk over
   * COLONIZE_UNITS_MAX dropped every unit with id >= 256 in a long game plus
   * the highest slot in a short one. DOS walks the unit ARRAY in record
   * order (raw 78159). Fixed 2026-09-10 (audit second-wave Leads item 2).
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(ctx->units, u->id)) {
      continue;
    }
    if (map_tile_is_high_seas(ctx->map, u->x, u->y)) {
      n++;
    }
  }
  return n;
}

/*
 * The crown Man-O-War's return-home beat — the real DOS one, replacing the
 * "despawn idle empty crown MoWs at wave start" stand-in that used to sit in
 * ai_king_ref_wave (and the `4d56 ship act` label it carried: overlay 4d56 is
 * the Indian AI overlay end to end — see FUNCTION_CATALOG rows 0038…4528 —
 * and holds no crown code at all).
 *
 * The beat lives at the tail of the `FUN_521d_20e6` ship band
 * (viceroy_unpacked.c:89717-89720; recovered listing
 * move_scoring_20e6_full.md:2036-2040). Every disjunct below is the DOS
 * *skip* test, so the arm fires when all of them are false:
 *
 *   if (  (DS:0x5382 & 1) == 0                  // not at war
 *      || unit+0x3146 != 0x12                   // not a Man-O-War
 *      || iStack_6 != 0                         // orders byte +0x314b is 't'/'i'
 *      || iStack_a8 != 0                        // 8aac(unit,2)-1: tile stack minus self
 *      || DS:0x53de != 0                        // MoW pool (expeditionary_force[2]) not empty
 *      || DS:0x9456[nation] != 0                // a ship of this nation is already in the Europe lane
 *      || DS:0x53da+0x53dc+0x53e0 == 0 )        // no land pools left
 *     { ...ordinary ship arms... }
 *   // else falls through to LAB_521d_3fa6:
 *   //   FUN_291f_02ea -> FUN_48d3_015e: expanding-ring hunt for a High Seas
 *   //   tile (class 0x1a, owner nibble < 0 or own nation), bump
 *   //   DS:0x9456+nation, act_state +0x314c = 3 (or 0xb), latch the tile in
 *   //   +0x314d/e and stamp orders +0x314b = 0x45 — i.e. sail home.
 *
 * Nothing credits a pool: `expeditionary_force[]` is untouched on the way
 * out. The fleet cadence comes from FUN_43f7_0982's own opening gate
 * (`force[2] == 0 && crown Man-O-War count (-0x6da2) == 0 -> force[2]++`,
 * raw 73990-73993), which can only fire once the hull is off the map.
 *
 * Port mapping of the two opaque operands:
 *   - `iStack_6` is set (raw 1144-1149 of the recovered listing) only from
 *     the 0a60 goal pass's order codes 't' (FOUND) / 'i' (MIL_EXPAND). No
 *     REF type can pursue FOUND/MIL_EXPAND (name-gated to Pioneer/Colonist
 *     kinds), so the term stays 0 for a crown Man-O-War even now that the
 *     crown runs the full euro turn (D1 closed 2026-09-07g).
 *   - `iStack_a8` = FUN_1000_8aac(unit, 2) - 1 = the ship's tile stack minus
 *     itself (case 2 = TOTAL stack count, ai_euro.c's 8aac table). DOS
 *     passengers sit at (-2,-2) and never count; the port's sit in
 *     cargo_ids, so this is the tile scan alone and the caller runs the arm
 *     only for an empty hull.
 *
 * Departure: DOS hands the hull to the Europe lane (x = 244+nation) and the
 * crown's own Europe dock. The port models no crown dock, so a crown MoW
 * standing on the high-seas tile it was sent to leaves the map here — the
 * same net effect (hull gone, no pool credit) the old stand-in produced, but
 * now on the DOS trigger instead of a turns_worked counter.
 */
int ai_king_mow_sail_home_20e6(ColonizeTurnContext* ctx, ColonizeUnit* u, int crown) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1 || !u) {
    return 0;
  }
  /* Reached the crossing: the hull is home (DOS: into the Europe lane). */
  if (map_tile_is_high_seas(ctx->map, u->x, u->y)) {
    (void)units_despawn(ctx->units, u->id);
    return 1;
  }
  const uint16_t* force = ctx->col1->head.expeditionary_force;
  if (force[2] != 0) {
    return 0; /* DS:0x53de */
  }
  if ((int)force[0] + (int)force[1] + (int)force[3] == 0) {
    return 0; /* DS:0x53da + 0x53dc + 0x53e0 */
  }
  if (ai_king_crown_ships_in_europe_lane(ctx, crown) != 0) {
    return 0; /* DS:0x9456[nation] */
  }
  /* iStack_a8: any other unit sharing the ship's tile blocks the beat. */
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->id == u->id || o->aboard_ship_id >= 0) {
      continue;
    }
    if (o->x == u->x && o->y == u->y) {
      return 0;
    }
  }
  int hx = 0;
  int hy = 0;
  if (!units_spiral_place_hs_near(ctx->units, ctx->map, u->x, u->y, crown, &hx, &hy)) {
    return 0;
  }
  u->orders = UNITS_ORDER_AI_SAIL; /* +0x314b = 0x45 */
  u->goto_x = hx;
  u->goto_y = hy;
  if (u->moves_left > 0) {
    const int sdx = (hx > u->x) - (hx < u->x);
    const int sdy = (hy > u->y) - (hy < u->y);
    const int nx = u->x + sdx;
    const int ny = u->y + sdy;
    if ((sdx != 0 || sdy != 0) && map_tile_is_water(ctx->map, nx, ny) &&
        units_id_at(ctx->units, nx, ny) < 0) {
      units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
    }
    if (u->active && map_tile_is_high_seas(ctx->map, u->x, u->y)) {
      (void)units_despawn(ctx->units, u->id);
    }
  }
  return 1;
}

static void ai_king_war_act(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /*
   * bugs.md: DS:0x5382 bit1 means "REF currently on the map", not "war
   * declared" — the port set it at the declaration and never cleared it,
   * which permanently blocked the bell-pool spend (foreign intervention /
   * next-wave trigger). Clear it once no crown unit remains in the New
   * World, so wiping a wave re-arms the pool.
   */
  if (ctx->col1->head.game_options.ref_present && ctx->units) {
    const int crown_now = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation);
    bool crown_on_map = false;
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      /* Loose test on purpose: any live crown-slot unit in the New World
       * (incl. hold passengers) keeps the presence armed. */
      if (u->active && u->nation_id == crown_now && u->x < 200) {
        crown_on_map = true;
        break;
      }
    }
    /* Colonies the crown captured keep the presence too. */
    if (!crown_on_map && ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (c->active && c->nation_id == crown_now) {
          crown_on_map = true;
          break;
        }
      }
    }
    if (!crown_on_map) {
      ai_king_set_ref_present(ctx->col1, 0);
    }
  }
  const int human = ctx->human_nation;
  /* bugs.md 256: DOS 2022 runs the mobilization gate BEFORE any other war
   * beat — the mobilization turn does nothing else (no intervene/merc). The
   * gate block below returns early, so intervene/merc moved after it. */
  const int mobilization_due =
    human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT &&
    (ctx->col1->nation[human].nation_flags & 0x08u) == 0;
  if (!mobilization_due) {
    /*
     * Rebel arm first: 10f0 while human ports still exist (crown move/capture
     * below may seize the landing pick). In addition to 06a6 in ref_wave.
     */
    ai_king_foreign_intervene(ctx);
    /* Real 2022: recurring per-turn rebel merc gift (hire CHOICE / auto). */
    ai_king_merc_offer(ctx);
  }

  /*
   * FUN_43f7_1eca full port. Per colony owned by the rebel nation with
   * colony SoL>49 (decomp `0x31 < iVar1`):
   *   cap = max(1, min(pop>>1, pop*(sol-50)/50))
   * Walk *only* the units stationed on that colony's own tile (decomp
   * FUN_281f_07e0/02e4 tile-stack walk — not every unit the nation owns)
   * and promote up to `cap` of them that are FORTIFIED, base type Soldier
   * or Dragoon (decomp tests raw type id 1 / 4 only — Regulars and
   * already-Continental units never match and are untouched), AND Veteran
   * status (decomp `unit+0x315b == 0x15` = UNITS_JOB_SOLDIER "Veteran
   * Soldiers" — confirmed 2026-08-14 via the same offset/adjacent-code
   * cross-reference as the case-8/9 Pioneer profession `0x14`; an ordinary
   * armed colonist without Veteran profession does not promote).
   * Soldier → Continental Army, Dragoon → Continental Cavalry. Pops a
   * singular/plural status line per colony that actually promoted someone
   * (decomp 0x132d "one unit" / 0x1336 "%d units"). Washington FF mass
   * promote / combat-upgrade path is separate and untouched here. The
   * SoL 40..49 "restless" band is status-text only (below), not a promote
   * band in 1eca.
   */
  /* bugs.md 256 + DOS FUN_43f7_2022: the 1eca mobilization runs exactly ONCE,
   * on the first war-act turn after the declaration — gated on nation_flags
   * bit 0x08 (`*(byte*)*[0x84fc] & 8`), set right after, and DOS `return`s so
   * the mobilization turn does nothing else on the war path. */
  if (human >= 0 && human < (int)COLONIZE_COL1_NATION_COUNT &&
      (ctx->col1->nation[human].nation_flags & 0x08u) == 0) {
    ctx->col1->nation[human].nation_flags |= 0x08u;
    int army = units_find_type(ctx->units, "Continental Army");
    if (army < 0) {
      army = units_find_type(ctx->units, "Cont. Army");
    }
    int cav = units_find_type(ctx->units, "Continental Cavalry");
    if (cav < 0) {
      cav = units_find_type(ctx->units, "Cont. Cav.");
    }
    const int soldier_ty = units_find_type(ctx->units, "Soldier");
    const int dragoon_ty = units_find_type(ctx->units, "Dragoon");
    if (ctx->col1->colony && (army >= 0 || cav >= 0) &&
        (soldier_ty >= 0 || dragoon_ty >= 0)) {
      for (uint16_t ci = 0; ci < ctx->col1->head.colony_count; ++ci) {
        const ColonizeCol1Colony* c = &ctx->col1->colony[ci];
        if ((int)c->nation_id != human) {
          continue;
        }
        const int sol_p = ai_king_colony_sol_at(ctx, human, (int)c->x, (int)c->y);
        if (sol_p <= 49) {
          continue;
        }
        const int pop = c->population;
        int cap = pop * (sol_p - 50) / 50;
        if (pop / 2 < cap) {
          cap = pop / 2;
        }
        if (cap < 1) {
          cap = 1;
        }
        int promoted = 0;
        const char* promoted_from = "Soldiers"; /* DOS %STRING1 = pre-promote type name */
        for (int i = 0; i < COLONIZE_UNITS_MAX && cap > 0; ++i) {
          ColonizeUnit* u = &ctx->units->units[i];
          if (!u->active || u->nation_id != human) {
            continue;
          }
          if (u->x != (int)c->x || u->y != (int)c->y) {
            continue;
          }
          /*
           * DOS gates on unit+0x3146 (raw type 1/4) and unit+0x315b == 0x15
           * only — that profession code is UNITS_JOB_SOLDIER ("Veteran
           * Soldiers"), confirmed 2026-08-14 by cross-referencing
           * FUN_43f7_1eca against the already-established 0x14=Pioneer
           * profession code from the case-8/9 terrain-improve investigation.
           * Only Veteran-status Soldier/Dragoon promote — an ordinary armed
           * colonist (profession UNITS_JOB_NONE) does not.
           * No FORTIFIED requirement: re-verified 2026-08-24 by reading the
           * complete raw FUN_43f7_1eca body (viceroy_unpacked.c:74910-74972)
           * end to end — its only two per-unit tests are the type byte at
           * unit+0x3146 and the profession byte at unit+0x315b. `orders`
           * (ViceroyUnit.orders, original_sources_annotated/include/
           * viceroy_types.h) lives at unit+0x08 (0x314c), an address this
           * function never touches. The prior `u->orders !=
           * UNITS_ORDER_FORTIFIED` gate here (removed this pass) was an
           * unsupported over-restriction — DOS promotes any Veteran-status
           * Soldier/Dragoon on the colony's own tile regardless of
           * fortified/sentry/active order state. See king_ref.md "1eca
           * Continental promote" and docs/sons_of_liberty.md (both
           * corrected 2026-08-24).
           */
          if (u->profession != UNITS_JOB_SOLDIER) {
            continue;
          }
          if (soldier_ty >= 0 && u->type_index == soldier_ty && army >= 0) {
            const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
            promoted_from = ty && ty->name[0] ? ty->name : "Soldiers";
            u->type_index = army;
          } else if (dragoon_ty >= 0 && u->type_index == dragoon_ty && cav >= 0) {
            const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
            promoted_from = ty && ty->name[0] ? ty->name : "Dragoons";
            u->type_index = cav;
          } else {
            continue;
          }
          --cap;
          ++promoted;
        }
        if (promoted > 0 && ctx->status && ctx->status_size) {
          if (promoted == 1) {
            snprintf(ctx->status, ctx->status_size,
                     "A rebel unit has been promoted to Continental status!");
          } else {
            snprintf(ctx->status, ctx->status_size,
                     "%d rebel units have been promoted to Continental status!",
                     promoted);
          }
        }
        /* bugs.md 238: the mustering gets its own dialog per colony —
         * GAME.TXT @MOBILIZE (one unit, %STRING0 colony / %STRING1 type) or
         * @MOBILIZE2 (%NUMBER0 units). */
        if (promoted > 0 && ai_king_human_popups(ctx)) {
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = c->name[0] ? c->name : "our colony";
          char body[AI_POPUP_BODY_LEN];
          if (promoted == 1) {
            /* DOS 1eca %STRING1 = the promoted unit's pre-promote type name
             * (last FUN_281f_0438 slot-1 write), not a constant. */
            tok.string1 = promoted_from;
            popup_msg_fill(
              ctx->messages, "MOBILIZE", &tok,
              "Continental Army mobilizes! Our Veteran unit has been promoted to "
              "Continental Army status.",
              body, sizeof(body)
            );
          } else {
            tok.has_number0 = true;
            tok.number0 = promoted;
            popup_msg_fill(
              ctx->messages, "MOBILIZE2", &tok,
              "Continental Army mobilizes! %NUMBER0 Veteran units have been "
              "promoted to Continental Army status.",
              body, sizeof(body)
            );
          }
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_INFO, human, -1, promoted, NULL, body
          );
        }
      }
    }
    /* DOS 2022: mobilization turn ends the war path here. */
    return;
  }

  /*
   * bugs.md: no rebel-side auto-play. The block that used to sit here marched
   * every idle human Cont. Army / Cont. Cav toward the founding capital
   * (AI_MOVE) and fortified two of them on it. That was never DOS: it was
   * sourced from a fandom description of the Continentals plus the REF's own
   * main-port MD slack, and it ran on the *human* nation's units every war
   * turn. FUN_43f7_1eca is a promote-only routine — the full end-to-end read
   * (viceroy_unpacked.c:74910-74972, king_ref.md "1eca Continental promote")
   * shows it touching only unit+0x3146 (type) and unit+0x315b (profession);
   * it never reads or writes unit+0x08 (orders), unit+0x0a/0x0b (goto), and
   * neither does 2022 around it. The player's own Continentals are ordinary
   * player units — the King's turn must not order them anywhere. Symptom it
   * caused: two turns of pressing space after declaring independence and
   * fortified units all over the map woke up with an invisible Go To on the
   * capital.
   */

  /*
   * D1 closed 2026-09-07g: the per-unit substitute hunt that lived here
   * (MoW sail/unload arms, garrison/Artillery fortify, hunt-target pick,
   * greedy march) is deleted. DOS's king beat never moves a unit — the
   * whole 43f7 overlay contains zero writes to orders/goto/act-state
   * (grep-verified) — and the crown slot (control = 1, raw 74833) runs the
   * full Euro nation turn FUN_521d_6d8e after this beat (raw 6394/6407),
   * so REF units are moved by the ordinary 5b66/20e6 arms via
   * ai_euro_nation_turn on the crown slot (turn.c). Colony capture chrome
   * lives in the shared path (units_try_capture_foreign_colony); the crown
   * MoW sail-home beat lives in the euro ship band
   * (ai_king_mow_sail_home_20e6, called from ai_euro).
   */
}

/*
 * The DOS king beat for the crown slot, run from turn.c's EURO step for
 * that slot BEFORE ai_euro_nation_turn (per-slot order raw 6394/6407:
 * 3844_00f2 → 43f7_2424 → 2022, then the 6d8e thunk). 2022 is a pure
 * spawner/bookkeeper — wave landing (0982/06a6), Continental mobilization
 * (1eca), intervention (10f0) and merc roll, ref_present maintenance; it
 * never moves a unit. Movement follows in the ordinary euro turn.
 */
void ai_king_ref_pre_euro_beat(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 ||
      !ai_king_independence_declared(ctx->col1)) {
    return;
  }
  ai_king_ref_wave(ctx);
  ai_king_war_act(ctx);
}

/*
 * Revolution end — DOS FUN_3844_0442 (viceroy_unpacked.c 58470-58556, 58630):
 *   Win: the C1 triple, NO year gate (raw 58473-58485) — crown holds no
 *     colony, crown land units below the give-up bar, REF pool score
 *     `ef[0] + (ef[1]!=0) + (ef[3]!=0) < 4`; see the inner comment on the
 *     win block below for the full read.
 *   Lose: one @LOSING%d selector, last-write-wins over three tests (raw
 *     58507-58534) — ports==0 → 1, pop share ≥90% → 3, colonies==0 → 2, so
 *     the effective precedence is colonies, then pop share, then ports.
 *   Warn: the same digit patch on "@WARN%d" (raw 58506-58534, 58540) —
 *     `ports < 3 → 1`, `share ≥ 80 → 3`, `colonies < 3 → 2`, again last write
 *     wins — shown only when neither the win nor the lose dialog took the
 *     turn, so at most one @WARN%d per turn.
 *   Wartime calendar stop: exact year 1850 (raw 58630) → @RETIRING2.
 * Latches unknown46[4]; score reads won/lost.
 */
static int ai_king_human_coastal_ports(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || human < 0 || human > 3) {
    return 0;
  }
  int n = 0;
  if (ctx->colonies && ctx->map && ctx->colonies->colony_count > 0) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (map_tile_is_coastal(ctx->map, c->x, c->y)) {
        ++n;
      }
    }
    return n;
  }
  /* Col1 colony list (smoke / bridge) when runtime pool empty. */
  if (ctx->col1_ok && ctx->col1 && ctx->map && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human) {
        continue;
      }
      if (map_tile_is_coastal(ctx->map, (int)c->x, (int)c->y)) {
        ++n;
      }
    }
  }
  return n;
}

/* Active human colony count (runtime pool; Col1 only if pool empty). */
static int ai_king_human_colonies(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || human < 0 || human > 3) {
    return 0;
  }
  if (ctx->colonies && ctx->colonies->colony_count > 0) {
    return ai_king_colony_count(ctx->colonies, human);
  }
  int n = 0;
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      if ((int)ctx->col1->colony[i].nation_id == human) {
        ++n;
      }
    }
  }
  return n;
}

/*
 * @RETIRING2's estate colony: DOS raw 58631-58637 walks the colony list and
 * keeps the human-owned record with the highest population byte (+0x1f), then
 * splices its name (`local_6c * 0xca + 0x5d48`) into %STRING2.
 */
static const char* ai_king_richest_colony_name(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->colonies || human < 0) {
    return "the colonies";
  }
  const ColonizeColony* best = NULL;
  int best_pop = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    if ((int)c->population >= best_pop) { /* raw 58636 `cVar1 <= pop`: last max wins */
      best_pop = (int)c->population;
      best = c;
    }
  }
  if (best && best->name[0] != '\0') {
    return best->name;
  }
  return "the colonies";
}

/*
 * Crown share of (human+crown) colony population during WoI (@WARN3 /
 * @LOSING3, %NUMBER2).
 *
 * DOS FUN_3844_0442 (viceroy_unpacked.c:58507-58521) reads the census
 * mirror, NOT the colony list:
 *   uVar3 = colony_pop_totals[*0x53d2]   (crown)
 *   uVar4 = colony_pop_totals[*0x5398]   (human)
 *   local_8 = (uVar3 + 1) * 100 / ((uVar4 + 1) + (uVar3 + 1))
 * −0x6bf4 = DS:0x940c = stuff.colony_pop_totals[] (save_format_map.md
 * row 243); the two `if (byte == 0) x = ~byte + 1; else x = byte;` arms in
 * the raw are a sign-extension artifact — both spell the plain byte.
 *
 * The +1 on BOTH sides is load-bearing: human 1 / crown 9 is 83% in DOS
 * (a @WARN3) but was 90% (instant @LOSING3 surrender) under the old
 * per-colony `pop>0?pop:1` re-sum. Ported 2026-09-10 (audit D7). With no
 * population at all DOS answers 50, not 0.
 *
 * The mirror is refreshed every turn by
 * col1_stuff_census_refresh_colony_counts (turn.c); a blank fixture window
 * (both rows zero) falls back to summing the live pools before the +1s.
 */
static int ai_king_woi_pop_share_pct(
  const ColonizeTurnContext* ctx,
  int human,
  int crown
) {
  if (!ctx || human < 0 || human >= 4 || crown < 0 || crown >= 4) {
    return 0;
  }
  int human_pop = 0;
  int crown_pop = 0;
  if (ctx->col1_ok && ctx->col1) {
    human_pop = (int)ctx->col1->stuff.colony_pop_totals[human];
    crown_pop = (int)ctx->col1->stuff.colony_pop_totals[crown];
  }
  if (human_pop == 0 && crown_pop == 0) {
    /* Blank census window (synthetic fixtures): re-tally the same sum. */
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active) {
          continue;
        }
        const int pop = c->population > 0 ? (int)c->population : 0;
        if (c->nation_id == human) {
          human_pop += pop;
        } else if (c->nation_id == crown) {
          crown_pop += pop;
        }
      }
    }
    if (human_pop == 0 && crown_pop == 0 && ctx->col1_ok && ctx->col1 &&
        ctx->col1->colony) {
      for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
        const ColonizeCol1Colony* c = &ctx->col1->colony[i];
        const int pop = c->population > 0 ? (int)c->population : 0;
        if ((int)c->nation_id == human) {
          human_pop += pop;
        } else if ((int)c->nation_id == crown) {
          crown_pop += pop;
        }
      }
    }
  }
  return ((crown_pop + 1) * 100) / ((human_pop + 1) + (crown_pop + 1));
}

/*
 * Full-screen throne audience follow-up to the war-end announcement (DOS
 * FUN_3844_0442 → FUN_291f_0aba → FUN_75c2_20e2): the King's parting word.
 * win: @KINGLOSE text on the KINGLOSE.SS king; loss: @KINGWIN on KINGWIN.SS
 * (%STRING0 = the mother country). game_loop renders the KING_THRONE tag as
 * the full-screen KINGLSS audience and opens the retire score on dismissal.
 */
static void ai_king_enqueue_throne_audience(
  ColonizeTurnContext* ctx,
  int human,
  int crown,
  int win
) {
  if (!ai_king_human_popups(ctx)) {
    return;
  }
  static const char* const k_crown[4] = {"England", "France", "Spain", "Netherlands"};
  const char* motherland = (human >= 0 && human <= 3) ? k_crown[human] : "the Crown";
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  char body[AI_POPUP_BODY_LEN];
  char fallback[AI_POPUP_BODY_LEN];
  if (win) {
    snprintf(
      fallback,
      sizeof(fallback),
      "\"In our wisdom, we have decided to let you go your own way. Do not "
      "seek our aid in the future, for it will not be forthcoming.\""
    );
    popup_msg_fill(ctx->messages, "KINGLOSE", &tok, fallback, body, sizeof(body));
  } else {
    tok.string0 = motherland;
    snprintf(
      fallback,
      sizeof(fallback),
      "\"As expected, your attempt to separate from mother %s has proven "
      "futile. Your Rag Tag armies are simply no match for our Royal "
      "forces.\"",
      motherland
    );
    popup_msg_fill(ctx->messages, "KINGWIN", &tok, fallback, body, sizeof(body));
  }
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups, AI_POPUP_TAG_KING_THRONE, human, crown, win ? 1 : 2, NULL, body
  );
}

/*
 * DOS FUN_3844_0442 fills all three number slots once, before it decides
 * which mid-war warn popup to show (viceroy_unpacked.c:58552-58556):
 *   FUN_281f_09ae(0, local_68)                 — coastal-port count
 *   FUN_281f_09ae(1, colony_counts[human])     — colonies still held
 *   FUN_281f_09ae(2, local_8)                  — crown pop share %
 * @WARN1 reads %NUMBER0, @WARN2 %NUMBER1, @WARN3 %NUMBER2 (GAME.TXT), so
 * every warn body gets the same live triple. Ported 2026-09-10 (audit D13);
 * @WARN1/@WARN2 used to hardcode 1 and read "all but 1" with 2 ports left.
 */
static void ai_king_warn_numbers(PopupMsgTokens* tok, int ports, int colonies, int pop_pct) {
  if (!tok) {
    return;
  }
  tok->has_number0 = true;
  tok->number0 = ports;
  tok->has_number1 = true;
  tok->number1 = colonies;
  tok->has_number2 = true;
  tok->number2 = pop_pct;
}

/*
 * DOS gate for the whole win/lose/warn group: `(*0x5382 & 1) != 0 &&
 * (*0x5382 & 8) == 0` (raw 58505) — WoI declared, war not already resolved.
 * No REF-present term, so this takes no `ref_already` argument any more
 * (2026-09-10 audit lead 5).
 */
static void ai_king_check_revolution_end(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  if (ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) != AI_KING_ENDGAME_NONE) {
    return; /* already resolved */
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  const int ports = ai_king_human_coastal_ports(ctx, human);
  const int colonies = ai_king_human_colonies(ctx, human);
  const int pop_pct = ai_king_woi_pop_share_pct(ctx, human, crown);
  const ColonizeCol1Player* pl = &ctx->col1->player[human];
  const char* country =
    (pl->country_name[0] != '\0') ? pl->country_name : "the colonies";
  const char* leader = (pl->name[0] != '\0') ? pl->name : "Your Excellency";
  /*
   * Mid-war warn selector — DOS FUN_3844_0442 builds the warn tag the same
   * way it builds the lose tag: one digit patched into a base name
   * (`FUN_1d1d_07e4(local_58, 0xf39)` loads DS:0xf39 = "WARN0", then
   * `local_54 = local_54 + cVar1`, raw 58540-58541), with the three tests
   * overwriting each other in source order (raw 58506-58534):
   *     cVar1 = (ports < 3);                       → @WARN1
   *     if (0x4f < share)  cVar1 = 3;              → @WARN3   (raw 58524)
   *     if (colonies < 3)  cVar1 = 2;              → @WARN2   (raw 58530)
   * Last write wins, so the precedence is colonies, then pop share, then
   * ports, and DOS shows at most ONE @WARN%d per turn (none at cVar1 == 0).
   * The port used to latch and fire all three independently.
   *
   * The lose dialog leaves the block (`goto LAB_3844_04ec`, raw 58548) and the
   * win dialog leaves it at raw 58500, so a turn that ends the war shows no
   * warn at all — the emission below therefore sits after both.
   *
   * The whole lose/warn group is gated only by `(*0x5382 & 1) != 0 &&
   * (*0x5382 & 8) == 0` (raw 58505) — WoI declared and the war not already
   * resolved, the two conditions this function tests at its head. There is NO
   * REF-present term in DOS; the port's extra `ref_already` gate kept the
   * whole group silent until the first wave had landed.
   */
  int warn_sel = (ports < 3) ? 1 : 0;
  if (pop_pct >= AI_KING_WARN3_PCT_MIN) { /* raw 58524 `0x4f < local_8` */
    warn_sel = 3;
  }
  if (colonies < 3) {
    warn_sel = 2;
  }
  /*
   * Episode latches (port-side; DOS re-shows the selected warn every turn the
   * condition holds — see the audit lead). Each clears when its own band is
   * left, so a later relapse re-fires.
   */
  if (ports >= 3) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN1_BYTE, 0);
  }
  if (colonies >= 3) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN2_BYTE, 0);
  }
  if (pop_pct < AI_KING_WARN3_PCT_MIN) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN3_BYTE, 0);
  }
  /*
   * Lose: same digit-patch selector on "@LOSING%d" (DS:0xf29 = "LOSING0"),
   * three tests overwriting each other in source order (raw 58507-58534):
   * `ports == 0 → 1`, `share >= 90 → 3`, `colonies == 0 → 2`. Last write
   * wins, so the branch order here has to be the reverse: colonies, then pop
   * share, then ports.
   *
   * @LOSING%d %STRING2 is `FUN_291f_0ac8(2, 0, *0x53d4)` (raw 58538) — the
   * COUNTRY name of rival slot 1, the intervention ally the deposed viceroy
   * flees to. All three branches used to hardcode "Europe" there.
   */
  static const char* const k_exile_country[4] = {
    "England", "France", "Spain", "Netherlands"
  };
  const int exile_nation = ai_king_intervention_nation_slot(ctx, human, 0);
  const char* exile =
    (exile_nation >= 0 && exile_nation < 4) ? k_exile_country[exile_nation] : "Europe";
  if (colonies <= 0) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = country;
    tok.string1 = leader;
    tok.string2 = exile;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "King's Forces control all colonies in %s! Continental Congress capitulates. "
      "%s, stripped of titles, escapes to exile in %s.",
      country,
      leader,
      exile
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "LOSING2", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_WAR_END, human, crown, 4, NULL, body
      );
    }
    /* DOS lose order: @LOSINGn dialog, then the @KINGWIN gloating audience
     * (291f_0aba(2,1,0xf31)); the retire score follows its dismissal. */
    ai_king_enqueue_throne_audience(ctx, human, crown, 0);
    return;
  }
  /*
   * Lose: crown controls ≥90% of human+crown colony population.
   * GAME.TXT @LOSING3 — outranks the ports test (raw 58526-58527 writes 3
   * after 58507 wrote 1).
   */
  if (pop_pct >= AI_KING_LOSING3_PCT) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = country;
    tok.string1 = leader;
    tok.string2 = exile;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "King's Forces control over 90%% of %s population! Continental Congress "
      "capitulates. %s, stripped of titles, escapes to exile in %s.",
      country,
      leader,
      exile
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "LOSING3", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_WAR_END, human, crown, 4, NULL, body
      );
    }
    ai_king_enqueue_throne_audience(ctx, human, crown, 0);
    return;
  }
  if (ports <= 0) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = country;
    tok.string1 = leader;
    tok.string2 = exile;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "King's Forces control all ports in %s! Continental Congress capitulates. "
      "%s, stripped of titles, escapes to exile in %s.",
      country,
      leader,
      exile
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "LOSING1", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_WAR_END, human, crown, 4, NULL, body
      );
    }
    ai_king_enqueue_throne_audience(ctx, human, crown, 0);
    return;
  }
  const int year = (int)ctx->col1->head.year;
  /*
   * WIN — full DOS FUN_3844_0442 C1 (viceroy_unpacked.c 58468-58497,
   * @KINGLOSE emitter found via EXE DS-string scan, tag 0xf20):
   *   1. crown holds no colony (`*(0x53d2 - 0x6d68) == 0`);
   *   2. crown LAND force on the map — types 6/8/0xb Regulars/Cavalry/
   *      Artillery only, ships never count — is below the give-up bar:
   *      <1 normally, <8 once bit 0x40 (crown captured a colony this war,
   *      game_options.ref_unit_threshold) is set;
   *   3. REF pool score `ef[0] + (ef[1]!=0) + (ef[3]!=0) < 4` — the MoW
   *      pool (ef[2]) is ignored, and up to 3 pooled Regulars still allow
   *      the concession.
   * game_options.independence_force (0x5382 bit 0x20, the cheat) bypasses
   * gates 2 and 3 and the colony gate, as in DOS. No year gate.
   */
  int crown_colonies = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == crown) {
        ++crown_colonies;
      }
    }
  }
  int crown_land = 0;
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != crown) {
        continue;
      }
      /* DOS counts types 6/8/0xb — Regulars / Cavalry / Artillery. Matched
       * by NAME here (synthetic test pools reorder type indices). */
      const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
      const char* n = t ? t->name : NULL;
      if (n && (strstr(n, "Regular") ||
                (strstr(n, "Cavalry") && !strstr(n, "Cont")) ||
                strstr(n, "Artillery") || strstr(n, "Cannon"))) {
        ++crown_land;
      }
    }
  }
  const int force_end = ctx->col1->head.game_options.independence_force != 0;
  const int giveup_bar = ctx->col1->head.game_options.ref_unit_threshold ? 8 : 1;
  const uint16_t* ef = ctx->col1->head.expeditionary_force;
  const int pool_score = (int)ef[0] + (ef[1] != 0 ? 1 : 0) + (ef[3] != 0 ? 1 : 0);
  if ((crown_colonies <= 0 || force_end) &&
      (crown_land < giveup_bar || force_end) && (pool_score < 4 || force_end)) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_WON);
    ai_king_latch_set(ctx->col1, AI_KING_REF_PRESENT_BYTE, 0);
    ctx->col1->head.game_options.ref_present = 0;
    /* DOS win sequence: 0x5382|=8 (war concluded), reveal map, scoring
     * latch — mirrors turn.c C1's LAB_0b4a effects so the win is complete
     * whichever check fires first. */
    ctx->col1->head.game_options.independence_chrome = 1;
    ctx->col1->head.show_entire_map = 1;
    const char* country =
      (pl->country_name[0] != '\0') ? pl->country_name : "the colonies";
    char body[AI_POPUP_BODY_LEN];
    char fallback[AI_POPUP_BODY_LEN];
    /* DOS 3844_0442 win order: victory tune pool (FUN_129f_0318(3)), the
     * @WINNING announcement, THEN the @KINGLOSE throne audience (bugs.md 264
     * — the first pass had the two inverted). */
    if (ai_king_human_popups(ctx)) {
      sound_set_bgm(3);
    }
    /* 1st: @WINNING — STRING0 leader, STRING1 country. Payload 1 = win; the
     * retire score waits for the throne audience behind it. */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = leader;
    tok.string1 = country;
    snprintf(
      fallback,
      sizeof(fallback),
      "Royal Expeditionary Force annihilated! General %s accepts surrender of "
      "all Tory forces. Parliament accepts independence of %s. Continental "
      "Congress proclaims %s the first President of the new republic!",
      leader,
      country,
      leader
    );
    popup_msg_fill(ctx->messages, "WINNING", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_WAR_END, human, crown, 1, NULL, body
      );
    }
    /* 2nd: @KINGLOSE — the King's parting word as the full-screen audience
     * (DOS 291f_0aba(1,2,0xf20)); dismissal opens the retire score chain. */
    ai_king_enqueue_throne_audience(ctx, human, crown, 1);
    return;
  }
  /*
   * The war did not end this turn — show the ONE warn the selector picked
   * (DOS raw 58538-58551, reached only when neither the win nor the lose
   * dialog jumped out of the block). %STRING0 is the human's new-world
   * country name (`0x5398 * 0x34 + 0x5426`, raw 58553) and all three number
   * slots are filled for every warn body (raw 58554-58556).
   */
  if (warn_sel > 0) {
    const int warn_byte = (warn_sel == 2)   ? AI_KING_WARN2_BYTE
                          : (warn_sel == 3) ? AI_KING_WARN3_BYTE
                                            : AI_KING_WARN1_BYTE;
    if (ai_king_latch_get(ctx->col1, warn_byte) == 0) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      ai_king_warn_numbers(&tok, ports, colonies, pop_pct);
      tok.string0 = country;
      char fallback[AI_POPUP_BODY_LEN];
      char tag[16];
      snprintf(tag, sizeof(tag), "WARN%d", warn_sel);
      if (warn_sel == 2) {
        snprintf(
          fallback,
          sizeof(fallback),
          "Your Excellency, the King's forces control all but %d of our colonies!  "
          "We need to protect our remaining colonies, or we will lose the war!",
          colonies
        );
      } else if (warn_sel == 3) {
        snprintf(
          fallback,
          sizeof(fallback),
          "Your Excellency, the King's forces control %d%% of the %s population.  "
          "If he ever controls 90%%, the Continental Congress will be unable to "
          "continue the war and we will have to surrender!",
          pop_pct,
          country
        );
      } else {
        snprintf(
          fallback,
          sizeof(fallback),
          "Your Excellency, the King's forces control all but %d of the ports in %s!  "
          "If we don't retain control of at least one port our commerce will be "
          "choked and we will have to surrender!",
          ports,
          country
        );
      }
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, tag, &tok, fallback, body, sizeof(body));
      /*
       * Do not clobber same-turn wave/war_act status (1528 @INVASION, 2244
       * merc). The warn still enqueues its INFO OK; status when buffer empty.
       */
      if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
        snprintf(ctx->status, ctx->status_size, "%s", body);
      }
      if (ai_king_human_popups(ctx)) {
        (void)ai_popup_enqueue_ok_ctx(
          ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, warn_sel, NULL, body
        );
      }
      ai_king_latch_set(ctx->col1, warn_byte, 1);
    }
  }
  /*
   * Wartime calendar end. DOS raw 58630:
   *   if (((year == 0x708) && !woi) || (year == 0x73a)) { ...retire... }
   * — under a declared WoI (this whole function's precondition) only the
   * `year == 1850` arm can fire, with NO crown-units term: the crown holding
   * colonies but zero live units used to run past 1850 forever here. Kept as
   * `>=` because DOS's own 1850 arm is unconditional (the year word 0x538a
   * can never step past it before the score chain runs), so `>=` differs from
   * `==` only for a port state that already overshot.
   * GAME.TXT @RETIRING2.
   */
  if (year >= AI_KING_YEAR_CAP) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    ai_king_latch_set(ctx->col1, AI_KING_REF_PRESENT_BYTE, 0);
    ctx->col1->head.game_options.ref_present = 0;
    const char* estate = ai_king_richest_colony_name(ctx, human);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    /* raw 58635 `FUN_281f_0438(0, *(0x53a6 * 2 - 0x7c6c))`: %STRING0 is the
     * DIFFICULTY title, not a fixed "Viceroy" (same table 38fd_5930 uses at
     * raw 68388). */
    static const char* const k_rank[5] = {
      "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
    };
    const int diff = (int)ctx->col1->head.difficulty;
    tok.string0 = k_rank[(diff >= 0 && diff < 5) ? diff : 4];
    tok.string1 = leader;
    tok.string2 = estate;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "War-weary Continental Congress sues for peace!  King accepts surrender "
      "from %s %s, who retires to country estate near %s.",
      tok.string0,
      leader,
      estate
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "RETIRING2", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups,
        AI_POPUP_TAG_KING_WAR_END,
        human,
        crown,
        2,
        NULL,
        body
      );
    }
  }
}

void ai_king_nation_turn(ColonizeTurnContext* ctx) {
  if (!ctx) {
    return;
  }
  /*
   * FUN_43f7_2424-shaped:
   *   SoL → peacetime (1d42 tax, SoL chrome, 2564/1a26 declare) | wartime (2022 wave+act)
   */
  /*
   * The lose/@WARN group used to be armed here by a port-invented
   * "WoI + REF-present at turn entry" precondition; DOS gates it on
   * `0x5382 & 1 && !(0x5382 & 8)` alone (raw 58505), which
   * ai_king_check_revolution_end tests for itself.
   */
  /* External boycott clear (Fugger/diplo) → drop refuse even mid-war / off-tax years. */
  if (ctx->col1_ok && ctx->col1) {
    ai_king_sync_boycott_refuse(ctx->col1, ctx->human_nation);
  }
  const int sol = ai_king_sol_percent(ctx, ctx->human_nation);

  if (!ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    /* DOS 1b3a SoL cache tail: `0x31 < SoL && *0x53d2 < 0` → the War of the
     * Spanish Succession vacates the King's future slot ahead of time. */
    if (sol > 49) {
      ai_king_succession(ctx);
    }
    /* FUN_43f7_2424: 1d42 runs FIRST on the peacetime path — the royal
     * purse stipend + threshold @KINGBUY pool buy (real 1d42; the tax
     * audience below is the separate 38fd_5be8 machinery). */
    ai_king_1d42_royal_purse(ctx);
    const int popups_before = ctx->ai_popups ? ctx->ai_popups->queue_count : 0;
    ai_king_tax_event(ctx);
    /* 38fd Europe-EOT king slot: @KINGNEWWAR only when the tax event stayed quiet. */
    if (!ctx->ai_popups || ctx->ai_popups->queue_count == popups_before) {
      (void)ai_king_new_war_event(ctx);
    }
    /* FUN_3844_00f2 tail: @KINGFRIGATE every 8th peacetime turn. */
    ai_king_frigate_offer(ctx, ctx->human_nation);
    /*
     * Peacetime Spring 1790 anniversary (year_end_chrome 0x6fe): @SOONRETIRING0
     * once before the 1800 @SCORED latch. Cite: turn/year_end_chrome.md.
     */
    if (ctx->col1_ok && ctx->col1 &&
        ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_NONE &&
        ai_king_latch_get(ctx->col1, AI_KING_SOONRETIRE0_BYTE) == 0 &&
        (int)ctx->col1->head.year == AI_KING_SOONRETIRE0_YEAR &&
        !(ctx->game_autumn && *ctx->game_autumn != 0)) {
      const int human = ctx->human_nation;
      const char* leader =
        (human >= 0 && human < 4 && ctx->col1->player[human].name[0] != '\0')
          ? ctx->col1->player[human].name
          : "Your Excellency";
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      /* raw 58622 `FUN_281f_0438(0, *(0x53a6 * 2 - 0x7c6c))`: %STRING0 is the
       * DIFFICULTY title, not a fixed "Viceroy" — the same splice @RETIRING2
       * makes at raw 58643 (0x53a6 = the difficulty byte). */
      static const char* const k_rank[5] = {
        "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
      };
      const int diff = (int)ctx->col1->head.difficulty;
      tok.string0 = k_rank[(diff >= 0 && diff < 5) ? diff : 4];
      tok.string1 = leader;
      char fallback[AI_POPUP_BODY_LEN];
      snprintf(
        fallback,
        sizeof(fallback),
        "%s %s plans to retire in 1800!  A rumor circulates that he would "
        "postpone his retirement were a War of Independence to begin.",
        tok.string0,
        leader
      );
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "SOONRETIRING0", &tok, fallback, body, sizeof(body));
      if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
        snprintf(ctx->status, ctx->status_size, "%s", body);
      }
      if (ai_king_human_popups(ctx)) {
        (void)ai_popup_enqueue_ok_ctx(
          ctx->ai_popups,
          AI_POPUP_TAG_INFO,
          human,
          ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
          AI_KING_SOONRETIRE0_YEAR,
          NULL,
          body
        );
      }
      ai_king_latch_set(ctx->col1, AI_KING_SOONRETIRE0_BYTE, 1);
    }
    /*
     * Peacetime calendar end (manual pp.10–12 / 1800–1850): without WoI,
     * year≥1800 latches once. Cite: docs/manual_gap.md Auto-end.
     */
    if (ctx->col1_ok && ctx->col1 &&
        ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_NONE &&
        (int)ctx->col1->head.year >= AI_KING_PEACE_YEAR_CAP) {
      ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_PEACE_1800);
      /* GAME.TXT @SCORED — peacetime calendar end (invent Colonial Era Ends demoted). */
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        ctx->messages,
        "SCORED",
        &tok,
        "Scoring for this game is now complete.",
        body,
        sizeof(body)
      );
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size, "%s", body);
      }
      if (ai_king_human_popups(ctx)) {
        char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
        const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "SCORED");
        int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
        const char* labels[2];
        const int ids[] = {AI_KING_CHOICE_THATS_ALL, AI_KING_CHOICE_KEEP_PLAYING};
        if (nch >= 2) {
          labels[0] = choice_buf[0];
          labels[1] = choice_buf[1];
        } else {
          labels[0] = "That's all.";
          labels[1] = "Keep playing anyway.";
        }
        (void)ai_popup_enqueue_choice_ctx(
          ctx->ai_popups,
          AI_POPUP_TAG_KING_SCORED,
          ctx->human_nation,
          ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation),
          AI_KING_PEACE_YEAR_CAP,
          NULL,
          body,
          labels,
          ids,
          2
        );
      }
    }
    /*
     * FUN_43f7_2424 tail: decile SoL notify (DS:0x53d8 dedup), full port
     * 2026-09-06 (was status-only with an invented founding_father_count
     * gate). DOS (viceroy_unpacked.c:75182-75211):
     *   gate: census_pop_proxy[human] > 3 (DS:nation-0x6bf0 = 0x9410 — the
     *     population proxy, NOT an SoL cache as older notes claimed);
     *   rising: 0x53d8 < report/10 -> @REBELUP (0x1362, SoL<50) or
     *     @REBELUP50 (0x1358, SoL>=50), %NUMBER0 = SoL, %STRING0 = the
     *     human's parent country (FUN_291f_0ac8 form-0 name);
     *   falling: 0x53d8 > (report+4)/10 -> @REBELDOWN (0x136a) — note the
     *     +4 hysteresis (a one-decile dip does not re-notify);
     *   both paths then write 0x53d8 = report/10.
     */
    if (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4 &&
        (int)ctx->col1->stuff.census_pop_proxy[ctx->human_nation] > 3) {
      const int last = (int)ctx->col1->head.sol_pct_last_notified;
      const int rising = last < sol / 10;
      const int falling = !rising && last > (sol + 4) / 10;
      if (rising || falling) {
        static const char* k_country[4] = {"England", "France", "Spain", "Netherlands"};
        const char* country = k_country[ctx->human_nation & 3];
        if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
          snprintf(
            ctx->status,
            ctx->status_size,
            rising ? "Congress notes rising Sons of Liberty (%d%%)."
                   : "Congress notes falling Sons of Liberty (%d%%).",
            sol
          );
        }
        if (ai_king_human_popups(ctx)) {
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.has_number0 = true;
          tok.number0 = sol;
          tok.string0 = country;
          const char* section = rising ? (sol >= 50 ? "REBELUP50" : "REBELUP") : "REBELDOWN";
          char body[AI_POPUP_BODY_LEN];
          char fallback[AI_POPUP_BODY_LEN];
          snprintf(
            fallback,
            sizeof(fallback),
            rising
              ? "Rebel sentiment is rising in the colonies! %d%% of the population "
                "supports the idea of independence from %s."
              : "Tory sentiment is once again on the rise. Only %d%% of the population "
                "now supports the notion of independence from %s.",
            sol,
            country
          );
          popup_msg_fill(ctx->messages, section, &tok, fallback, body, sizeof(body));
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_INFO, ctx->human_nation,
            ai_king_crown_nation_col1(ctx->col1, ctx->human_nation), sol, NULL, body
          );
        }
        ctx->col1->head.sol_pct_last_notified = (int16_t)(sol / 10);
      }
    }
    /*
     * Thin pre-declare SoL chrome:
     * SoL AI_KING_RESTLESS_SOL_MIN..(DECLARE_MIN-1) → restless status line
     * before the auto-declare gate. unknown46 consistency: do not set WoI[0] /
     * congress[5] here (declare only). Optional tax mention when tax_rate
     * already in the refuse band (≥20) — reads existing tax_rate; no invented
     * tax formula. Do not clobber thin 38fd_5be8 tax audience / hike status
     * from 1d42 (ai_popup CHOICE when queue attached). (2564 in try_declare.)
     */
    if (sol >= AI_KING_RESTLESS_SOL_MIN && sol < AI_KING_DECLARE_SOL_MIN && ctx->status &&
        ctx->status_size) {
      const int keep_tax_audience =
          strstr(ctx->status, "refuse") || strstr(ctx->status, "Audience") ||
          strstr(ctx->status, "raises taxes") || strstr(ctx->status, "Tax stays") ||
          strstr(ctx->status, "Congress notes"); /* decile SoL notify above; don't clobber */
      if (!keep_tax_audience) {
        const uint8_t tax =
            (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4)
                ? ctx->col1->nation[ctx->human_nation].tax_rate
                : 0;
        if (tax >= AI_KING_BOYCOTT_TAX_MIN) {
          snprintf(ctx->status, ctx->status_size,
                   "Sons of Liberty grow restless (%d%%). Tax is at %u%%.", sol, tax);
        } else {
          snprintf(ctx->status, ctx->status_size, "Sons of Liberty grow restless (%d%%).", sol);
        }
        /* Restless: status only (no invented wood OK). */
      }
    }
    ai_king_try_declare(ctx);
  }

  if (ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    /*
     * Wartime 1840 anniversary (year_end_chrome 0x730): @SOONRETIRING1 once.
     * Any season while WoI; does not latch endgame.
     */
    if (ctx->col1_ok && ctx->col1 &&
        ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_NONE &&
        ai_king_latch_get(ctx->col1, AI_KING_SOONRETIRE1_BYTE) == 0 &&
        (int)ctx->col1->head.year == AI_KING_SOONRETIRE1_YEAR) {
      const int human = ctx->human_nation;
      const char* leader =
        (human >= 0 && human < 4 && ctx->col1->player[human].name[0] != '\0')
          ? ctx->col1->player[human].name
          : "Your Excellency";
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      /* Same emitter as @SOONRETIRING0 (raw 58618-58628): %STRING0 is the
       * difficulty title, %STRING1 the leader. The 1840 body reads only
       * %STRING1, but DOS fills both slots. */
      {
        static const char* const k_rank[5] = {
          "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
        };
        const int diff = (int)ctx->col1->head.difficulty;
        tok.string0 = k_rank[(diff >= 0 && diff < 5) ? diff : 4];
      }
      tok.string1 = leader;
      char fallback[AI_POPUP_BODY_LEN];
      snprintf(
        fallback,
        sizeof(fallback),
        "\"General %s, the people are weary of this long war.  If we cannot "
        "force a conclusion by 1850, the Continental Congress will sue for "
        "peace and seek to swear renewed allegiance to the King.\"",
        leader
      );
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "SOONRETIRING1", &tok, fallback, body, sizeof(body));
      if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
        snprintf(ctx->status, ctx->status_size, "%s", body);
      }
      if (ai_king_human_popups(ctx)) {
        (void)ai_popup_enqueue_ok_ctx(
          ctx->ai_popups,
          AI_POPUP_TAG_INFO,
          human,
          ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
          AI_KING_SOONRETIRE1_YEAR,
          NULL,
          body
        );
      }
      ai_king_latch_set(ctx->col1, AI_KING_SOONRETIRE1_BYTE, 1);
    }
    /*
     * D1 (2026-09-07g): the wave + war bookkeeping moved to
     * ai_king_ref_pre_euro_beat, run from the crown slot's own EURO step
     * BEFORE ai_euro_nation_turn — DOS's per-slot order (00f2→2424 king
     * beat, then 6d8e; raw 6394/6407). This king slice keeps only the
     * chrome above and the end check, which DOS also evaluates after the
     * movement it observes.
     */
    ai_king_check_revolution_end(ctx);
  }

  if (ctx->active_turn_nation) {
    *ctx->active_turn_nation = ctx->human_nation;
  }
  if (ctx->col1_ok && ctx->col1) {
    /*
     * FUN_43f7_2424 (43f7:2478..2492): every nation caches its own SoL into
     * `nation + 0x19` — the byte the Foreign Affairs report multiplies the
     * census population by to split Rebels from Tories (3f41:2902 reads
     * `[nation*0x13c + 0x8821]`, IMULs it by `census_pop_proxy`, IDIVs by
     * 100). Only the human's copy in DS:0x53d0 was being written here, so
     * `rebel_sentiment` stayed 0 for the whole campaign and the report showed
     * 0 Rebels / all Tories even at 100% SoL (bugs.md). DOS writes the byte
     * for whichever nation it is ticking, human or AI, before the DS:0x53d0
     * human-only half.
     */
    for (int n = 0; n < 4; ++n) {
      int sol = ai_king_sol_percent(ctx, n);
      if (sol < 0) {
        sol = 0;
      }
      if (sol > 100) {
        sol = 100;
      }
      ctx->col1->nation[n].rebel_sentiment = (uint8_t)sol;
    }
    /* FUN_43f7_2424 tail: cache nation SoL for next turn's tax-audience score. */
    ctx->col1->head.rebel_sentiment_report =
      (uint8_t)ai_king_sol_percent(ctx, ctx->human_nation);
  }
}

void ai_king_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup) {
  if (!ctx || !popup || !popup->has_result || popup->result_cancelled) {
    return;
  }
  const int human = (popup->result_nation_a >= 0 && popup->result_nation_a < 4)
                      ? popup->result_nation_a
                      : ctx->human_nation;
  switch (popup->result_tag) {
    case AI_POPUP_TAG_KING_AUDIENCE:
      /*
       * FUN_38fd_3dc8 village-goods choice. The hike is NOT applied yet
       * (bugs.md: the popup proposes it, the answer settles it). Accept
       * ("kiss the ring") → commit the delta packed in the payload. Refuse
       * ("tea party") → leave the rate alone and boycott the picked cargo.
       */
      {
        int applied = 0;
        int cargo = -1;
        ai_king_teaparty_payload_parts(popup->result_payload, &applied, &cargo);
        if (popup->result_choice_id == AI_KING_CHOICE_ACCEPT) {
          ai_king_tax_commit(ctx, human, applied);
          if (ctx->status && ctx->status_size && ctx->col1_ok && ctx->col1 &&
              human >= 0 && human < 4) {
            snprintf(ctx->status, ctx->status_size,
                     "Audience: taxes raised to %u%%.",
                     ctx->col1->nation[human].tax_rate);
          }
        } else if (popup->result_choice_id == AI_KING_CHOICE_REFUSE) {
          ai_king_tax_teaparty(ctx, human, cargo);
        }
      }
      break;
    case AI_POPUP_TAG_KING_DUMP_GOODS:
      /* Dump-goods modal: choice_id is cargo index to OR into boycott_bitmap. */
      ai_king_apply_dump_goods_choice(ctx, human, popup->result_choice_id);
      break;
    case AI_POPUP_TAG_KING_MERC:
      /* FUN_43f7_2022 rebel branch: Hire → spend the rolled price, spawn at
       * the offer-time landing pick (payload); Decline → status only, no
       * gate — DOS has no once-per-war flag, next turn may roll again. */
      if (popup->result_choice_id == AI_KING_CHOICE_HIRE) {
        int hx = 0;
        int hy = 0;
        int qty_a = 0;
        int extra_flag = 0;
        int price = 0;
        ai_king_merc_payload_parts(popup->result_payload, &hx, &hy, &qty_a, &extra_flag, &price);
        if (!ai_king_do_merc_hire_at(ctx, human, hx, hy, qty_a, extra_flag, price) &&
            ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Cannot afford mercenaries.");
        }
      } else if (popup->result_choice_id == AI_KING_CHOICE_DECLINE) {
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Mercenaries declined.");
        }
      }
      break;
    case AI_POPUP_TAG_KING_FRIGATE:
      /* FUN_3844_00f2 tail: Yes → Frigate sails from Europe + 3dc8(KINGTAX, 10). */
      if (popup->result_choice_id == AI_KING_CHOICE_ACCEPT) {
        ai_king_frigate_accept(ctx, human);
      } else if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size, "The Crown's frigate is declined.");
      }
      break;
    case AI_POPUP_TAG_KING_CONGRESS:
      /* FUN_43f7_2564 / 1a26: Confirm → declare; Not yet → leave peacetime. */
      if (popup->result_choice_id == AI_KING_CHOICE_CONFIRM) {
        ai_king_do_declare(ctx, human);
        /*
         * Same-turn REF wave + 1eca Continental muster (FUN_43f7_0982 / 1eca).
         * Auto-declare gets these from the crown slot's pre-euro beat; popup
         * Confirm applies outside that turn slice.
         */
        ai_king_ref_pre_euro_beat(ctx);
      }
      break;
    case AI_POPUP_TAG_KING_SCORED:
      /* Peacetime @SCORED: That's all → @RETIRING then score UI; Keep playing → continue. */
      if (popup->result_choice_id == AI_KING_CHOICE_THATS_ALL) {
        const char* leader =
          (ctx->col1_ok && ctx->col1 && human >= 0 && human < 4 &&
           ctx->col1->player[human].name[0] != '\0')
            ? ctx->col1->player[human].name
            : "Your Excellency";
        const char* estate = ai_king_richest_colony_name(ctx, human);
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = "Viceroy";
        tok.string1 = leader;
        tok.string2 = estate;
        char fallback[AI_POPUP_BODY_LEN];
        snprintf(
          fallback,
          sizeof(fallback),
          "Viceroy %s steps down after over 300 years of loyal service to the "
          "Crown.  King knights aging Viceroy, who retires to country estate "
          "near %s.",
          leader,
          estate
        );
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(ctx->messages, "RETIRING", &tok, fallback, body, sizeof(body));
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "%s", body);
        }
        if (ai_king_human_popups(ctx)) {
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups,
            AI_POPUP_TAG_INFO,
            human,
            ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
            0,
            NULL,
            body
          );
        }
      } else if (popup->result_choice_id == AI_KING_CHOICE_KEEP_PLAYING) {
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Continuing the campaign.");
        }
      }
      break;
    default:
      break;
  }
}
