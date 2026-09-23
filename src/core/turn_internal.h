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

/* ===== Cross-file seams of the turn.c split (2026-09-23) =====
 * turn.c was 4.3k lines; it is now split along its banners into
 * turn{,_production,_colony,_year_end}.c. The declarations below are the
 * only symbols used across those files; everything else stayed `static` in
 * its own file. Code moved verbatim - these are the only de-static'd names,
 * and they carry the `turn_` file-scope prefix.
 * ============================================================= */

#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/units.h"

/* --- owned by turn.c --- */
int turn_report_ok_trained(const ColonizeCol1Save* col1);
int turn_report_ok_raw(const ColonizeCol1Save* col1);
int turn_report_ok_food(const ColonizeCol1Save* col1);
int turn_report_ok_new_cargo(const ColonizeCol1Save* col1);
int turn_report_ok_rebel_maj(const ColonizeCol1Save* col1);
int turn_report_ok_sons(const ColonizeCol1Save* col1);
int turn_report_ok_inefficient(const ColonizeCol1Save* col1);
void turn_set_birth_units_pool(ColonizeUnitPool* units);
extern ColonizeUnitPool* turn_birth_units;

/* --- owned by turn_production.c --- */
extern const ColonizeMsgCatalog* turn_labels;
extern uint8_t turn_prof_census[COLONIZE_COL1_NATION_COUNT][32];
extern int turn_prod_only_nation;
extern bool turn_prod_only_set;
extern int turn_prod_skip_nation;
extern bool turn_prod_skip_set;
bool turn_prod_nation_in_scope(int nation_id);
void turn_emit_built_chrome(
  const ColonizeMsgCatalog* messages,
  AiPopupState* ai_popups,
  const ColonizeColony* colony,
  const char* built_name,
  const char* fallback
);
void turn_produce_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  ColonizeTurnResult* out,
  ColonizeColonyProdDelta* delta,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  ColonizeDosRng* rng
);

/* --- owned by turn_colony.c --- */
void turn_run_colony_eot(ColonizeTurnContext* ctx, ColonizeTurnResult* out);
int turn_run_coastal_fort_fire(ColonizeTurnContext* ctx);
void turn_route_damaged_ships(ColonizeTurnContext* ctx, int nation);
bool turn_euro_nation_is_ref(const ColonizeTurnContext* ctx, int n);

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
