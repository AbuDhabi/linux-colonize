#ifndef COLONIZE_CORE_TURN_INTERNAL_H
#define COLONIZE_CORE_TURN_INTERNAL_H

/*
 * Stage seams for turn.c's ColonizeTurnProcessor dispatcher (turn_step_*)
 * and its year-end chrome helpers (turn_year_end_*). ColonizeTurnContext
 * and ColonizeTurnProcessor are already public (core/turn.h); this header
 * only adds prototypes, gated behind COLONIZE_TESTING (see core/internal.h).
 */

#include "core/col1_save.h"
#include "core/internal.h"
#include "core/popup_msg.h"
#include "core/turn.h"

#ifdef COLONIZE_TESTING
int turn_year_end_rival_rebels(const ColonizeCol1Save* col1, int rival);
const char* turn_year_end_independent_name(int nation);
const char* turn_year_end_country_name(const ColonizeCol1Save* col1, int nation);
const char* turn_year_end_leader_name(const ColonizeCol1Save* col1, int nation);
void turn_year_end_rival_popup(
  ColonizeTurnContext* ctx, const char* tag, const PopupMsgTokens* tok, const char* fallback
);
void turn_year_end_anniversary(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, uint16_t year, int splash_done,
  int woi_latched
);
void turn_year_end_era_end(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, uint16_t year, int splash_done,
  int woi_latched
);
bool turn_year_end_woi_chrome(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, int woi_latched
);
void turn_year_end_rival_independence(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, int woi
);
void turn_year_end_defeat_check(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, uint16_t year, int woi
);
void turn_step_setup(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx);
void turn_step_euro(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx);
void turn_step_indian(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx);
void turn_step_finish(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx);
void turn_step_king(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx);
#endif /* COLONIZE_TESTING */

#endif /* COLONIZE_CORE_TURN_INTERNAL_H */
