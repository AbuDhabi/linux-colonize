#ifndef COLONIZE_CORE_AI_CONTACT_INTERNAL_H
#define COLONIZE_CORE_AI_CONTACT_INTERNAL_H

/*
 * Stage seams for ai_contact.c's ai_contact_indian_raids pass
 * (ai_contact_raid_*). See core/internal.h for the COLONIZE_INTERNAL /
 * COLONIZE_TESTING pattern this follows.
 */

#include "core/ai_contact.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/internal.h"
#include "core/turn.h"

typedef enum {
  AI_RAID_CONTINUE = 0,  /* stage fell through — run the next one */
  AI_RAID_NEXT_BRAVE = 1 /* stage ended this Brave (was a bare `continue;`) */
} AiRaidStatus;

/* Per-Brave state shared between the stages of one ai_contact_indian_raids pass. */
struct ai_contact_raid_ctx {
  ColonizeTurnContext* ctx;
  ColonizeCol1Indian* ind;
  ColonizeDosRng* rng;
  int nation_id;
  ColonizeUnit* brave; /* re-read after any call that can free/replace it */
  int target_euro;
  int max_alarm;
  int attacked;
};

#ifdef COLONIZE_TESTING
int ai_contact_raid_port_ship(ColonizeTurnContext* ctx, const ColonizeColony* c);
AiRaidKind ai_contact_raid_kind_demote(
  ColonizeTurnContext* ctx, ColonizeColony* c, AiRaidKind kind
);
int ai_contact_raid_gate_target(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id,
  int* out_euro, int* out_alarm
);
int ai_contact_raid_alarm_delta(AiRaidKind kind);
void ai_contact_raid_stage_combat(struct ai_contact_raid_ctx* a);
int ai_contact_raid_pick_colony(struct ai_contact_raid_ctx* a);
AiRaidStatus ai_contact_raid_stage_colony(struct ai_contact_raid_ctx* a);
#endif /* COLONIZE_TESTING */

#endif /* COLONIZE_CORE_AI_CONTACT_INTERNAL_H */
