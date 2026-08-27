#ifndef COLONIZE_AI_CONTACT_H
#define COLONIZE_AI_CONTACT_H

#include "core/turn.h"

/*
 * Indian contact / missions / trade / raids — partial structural port.
 * Thin maps: original_sources_annotated/ai/indian_contact.md,
 *            indian_raid_outcomes.md
 *
 * FUN_4d56_1816 phases (prelude / relation), FUN_5bfb_022e meet,
 * FUN_4d56_4528 / FUN_5fef_0f14 raid outcomes, FUN_4d56_359c scout stub.
 *
 * PARK deep FUN_4d56_2820 (~1.4k; thunk 2a1f_044c): full meet/raid decision
 * matrix + nested trade 2aac…311e (dispatch / buy / hard-bargain / demand).
 * Linux keeps thin trade-goods→alarm (colony / ship / wagon hold) + Trade
 * CHOICE refuse/concluded chrome + gift/demand / teach / convert / @RAID*
 * stand-ins only — no 2820 body port
 * (Marathon2 R6 / R5 / R4 / prior R14 PARK).
 *
 * Jesuit-grade convert: PEDIA @JOB24 name/prof 24, or nation owns Brebeuf
 * (docs/fandom_col1994.md — all missionaries function as experts). Mid-band
 * (40..54) convert gated on Jesuit-grade; plain Missionary + Brebeuf unlocks.
 * Las Casas Convert→Free Colonist assimilate lives in founding_fathers
 * (PEDIA @FATHER24 elect + ownership tick). Sepulveda convert-join:
 * founding_fathers_sepulveda_convert_join_bonus + FUN_5fef_31ea peel in
 * units_try_native_settlement_fallout (mission-owned tribe conquer).
 * Full 2820/4528 stay PARKED. Thin Brave escort (units_follow_unit) and
 * @RAIDBURN colonies_destroy_building + building-name status: Done thin.
 *
 * Human chrome: status lines + AI popup enqueue (OK / first-contact WELCOME
 * Yes/No / Meet CHOICE incl. Leave→Farewell / Gift amount / Demand
 * tools-vs-gold / alarmed Demand OK). Cite: indian_contact.md;
 * docs/ai_transcription.md FUN_5bfb_022e / FUN_5bfb_0182; peel
 * layer_b_combat_raid / layer_b_2a1f_midlo / layer_b_ai_diplo.
 */

/* @RAID* kind stand-ins (COLONIZE/GAME.TXT tags). */
typedef enum AiRaidKind {
  AI_RAID_NOTHING = 0,
  AI_RAID_WREAK = 1,
  AI_RAID_STORES = 2,
  AI_RAID_BURN = 3,
  AI_RAID_SCALP = 4,
  AI_RAID_SHIP = 5,
  AI_RAID_GOLD = 6
} AiRaidKind;

/* Last colony-raid loot kind applied (for smoke / diagnostics). */
int ai_contact_last_raid_kind(void);

/*
 * FUN_4d56_1816 §2 War of Independence tribe defection — thin port. See
 * indian_woi_defect_1816.md.
 */
void ai_contact_indian_woi_defect(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_4d56_1816 §3 alarm prelude: clamp, mission clear, war/alarm flags.
 * Must not burn the quiet-pulse LCG (uses isolated contact RNG when needed).
 */
void ai_contact_indian_prelude(ColonizeTurnContext* ctx, int nation_id);

/* FUN_4d56_1816 §6 / FUN_4cc6_00f2-shaped relation tick vs each Euro. */
void ai_contact_indian_relation_tick(ColonizeTurnContext* ctx, int nation_id);

/* FUN_5bfb_022e meet / auto-trade (status + AI popup CHOICE/OK when queued). */
void ai_contact_indian_meet_trade(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_5bfb_022e already-met Brave/Euro adjacency accept/refuse
 * (@INDIANBEGFOOD) — village begs a well-stocked colony for food; human
 * gets a real Give/Refuse CHOICE, AI Euro auto-accepts. See
 * ai_contact.c's own header comment on the implementation and
 * settlement_record_8d4a.md for the sign-convention resolution.
 */
void ai_contact_try_village_beg_food(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_5bfb_022e first contact: if unmet, set met and enqueue @INDIANWELCOME
 * Yes/No for human (or auto-accept for AI / no popups). Returns 1 if this
 * call started first contact. indian_nation is Col1 id 4..11.
 */
int ai_contact_try_first_welcome(ColonizeTurnContext* ctx, int euro_nation, int indian_nation);

/*
 * Village-enter Meet CHOICE (already-met human Euro on tribe tile). Enqueues
 * Trade/Gift/Demand/Teach/Leave. First-contact still uses WELCOME only.
 * Cite: indian_contact.md village Meet CHOICE; FUN_5bfb_022e. Deep 2820 PARKED.
 * `is_missionary`/`is_capital`: the acting unit's/visited village's real
 * status, captured by the caller at trigger time (each already has the
 * specific unit and ColonizeCol1Tribe record) and carried through the Meet
 * CHOICE's payload for the Incite sub-flow's two DOS flat discounts — see
 * indian_incite_417e.md. Pass 0/0 when not applicable (e.g. ship contact).
 * Returns 1 if a CHOICE was enqueued.
 */
int ai_contact_try_village_meet(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  int is_missionary,
  int is_capital
);

/*
 * FUN_4d56_4528 human village-enter warn CHOICE (Attack / Leave) for combatish
 * land units. Defers the move: nation_a=unit_id, nation_b=indian, payload=
 * dest_x|(dest_y<<8). Cite: indian_settlement_4528.md head; game_loop apply.
 * Returns 1 if CHOICE enqueued.
 */
int ai_contact_try_village_raid_warn(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  int unit_id,
  int dest_x,
  int dest_y
);

/*
 * FUN_465b_0000 @WHACKINDIANS (static port 2026-08-27): a human land unit
 * moving onto a tile held by a native unit, while that tribe's alarm toward
 * the human is < 0x4b and the pair's euro_diplo bit 0x04 is clear, gets a
 * "Shall we attack the {tribe}, Your Excellency?" Yes/No first. Yes sets the
 * bit (asked once until the tribe cools, FUN_4cc6_00f2 clears it) and the
 * move resumes; No aborts. Returns 1 if the CHOICE was enqueued (move
 * deferred), 0 if no confirmation is needed.
 */
int ai_contact_try_whack_confirm(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  int unit_id,
  int dest_x,
  int dest_y
);
int ai_contact_whack_pending(const AiPopupState* st, int unit_id);

/* @BUY0 haggle arm (pure). Returns 1 = re-ask at *io_price, 0 = tribe refuses. Exposed for tests. */
int ai_contact_2e92_haggle(int difficulty, int bid, ColonizeDosRng* rng, int* io_price, int* out_alarm_delta);

/* FUN_4d56_2820 LAB_002e92 AI branch — empty-handed AI unit buys the tribe's
 * own goods (see ai_contact.c). Exposed for tests. Returns 1 on purchase. */
int ai_contact_auto_buy_2e92(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  ColonizeUnit* unit
);

/* FUN_4d56_417e Mode 2 — AI Missionary at a village incites the tribe against
 * the human (see ai_contact.c header). Exposed for tests. */
int ai_contact_ai_incite_human(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  ColonizeCol1Tribe* t,
  int nation_id,
  int e,
  int is_missionary
);

/*
 * Open hostilities for a village Attack choice (peace clear + at-war relation
 * floor + alarm/friction burn band). Cite: welcome-reject / FUN_4cc6_00f2 thin.
 */
void ai_contact_village_open_hostilities(
  ColonizeTurnContext* ctx,
  int indian_nation,
  int euro_nation
);

/*
 * Ship ordered onto a native village tile (FUN_4d56_4528 ship head). Never
 * landfall. Unmet (euro_diplo met bit 0x20 clear) → @DONTKNOWSHIPS; met and
 * (relation≥75 or tribe friction≥64) → @MADATSHIPS; met mid-band relation
 * 50..74 → wary status then thin village Meet; else thin village Meet.
 * Ship does not enter the tile. Returns 1 if (x,y) is a village and handled.
 */
int ai_contact_try_ship_village(ColonizeTurnContext* ctx, int euro_nation, int x, int y);

/* FUN_5bfb_0182 peace bit on indian.euro_diplo[euro] (COL1_INDIAN_PEACE_BIT). */
int ai_contact_indian_has_peace(const ColonizeCol1Save* col1, int indian_nation, int euro_nation);

/*
 * Fandom capital-destroy surrender: when a capital village falls, reset that
 * Indian nation's alarm/friction toward the attacker and restore peace once.
 * Cite: docs/fandom_col1994.md Capital destroy; units_try_native_settlement_fallout.
 */
void ai_contact_indian_capital_surrender(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation
);

/* FUN_4d56_4528 / 5fef_0f14 raid outcomes + 359c scout stub. */
void ai_contact_indian_raids(ColonizeTurnContext* ctx, int nation_id);

/* Apply human choice from map AI popup (welcome / meet / teach / gift|demand). */
void ai_contact_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup);

#endif
