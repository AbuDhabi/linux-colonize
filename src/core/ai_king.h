#ifndef COLONIZE_AI_KING_H
#define COLONIZE_AI_KING_H

#include <stdint.h>

#include "core/dos_rng.h"
#include "core/turn.h"

/*
 * King / tax / REF / independence — partial structural port of FUN_43f7_*.
 * Thin map: original_sources_annotated/ai/king_ref.md
 * Replaces turn_run_king_stub body.
 */


/* King latch ids (storage: real 0x5382 bits for WoI / REF-present; the
 * human nation's DOS-dead unknown23_pad[] for the port-only latches —
 * see ai_king_latch_get in ai_king.c). */
#define AI_KING_WOI_BYTE 0
#define AI_KING_REF_PRESENT_BYTE 1
#define AI_KING_BOYCOTT_BYTE 2
#define AI_KING_MERC_HIRED_BYTE 3
/* Endgame latch: 0 none, 1 revolution won, 2 revolution lost (was rename-reserved). */
#define AI_KING_ENDGAME_BYTE 4
#define AI_KING_ENDGAME_NONE 0
#define AI_KING_ENDGAME_WON 1
#define AI_KING_ENDGAME_LOST 2
#define AI_KING_ENDGAME_PEACE_1800 3
#define AI_KING_CONGRESS_BYTE 5
/* Mid-war @WARN1 once-per-episode (ports<3, DOS 3844_0442); clear at >=3. */
#define AI_KING_WARN1_BYTE 6
/* Mid-war @WARN2 once-per-episode (colonies<3, DOS); clear at >=3. */
#define AI_KING_WARN2_BYTE 7
/* Peacetime Spring 1790 @SOONRETIRING0 once. */
#define AI_KING_SOONRETIRE0_BYTE 8
/* Wartime 1840 @SOONRETIRING1 once. */
#define AI_KING_SOONRETIRE1_BYTE 9
/* Mid-war @WARN3 once-per-episode (crown pop share 80–89%, DOS); clear when <80%. */
#define AI_KING_WARN3_BYTE 10
/* bugs.md: the first REF wave waits one full turn after the declaration —
 * set to 1 on declare, consumed (cleared, no landing) by the first wave
 * tick. */
#define AI_KING_REF_WAVE_WAIT_BYTE 11
/* bugs.md 258: @INTERVENTION "declares war" announcement fires once (DOS
 * FUN_43f7_1528 latches 0x5382 bit2 after showing it; the port's ref_present
 * bit gets cleared when no crown unit remains, so it keeps its own bit). */
#define AI_KING_INTERVENE_ANNOUNCED_BYTE 12


/*
 * King latch storage (2026-08-28). These used to live in head.unknown46[],
 * which is really DOS price_group_state[16] (the Europe market pool words,
 * rewritten every EOT by europe_tick_market_prices) — on any real DOS save
 * byte 4 held live market data, so the endgame latch read as "already
 * ended" and every WoI end-check bailed. WoI / REF-present use their real
 * 0x5382 bits; the port-only latches pack into the human nation's
 * unknown23_pad[3] (nation+0x1b..+0x1d), confirmed never touched by DOS.
 */
static inline uint8_t* ai_king_latch_pad(ColonizeCol1Save* col1) {
  int n = (int)col1->head.human_player;
  if (n < 0 || n >= (int)COLONIZE_COL1_NATION_COUNT) {
    n = 0;
  }
  return col1->nation[n].unknown23_pad;
}

static inline int ai_king_latch_bit(int which) {
  switch (which) {
    case AI_KING_BOYCOTT_BYTE: return 0x01;
    case AI_KING_MERC_HIRED_BYTE: return 0x02;
    case AI_KING_CONGRESS_BYTE: return 0x04;
    case AI_KING_WARN1_BYTE: return 0x08;
    case AI_KING_WARN2_BYTE: return 0x10;
    case AI_KING_WARN3_BYTE: return 0x20;
    case AI_KING_SOONRETIRE0_BYTE: return 0x40;
    case AI_KING_SOONRETIRE1_BYTE: return 0x80;
    default: return 0;
  }
}

static inline int ai_king_latch_get(const ColonizeCol1Save* col1, int which) {
  if (!col1) {
    return 0;
  }
  if (which == AI_KING_WOI_BYTE) {
    return col1->head.game_options.woi ? 1 : 0;
  }
  if (which == AI_KING_REF_PRESENT_BYTE) {
    return col1->head.game_options.ref_present ? 1 : 0;
  }
  const uint8_t* pad = ai_king_latch_pad((ColonizeCol1Save*)col1);
  if (which == AI_KING_ENDGAME_BYTE) {
    return pad[0] & 0x03;
  }
  if (which == AI_KING_REF_WAVE_WAIT_BYTE) {
    return (pad[0] & 0x04) ? 1 : 0;
  }
  if (which == AI_KING_INTERVENE_ANNOUNCED_BYTE) {
    return (pad[0] & 0x08) ? 1 : 0;
  }
  const int bit = ai_king_latch_bit(which);
  return (bit && (pad[1] & bit)) ? 1 : 0;
}

static inline void ai_king_latch_set(ColonizeCol1Save* col1, int which, int value) {
  if (!col1) {
    return;
  }
  if (which == AI_KING_WOI_BYTE) {
    col1->head.game_options.woi = value ? 1 : 0;
    return;
  }
  if (which == AI_KING_REF_PRESENT_BYTE) {
    col1->head.game_options.ref_present = value ? 1 : 0;
    return;
  }
  uint8_t* pad = ai_king_latch_pad(col1);
  if (which == AI_KING_REF_WAVE_WAIT_BYTE) {
    pad[0] = (uint8_t)(value ? (pad[0] | 0x04) : (pad[0] & ~0x04));
    return;
  }
  if (which == AI_KING_INTERVENE_ANNOUNCED_BYTE) {
    pad[0] = (uint8_t)(value ? (pad[0] | 0x08) : (pad[0] & ~0x08));
    return;
  }
  if (which == AI_KING_ENDGAME_BYTE) {
    pad[0] = (uint8_t)((pad[0] & ~0x03) | (value & 0x03));
    return;
  }
  const int bit = ai_king_latch_bit(which);
  if (!bit) {
    return;
  }
  if (value) {
    pad[1] |= (uint8_t)bit;
  } else {
    pad[1] &= (uint8_t)~bit;
  }
}

/* Clear WoI / REF-present / every port latch (tests, new game). */
static inline void ai_king_latch_clear(ColonizeCol1Save* col1) {
  if (!col1) {
    return;
  }
  col1->head.game_options.woi = 0;
  col1->head.game_options.ref_present = 0;
  uint8_t* pad = ai_king_latch_pad(col1);
  pad[0] = 0;
  pad[1] = 0;
}

void ai_king_nation_turn(ColonizeTurnContext* ctx);

/*
 * FUN_43f7_2244 — peacetime AI-nation-only self/ally-funded troop gift.
 * Called once per AI-controlled Euro nation's own turn (never the human —
 * see ai_king.c's header comment on the function body for the full
 * derivation). No-op post-WoI.
 */
void ai_king_ai_peacetime_gift(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_3844_00f2 tail — @KINGFRIGATE. Every 8th peacetime turn, a nation
 * with no Frigate whose colonies are harassed by foreign warships is
 * offered one (human: Yes/No CHOICE, +10% tax on Yes; AI: auto-accept).
 * Call once per Euro nation EOT.
 */
void ai_king_frigate_offer(ColonizeTurnContext* ctx, int nation);

/*
 * FUN_38fd_5930 — @KINGNEWWAR (static port 2026-08-27). Human nation only,
 * runs in the Europe-EOT king slot when the tax event did not fire: with
 * every met peer at peace, none met-but-unpeaced, and our land strength >=
 * theirs, a difficulty-gated roll picks a random peace peer; the Crown grants
 * gold + N Veteran Soldiers (both scaled by the field-combat gap, capped by
 * difficulty), clears PEACE both ways and sets AI_DIPLO_CROWN_ARMED (0x10).
 * Franklin (FF 19) suppresses it. Returns 1 when it fired.
 */
int ai_king_new_war_event(ColonizeTurnContext* ctx);

/*
 * Apply human choice from map AI popup:
 *   KING_AUDIENCE Accept/Refuse (38fd_5be8; Refuse → @TEAPARTY OK /
 *     KING_DUMP_GOODS then @TEAPARTY),
 *   KING_MERC Hire/Decline (2244; Hire → success follow-up OK;
 *     Decline → declined follow-up OK),
 *   KING_CONGRESS Confirm (2564/1a26 → rename + WoI-begins OK chain).
 * No-op if tag mismatch / cancelled.
 */
void ai_king_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup);

/* FUN_43f7_0004: pop-weighted SoL percent for a European nation (0..100). */
int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id);

/* True once WoI is declared (head.game_options.woi). */
int ai_king_independence_declared(const ColonizeCol1Save* col1);

/*
 * MENU.TXT @GAME "DECLARE INDEPENDENCE" command (MAP_MENU_ACTION_
 * DECLARE_INDEPENDENCE). Below AI_KING_DECLARE_SOL_MIN: GAME.TXT @TOOTORY OK
 * notice. At/above threshold and not yet at war: same Never/Yes @DECLARE
 * confirm the per-turn auto-check shows. No-op once already at war (status
 * message only).
 */
void ai_king_menu_declare_independence(ColonizeTurnContext* ctx);

/*
 * FUN_4345_0a22 wartime branch: spend the bell pool on foreign intervention
 * or a REF wave instead of electing a Founding Father. Returns 1 when the
 * pool was consumed (caller resets founding_fathers side-table pool).
 * No-op when REF already present (DOS 0x5382 bit1 set).
 */
int ai_king_spend_woi_bell_pool(ColonizeTurnContext* ctx, int nation_id);

/* Crown nation-slot stand-in (nation 1 if human is 0, else nation 0) — no
 * real 5th DOS crown identity in Linux, this reuses an existing slot. */
int ai_king_crown_nation(int human_nation);

/*
 * FUN_38fd_3dc8 dump-goods cargo pick (thin API for AI / tax refuse callers).
 *
 * Among @CARGO indices 0..15 whose bit is set in candidate_mask and clear in
 * boycott_bitmap, pick one via dos_rng. When @cargo_bid is NULL, pick
 * uniformly among that set (europe unavailable / prior behavior). When
 * non-NULL, eligible further requires cargo_bid[c] > 0 (live Europe bid;
 * zero-price goods are not dumped), then roulette by bid[c] (Europe local_7a
 * stand-in). Returns cargo index, or -1 if none / null rng.
 *
 * Cite: viceroy_unpacked.c FUN_38fd_3dc8 — builds eligible list from cargos
 * not already in nation boycott_bitmap (local_a6), then RNG-picks weighted by
 * Europe prices (local_7a). docs/fandom_col1994.md Boycott: throw "named
 * goods" (RNG unnamed list — not a fixed Tobacco second cargo). Does not
 * mutate boycott_bitmap; caller ORs (1u << idx) when applying. King refuse
 * path still freezes Sugar only.
 */
int ai_king_pick_dump_goods_cargo(
  uint16_t boycott_bitmap,
  uint16_t candidate_mask,
  ColonizeDosRng* rng,
  const int* cargo_bid /* COLONIZE_CARGO_COUNT, or NULL → uniform */
);

#endif
