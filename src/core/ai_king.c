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
 * 160a rename: player[human].country_name → "United Colonies"
 *   (thin rename + KING_LETTER Done; the 160a signing cinematic itself is
 *   core/declaration.c, armed from game_loop on the KING_LETTER popup).
 *   unknown46[4] endgame latch: 0 none / 1 won / 2 lost.
 *   On declare + ai_popups: thin rename OK + GAME.TXT @HOWTOWIN INFO
 *   (invent "War of Independence begins!" demoted).
 * Congress confirm: head.unknown46[5] + thin 2564 (ai_popup CHOICE from
 *   GAME.TXT @DECLARE Never/Yes when ctx->ai_popups; auto-declare when NULL;
 *   same-turn 1528 may overwrite status).
 * Mid-war @WARN1 (one coastal port left): head.unknown46[6] episode latch;
 *   clear when ports>1 so reclaim→lose-to-one can warn again.
 * Mid-war @WARN2 (one colony left): head.unknown46[7] episode latch;
 *   clear when colonies>1.
 * Mid-war @WARN3 (crown pop share 50–89%): head.unknown46[10] episode latch;
 *   clear when share <50%. @LOSING3 when share ≥90%.
 * Calendar @SOONRETIRING0 (1790 spring peacetime): head.unknown46[8] once.
 * Calendar @SOONRETIRING1 (1840 WoI): head.unknown46[9] once.
 * Revolution end: lose if 0 colonies (@LOSING2) or 0 coastal ports (@LOSING1);
 *   win if year≥1850 + no crown units; @RETIRING2 if year≥1850 + crown remains.
 * SoL restless chrome (40..49): status only (no invented wood OK).
 * backup_force: DOS 0x53e2… foreign pools — 10f0 stand-in (seeded on declare).
 * Crown nation_id: non-human Euro slot (1 if human==0 else 0).
 */

#define AI_KING_INDEP_COUNTRY "United Colonies"
#define AI_KING_YEAR_CAP 1850
#define AI_KING_PEACE_YEAR_CAP 1800
#define AI_KING_SOONRETIRE0_YEAR 1790
#define AI_KING_SOONRETIRE1_YEAR 1840
#define AI_KING_WARN3_PCT_MIN 50
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

  char title[AI_POPUP_BODY_LEN];
  snprintf(title, sizeof(title), "%s Party", party);

  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "TEAPARTY", &tok, fallback, body, sizeof(body));
  sound_play(0x56); /* FUN_38fd_3dc8 tea party (COLDIG 9 cheering) */
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups,
    AI_POPUP_TAG_KING_TAX,
    human,
    ai_king_crown_nation(human),
    ctx->col1 && ctx->col1_ok ? (int)ctx->col1->nation[human].tax_rate : 0,
    title,
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
  const int crown = ai_king_crown_nation(human_nation);
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
  const int crown = ai_king_crown_nation(human_nation);
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

/* FUN_43f7_1a26: cache first two non-human/non-crown Euro slots (DOS 0x53d4/0x53d6). */
static void ai_king_write_rival_nation_slots(ColonizeCol1Save* col1, int human) {
  if (!col1 || human < 0 || human >= 4) {
    return;
  }
  const int crown = ai_king_crown_nation(human);
  col1->head.rival_nation_slot_1 = -1;
  col1->head.rival_nation_slot_2 = -1;
  int w = 0;
  for (int n = 0; n < 4 && w < 2; ++n) {
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
 * 0x53e2+i*2), confirmed byte-for-byte against viceroy_unpacked.c:74765-74795
 * (formula-to-address) and :74424 (`thunk_FUN_2a1f_0070` type lookup,
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
  const ColonizeCol1Nation* anat = &ctx->col1->nation[ally];
  const int local_4 = ctx->colonies ? ai_king_colony_count(ctx->colonies, ally) : 0;
  const int n6bf0 = (int)(anat->founding_father_count & 0xffu);
  const int n6bd4 = (int)anat->rebel_sentiment;
  const int n6be4 = (int)anat->liberty_bells_total;

  int pool0 = (n6bf0 / 10) - diff + 8;
  const int iVar7 = (4 - diff) / 2;
  int pool1 = ((n6bd4 + 1) >> 4) + iVar7 + 1;
  int pool3 = iVar7 + ((n6be4 + 1) >> 5) + 3;
  int pool2 = iVar7 + local_4 + 3;

  pool0 = (pool0 + 9) / 2;
  pool1 = (pool1 + 2) / 2;
  pool3 = (pool3 + 4) / 2;
  pool2 = (pool2 + 4) / 2;

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

/* True if unit type/display name is Artillery (or Cannon fallback). */
static int ai_king_is_artillery(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Artillery")) || (dname && strstr(dname, "Artillery"))) {
    return 1;
  }
  if ((tname && strstr(tname, "Cannon")) || (dname && strstr(dname, "Cannon"))) {
    return 1;
  }
  return 0;
}

/* Artillery type in pool, or -1 (thin siege bias gates on this). */
static int ai_king_artillery_type(const ColonizeUnitPool* units) {
  if (!units) {
    return -1;
  }
  int ty = units_find_type(units, "Artillery");
  if (ty < 0) {
    ty = units_find_type(units, "Cannon");
  }
  return ty;
}

/*
 * Continental Army / Cont. Army / Cont. Cav after 1eca — include in hunter
 * name check so Cont. types hunt like Regular/Dragoon (fandom Independence:
 * Veteran → Continental Army/Cavalry).
 */
static int ai_king_is_continental(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Continental")) || (dname && strstr(dname, "Continental"))) {
    return 1;
  }
  if ((tname && strstr(tname, "Cont. Army")) || (dname && strstr(dname, "Cont. Army"))) {
    return 1;
  }
  if ((tname && strstr(tname, "Cont. Cav")) || (dname && strstr(dname, "Cont. Cav"))) {
    return 1;
  }
  return 0;
}

/*
 * REF land hunters: Regular / Dragoon (fandom REF AI land arm).
 * Thin Artillery siege: when Artillery type exists in pool, Artillery also hunts
 * (prefer fortified colony — see ai_king_ref_hunt_target). Cont. Army / Cont. Cav
 * included in name check (1eca promote; human Cont. hunt → capital/colony below).
 * Deep multi-step siege scoring remains PARKED.
 */
static int ai_king_is_ref_land_hunter(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u || !u->active || units_is_sea(units, u->id)) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Regular")) || (dname && strstr(dname, "Regular"))) {
    return 1;
  }
  if ((tname && strstr(tname, "Dragoon")) || (dname && strstr(dname, "Dragoon"))) {
    return 1;
  }
  if (ai_king_is_continental(units, u)) {
    return 1;
  }
  if (ai_king_artillery_type(units) >= 0 && ai_king_is_artillery(units, u)) {
    return 1;
  }
  return 0;
}

/* True if unit type/display is Regular (for post-capture fortify). */
static int ai_king_is_regular(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Regular")) || (dname && strstr(dname, "Regular"))) {
    return 1;
  }
  return 0;
}

/* True if unit type/display is Dragoon (open-land hunt bias when Artillery exists). */
static int ai_king_is_dragoon(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Dragoon")) || (dname && strstr(dname, "Dragoon"))) {
    return 1;
  }
  return 0;
}

/*
 * Open-land hunt role (Dragoon thin bias + Cont. Cav as cavalry promote).
 * Source: fandom REF AI land arm / Independence Cont. Cavalry; when Artillery
 * type exists, leave fortified ports to Artillery. Cont. Army stays nearest.
 */
static int ai_king_prefers_open_land(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (ai_king_is_dragoon(units, u)) {
    return 1;
  }
  if (!units || !u || !ai_king_is_continental(units, u)) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  /* Cont. Cav / Continental Cavalry only — not Cont. Army. */
  if ((tname && (strstr(tname, "Cav") || strstr(tname, "Cavalry"))) ||
      (dname && (strstr(dname, "Cav") || strstr(dname, "Cavalry")))) {
    return 1;
  }
  return 0;
}

/*
 * Cavalry garrison fallback when no Regular: Dragoon or Cont. Cav (not Cont. Army).
 * Cite: Colonization.pdf Defending a Colony ("fortify soldiers, dragoons…");
 * king_ref one-garrison. Reuses open-land cavalry name check.
 */
static int ai_king_is_garrison_cavalry(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  return ai_king_prefers_open_land(units, u);
}

/*
 * Count crown garrison units (Regular or Dragoon/Cont. Cav) on (x,y) already
 * FORTIFY/FORTIFIED. Cap-2 structural multi-garrison (Defending a Colony);
 * Artillery holds separately (not this stack).
 * Cite: Colonization.pdf Defending a Colony; king_ref thin multi-garrison.
 */
static int ai_king_tile_fortified_garrison_count(const ColonizeTurnContext* ctx, int crown,
                                                 int x, int y, int except_id) {
  if (!ctx || !ctx->units || crown < 0) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown || u->id == except_id) {
      continue;
    }
    if (u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_regular(ctx->units, u) && !ai_king_is_garrison_cavalry(ctx->units, u)) {
      continue;
    }
    if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
      count++;
    }
  }
  return count;
}

/* Cont. Army (not Cav) — human founding-capital garrison pool. */
static int ai_king_is_cont_army(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  return ai_king_is_continental(units, u) && !ai_king_prefers_open_land(units, u);
}

/* Cont. Cav — human founding-capital garrison pool (crown cap-2 uses Cont.Cav only). */
static int ai_king_is_cont_cav_garrison(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  return ai_king_is_continental(units, u) && ai_king_prefers_open_land(units, u);
}

static int ai_king_is_cont_garrison(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  return ai_king_is_cont_army(units, u) || ai_king_is_cont_cav_garrison(units, u);
}

/*
 * Count human Cont. Army / Cont. Cav on (x,y) already FORTIFY/FORTIFIED.
 * Cite: Colonization.pdf Defending a Colony; king_ref Cont. capital-rally fortify.
 */
static int ai_king_tile_human_cont_fortified_count(const ColonizeTurnContext* ctx, int human,
                                                   int x, int y, int except_id) {
  if (!ctx || !ctx->units || human < 0) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != human || u->id == except_id) {
      continue;
    }
    if (u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_cont_garrison(ctx->units, u)) {
      continue;
    }
    if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
      count++;
    }
  }
  return count;
}

/*
 * Fortify one idle human Cont. garrison on (x,y). Prefer Cont. Army over
 * Cont. Cav; prefer prefer_id when eligible. Returns unit id fortified, or -1.
 */
static int ai_king_fortify_one_cont_on_tile(ColonizeTurnContext* ctx, int human, int x, int y,
                                           int require_moves, int prefer_id) {
  if (!ctx || !ctx->units || human < 0) {
    return -1;
  }
  ColonizeUnitPool* pool = ctx->units;

  if (prefer_id >= 0) {
    ColonizeUnit* p = units_get(pool, prefer_id);
    if (p && p->active && p->nation_id == human && p->x == x && p->y == y &&
        ai_king_is_cont_army(pool, p) && p->orders != UNITS_ORDER_FORTIFY &&
        p->orders != UNITS_ORDER_FORTIFIED && (!require_moves || p->moves_left > 0)) {
      (void)units_order_fortify(pool, p->id);
      return p->id;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != human || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_cont_army(pool, u) || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (require_moves && u->moves_left <= 0) {
      continue;
    }
    (void)units_order_fortify(pool, u->id);
    return u->id;
  }
  if (prefer_id >= 0) {
    ColonizeUnit* p = units_get(pool, prefer_id);
    if (p && p->active && p->nation_id == human && p->x == x && p->y == y &&
        ai_king_is_cont_cav_garrison(pool, p) && p->orders != UNITS_ORDER_FORTIFY &&
        p->orders != UNITS_ORDER_FORTIFIED && (!require_moves || p->moves_left > 0)) {
      (void)units_order_fortify(pool, p->id);
      return p->id;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != human || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_cont_cav_garrison(pool, u) || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (require_moves && u->moves_left <= 0) {
      continue;
    }
    (void)units_order_fortify(pool, u->id);
    return u->id;
  }
  return -1;
}

/*
 * Idle human Cont. on founding capital: fortify up to two (Army prefer, then
 * Cav). Second slot needs moves_left > 0. Cite: Defending a Colony cap 2;
 * king_ref Cont. capital-rally hold + fortify.
 */
static void ai_king_fortify_human_cont_at(ColonizeTurnContext* ctx, ColonizeUnit* u,
                                          int human, int x, int y) {
  if (!ctx || !ctx->units || human < 0) {
    return;
  }
  if (ai_king_tile_human_cont_fortified_count(ctx, human, x, y, -1) >= 2) {
    return;
  }
  const int prefer = u ? u->id : -1;
  if (ai_king_tile_human_cont_fortified_count(ctx, human, x, y, -1) < 1) {
    (void)ai_king_fortify_one_cont_on_tile(ctx, human, x, y, 0, prefer);
  }
  if (ai_king_tile_human_cont_fortified_count(ctx, human, x, y, -1) < 2) {
    (void)ai_king_fortify_one_cont_on_tile(ctx, human, x, y, 1, -1);
  }
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

/*
 * Human founding-capital stand-in: lowest active colony id for the nation.
 * Euro colonies have no Col1 capital bit (unlike tribes); cite fandom REF
 * pressure on main ports — first-founded colony as capital. Returns 1 if found.
 */
static int ai_king_human_capital(const ColonizeTurnContext* ctx, int human, int* out_x,
                                 int* out_y) {
  if (!ctx || !ctx->colonies || !out_x || !out_y || human < 0) {
    return 0;
  }
  int best_id = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    if (best_id < 0 || c->id < best_id) {
      best_id = c->id;
      bx = c->x;
      by = c->y;
    }
  }
  if (best_id < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Nearest remaining human colony by Manhattan distance (no capital MD slack).
 * Used after capture: idle hunters not on the fortify-stack garrison slot prefer
 * the next nearest uncaptured colony over closer human land units.
 * Source: fandom REF AI — attack uncaptured colonies / weakest ports; capital
 * MD bias is for peacetime-of-war idle hunt, not post-garrison extras.
 * Returns 1 if a human colony exists.
 */
static int ai_king_nearest_human_colony(const ColonizeTurnContext* ctx, int human, int from_x,
                                        int from_y, int* out_x, int* out_y) {
  if (!ctx || !ctx->colonies || !out_x || !out_y || human < 0) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    const int dist = abs(c->x - from_x) + abs(c->y - from_y);
    if (best < 0 || dist < best) {
      best = dist;
      bx = c->x;
      by = c->y;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Among colony candidates already chosen as nearest: if founding capital MD is
 * within AI_KING_CAPITAL_MD_SLACK of that nearest colony MD, retarget to capital.
 * Does not invent a closer capital when MD is far worse. Returns 1 if retargeted.
 * Source: fandom REF AI — main-port pressure; idle hunters vs distant colonies.
 */
static int ai_king_prefer_capital_if_comparable(const ColonizeTurnContext* ctx, int human,
                                                int from_x, int from_y, int* bx, int* by,
                                                int best_colony_md) {
  int cx = 0;
  int cy = 0;
  if (!bx || !by || best_colony_md < 0) {
    return 0;
  }
  if (!ai_king_human_capital(ctx, human, &cx, &cy)) {
    return 0;
  }
  if (*bx == cx && *by == cy) {
    return 0; /* already on capital */
  }
  const int cap_md = abs(cx - from_x) + abs(cy - from_y);
  if (cap_md <= best_colony_md + AI_KING_CAPITAL_MD_SLACK) {
    *bx = cx;
    *by = cy;
    return 1;
  }
  return 0;
}

/*
 * Nearest human colony or human land unit (Manhattan) for REF land hunt.
 * Source: fandom REF AI — hunt ports / land; conceptual reuse of ai_euro land hunt
 * (implemented inside ai_king only). Returns 1 if a target was found.
 * prefer_fortified: Artillery thin siege — when set, prefer a fortified human
 * colony (Stockade/Fort/Fortress) if any exist; else fall back to nearest.
 * prefer_open: Dragoon / Cont. Cav thin bias when Artillery type exists — prefer
 * human land units / unfortified colonies (leave fortified ports to Artillery);
 * else fall back to nearest (including fortified). Deep role-split scoring PARKED.
 * Capital MD bias: among colony picks, prefer founding capital when MD is
 * within AI_KING_CAPITAL_MD_SLACK of the nearest other colony (idle hunters).
 * Artillery siege: same slack when the capital itself is fortified (else keep
 * nearest fortified). Strictly closer human land units still win over capital.
 */
static int ai_king_ref_hunt_target(const ColonizeTurnContext* ctx, int human, int from_x,
                                   int from_y, int* out_x, int* out_y,
                                   int prefer_fortified, int prefer_open) {
  if (!ctx || !ctx->units || !out_x || !out_y || human < 0) {
    return 0;
  }

  if (prefer_fortified && ctx->colonies) {
    int best = -1;
    int bx = 0;
    int by = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (!colonies_has_fortification(ctx->colonies, c)) {
        continue;
      }
      const int dist = abs(c->x - from_x) + abs(c->y - from_y);
      if (best < 0 || dist < best) {
        best = dist;
        bx = c->x;
        by = c->y;
      }
    }
    if (best >= 0) {
      /* Capital bias only if capital itself is fortified (Artillery siege set). */
      int cx = 0;
      int cy = 0;
      if (ai_king_human_capital(ctx, human, &cx, &cy)) {
        const int cid = colonies_id_at(ctx->colonies, cx, cy);
        if (cid >= 0) {
          const ColonizeColony* cap = &ctx->colonies->colonies[cid];
          if (colonies_has_fortification(ctx->colonies, cap)) {
            (void)ai_king_prefer_capital_if_comparable(ctx, human, from_x, from_y, &bx, &by,
                                                       best);
          }
        }
      }
      *out_x = bx;
      *out_y = by;
      return 1;
    }
  }

  /*
   * Dragoon / Cont. Cav open-land pass: human land units + unfortified colonies
   * only. Skip fortified colonies so Artillery (prefer_fortified) owns that job.
   */
  if (prefer_open) {
    int best_unit = -1;
    int ux = 0;
    int uy = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* f = &ctx->units->units[i];
      if (!f->active || f->nation_id != human) {
        continue;
      }
      if (!units_is_on_map(f) || units_is_sea(ctx->units, f->id)) {
        continue;
      }
      const int dist = abs(f->x - from_x) + abs(f->y - from_y);
      if (best_unit < 0 || dist < best_unit) {
        best_unit = dist;
        ux = f->x;
        uy = f->y;
      }
    }
    int best_col = -1;
    int bx = 0;
    int by = 0;
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active || c->nation_id != human) {
          continue;
        }
        if (colonies_has_fortification(ctx->colonies, c)) {
          continue;
        }
        const int dist = abs(c->x - from_x) + abs(c->y - from_y);
        if (best_col < 0 || dist < best_col) {
          best_col = dist;
          bx = c->x;
          by = c->y;
        }
      }
      if (best_col >= 0) {
        int cx = 0;
        int cy = 0;
        if (ai_king_human_capital(ctx, human, &cx, &cy)) {
          const int cid = colonies_id_at(ctx->colonies, cx, cy);
          if (cid >= 0) {
            const ColonizeColony* cap = &ctx->colonies->colonies[cid];
            /* Open-land: capital bias only when capital is unfortified. */
            if (!colonies_has_fortification(ctx->colonies, cap)) {
              (void)ai_king_prefer_capital_if_comparable(ctx, human, from_x, from_y, &bx, &by,
                                                         best_col);
              best_col = abs(bx - from_x) + abs(by - from_y);
            }
          }
        }
      }
    }
    if (best_unit >= 0 || best_col >= 0) {
      /* Unit wins on equal MD (same as prior combined pass); else colony. */
      if (best_unit >= 0 && (best_col < 0 || best_unit <= best_col)) {
        *out_x = ux;
        *out_y = uy;
      } else {
        *out_x = bx;
        *out_y = by;
      }
      return 1;
    }
    /* No open target — fall through to nearest (may be fortified). */
  }

  int best_unit = -1;
  int ux = 0;
  int uy = 0;

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* f = &ctx->units->units[i];
    if (!f->active || f->nation_id != human) {
      continue;
    }
    if (!units_is_on_map(f) || units_is_sea(ctx->units, f->id)) {
      continue;
    }
    const int dist = abs(f->x - from_x) + abs(f->y - from_y);
    if (best_unit < 0 || dist < best_unit) {
      best_unit = dist;
      ux = f->x;
      uy = f->y;
    }
  }

  int best_col = -1;
  int bx = 0;
  int by = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      const int dist = abs(c->x - from_x) + abs(c->y - from_y);
      if (best_col < 0 || dist < best_col) {
        best_col = dist;
        bx = c->x;
        by = c->y;
      }
    }
    if (best_col >= 0) {
      (void)ai_king_prefer_capital_if_comparable(ctx, human, from_x, from_y, &bx, &by,
                                                 best_col);
      best_col = abs(bx - from_x) + abs(by - from_y);
    }
  }

  if (best_unit < 0 && best_col < 0) {
    return 0;
  }
  /* Unit wins on equal MD; capital-biased colony otherwise. */
  if (best_unit >= 0 && (best_col < 0 || best_unit <= best_col)) {
    *out_x = ux;
    *out_y = uy;
  } else {
    *out_x = bx;
    *out_y = by;
  }
  return 1;
}

/* True if any human land unit is adjacent to (x,y). */
/* Chebyshev distance (map "MD" used throughout this file's hunt code). */
static int ai_king_md(int ax, int ay, int bx, int by) {
  const int dx = abs(ax - bx);
  const int dy = abs(ay - by);
  return dx > dy ? dx : dy;
}

/*
 * Human land unit on (x,y) that can still defend a colony: armed / mounted
 * colonist, or a type with an attack value (Artillery, Cont. Army, ...).
 * Unarmed colonists, wagon trains and treasure do not hold a colony — DOS
 * captures once the last soldier loses, the civilians change hands.
 */
static int ai_king_human_defender_at(const ColonizeTurnContext* ctx, int human, int x, int y) {
  if (!ctx || !ctx->units || human < 0) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* f = &ctx->units->units[i];
    if (!f->active || f->nation_id != human || !units_is_on_map(f) || f->x != x || f->y != y) {
      continue;
    }
    if (units_is_sea(ctx->units, f->id)) {
      continue;
    }
    const ColonizeUnitType* ft = units_type(ctx->units, f->type_index);
    if (f->muskets > 0 || f->horses > 0 || (ft && ft->attack > 0)) {
      return f->id;
    }
  }
  return -1;
}

static int ai_king_adjacent_human_unit(const ColonizeTurnContext* ctx, int human, int x,
                                       int y) {
  if (!ctx || !ctx->units || human < 0) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int nx = x + dx[d];
    const int ny = y + dy[d];
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* f = &ctx->units->units[i];
      if (!f->active || f->nation_id != human || !units_is_on_map(f)) {
        continue;
      }
      if (units_is_sea(ctx->units, f->id)) {
        continue;
      }
      if (f->x == nx && f->y == ny) {
        return 1;
      }
    }
  }
  return 0;
}

/* True if any active human colony is adjacent to (x,y). */
static int ai_king_adjacent_human_colony(const ColonizeTurnContext* ctx, int human, int x,
                                         int y) {
  if (!ctx || !ctx->colonies || human < 0) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int cid = colonies_id_at(ctx->colonies, x + dx[d], y + dy[d]);
    if (cid < 0) {
      continue;
    }
    const ColonizeColony* c = &ctx->colonies->colonies[cid];
    if (c->active && c->nation_id == human) {
      return 1;
    }
  }
  return 0;
}

/*
 * Water tile adjacent to a human colony (nearest to from_x/from_y).
 * Source: fandom REF AI man-o-war → ports; used for wartime MoW AI_SAIL.
 * When skip_adjacent_colony != 0, prefer a coast for a human colony the ship is
 * *not* already adjacent to (post-full-unload → next human coast). Falls back to
 * any human coast if no other port has water. Returns 1 if found.
 */
static int ai_king_human_coast_water(const ColonizeTurnContext* ctx, int human, int from_x,
                                     int from_y, int* out_x, int* out_y,
                                     int skip_adjacent_colony) {
  if (!ctx || !ctx->map || !ctx->colonies || !out_x || !out_y || human < 0) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int best = -1;
  int bx = 0;
  int by = 0;
  int best_any = -1;
  int bx_any = 0;
  int by_any = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    /* Adjacent includes 8-neigh + same tile; skip that colony when requested. */
    int adj = 0;
    if (skip_adjacent_colony) {
      if (c->x == from_x && c->y == from_y) {
        adj = 1;
      } else {
        for (int d = 0; d < 8; ++d) {
          if (from_x == c->x + dx[d] && from_y == c->y + dy[d]) {
            adj = 1;
            break;
          }
        }
      }
    }
    for (int d = 0; d < 8; ++d) {
      const int nx = c->x + dx[d];
      const int ny = c->y + dy[d];
      if (!map_tile_is_water(ctx->map, nx, ny)) {
        continue;
      }
      const int dist = abs(nx - from_x) + abs(ny - from_y);
      if (best_any < 0 || dist < best_any) {
        best_any = dist;
        bx_any = nx;
        by_any = ny;
      }
      if (adj) {
        continue; /* just-served port — look for next human coast */
      }
      if (best < 0 || dist < best) {
        best = dist;
        bx = nx;
        by = ny;
      }
    }
  }
  if (best >= 0) {
    *out_x = bx;
    *out_y = by;
    return 1;
  }
  if (best_any < 0) {
    return 0;
  }
  *out_x = bx_any;
  *out_y = by_any;
  return 1;
}

/*
 * Adjacent land for wartime MoW unload near a human colony.
 * Foundable or coastal land adjacent to a human colony is always preferred
 * over the colony tile itself. Source: fandom REF man-o-war → ports / seize
 * landing (this destination-picker is a fandom-shaped reconstruction, not a
 * byte-traced port of FUN_43f7_0982's own target selection — no confirmed
 * DOS ground truth for the exact tile choice).
 * DOS FUN_43f7_0982/2022: disembarking spends the landed unit's moves for
 * the beat, so a fresh REF unit comes ashore *adjacent* and only walks onto
 * (and, if undefended, captures) the colony on a later activation with
 * moves restored — never same-beat. Landing straight onto the colony tile
 * here would let ai_king_mow_post_unload_land's walk-in capture fire the
 * same turn as the wave/declare, skipping that lag entirely (bug report
 * 2026-08-24: "REF appears to land directly on the colony ... not as it
 * happens in DOS").
 * Colony-tile fallback (2026-08-24, same-day follow-up): a colony sited on
 * a single-tile island has *no* adjacent land at all — every one of its
 * neighbors is water, so the loop below never finds a non-colony land
 * candidate. Excluding the colony tile unconditionally would leave such a
 * colony permanently un-landable (REF could never invade it at all, which
 * is a worse divergence than same-beat capture). The colony tile is
 * therefore still recorded as a last-resort fallback, used only when no
 * other adjacent land/coastal tile exists — the normal mainland/peninsula
 * case is unaffected (it always has a real coastal-neighbor candidate that
 * outscores the fallback). Returns 1 if a dest was found.
 */
static int ai_king_mow_unload_land_dest(const ColonizeTurnContext* ctx, int human,
                                        const ColonizeUnit* ship, int* out_x, int* out_y) {
  if (!ctx || !ctx->map || !ctx->units || !ship || !out_x || !out_y || human < 0) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int pax_id = -1;
  int pax_type = -1;
  if (ship->cargo_count > 0) {
    pax_id = ship->cargo_ids[0];
    const ColonizeUnit* pax = units_get_const(ctx->units, pax_id);
    if (pax) {
      pax_type = pax->type_index;
    }
  }
  if (pax_type < 0) {
    return 0;
  }

  int best_score = -1;
  int bx = 0;
  int by = 0;
  int have_colony_fallback = 0;
  int fcx = 0;
  int fcy = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = ship->x + dx[d];
    const int ny = ship->y + dy[d];
    if (!map_tile_is_land(ctx->map, nx, ny)) {
      continue;
    }
    if (!units_can_enter(ctx->units, pax_type, ctx->map, nx, ny, pax_id, ctx->colonies)) {
      continue;
    }
    int score = 0;
    const int cid = ctx->colonies ? colonies_id_at(ctx->colonies, nx, ny) : -1;
    if (cid >= 0) {
      /*
       * Never *prefer* landing straight onto a colony tile (see header) —
       * but remember the human's own colony tile as a last-resort fallback
       * for a single-tile-island colony, where no other adjacent land
       * exists at all and this is the only way REF can ever come ashore.
       */
      const ColonizeColony* c = &ctx->colonies->colonies[cid];
      if (c->active && c->nation_id == human && !have_colony_fallback) {
        have_colony_fallback = 1;
        fcx = nx;
        fcy = ny;
      }
      continue;
    } else {
      /*
       * Soft coastal / foundable land next to a human colony — only when the
       * ship is already adjacent to that colony (port water). One tile shy of
       * the port must not dump onto foundable tiles (MD-early soft coast);
       * sail onto coast water first, then unload.
       */
      if (!ai_king_adjacent_human_colony(ctx, human, ship->x, ship->y)) {
        continue;
      }
      const int foundable =
          ctx->colonies && colonies_can_found(ctx->colonies, ctx->map, nx, ny);
      const int coastal = map_tile_is_coastal(ctx->map, nx, ny);
      if (!foundable && !coastal) {
        continue;
      }
      if (!ai_king_adjacent_human_colony(ctx, human, nx, ny)) {
        continue;
      }
      score = foundable ? 50 : 40;
    }
    if (score > best_score) {
      best_score = score;
      bx = nx;
      by = ny;
    }
  }
  if (best_score < 0) {
    if (!have_colony_fallback) {
      return 0;
    }
    bx = fcx;
    by = fcy;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Pick one cargo id to unload: prefer Regular; else Dragoon; else first slot.
 * Returns passenger id or -1.
 */
static int ai_king_mow_pick_unload_pax(const ColonizeTurnContext* ctx,
                                       const ColonizeUnit* ship) {
  if (!ctx || !ctx->units || !ship || ship->cargo_count <= 0) {
    return -1;
  }
  for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[c]);
    if (p && ai_king_is_regular(ctx->units, p)) {
      return ship->cargo_ids[c];
    }
  }
  for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[c]);
    if (p && ai_king_is_dragoon(ctx->units, p)) {
      return ship->cargo_ids[c];
    }
  }
  return ship->cargo_ids[0];
}

/*
 * Wartime MoW adjacent to coast/colony land: unload passengers this beat.
 * Prefer Regular in hold; else Dragoon (REF land arm — fandom Regulars +
 * Cavalry/Dragoons). Reuses units_unload_passenger (Euro landfall path).
 * Multi-slot: dump up to min(moves_left, ship capacity, cargo_count) — real
 * fields only; do not invent passengers. Cite: fandom man-o-war ×6 / seize.
 * Spends one ship move per passenger unloaded so leftover MP can AI_SAIL to
 * the next human coast same beat after a full unload.
 * Returns count unloaded (0 if none). Writes dest via out_x/out_y when non-NULL
 * and n>0 (for same-beat capture/fortify of passengers skipped while aboard).
 */
static int ai_king_mow_try_unload(ColonizeTurnContext* ctx, ColonizeUnit* ship,
                                 int human, int* out_x, int* out_y) {
  if (!ctx || !ctx->units || !ship || ship->cargo_count <= 0) {
    return 0;
  }
  if (ship->moves_left <= 0) {
    return 0;
  }
  int dest_x = 0;
  int dest_y = 0;
  if (!ai_king_mow_unload_land_dest(ctx, human, ship, &dest_x, &dest_y)) {
    return 0;
  }
  int cap = units_ship_capacity(ctx->units, ship->id);
  if (cap <= 0) {
    cap = COLONIZE_UNIT_CARGO_MAX;
  }
  int budget = ship->moves_left / UNITS_MP_PER_TILE; /* 1 MP per pax, in thirds */
  if (budget > cap) {
    budget = cap;
  }
  if (budget > ship->cargo_count) {
    budget = ship->cargo_count;
  }
  int n = 0;
  while (n < budget && ship->cargo_count > 0 && ship->moves_left > 0) {
    const int pax_id = ai_king_mow_pick_unload_pax(ctx, ship);
    if (pax_id < 0) {
      break;
    }
    if (!units_unload_passenger(ctx->units, ship->id, pax_id, ctx->map, dest_x, dest_y,
                                ctx->colonies)) {
      break;
    }
    ship->moves_left -= UNITS_MP_PER_TILE;
    if (ship->moves_left < 0) {
      ship->moves_left = 0;
    }
    n++;
  }
  if (n > 0) {
    if (out_x) {
      *out_x = dest_x;
    }
    if (out_y) {
      *out_y = dest_y;
    }
  }
  return n;
}

/*
 * Fortify one idle garrison unit on (x,y). require_moves: second slot needs MP.
 * Prefer Regular over Dragoon/Cont. Cav; prefer prefer_id when eligible.
 * Returns unit id fortified, or -1.
 */
static int ai_king_fortify_one_garrison_on_tile(ColonizeTurnContext* ctx, int crown, int x, int y,
                                                int require_moves, int prefer_id) {
  if (!ctx || !ctx->units || crown < 0) {
    return -1;
  }
  ColonizeUnitPool* pool = ctx->units;

  if (prefer_id >= 0) {
    ColonizeUnit* p = units_get(pool, prefer_id);
    if (p && p->active && p->nation_id == crown && p->x == x && p->y == y &&
        ai_king_is_regular(pool, p) && p->orders != UNITS_ORDER_FORTIFY &&
        p->orders != UNITS_ORDER_FORTIFIED && (!require_moves || p->moves_left > 0)) {
      (void)units_order_fortify(pool, p->id);
      return p->id;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != crown || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_regular(pool, u) || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (require_moves && u->moves_left <= 0) {
      continue;
    }
    (void)units_order_fortify(pool, u->id);
    return u->id;
  }
  if (prefer_id >= 0) {
    ColonizeUnit* p = units_get(pool, prefer_id);
    if (p && p->active && p->nation_id == crown && p->x == x && p->y == y &&
        ai_king_is_garrison_cavalry(pool, p) && p->orders != UNITS_ORDER_FORTIFY &&
        p->orders != UNITS_ORDER_FORTIFIED && (!require_moves || p->moves_left > 0)) {
      (void)units_order_fortify(pool, p->id);
      return p->id;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != crown || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_garrison_cavalry(pool, u) || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (require_moves && u->moves_left <= 0) {
      continue;
    }
    (void)units_order_fortify(pool, u->id);
    return u->id;
  }
  return -1;
}

/*
 * After capture / idle on crown colony: fortify up to two garrison units
 * (UNITS_ORDER_FORTIFY). First slot: Regular prefer, else Dragoon/Cont. Cav
 * (capture may use 0 MP). Second slot: another idle garrison on tile with
 * moves_left > 0. Extras beyond two hunt (idle-garrison gate).
 * Cite: Colonization.pdf Defending a Colony ("fortify soldiers, dragoons…");
 * king_ref thin multi-garrison (cap 2). Deep multi-step siege PARKED.
 */
static void ai_king_fortify_garrison_at(ColonizeTurnContext* ctx, ColonizeUnit* capturer,
                                        int crown, int x, int y) {
  if (!ctx || !ctx->units || crown < 0) {
    return;
  }
  if (ai_king_tile_fortified_garrison_count(ctx, crown, x, y, -1) >= 2) {
    return;
  }
  const int prefer = capturer ? capturer->id : -1;
  if (ai_king_tile_fortified_garrison_count(ctx, crown, x, y, -1) < 1) {
    (void)ai_king_fortify_one_garrison_on_tile(ctx, crown, x, y, 0, prefer);
  }
  if (ai_king_tile_fortified_garrison_count(ctx, crown, x, y, -1) < 2) {
    (void)ai_king_fortify_one_garrison_on_tile(ctx, crown, x, y, 1, -1);
  }
}

/*
 * After capture / idle on own colony: fortify crown Artillery on the tile
 * (UNITS_ORDER_FORTIFY). Euro pattern — case 0x0b fortify arm; Colonization.pdf
 * fortify defense / Artillery siege; euro_unit_act Artillery fortify after siege.
 * Unlike Regular stack (one garrison), each idle Artillery on the colony holds.
 * Artillery elsewhere still hunts fortified ports (thin siege bias).
 */
static void ai_king_fortify_artillery_at(ColonizeTurnContext* ctx, ColonizeUnit* capturer,
                                         int crown, int x, int y) {
  if (!ctx || !ctx->units || crown < 0) {
    return;
  }
  if (capturer && capturer->active && capturer->nation_id == crown &&
      ai_king_is_artillery(ctx->units, capturer) && capturer->x == x &&
      capturer->y == y) {
    if (capturer->orders != UNITS_ORDER_FORTIFY &&
        capturer->orders != UNITS_ORDER_FORTIFIED) {
      (void)units_order_fortify(ctx->units, capturer->id);
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_artillery(ctx->units, u)) {
      continue;
    }
    if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    (void)units_order_fortify(ctx->units, u->id);
  }
}

/* If REF stands on a human colony tile, capture (conquest — colonies_capture). */
static void ai_king_try_capture_at(ColonizeTurnContext* ctx, ColonizeUnit* u, int crown,
                                   int human) {
  if (!ctx || !u || !u->active || !ctx->colonies) {
    return;
  }
  const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
  if (cid < 0) {
    return;
  }
  ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
  if (c && c->nation_id == human && ai_king_human_defender_at(ctx, human, u->x, u->y) < 0 &&
      !units_is_sea(ctx->units, u->id) && !ai_king_is_artillery(ctx->units, u)) {
    /* Civilians left in the port change hands with it (DOS conquest). */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* f = &ctx->units->units[i];
      if (f->active && f->nation_id == human && units_is_on_map(f) && f->x == u->x &&
          f->y == u->y && !units_is_sea(ctx->units, f->id)) {
        units_set_nation(f, crown);
      }
    }
    /* Source: conquest / colonies_capture — Euro owner swap; no gold fiction. */
    char cname[COLONIZE_COLONY_NAME_MAX];
    snprintf(cname, sizeof(cname), "%s", c->name[0] ? c->name : "your colony");
    if (colonies_capture(ctx->colonies, cid, crown)) {
      /*
       * Thin human status on REF capture (full conquest chrome PARKED).
       * Do not clobber same-beat 2244 merc hire status; may replace 1528 arrival.
       * ai_popup OK (AI_POPUP_TAG_KING_CAPTURE) when queue attached.
       */
      if (ctx->status && ctx->status_size &&
          !(strstr(ctx->status, "Mercenaries join") ||
            strstr(ctx->status, "Continental cause") ||
            strstr(ctx->status, "Cannot afford mercenaries"))) {
        snprintf(ctx->status, ctx->status_size, "The King's forces have captured %s!",
                 cname);
      }
      if (ai_king_human_popups(ctx)) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = "The King's forces";
        tok.string2 = cname;
        char fallback[AI_POPUP_BODY_LEN];
        snprintf(fallback, sizeof(fallback), "The King's forces march into %s!", cname);
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages, "CAPTURED3", &tok, fallback, body, sizeof(body)
        );
        (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_CAPTURE, human,
                                      crown, cid, "Colony Captured", body);
      }
      ai_king_fortify_garrison_at(ctx, u, crown, u->x, u->y);
      /* Euro pattern: idle Artillery on newly captured colony → FORTIFY. */
      ai_king_fortify_artillery_at(ctx, u, crown, u->x, u->y);
    }
  }
}

/*
 * After a land step onto a colony tile: seize if still human-owned, and always
 * apply post-seize fortify when the tile is (now) crown-owned.
 *
 * units_try_move may flip ownership first via units_try_capture_foreign_colony;
 * ai_king_try_capture_at then sees nation_id==crown and would skip fortify.
 * Call this instead of bare try_capture_at after any successful step/combat
 * enter onto a colony. Cap-2 / already-fortified are no-ops in the helpers.
 */
static void ai_king_after_step_onto_colony(ColonizeTurnContext* ctx, ColonizeUnit* u,
                                           int crown, int human) {
  if (!ctx || !u || !ctx->colonies || crown < 0) {
    return;
  }
  const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
  if (cid < 0) {
    return;
  }
  ai_king_try_capture_at(ctx, u, crown, human);
  if (!u->active) {
    return;
  }
  const ColonizeColony* c = &ctx->colonies->colonies[cid];
  if (!c->active || c->nation_id != crown) {
    return;
  }
  if (units_foreign_unit_at(ctx->units, u->x, u->y, u->id, u->nation_id) >= 0) {
    return;
  }
  if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
    return;
  }
  ai_king_fortify_garrison_at(ctx, u, crown, u->x, u->y);
  ai_king_fortify_artillery_at(ctx, u, crown, u->x, u->y);
}

/*
 * Same-beat seize/fortify for passengers just put ashore (unit-index order may
 * have skipped them while aboard with moves_left==0). Cite: fandom REF seize
 * landing + fortify one Regular (else Dragoon/Cont.Cav) after capture / multi-unload.
 */
static void ai_king_mow_post_unload_land(ColonizeTurnContext* ctx, int crown, int human,
                                         int dest_x, int dest_y) {
  if (!ctx || !ctx->units || crown < 0) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown || u->x != dest_x || u->y != dest_y) {
      continue;
    }
    if (units_is_sea(ctx->units, u->id) || u->aboard_ship_id >= 0) {
      continue;
    }
    ai_king_try_capture_at(ctx, u, crown, human);
  }
}

static int ai_king_force_total(const uint16_t force[4]) {
  if (!force) {
    return 0;
  }
  return (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];
}

/*
 * Spawn one land unit near (hx,hy) for nation_id — shared by 06a6 / 10f0.
 * Returns unit id or -1.
 */
static int ai_king_spawn_landing(ColonizeTurnContext* ctx, int nation_id, int sx, int sy,
                                 const char* type_name, const char* alt_name, bool veteran_10f0) {
  if (!ctx || !ctx->units || nation_id < 0) {
    return -1;
  }
  int ty = units_find_type(ctx->units, type_name);
  if (ty < 0 && alt_name) {
    ty = units_find_type(ctx->units, alt_name);
  }
  if (ty < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(ctx->units, ty, sx, sy);
  if (uid < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (u) {
    units_set_nation(u, nation_id);
    if (veteran_10f0) {
      u->profession = UNITS_JOB_SOLDIER; /* DOS 0x15 Veteran Soldiers */
    }
    u->orders = UNITS_ORDER_AI_MOVE;
    u->goto_x = sx;
    u->goto_y = sy;
  }
  return uid;
}

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

/* Grow REF pools by current tax band (1d42 crumb; no tax_rate change). */
static void ai_king_grow_ref_from_tax(ColonizeCol1Save* col1, uint8_t tax_rate) {
  if (!col1) {
    return;
  }
  col1->head.expeditionary_force[0] += 1; /* regulars */
  if (tax_rate >= 10) {
    col1->head.expeditionary_force[1] += 1; /* dragoons */
  }
  if (tax_rate >= 20) {
    col1->head.expeditionary_force[2] += (tax_rate % 5 == 0) ? 1 : 0; /* MoW */
  }
  if (tax_rate >= 30 && (tax_rate % 10 == 0)) {
    col1->head.expeditionary_force[3] += 1; /* artillery */
  }
  if (col1->head.expeditionary_force[0] > 0) {
    ai_king_set_ref_present(col1, 1);
  }
}

int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  /*
   * FUN_43f7_0004: pop-weighted colony SoL via FUN_15eb_0274 (incl. Bolivar).
   * Prefer Col1 rebel_dividend/divisor + display boost; else liberty bells.
   */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    uint64_t pop_sum = 0;
    uint64_t sol_sum = 0;
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != nation_id) {
        continue;
      }
      const uint64_t pop = (uint64_t)(c->population > 0 ? c->population : 1);
      int sol = 0;
      if (c->rebel_divisor > 0) {
        sol = (int)((c->rebel_dividend * 100u) / c->rebel_divisor);
      } else {
        sol = (int)ctx->col1->nation[nation_id].liberty_bells_total / 4;
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
  if (ctx->col1_ok && ctx->col1) {
    const ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
    const int bells = (int)nat->liberty_bells_total;
    int sol = bells / 4;
    sol += founding_fathers_bolivar_sol_bonus(ctx->col1, nation_id);
    if (sol > 100) {
      sol = 100;
    }
    return sol;
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
      const uint32_t div = c->rebel_divisor > 0 ? c->rebel_divisor : 1;
      int sol = (int)(((uint64_t)c->rebel_dividend * 100ull) / (uint64_t)div);
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

/*
 * FUN_38fd_5be8: King-audience favor-score ladder → signed tax-rate delta.
 * Real DOS gating/formula (no invented Accept/Refuse-whether-it-happens
 * gate here — see divergence note above ai_king_tax_event for history).
 * Source: original_sources_decompiled/viceroy_unpacked.c:68420.
 *
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

  const int score =
    dos_rng_range(ctx->rng, 1, 1000) +
    (col1->head.rebel_sentiment_report * 2 - (int)nat->tax_rate) * 5 +
    (int)(nat->gold / 100) +
    ai_king_sol_percent(ctx, human) +
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
static void ai_king_tax_teaparty(ColonizeTurnContext* ctx, int human, int applied, int cargo) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT || applied <= 0) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  int revert = applied;
  if (revert > (int)nat->tax_rate) {
    revert = (int)nat->tax_rate;
  }
  nat->tax_rate = (uint8_t)(nat->tax_rate - revert);
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
 * whether the hike happens" design (see docs/mysteries_catalog.md,
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
static void ai_king_tax_hike_apply(ColonizeTurnContext* ctx, int human, int delta) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  int applied = 0;
  ai_king_audience_apply_delta(nat, delta, &applied);
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  /* FUN_43f7_1d42 REF-pool growth: a separate DOS mechanic (treasury
   * threshold, not tax delta) that this port still only approximates by
   * tax band — unchanged by this pass, kept for existing REF-growth
   * behavior continuity. */
  ai_king_grow_ref_from_tax(ctx->col1, nat->tax_rate);

  if (applied < 0) {
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "Audience: the King lowers taxes to %u%%.", nat->tax_rate);
    }
    if (ai_king_human_popups(ctx)) {
      char body[AI_POPUP_BODY_LEN];
      snprintf(body, sizeof(body),
               "The King, moved by your poverty, lowers taxes to %u%%.", nat->tax_rate);
      (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human,
                                    ai_king_crown_nation(human), (int)nat->tax_rate,
                                    "Royal Audience", body);
    }
    return;
  }

  if (applied == 0) {
    return; /* delta rolled but fully clamped away (already at 75%) */
  }

  /* applied > 0: a real hike happened. */
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size,
             "Audience: the King raises taxes to %u%%.", nat->tax_rate);
  }

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
     * nation — DOS's own choice UI has nothing to drive here either. */
    if (ai_king_human_popups(ctx)) {
      char body[AI_POPUP_BODY_LEN];
      snprintf(body, sizeof(body), "The King raises taxes to %u%%.", nat->tax_rate);
      sound_play(0x56); /* FUN_38fd_3dc8 tax raise (COLDIG 9 cheering) */
      (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human,
                                    ai_king_crown_nation(human), (int)nat->tax_rate,
                                    "Royal Tax", body);
    }
    return;
  }

  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.number0 = 1;
    tok.has_number0 = true;
    tok.number1 = (int)nat->tax_rate;
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
                                    ai_king_crown_nation(human),
                                    ai_king_teaparty_payload(applied, picked),
                                    NULL, body, labels, ids, 2)) {
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
      ((int)nat->tax_rate >= AI_KING_BOYCOTT_TAX_MIN) &&
      (sol >= AI_KING_BOYCOTT_SOL_MIN || nat->liberty_bells_total >= AI_KING_BOYCOTT_BELLS_MIN);
  if (auto_teaparty) {
    ai_king_tax_teaparty(ctx, human, applied, picked);
  }
}

/*
 * FUN_43f7_1a26 declare body (after 2564 confirm / auto).
 * Seeds REF by difficulty; thin backup_force as 10f0 foreign-pool stand-in;
 * withdraws other Euros; thin 160a rename; unknown46[5] congress.
 */
static void ai_king_do_declare(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  ai_king_set_independence(ctx->col1, 1); /* WoI: unknown46[0] if not already */
  ai_king_write_rival_nation_slots(ctx->col1, human);
  /* FUN_43f7_2564 congress-confirm stand-in. */
  ai_king_latch_set(ctx->col1, AI_KING_CONGRESS_BYTE, 1);
  const int diff = ctx->col1->head.difficulty;
  ctx->col1->head.expeditionary_force[0] = (uint16_t)(8 + diff * 4);
  ctx->col1->head.expeditionary_force[1] = (uint16_t)(4 + diff * 2);
  ctx->col1->head.expeditionary_force[2] = (uint16_t)(2 + diff);
  ctx->col1->head.expeditionary_force[3] = (uint16_t)(2 + diff);
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
  const int crown_fold = ai_king_crown_nation(human);
  for (int n = 0; n < 4; ++n) {
    if (n == human) {
      continue;
    }
    ctx->col1->player[n].control = 2;
    if (n != crown_fold && ctx->col1_ok) {
      /*
       * FUN_43f7_0108 diplo-clear/set (0xb clear / 0x60 OR bitmasks vs
       * DS:0x5398 declaring nation and DS:0x53d2 crown -- DOS never targets
       * the crown itself with 0108, matching the n==crown_fold skip here).
       * 0xb = WAR|PEACE|unmapped-bit3; 0x60 = unmapped-bit5|MET. The two
       * unmapped bits are intentionally not applied -- see king_ref.md.
       */
      ai_diplo_clear_both(ctx->col1, n, human, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_PEACE));
      ai_diplo_or_both(ctx->col1, n, human, (uint8_t)AI_DIPLO_MET);
      ai_diplo_clear_both(ctx->col1, n, crown_fold, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_PEACE));
      ai_diplo_or_both(ctx->col1, n, crown_fold, (uint8_t)AI_DIPLO_MET);
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
  snprintf(ctx->col1->player[human].country_name,
           sizeof(ctx->col1->player[human].country_name), "%s", AI_KING_INDEP_COUNTRY);
  if (ctx->europe) {
    snprintf(ctx->europe->nation_name, sizeof(ctx->europe->nation_name), "%s",
             AI_KING_INDEP_COUNTRY);
  }
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
      "Abuses and usurpations cited! The colonies are renamed the United Colonies.",
      letter,
      sizeof(letter)
    );
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_KING_LETTER,
      human,
      ai_king_crown_nation(human),
      0,
      "Declaration of Independence",
      letter
    );
    /* FUN_43f7_1a26 / 2564: @HOWTOWIN briefing after Confirm/auto declare. */
    char how[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "HOWTOWIN",
      NULL,
      "We have just won a glorious victory on the road to freedom, Your Excellency. "
      "In order to defeat the King's forces and win our independence, we must "
      "recapture all of our colonies from the King, and we must destroy most of "
      "his ground forces in the New World.",
      how,
      sizeof(how)
    );
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_INFO, human, ai_king_crown_nation(human), 1,
      "Road to Freedom", how
    );
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
    const char* motherland = "the Crown";
    if (ctx->col1->player[human].country_name[0] != '\0') {
      motherland = ctx->col1->player[human].country_name;
    } else if (ctx->europe && ctx->europe->nation_name[0] != '\0') {
      motherland = ctx->europe->nation_name;
    }
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
                                    ai_king_crown_nation(human), sol, "Continental Congress",
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
                                    ai_king_crown_nation(human), sol, "Continental Congress",
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

/* FUN_43f7_10f0 ~74307: pop-weighted coastal colony roulette (thin). */
static int ai_king_10f0_pick_colony(const ColonizeTurnContext* ctx, int human, int* out_x,
                                    int* out_y) {
  if (!ctx || !out_x || !out_y || human < 0 || human >= 4) {
    return -1;
  }
  int total_pop = 0;
  int best_i = -1;
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human || !c->flags.coastal) {
        continue;
      }
      const int pop = c->population > 0 ? (int)c->population : 1;
      total_pop += pop;
    }
    if (total_pop <= 0) {
      return -1;
    }
    int pick = total_pop / 2 + 1;
    if (ctx->rng) {
      pick = dos_rng_range(ctx->rng, 1, total_pop);
    }
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human || !c->flags.coastal) {
        continue;
      }
      const int pop = c->population > 0 ? (int)c->population : 1;
      pick -= pop;
      if (pick <= 0) {
        *out_x = (int)c->x;
        *out_y = (int)c->y;
        return (int)i;
      }
    }
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
  int score = 1;
  for (int i = 0; i < COLONIZE_UNITS_MAX && ctx->units; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->x != tx || u->y != ty || u->aboard_ship_id >= 0) {
      continue;
    }
    if (u->nation_id != human) {
      return -1;
    }
    if (u->type_index == 0x12) {
      score -= 999;
    }
  }
  if (score < 0) {
    return score;
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
          ctx->ai_popups, AI_POPUP_TAG_INFO, ctx->human_nation, crown, 0, "Seizure", body
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
  static const char* names[4] = {"Regulars", "Dragoons", "Man-O-War", "Artillery"};
  static const char* alts[4] = {"Soldiers", "Scouts", "Galleon", "Cannon"};
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
  u->orders = UNITS_ORDER_AI_MOVE;
  u->goto_x = x;
  u->goto_y = y;
  /* 0982: the landed unit's moves are spent this beat (02d0 animate + 0948). */
  u->moves_left = 0;
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
 * Thin: 08bc stack strength = Σ defense×8>>4; the 4d56 crown ship act that
 * takes an emptied MoW home is stood in for by despawning idle empty crown
 * MoWs at wave start while land pools remain (so the MoW pool can regrow).
 */
static void ai_king_ref_wave(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->map) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int crown = ai_king_crown_nation(ctx->human_nation);
  const int human = ctx->human_nation;
  uint16_t* force = ctx->col1->head.expeditionary_force;
  int total = (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];

  if (total <= 0) {
    /* 06a6 irregulars near player colony — crown nation_id, never human. */
    int hx = 0;
    int hy = 0;
    if (ai_king_weakest_port(ctx, human, &hx, &hy) < 0) {
      return;
    }
    (void)ai_king_spawn_landing(ctx, crown, hx, hy + 1, "Regular", "Soldier", false);
    return;
  }

  /* Thin 4d56 ship act: an emptied Man-O-War sails home for the next wave. */
  if ((int)force[0] + (int)force[1] + (int)force[3] > 0) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->nation_id == crown && ai_king_is_mow(ctx->units, u) &&
          u->cargo_count == 0 && u->turns_worked > 0) {
        (void)units_despawn(ctx->units, u->id);
      } else if (u->active && u->nation_id == crown && ai_king_is_mow(ctx->units, u)) {
        u->turns_worked = (uint8_t)(u->turns_worked + 1);
      }
    }
  }

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
    /* Score human coastal colonies (≤10), weakest first. */
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
    /* Pick: three relaxing passes over the weakest-first list. */
    int pick = -1;
    int need = 0;
    for (int pass = 0; pass < 3 && pick < 0; ++pass) {
      for (int i = 0; i < n; ++i) {
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
              ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, crown, 0,
              "Royal Expeditionary Force", body
            );
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
          /* DOS lands on every tile whose stack is no stronger than the weakest. */
          int usable = 0;
          while (usable < nc && cs[usable] <= cs[0]) {
            usable++;
          }
          for (int t = 0; t < usable; ++t) {
            ai_king_0982_purge_tile(ctx, crown, cx[t], cy[t]);
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
            const int uid = ai_king_0982_spawn_pool_unit(ctx, crown, k, cx[slot], cy[slot]);
            if (uid < 0) {
              break;
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
 * FUN_43f7_10f0-shaped: foreign-intervention landing when REF empty and
 * backup_force (DOS 0x53e2… stand-in) still has pools. Up to two landings
 * per call; third when difficulty ≥ AI_KING_INTERVENE_DIFF_THIRD (REF
 * pressure). Prefer Regular + Dragoon when both pools > 0. Intervene nation:
 * Euro with most colonies (tie-break land-unit force). Thin arrival OK once
 * when landings>0 + ai_popups (1528-shaped; deep economy / merc chrome PARKED).
 */
static void ai_king_foreign_intervene(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  uint16_t* backup = ctx->col1->head.backup_force;
  if (ai_king_force_total(ctx->col1->head.expeditionary_force) > 0) {
    return;
  }
  if (ai_king_force_total(backup) <= 0) {
    return;
  }
  int hx = 0;
  int hy = 0;
  int sx = 0;
  int sy = 0;
  if (ai_king_10f0_pick_colony(ctx, ctx->human_nation, &hx, &hy) < 0) {
    if (ai_king_weakest_port(ctx, ctx->human_nation, &hx, &hy) < 0) {
      return;
    }
  }
  const int human = ctx->human_nation;
  const int ally1 = ai_king_intervention_nation_slot(ctx, human, 0);
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
  if (backup[2] > 0) {
    backup[2]--;
  }
  if (ctx->map) {
    map_reveal_tile(ctx->map, sx, sy, human);
  }
  int landings = 1;
  int caps[4] = {0, 0, 0, 0};
  caps[1] = backup[1] > 2 ? 2 : (int)backup[1];
  caps[3] = backup[3] > 2 ? 2 : (int)backup[3];
  caps[0] = 6 - (caps[1] + caps[3]);
  static const int pool_k[3] = {0, 1, 3};
  bool ok = true;
  for (int pi = 0; pi < 3 && ok; ++pi) {
    const int k = pool_k[pi];
    int n = caps[k];
    if (n > (int)backup[k]) {
      n = (int)backup[k];
    }
    for (int s = 0; s < n; ++s) {
      if (ai_king_10f0_spawn_unit(ctx, human, k, hx, hy) < 0) {
        ok = false;
        break;
      }
      backup[k]--;
      landings++;
    }
  }
  if (ctx->map) {
    map_reveal_radius(ctx->map, hx, hy, human, 2);
  }

  if (landings > 0) {
    static const char* k_euro[4] = {"English", "French", "Spanish", "Dutch"};
    const int crown = ai_king_crown_nation(human);
    const char* ally_name =
      (ally1 >= 0 && ally1 < 4 && ctx->col1 && ctx->col1->player[ally1].country_name[0])
        ? ctx->col1->player[ally1].country_name
        : ((ally1 >= 0 && ally1 < 4) ? k_euro[ally1] : "Foreign");
    const char* crown_name =
      (crown >= 0 && crown < 4 && ctx->col1 && ctx->col1->player[crown].country_name[0])
        ? ctx->col1->player[crown].country_name
        : ((crown >= 0 && crown < 4) ? k_euro[crown] : "the Crown");
    const char* colony = "the colonies";
    if (ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, hx, hy);
      const ColonizeColony* c = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
      if (c && c->name[0]) {
        colony = c->name;
      }
    }
    char general[64];
    snprintf(general, sizeof(general), "%s General", ally_name);

    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s Intervention Force arrives in %s!",
               ally_name, colony);
    }
    if (ai_king_human_popups(ctx)) {
      PopupMsgTokens itok;
      memset(&itok, 0, sizeof(itok));
      itok.string0 = ally_name;
      itok.string1 = crown_name;
      itok.string2 = general;
      itok.string3 = colony;
      itok.string4 = ally_name;
      char body[AI_POPUP_BODY_LEN];
      char fallback[AI_POPUP_BODY_LEN];
      snprintf(
        fallback,
        sizeof(fallback),
        "%s declares war on %s and joins the War of Independence on the Rebel side!",
        ally_name,
        crown_name
      );
      popup_msg_fill(ctx->messages, "INTERVENTION", &itok, fallback, body, sizeof(body));
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, ally1, landings,
        "Foreign Intervention", body
      );

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
        ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, human, ally1, landings,
        "Foreign Intervention", body
      );
      sound_set_bgm(3); /* FUN_43f7_10f0 43f7:145b: 281f_0498(3) Independence pool… */
      sound_play(0x3f); /* …then 43f7:1465: intervention tune after @INTERVENE */
    }
  }
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
  /* DOS: (*(byte *)0x5382 & 2) != 0 → return without spending. */
  if (ctx->col1->head.game_options.ref_present) {
    return 0;
  }
  if (nation_id == ctx->human_nation) {
    const int exp_total = ai_king_force_total(ctx->col1->head.expeditionary_force);
    if (exp_total > 0) {
      ai_king_ref_wave(ctx);
    } else if (ai_king_force_total(ctx->col1->head.backup_force) > 0) {
      ai_king_foreign_intervene(ctx);
    } else {
      ai_king_ref_wave(ctx); /* 06a6 irregulars when both pools empty. */
    }
  }
  return 1;
}

/*
 * Pack offer-time roll + landing pick into the popup payload:
 * hx(6b)<<26 | hy(6b)<<20 | qty_a(4b)<<16 | extra_flag(1b)<<15 | price(15b).
 * Landing coords are captured at OFFER time (not re-derived at apply time)
 * — same discipline the old fixed-cost hire used ("a same-turn REF capture
 * cannot void the hire after CHOICE was queued"). Re-deriving via
 * ai_king_weakest_port at apply time was tried and is NOT harmless: if the
 * only human colony gets captured in the same beat the offer was rolled
 * (a real, observed same-turn race in ai_king_war_act's own capture path),
 * weakest_port returns -1 and the whole accept silently fails. Price fits
 * 15 bits (max observed (8+2)*((4+3)*2+6)*100 = 20000 < 32768); qty_a fits
 * 4 bits (range 2-8); hx/hy fit 6 bits each (map width/height ≤ 63 in this
 * project's fixed 58×72 world).
 */
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
 * FUN_43f7_2022 rebel-branch accept: spend the rolled price, spawn qty_a
 * Regular + 1 extra (Dragoon or Artillery, per the offer-time coin flip)
 * at the offer-time landing pick (hx,hy — NOT re-derived here; a same-turn
 * capture of the only human colony would otherwise silently void an
 * already-accepted offer, see ai_king_merc_payload's comment). Returns 1
 * on success (at least the Regular batch landed), 0 if unaffordable or
 * nothing could spawn. Real DOS type codes for a human-controlled rebel
 * are FUN_43f7_0082's fixed slot0/1 values (6/8), not independently named
 * this pass — using the already-established "Regular"/"Dragoon"/
 * "Artillery" display names (same as ai_king_intervene_one) as a
 * documented approximation.
 */
static int ai_king_do_merc_hire_at(ColonizeTurnContext* ctx, int human, int hx, int hy,
                                   int qty_a, int extra_flag, int price) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || human < 0 || human >= 4) {
    return 0;
  }
  if (qty_a < 1 || price < 0 || hx < 0 || hy < 0) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  if (nat->gold < (uint32_t)price) {
    return 0;
  }
  int landed = 0;
  for (int i = 0; i < qty_a; ++i) {
    if (ai_king_spawn_landing(ctx, human, hx, hy + 1, "Regular", "Soldier", false) >= 0) {
      ++landed;
    }
  }
  if (extra_flag) {
    (void)ai_king_spawn_landing(ctx, human, hx, hy + 1, "Artillery", NULL, false);
  } else {
    (void)ai_king_spawn_landing(ctx, human, hx, hy + 1, "Dragoon", "Scout", false);
  }
  if (landed == 0) {
    return 0;
  }
  nat->gold -= (uint32_t)price;
  if (ctx->europe) {
    ctx->europe->gold = (int)nat->gold;
  }
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size,
             "Mercenaries join the Continental cause (−%d gold).", price);
  }
  /* GAME.TXT @MERCS arrival OK. */
  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = "the colonies";
    tok.string1 = "Trained";
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages, "MERCS", &tok,
      "Mercenaries arrive.", body, sizeof(body)
    );
    (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_MERC, human,
                                  ai_king_crown_nation(human),
                                  ai_king_merc_payload(hx, hy, qty_a, extra_flag, price), NULL,
                                  body);
  }
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
  const int ref_present = ctx->col1->head.game_options.ref_present != 0;
  /*
   * FUN_43f7_2022 line 75007: `(*(byte*)0x5382 & 2) == 0 || *(int*)0x53e6 == 0`
   * — gate reads the Man-O-War/colony-count pool (backup_force[2], see
   * ai_king_seed_backup_force_1a26), not the Artillery pool.
   */
  const int mow_pool = ctx->col1->head.backup_force[2];
  if (ref_present && mow_pool != 0) {
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
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  if (nat->gold < (uint32_t)price) {
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
    tok.string0 = "Europe"; /* king nation stand-in */
    tok.string1 = extra_flag ? "Artillery" : "Dragoons";
    tok.number0 = price;
    tok.has_number0 = true;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "MERCENARIES",
      &tok,
      "The King has offered to send mercenaries in exchange for gold.",
      body,
      sizeof(body)
    );
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
                                    ai_king_crown_nation(human),
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
 * choice. Threat radius is approximated as Chebyshev ≤ 5 (the 11x11 box);
 * the extra FUN_2a1f_027e path-distance < 6 refinement is not replicated.
 */
static int ai_king_frigate_threat_counts(
  const ColonizeTurnContext* ctx, int nation, int* out_frigate, int* out_other
) {
  int frig = 0;
  int other = 0;
  if (!ctx || !ctx->units || !ctx->colonies) {
    return 0;
  }
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    int bits = 0;
    for (int ui = 0; ui < ctx->units->unit_count; ++ui) {
      const ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || !units_is_on_map(u) || u->nation_id == nation) {
        continue;
      }
      const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
      if (!t || t->domain != COLONIZE_UNIT_DOMAIN_SEA || t->attack <= 0) {
        continue;
      }
      if (abs(u->x - c->x) > 5 || abs(u->y - c->y) > 5) {
        continue;
      }
      bits |= (t->name[0] && strcmp(t->name, "Frigate") == 0) ? 2 : 1;
    }
    if (bits & 2) {
      frig++;
    }
    if (bits & 1) {
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
  const int dur = europe_voyage_turns_roll(
    ctx->rng, magellan, units_count_sea_for_nation(ctx->units, nation)
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
  for (int ui = 0; ui < ctx->units->unit_count; ++ui) {
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
                                   ai_king_crown_nation(nation), 0, NULL, body, labels, ids, 2)) {
    ai_king_frigate_accept(ctx, nation); /* queue full: DOS has no "no answer" path */
  }
}

/*
 * FUN_43f7_2244 — peacetime AI-nation-only twin of 2022's rebel gift,
 * implemented 2026-08-14 (see king_ref.md "2244/2022 — corrected").
 * Reached via FUN_281f_0668 from the generic per-Euro-nation turn loop
 * (viceroy_unpacked.c:6409-6421), gated on the SAME human-controlled flag
 * byte (`nation*0x34+0x543f==0`) Linux's turn_run_european_ai_stubs
 * already uses to skip the human nation entirely — confirmed AI-only,
 * never reachable for a human turn.
 *
 * Gate: WoI not yet declared, 1-in-21 roll (dos_rng_range(0,20)==0). Then
 * picks a random Euro nation 0-3 as beneficiary; eligible only if that's
 * this AI nation itself or a nation it's allied with (AI_DIPLO_ALLY bit).
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
 * (self or ally) at its own weakest port. No human popup is reachable
 * through this call chain (DOS's own popup-flush call presumably
 * auto-resolves for AI without blocking, same as every other AI-context
 * dialog in this codebase) — Linux always auto-accepts when affordable,
 * matching 2022's own no-popup fallback path.
 *
 * Approximated: DOS's own "which nation is FOCUS_NATION for the
 * self/ally eligibility check" reads DS:0x5398, a global this specific
 * call chain doesn't visibly (re)assign in the read window — the more
 * locally-scoped per-nation loop variable is DS:0x5396/0x5394. Used the
 * Linux loop's own `nation_id` (the AI nation whose turn is running) as
 * the natural reading of "self" for both eligibility and payer, which
 * matches every other established per-AI-nation-turn convention in this
 * codebase; not independently confirmed byte-exact against 0x5398's
 * specific role in this one call chain.
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
  int hx = 0;
  int hy = 0;
  if (ai_king_weakest_port(ctx, beneficiary, &hx, &hy) < 0) {
    return;
  }
  int landed = 0;
  for (int i = 0; i < regular; ++i) {
    if (ai_king_spawn_landing(ctx, beneficiary, hx, hy + 1, "Regular", "Soldier", false) >= 0) {
      ++landed;
    }
  }
  for (int i = 0; i < artillery; ++i) {
    if (ai_king_spawn_landing(ctx, beneficiary, hx, hy + 1, "Artillery", NULL, false) >= 0) {
      ++landed;
    }
  }
  if (landed == 0) {
    return;
  }
  /* Payer (nation_id) is always AI-controlled (the caller only runs this
   * for AI turns) — no ctx->europe mirror to sync, that field only
   * shadows the human's own treasury. */
  payer->gold -= (uint32_t)price;
}

/*
 * FUN_43f7_2022 war act + 1eca promote.
 * REF land hunt (Regular/Dragoon/Artillery/Cont.→nearest human colony/unit) + combat/capture;
 * thin Artillery siege prefer fortified (adjacent unfortified must not override);
 * thin Dragoon/Cont. Cav prefer open when Artillery type exists; capital MD bias
 * (founding capital over distant colonies when MD within slack); post-capture
 * fortify one Regular (else Dragoon/Cont.Cav; stack extras hunt) + human status; wartime MoW with cargo
 * → unload-at-coast up to min(moves,capacity) Regular-prefer else Dragoon
 * (prefer colony tile / seize; spend 1 MP/pax) else AI_SAIL→human coast; after
 * *full* unload with moves left → AI_SAIL toward *next* human coast (skip
 * just-served port); after that sail step (or already on next-coast water)
 * prefer unload if still carrying and adjacent; same-beat post-unload
 * capture/fortify for passengers skipped while aboard; idle empty MoW →
 * AI_SAIL coastal patrol (nearest human coast water; no new ships); 0982
 * boards up to ship capacity into cargo_ids;
 * 1eca full port: per colony with SoL>49, cap = max(1, min(pop>>1,
 * pop*(sol-50)/50)) shared across a colony's own-tile, Veteran-status
 * (profession UNITS_JOB_SOLDIER — DOS unit+0x315b==0x15) Soldier/Dragoon
 * (Regular/already-Continental untouched — decomp tests raw type 1/4; an
 * ordinary armed colonist without Veteran profession is also skipped,
 * confirmed 2026-08-14; no FORTIFIED requirement — re-verified 2026-08-24,
 * decomp never reads unit+0x08/orders). Cont. Army/Cav after promote → capital-rally
 * (founding capital; weakest_port fallback);
 * 10f0 intervene arm (≤3 @ difficulty≥2); real 2022 rebel merc gift
 * (recurring per-turn roll, CHOICE or auto-accept).
 * REF idle Regular on crown colony (no adjacent foe) → fortify only if no other
 * Regular/Dragoon/Cont.Cav on tile is already FORTIFY/FORTIFIED; if no Regular,
 * fortify one Dragoon/Cont.Cav (Colonization.pdf Defending a Colony; king_ref
 * one-garrison); already-garrisoned stay put; extras hunt.
 * Idle Artillery on crown/captured colony → FORTIFY (Euro after-siege pattern;
 * Colonization.pdf fortify defense; euro_unit_act Artillery fortify).
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
  const int crown = ai_king_crown_nation(human);
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
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human, peer, gold, "New War", body
    );
  }
  col1->nation[human].gold += (uint32_t)gold;
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

static void ai_king_war_act(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /*
   * Rebel arm first: 10f0 while human ports still exist (crown move/capture
   * below may seize the landing pick). In addition to 06a6 in ref_wave.
   */
  ai_king_foreign_intervene(ctx);
  /* Real 2022: recurring per-turn rebel merc gift (hire CHOICE / auto). */
  ai_king_merc_offer(ctx);

  const int crown = ai_king_crown_nation(ctx->human_nation);
  const int human = ctx->human_nation;

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
  {
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
            u->type_index = army;
          } else if (dragoon_ty >= 0 && u->type_index == dragoon_ty && cav >= 0) {
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
      }
    }
  }

  /*
   * After 1eca Continental promote: Cont. Army / Cont. Cav (hunter name check
   * includes both) AI_MOVE toward nearest human colony, then prefer founding
   * capital when MD within AI_KING_CAPITAL_MD_SLACK (same helper as REF idle
   * hunters). Fallback weakest_port when no human colony. Hold if already on a
   * human colony tile. Source: fandom Independence Cont. Army / Cont. Cavalry +
   * REF main-port MD slack; deep rebel AI PARKED.
   */
  {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != human || u->moves_left <= 0) {
        continue;
      }
      if (units_is_sea(ctx->units, u->id)) {
        continue;
      }
      if (!ai_king_is_continental(ctx->units, u)) {
        continue;
      }
      /* Already on a human colony tile — hold; founding capital may fortify
       * up to two Cont. Army / Cont. Cav (Defending a Colony cap 2). */
      if (ctx->colonies) {
        const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
        if (cid >= 0) {
          const ColonizeColony* c = &ctx->colonies->colonies[cid];
          if (c->active && c->nation_id == human) {
            int cap_x = 0;
            int cap_y = 0;
            if (ai_king_human_capital(ctx, human, &cap_x, &cap_y) && u->x == cap_x &&
                u->y == cap_y) {
              if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
                continue;
              }
              if (ai_king_tile_human_cont_fortified_count(ctx, human, u->x, u->y, u->id) < 2) {
                ai_king_fortify_human_cont_at(ctx, u, human, u->x, u->y);
                if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
                  continue;
                }
              }
            }
            continue;
          }
        }
      }
      int hx = 0;
      int hy = 0;
      if (ai_king_nearest_human_colony(ctx, human, u->x, u->y, &hx, &hy)) {
        const int nearest_md = abs(hx - u->x) + abs(hy - u->y);
        (void)ai_king_prefer_capital_if_comparable(ctx, human, u->x, u->y, &hx, &hy,
                                                   nearest_md);
      } else if (ai_king_weakest_port(ctx, human, &hx, &hy) < 0) {
        continue;
      }
      u->orders = UNITS_ORDER_AI_MOVE;
      u->goto_x = hx;
      u->goto_y = hy;
    }
  }

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown) {
      continue;
    }
    /* Already on a human colony → capture even with 0 moves (conquest). */
    ai_king_try_capture_at(ctx, u, crown, human);
    if (!u->active || u->moves_left <= 0) {
      continue;
    }

    /*
     * Wartime MoW (fandom REF man-o-war → ports):
     *   cargo > 0 → unload Regular-prefer (else Dragoon) when adjacent to
     *     foundable/coastal land by a human colony (units_unload_passenger),
     *     up to min(moves_left, capacity) this beat (1 MP per pax); same-beat
     *     seize/fortify passengers that were skipped while aboard.
     *     Partial unload (cargo left) → hold; leftover sails next beat.
     *     Full unload + moves left → AI_SAIL toward *next* human coast
     *     (skip the port just served). Else (not at coast) AI_SAIL→coast.
     *     After that coast/next-coast sail step: if still carrying and now
     *     adjacent to a human colony → prefer unload same beat (then, if the
     *     hold empties with moves left, retarget next human coast).
     *   cargo == 0 (idle empty) → AI_SAIL coastal patrol toward water adjacent
     *     to nearest human coastal colony. Redirects existing ships only —
     *     do not invent new MoW. Hold fill is units_ship_capacity (MoW×6);
     *     embark UI chrome PARKED; 160a letter cinematic Done (core/declaration.c).
     */
    if (ai_king_is_mow(ctx->units, u)) {
      int unloaded = 0;
      int land_x = 0;
      int land_y = 0;
      if (u->cargo_count > 0) {
        unloaded = ai_king_mow_try_unload(ctx, u, human, &land_x, &land_y);
        if (unloaded > 0) {
          ai_king_mow_post_unload_land(ctx, crown, human, land_x, land_y);
        }
        if (u->cargo_count > 0) {
          /* Partial unload or unload failed mid-hold — leftover next beat. */
          if (unloaded > 0) {
            continue;
          }
          /* Not at coast — fall through to AI_SAIL toward human coast. */
        } else if (u->moves_left <= 0) {
          /* Full unload spent remaining MP — no same-beat sail. */
          continue;
        }
        /* Full unload with moves left → sail to next human coast below. */
      }
      int wx = 0;
      int wy = 0;
      /* After full unload, prefer another human port's water (next coast). */
      const int skip_adj = (unloaded > 0 && u->cargo_count == 0) ? 1 : 0;
      if (ai_king_human_coast_water(ctx, human, u->x, u->y, &wx, &wy, skip_adj)) {
        u->orders = UNITS_ORDER_AI_SAIL;
        u->goto_x = wx;
        u->goto_y = wy;
      }
      /*
       * Already on next-coast water and still carrying: prefer unload at the
       * adjacent colony over burning MP on a zero-step sail (R5). Gate on
       * human colony adjacency — not soft coastal land alone — so mid-route
       * sail steps do not dump onto foundable tiles one MD early.
       */
      if (u->cargo_count > 0 && u->moves_left > 0 && u->goto_x == u->x &&
          u->goto_y == u->y &&
          ai_king_adjacent_human_colony(ctx, human, u->x, u->y)) {
        land_x = 0;
        land_y = 0;
        const int u_here = ai_king_mow_try_unload(ctx, u, human, &land_x, &land_y);
        if (u_here > 0) {
          ai_king_mow_post_unload_land(ctx, crown, human, land_x, land_y);
          unloaded += u_here;
          if (u->cargo_count == 0 && u->moves_left > 0) {
            if (ai_king_human_coast_water(ctx, human, u->x, u->y, &wx, &wy, 1)) {
              u->orders = UNITS_ORDER_AI_SAIL;
              u->goto_x = wx;
              u->goto_y = wy;
            }
          } else {
            continue;
          }
        }
      }
      if (u->moves_left <= 0) {
        continue;
      }
      int tx = u->goto_x;
      int ty = u->goto_y;
      if (tx < 0 || ty < 0 || tx >= 255 || ty >= 255) {
        continue;
      }
      const int sdx = (tx > u->x) - (tx < u->x);
      const int sdy = (ty > u->y) - (ty < u->y);
      const int nx = u->x + sdx;
      const int ny = u->y + sdy;
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe >= 0) {
        const ColonizeUnit* f = units_get_const(ctx->units, foe);
        if (f && f->nation_id == human && units_is_sea(ctx->units, foe)) {
          units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
        }
        continue;
      }
      if ((sdx != 0 || sdy != 0) && map_tile_is_water(ctx->map, nx, ny)) {
        units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
      }
      /*
       * After coast / next-coast sail step: if still carrying and now adjacent
       * to a human colony tile, prefer unload same beat (fandom man-o-war →
       * ports). Soft-coast-only adjacency must not trigger mid-route.
       */
      if (u->cargo_count > 0 && u->moves_left > 0 &&
          ai_king_adjacent_human_colony(ctx, human, u->x, u->y)) {
        land_x = 0;
        land_y = 0;
        const int u_after = ai_king_mow_try_unload(ctx, u, human, &land_x, &land_y);
        if (u_after > 0) {
          ai_king_mow_post_unload_land(ctx, crown, human, land_x, land_y);
          if (u->cargo_count == 0 && u->moves_left > 0) {
            if (ai_king_human_coast_water(ctx, human, u->x, u->y, &wx, &wy, 1)) {
              u->orders = UNITS_ORDER_AI_SAIL;
              u->goto_x = wx;
              u->goto_y = wy;
            }
          }
        }
      }
      continue;
    }

    /*
     * REF idle garrison (heal/fortify stand-in): Regular, or Dragoon/Cont. Cav
     * when no Regular, on own (crown) colony — including a captured human
     * capital — with no adjacent human foe/colony → fortify **one** when the
     * stack rule allows (cap 2). Already FORTIFY/FORTIFIED: stay (do not wake).
     * When two crown garrison units are already FORTIFY/FORTIFIED on the tile,
     * leave this unit free to hunt. Second slot needs moves_left > 0.
     * Cite: Colonization.pdf Defending a Colony ("fortify soldiers, dragoons…");
     * king_ref thin multi-garrison; units_order_fortify. Deep siege PARKED.
     */
    if ((ai_king_is_regular(ctx->units, u) ||
         ai_king_is_garrison_cavalry(ctx->units, u)) &&
        ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        const ColonizeColony* c = &ctx->colonies->colonies[cid];
        if (c->active && c->nation_id == crown &&
            !ai_king_adjacent_human_unit(ctx, human, u->x, u->y) &&
            !ai_king_adjacent_human_colony(ctx, human, u->x, u->y)) {
          /* Prefer stay garrisoned once stack slot is taken by this unit. */
          if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
            continue;
          }
          if (ai_king_tile_fortified_garrison_count(ctx, crown, u->x, u->y, u->id) < 2) {
            /* Prefer Regular over Dragoon/Cont.Cav on same tile (cap 2). */
            ai_king_fortify_garrison_at(ctx, u, crown, u->x, u->y);
            if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
              continue;
            }
            /* Regular claimed the slot (or none eligible) — fall through to hunt. */
          }
          /* Extra on garrisoned colony — fall through to hunt. */
        }
      }
    }

    /*
     * Artillery idle fortify (Euro after-siege pattern): Artillery on own
     * (crown) colony — including newly captured — with no adjacent human
     * foe/colony → FORTIFY and hold. Already FORTIFY/FORTIFIED stay put.
     * Cite: euro_unit_act Artillery fortify after siege; Colonization.pdf
     * fortify defense / Artillery; case 0x0b fortify arm. Off-colony Artillery
     * still hunts fortified ports (thin siege bias below).
     */
    if (ai_king_is_artillery(ctx->units, u) && ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        const ColonizeColony* c = &ctx->colonies->colonies[cid];
        if (c->active && c->nation_id == crown &&
            !ai_king_adjacent_human_unit(ctx, human, u->x, u->y) &&
            !ai_king_adjacent_human_colony(ctx, human, u->x, u->y)) {
          if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
            continue;
          }
          (void)units_order_fortify(ctx->units, u->id);
          continue;
        }
      }
    }

    /*
     * REF land hunt (fandom REF AI; conceptual reuse of ai_euro land hunt):
     * idle Regular/Dragoon/Artillery/Cont. with moves → AI_MOVE toward nearest
     * human colony or human land unit. Artillery prefers fortified colonies when
     * the Artillery type exists in pool. Dragoon / Cont. Cav prefer open land /
     * unfortified colonies when Artillery exists (thin role split; else nearest).
     * Capital MD bias: founding capital preferred over a nearer distant colony
     * when MD within AI_KING_CAPITAL_MD_SLACK (idle hunters). Prefer adjacent
     * uncaptured colony over marching past (Artillery: adjacent unfortified must
     * not override a fortified hunt). Multi-step siege/hunt drains moves_left
 * (combat/step/capture). Deeper DOS combat×8 scoring PARKED.
     *
     * After-capture extras: standing on a crown colony whose two fortify slots
     * are already taken (two Regular/Dragoon/Cont.Cav FORTIFY/FORTIFIED) →
     * prefer next nearest remaining human colony (strict MD; no capital slack /
     * no closer unit bait). Source: fandom REF AI uncaptured-colony pressure;
     * Colonization.pdf Defending a Colony; king_ref thin multi-garrison (cap 2).
     */
    if (ai_king_is_ref_land_hunter(ctx->units, u)) {
      const int have_arty = (ai_king_artillery_type(ctx->units) >= 0) ? 1 : 0;
      const int prefer_fort =
          (have_arty && ai_king_is_artillery(ctx->units, u)) ? 1 : 0;
      const int prefer_open =
          (have_arty && ai_king_prefers_open_land(ctx->units, u)) ? 1 : 0;
      /*
       * Extra on garrisoned crown colony (post-capture / stack): next colony.
       * Captured founding capital is no longer human — do not re-apply capital
       * MD slack to the next-lowest colony id.
       */
      int after_capture_next = 0;
      if (ctx->colonies) {
        const int own_cid = colonies_id_at(ctx->colonies, u->x, u->y);
        if (own_cid >= 0) {
          const ColonizeColony* own = &ctx->colonies->colonies[own_cid];
          if (own->active && own->nation_id == crown &&
              ai_king_tile_fortified_garrison_count(ctx, crown, u->x, u->y, u->id) >= 2) {
            after_capture_next = 1;
          }
        }
      }
      int hx = 0;
      int hy = 0;
      int have_hunt = 0;
      if (after_capture_next) {
        /* Prefer next uncaptured colony; if none left, fall back to unit hunt. */
        have_hunt = ai_king_nearest_human_colony(ctx, human, u->x, u->y, &hx, &hy);
        if (!have_hunt) {
          have_hunt =
              ai_king_ref_hunt_target(ctx, human, u->x, u->y, &hx, &hy, prefer_fort,
                                      prefer_open);
        }
      } else {
        have_hunt =
            ai_king_ref_hunt_target(ctx, human, u->x, u->y, &hx, &hy, prefer_fort,
                                    prefer_open);
      }
      /*
       * Adjacent human colony normally wins over a farther hunt (fandom: attack
       * adjacent uncaptured colony rather than march past). Artillery siege
       * tighten: adjacent fortified wins; do not override a fortified hunt
       * target with an unfortified adjacent colony (leave open ports to
       * Dragoon/Regular). Deep multi-step siege scoring PARKED.
       */
      if (ctx->colonies) {
        static const int adx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int ady[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        int adj_fort_x = -1;
        int adj_fort_y = -1;
        int adj_any_x = -1;
        int adj_any_y = -1;
        for (int d = 0; d < 8; ++d) {
          const int cx = u->x + adx[d];
          const int cy = u->y + ady[d];
          const int cid = colonies_id_at(ctx->colonies, cx, cy);
          if (cid < 0) {
            continue;
          }
          const ColonizeColony* c = &ctx->colonies->colonies[cid];
          if (!c->active || c->nation_id != human) {
            continue;
          }
          if (adj_any_x < 0) {
            adj_any_x = cx;
            adj_any_y = cy;
          }
          if (colonies_has_fortification(ctx->colonies, c)) {
            adj_fort_x = cx;
            adj_fort_y = cy;
            break;
          }
        }
        if (prefer_fort) {
          if (adj_fort_x >= 0) {
            hx = adj_fort_x;
            hy = adj_fort_y;
            have_hunt = 1;
          } else {
            int hunt_is_fort = 0;
            if (have_hunt) {
              const int hid = colonies_id_at(ctx->colonies, hx, hy);
              if (hid >= 0) {
                const ColonizeColony* hc = &ctx->colonies->colonies[hid];
                if (hc->active && hc->nation_id == human &&
                    colonies_has_fortification(ctx->colonies, hc)) {
                  hunt_is_fort = 1;
                }
              }
            }
            /* No fortified hunt — any adjacent colony may override (fallback). */
            if (!hunt_is_fort && adj_any_x >= 0) {
              hx = adj_any_x;
              hy = adj_any_y;
              have_hunt = 1;
            }
          }
        } else if (adj_any_x >= 0) {
          hx = adj_any_x;
          hy = adj_any_y;
          have_hunt = 1;
        }
      }
      if (have_hunt) {
        u->orders = UNITS_ORDER_AI_MOVE;
        u->goto_x = hx;
        u->goto_y = hy;
      } else if (u->goto_x < 0 || u->goto_y < 0 || u->goto_x >= 255 || u->goto_y >= 255) {
        if (ai_king_weakest_port(ctx, human, &hx, &hy) >= 0) {
          u->orders = UNITS_ORDER_AI_MOVE;
          u->goto_x = hx;
          u->goto_y = hy;
        }
      }
    } else if (u->goto_x < 0 || u->goto_y < 0 || u->goto_x >= 255 || u->goto_y >= 255) {
      int tx = 0;
      int ty = 0;
      if (ai_king_weakest_port(ctx, human, &tx, &ty) < 0) {
        continue;
      }
      u->goto_x = tx;
      u->goto_y = ty;
    }

    /*
     * Multi-step siege / hunt (10f0 deepen): drain moves_left toward goto —
     * combat if human on next tile, else step; capture on colony enter.
     * Cap steps so a failed spend cannot spin. Cite: king_ref multi-step;
     * ai_euro land_try_adjacent_attack mirror. Full DOS siege scoring PARKED.
     */
    for (int step = 0; step < 8 && u->active && u->moves_left > 0; ++step) {
      int tx = u->goto_x;
      int ty = u->goto_y;
      if (tx < 0 || ty < 0 || tx >= 255 || ty >= 255) {
        break;
      }
      if (u->x == tx && u->y == ty) {
        /* On the target tile: a human colony still holding a defender is
         * fought from the tile itself (units_try_move already let the
         * winner in beside a demoted loser); capture once none is left. */
        const int def = ai_king_human_defender_at(ctx, human, u->x, u->y);
        if (def >= 0 && !units_is_sea(ctx->units, u->id)) {
          const int ml0 = u->moves_left;
          (void)units_resolve_land_combat(ctx->units, u->id, def, ctx->rng);
          if (!u->active) {
            break;
          }
          if (u->moves_left >= ml0) {
            u->moves_left = 0; /* attack ends the beat either way */
          }
        }
        ai_king_try_capture_at(ctx, u, crown, human);
        break;
      }
      /*
       * Greedy detour: straight step first, then the neighbours that do not
       * lose ground toward goto (a coast inlet / mountain / own-blocked
       * tile in the straight line froze whole REF columns for the rest of
       * the war in the 2026-08-28 headless run — no pathfinder here).
       */
      static const int ddx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int ddy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      const int cur_md = ai_king_md(u->x, u->y, tx, ty);
      int order[8];
      int n_order = 0;
      {
        const int sdx = (tx > u->x) - (tx < u->x);
        const int sdy = (ty > u->y) - (ty < u->y);
        for (int d = 0; d < 8; ++d) {
          if (ddx[d] == sdx && ddy[d] == sdy) {
            order[n_order++] = d;
          }
        }
        for (int d = 0; d < 8; ++d) {
          if (ddx[d] == sdx && ddy[d] == sdy) {
            continue;
          }
          if (ai_king_md(u->x + ddx[d], u->y + ddy[d], tx, ty) <= cur_md) {
            order[n_order++] = d;
          }
        }
      }
      const int ml0 = u->moves_left;
      int advanced = 0;
      for (int oi = 0; oi < n_order && u->active && u->moves_left > 0; ++oi) {
        const int nx = u->x + ddx[order[oi]];
        const int ny = u->y + ddy[order[oi]];
        /* Foreign occupant only — an own stack (REF column, crown wagon train
         * visiting the port) is not a blocker; units_try_move stacks onto it.
         * units_id_at here froze whole REF columns behind their own lead unit
         * for 28 turns in the 2026-08-28 headless WoI run. */
        int foe = units_foreign_unit_at(ctx->units, nx, ny, u->id, u->nation_id);
        if (foe >= 0 && ctx->colonies) {
          /*
           * A human colony is fought through its armed defender; visiting
           * third-nation units (a rival's Privateer/Artillery in a Dutch port
           * on the real 2026-08-28 fixture) are not defenders and must not
           * freeze the column — an undefended port is simply entered and
           * captured (DOS 0512 seizes whatever stands there).
           */
          const int hcid = colonies_id_at(ctx->colonies, nx, ny);
          const ColonizeColony* hc = hcid >= 0 ? &ctx->colonies->colonies[hcid] : NULL;
          if (hc && hc->active && hc->nation_id == human && !units_is_sea(ctx->units, u->id)) {
            const int def = ai_king_human_defender_at(ctx, human, nx, ny);
            if (def >= 0) {
              foe = def;
            } else {
              {
                const int step_ox = u->x;
                const int step_oy = u->y;
                u->x = nx;
                u->y = ny;
                units_occupancy_notify_moved(ctx->units, step_ox, step_oy, nx, ny);
              }
              u->moves_left = 0;
              ai_king_after_step_onto_colony(ctx, u, crown, human);
              advanced = 1;
              break;
            }
          }
        }
        if (foe >= 0) {
          const ColonizeUnit* f = units_get_const(ctx->units, foe);
          if (!f || f->nation_id != human) {
            continue; /* non-human stack: sidestep */
          }
          if (oi > 0 && ai_king_md(nx, ny, tx, ty) >= cur_md) {
            continue; /* only fight sideways when it is the straight line */
          }
          if (units_is_sea(ctx->units, u->id)) {
            units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
          } else if (units_resolve_land_combat(ctx->units, u->id, foe, ctx->rng)) {
            units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
            ai_king_after_step_onto_colony(ctx, u, crown, human);
          }
          advanced = 1;
          break;
        }
        if (units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng)) {
          ai_king_after_step_onto_colony(ctx, u, crown, human);
          advanced = 1;
          break;
        }
      }
      if (!advanced || !u->active || u->moves_left >= ml0) {
        break;
      }
    }
  }
}

/*
 * Revolution end (fandom Independence / manual 1800–1850):
 *   Lose: WoI + zero coastal human ports.
 *   Win: WoI + year≥1850 + no crown REF units on map.
 * Latches unknown46[4]; score reads won/lost. Cite: docs/fandom_col1994.md.
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

static int ai_king_crown_units_alive(const ColonizeTurnContext* ctx, int crown) {
  if (!ctx || !ctx->units || crown < 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (u->active && u->nation_id == crown) {
      ++n;
    }
  }
  return n;
}

/* Richest human colony name by population (estate stand-in for @RETIRING2). */
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
    if ((int)c->population > best_pop) {
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
 * Crown share of (human+crown) colony population during WoI.
 * Used for @WARN3 / @LOSING3. Returns 0 if no counted population.
 */
static int ai_king_woi_pop_share_pct(
  const ColonizeTurnContext* ctx,
  int human,
  int crown
) {
  if (!ctx || human < 0 || crown < 0) {
    return 0;
  }
  int human_pop = 0;
  int crown_pop = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active) {
        continue;
      }
      const int pop = c->population > 0 ? (int)c->population : 1;
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
      const int pop = c->population > 0 ? (int)c->population : 1;
      if ((int)c->nation_id == human) {
        human_pop += pop;
      } else if ((int)c->nation_id == crown) {
        crown_pop += pop;
      }
    }
  }
  const int total = human_pop + crown_pop;
  if (total <= 0) {
    return 0;
  }
  return (crown_pop * 100) / total;
}

static void ai_king_check_revolution_end(ColonizeTurnContext* ctx, int ref_already) {
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
  const int crown = ai_king_crown_nation(human);
  const int ports = ai_king_human_coastal_ports(ctx, human);
  const ColonizeCol1Player* pl = &ctx->col1->player[human];
  const char* country =
    (pl->country_name[0] != '\0') ? pl->country_name : "the colonies";
  const char* leader = (pl->name[0] != '\0') ? pl->name : "Your Excellency";
  /* Reclaiming ports clears the mid-war warn episode so a later drop to one can re-fire. */
  if (ports > 1) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN1_BYTE, 0);
  }
  /*
   * Mid-war warn: exactly one coastal port left while REF already invading.
   * GAME.TXT @WARN1. Once per episode (unknown46[6]); does not latch endgame.
   */
  if (ports == 1 && ref_already &&
      ai_king_latch_get(ctx->col1, AI_KING_WARN1_BYTE) == 0) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.has_number0 = true;
    tok.number0 = 1;
    tok.string0 = country;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "Your Excellency, the King's forces control all but 1 of the ports in %s!  "
      "If we don't retain control of at least one port our commerce will be "
      "choked and we will have to surrender!",
      country
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "WARN1", &tok, fallback, body, sizeof(body));
    /*
     * Do not clobber same-turn wave/war_act status (1528 @INVASION, 2244 merc).
     * Dedicated warn chrome still enqueues INFO OK; status when buffer empty.
     */
    if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, 1, "Port Warning", body
      );
    }
    ai_king_latch_set(ctx->col1, AI_KING_WARN1_BYTE, 1);
  }
  const int colonies = ai_king_human_colonies(ctx, human);
  /* Reclaiming colonies clears the mid-war colony-warn episode. */
  if (colonies > 1) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN2_BYTE, 0);
  }
  /*
   * Mid-war warn: exactly one colony left while REF already invading.
   * GAME.TXT @WARN2 (%NUMBER1). Once per episode (unknown46[7]); no endgame latch.
   */
  if (colonies == 1 && ref_already &&
      ai_king_latch_get(ctx->col1, AI_KING_WARN2_BYTE) == 0) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.has_number1 = true;
    tok.number1 = 1;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "Your Excellency, the King's forces control all but 1 of our colonies!  "
      "We need to protect our remaining colonies, or we will lose the war!"
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "WARN2", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, 1, "Colony Warning", body
      );
    }
    ai_king_latch_set(ctx->col1, AI_KING_WARN2_BYTE, 1);
  }
  const int pop_pct = ai_king_woi_pop_share_pct(ctx, human, crown);
  /* Reclaiming population share clears the mid-war pop-warn episode. */
  if (pop_pct < AI_KING_WARN3_PCT_MIN) {
    ai_king_latch_set(ctx->col1, AI_KING_WARN3_BYTE, 0);
  }
  /*
   * Mid-war warn: crown controls 50–89% of human+crown colony population.
   * GAME.TXT @WARN3 (%NUMBER2). Once per episode (unknown46[10]).
   */
  if (ref_already && pop_pct >= AI_KING_WARN3_PCT_MIN &&
      pop_pct < AI_KING_LOSING3_PCT &&
      ai_king_latch_get(ctx->col1, AI_KING_WARN3_BYTE) == 0) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.has_number2 = true;
    tok.number2 = pop_pct;
    tok.string0 = country;
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "Your Excellency, the King's forces control %d%% of the %s population.  "
      "If he ever controls 90%%, the Continental Congress will be unable to "
      "continue the war and we will have to surrender!",
      pop_pct,
      country
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "WARN3", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, pop_pct, "Population Warning",
        body
      );
    }
    ai_king_latch_set(ctx->col1, AI_KING_WARN3_BYTE, 1);
  }
  /*
   * Lose: REF already invading (end_checks_armed). Prefer @LOSING2 when no
   * colonies remain; else @LOSING1 when coastal ports are gone but inland
   * colonies may still exist. Cite: docs/fandom_col1994.md Independence.
   */
  if (colonies <= 0 && ref_already) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = country;
    tok.string1 = leader;
    tok.string2 = "Europe";
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "King's Forces control all colonies in %s! Continental Congress capitulates. "
      "%s, stripped of titles, escapes to exile in Europe.",
      country,
      leader
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "LOSING2", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, 2, "Revolution Failed", body
      );
    }
    return;
  }
  if (ports <= 0 && ref_already) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = country;
    tok.string1 = leader;
    tok.string2 = "Europe";
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "King's Forces control all ports in %s! Continental Congress capitulates. "
      "%s, stripped of titles, escapes to exile in Europe.",
      country,
      leader
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "LOSING1", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, 2, "Revolution Failed", body
      );
    }
    return;
  }
  /*
   * Lose: crown controls ≥90% of human+crown colony population.
   * GAME.TXT @LOSING3.
   */
  if (pop_pct >= AI_KING_LOSING3_PCT && ref_already) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = country;
    tok.string1 = leader;
    tok.string2 = "Europe";
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "King's Forces control over 90%% of %s population! Continental Congress "
      "capitulates. %s, stripped of titles, escapes to exile in Europe.",
      country,
      leader
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "LOSING3", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, 2, "Revolution Failed", body
      );
    }
    return;
  }
  const int year = (int)ctx->col1->head.year;
  if (year >= AI_KING_YEAR_CAP && ai_king_crown_units_alive(ctx, crown) <= 0) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_WON);
    ai_king_latch_set(ctx->col1, AI_KING_REF_PRESENT_BYTE, 0);
    ctx->col1->head.game_options.ref_present = 0;
    /* GAME.TXT @WINNING — STRING0 leader, STRING1 country. */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = leader;
    tok.string1 =
      (pl->country_name[0] != '\0') ? pl->country_name : "the United Colonies";
    char fallback[AI_POPUP_BODY_LEN];
    snprintf(
      fallback,
      sizeof(fallback),
      "Royal Expeditionary Force annihilated! General %s accepts surrender of "
      "all Tory forces. Parliament accepts independence of %s. Continental "
      "Congress proclaims %s the first President of the new republic!",
      leader,
      tok.string1,
      leader
    );
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "WINNING", &tok, fallback, body, sizeof(body));
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, 1, "Independence", body
      );
    }
    return;
  }
  /*
   * Wartime calendar end (year_end_chrome 0x73a): year≥1850 with crown still
   * alive → Congress sues for peace. GAME.TXT @RETIRING2.
   */
  if (year >= AI_KING_YEAR_CAP && ai_king_crown_units_alive(ctx, crown) > 0) {
    ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    ai_king_latch_set(ctx->col1, AI_KING_REF_PRESENT_BYTE, 0);
    ctx->col1->head.game_options.ref_present = 0;
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
      "War-weary Continental Congress sues for peace!  King accepts surrender "
      "from Viceroy %s, who retires to country estate near %s.",
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
        AI_POPUP_TAG_INFO,
        human,
        crown,
        2,
        "Congress Sues for Peace",
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
   * Arm lose/@WARN1/@WARN2 only when WoI + REF were already set at turn entry.
   * Declare same-turn seeds both; peacetime tax can set REF-present early
   * via pool growth — so REF alone must not arm end checks (keeps 1528
   * @INVASION status on the declare beat).
   */
  const int end_checks_armed =
    ctx->col1_ok && ctx->col1 &&
    ai_king_independence_declared(ctx->col1) &&
    ai_king_latch_get(ctx->col1, AI_KING_REF_PRESENT_BYTE) != 0;
  /* External boycott clear (Fugger/diplo) → drop refuse even mid-war / off-tax years. */
  if (ctx->col1_ok && ctx->col1) {
    ai_king_sync_boycott_refuse(ctx->col1, ctx->human_nation);
  }
  const int sol = ai_king_sol_percent(ctx, ctx->human_nation);

  if (!ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
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
      tok.string0 = "Viceroy";
      tok.string1 = leader;
      char fallback[AI_POPUP_BODY_LEN];
      snprintf(
        fallback,
        sizeof(fallback),
        "Viceroy %s plans to retire in 1800!  A rumor circulates that he would "
        "postpone his retirement were a War of Independence to begin.",
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
          ai_king_crown_nation(human),
          AI_KING_SOONRETIRE0_YEAR,
          "Retirement Rumors",
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
          ai_king_crown_nation(ctx->human_nation),
          AI_KING_PEACE_YEAR_CAP,
          "Scoring Complete",
          body,
          labels,
          ids,
          2
        );
      }
    }
    /*
     * FUN_43f7_2424 tail: decile SoL notify (DS:0x53d8 dedup). Status-only;
     * full 0x1362/0x1358/0x136a popup chrome PARKED.
     */
    if (ctx->col1_ok && ctx->col1 && ctx->status && ctx->status_size &&
        ctx->human_nation >= 0 && ctx->human_nation < 4 &&
        ctx->col1->nation[ctx->human_nation].founding_father_count > 3 &&
        ctx->status[0] == '\0') {
      const int decile = sol / 10;
      const int last = (int)ctx->col1->head.sol_pct_last_notified;
      if (decile != last) {
        if (decile > last) {
          snprintf(
            ctx->status,
            ctx->status_size,
            "Congress notes rising Sons of Liberty (%d%%).",
            sol
          );
        } else {
          snprintf(
            ctx->status,
            ctx->status_size,
            "Congress notes falling Sons of Liberty (%d%%).",
            sol
          );
        }
        ctx->col1->head.sol_pct_last_notified = (int16_t)decile;
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
          ai_king_crown_nation(human),
          AI_KING_SOONRETIRE1_YEAR,
          "War Weariness",
          body
        );
      }
      ai_king_latch_set(ctx->col1, AI_KING_SOONRETIRE1_BYTE, 1);
    }
    ai_king_ref_wave(ctx);
    ai_king_war_act(ctx);
    /* Lose/@WARN1 only when WoI+REF already armed at turn entry. */
    ai_king_check_revolution_end(ctx, end_checks_armed);
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
       * FUN_38fd_3dc8 village-goods choice: the hike is already applied
       * (ai_king_tax_event); this only decides whether it's reverted.
       * Accept ("kiss the ring") → keep it, status only. Refuse
       * ("tea party") → ai_king_tax_teaparty reverts the delta packed in
       * the payload and boycotts the picked cargo.
       */
      if (popup->result_choice_id == AI_KING_CHOICE_ACCEPT) {
        if (ctx->status && ctx->status_size && ctx->col1_ok && ctx->col1 &&
            human >= 0 && human < 4) {
          snprintf(ctx->status, ctx->status_size,
                   "Audience: the tax increase to %u%% stands.",
                   ctx->col1->nation[human].tax_rate);
        }
      } else if (popup->result_choice_id == AI_KING_CHOICE_REFUSE) {
        int applied = 0;
        int cargo = -1;
        ai_king_teaparty_payload_parts(popup->result_payload, &applied, &cargo);
        ai_king_tax_teaparty(ctx, human, applied, cargo);
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
         * Auto-declare gets these from ai_king_nation_turn's WoI block; popup
         * Confirm applies outside that turn slice.
         */
        ai_king_ref_wave(ctx);
        ai_king_war_act(ctx);
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
            ai_king_crown_nation(human),
            0,
            "Retirement",
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
