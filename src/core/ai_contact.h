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
 * Linux keeps thin trade-goods→alarm + gift/demand / teach / convert / @RAID*
 * stand-ins only — R14 PARK only (no 2820 body port).
 *
 * Human chrome: status lines + AI popup enqueue (OK / Meet CHOICE). Cite:
 * indian_contact.md; docs/ai_transcription.md FUN_4d56_2820; peel
 * layer_b_combat_raid / layer_b_2a1f_midlo.
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
 * FUN_4d56_1816 §3 alarm prelude: clamp, mission clear, war/alarm flags.
 * Must not burn the quiet-pulse LCG (uses isolated contact RNG when needed).
 */
void ai_contact_indian_prelude(ColonizeTurnContext* ctx, int nation_id);

/* FUN_4d56_1816 §6 / FUN_4cc6_00f2-shaped relation tick vs each Euro. */
void ai_contact_indian_relation_tick(ColonizeTurnContext* ctx, int nation_id);

/* FUN_5bfb_022e meet / auto-trade (status + AI popup CHOICE/OK when queued). */
void ai_contact_indian_meet_trade(ColonizeTurnContext* ctx, int nation_id);

/* FUN_4d56_4528 / 5fef_0f14 raid outcomes + 359c scout stub. */
void ai_contact_indian_raids(ColonizeTurnContext* ctx, int nation_id);

/* Apply human choice from map AI popup (meet / teach / gift / demand). No-op if tag mismatch. */
void ai_contact_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup);

#endif
