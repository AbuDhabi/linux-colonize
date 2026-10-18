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
typedef void (*TurnColonyBeforeProduction)(void* user, ColonizeColony* colony);
void turn_run_colony_production_with_w(
  const ColonizeWorld* w,
  int human_nation,
  ColonizeTurnResult* out,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  TurnColonyBeforeProduction before,
  void* before_user
);
void turn_emit_built_chrome(
  const ColonizeMsgCatalog* messages,
  AiPopupState* ai_popups,
  const ColonizeColony* colony,
  const char* built_name
);
/* Phase A field composition (FUN_15eb_18ec) shared by the EOT tick and
 * colony_preview.c (bugs.md #1111). gross[] = field workers + town-commons
 * secondary; town-commons food is kept apart in commons_food. */
typedef struct TurnColonyFieldYield {
  int commons_food;
  int gross[COLONIZE_CARGO_COUNT];
  int fish;            /* Fisherman share of gross[FOOD] */
  int depletion_units; /* DS:0xa896 deposit tally (Col1 +0x97) */
} TurnColonyFieldYield;
void turn_compose_colony_fields(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  const ColonizeColony* colony,
  TurnColonyFieldYield* out
);
/* turn_produce_one_colony (FUN_364b_0688) stage ctx, bugs.md #1107. Inputs
 * are fixed for the tick except `colony`, which the Phase B application
 * switch (turn_produce_custom_house) rebinds to the 1528 record. Every other
 * field is a DOS local that crosses stages; its writer stage stores it here
 * before returning, readers take a const copy. */
typedef enum TurnProduceStatus {
  TURN_PRODUCE_CONTINUE = 0,
  TURN_PRODUCE_RETURN /* colony empty or abandoned: stop the tick */
} TurnProduceStatus;
typedef struct TurnProduceCtx {
  ColonizeColonyPool* pool;
  ColonizeColony* colony;
  const ColonizeWorldMap* map;
  ColonizeCol1Save* col1;
  EuropeScreen* europe;
  int human_nation;
  ColonizeTurnResult* out;
  ColonizeColonyProdDelta* delta;
  AiPopupState* ai_popups;
  const ColonizeMsgCatalog* messages;
  ColonizeDosRng* rng;
  ColonizeColonyProdDelta local_delta; /* delta when the caller passes NULL */
  /* begin */
  uint16_t mask_before_a;
  char status_at_entry[sizeof(((EuropeScreen*)0)->status)];
  int stock_before[COLONIZE_CARGO_COUNT]; /* aiStack_ae / local_6c */
  int pop;
  /* compose (Phase A) */
  int depl_n;
  int field_food;
  int sol_b_phase_a;
  bool colony_ai_controlled;
  int craft_unmet_cap[COLONIZE_CARGO_COUNT];
  int craft_unmet_gross[COLONIZE_CARGO_COUNT];
  int craft_gross[COLONIZE_CARGO_COUNT];
  int craft_net[COLONIZE_CARGO_COUNT];
  int hammers_phase_a;
  /* food / sol / lumber / custom_house (Phase B, C/D) */
  int ai_food_subsidy;
  int consumed;
  ColonizeColony* app;
  int colony_nation_id;
  int food_stock_before;
  int lumber_before_hammers;
  int hammers_paid;
  int ch_sold[COLONIZE_CARGO_COUNT];
  /* birth (Phase I) */
  int birth_food_debit;
  int was_starving;
} TurnProduceCtx;
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
void turn_run_human_colony_eot(ColonizeTurnContext* ctx, ColonizeTurnResult* out);
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
/* turn_produce_one_colony stages, DOS order (turn_production.c). */
TurnProduceStatus turn_produce_begin(TurnProduceCtx* pc);
void turn_produce_compose(TurnProduceCtx* pc);
void turn_produce_food(TurnProduceCtx* pc);
void turn_produce_sol(TurnProduceCtx* pc);
void turn_produce_horses(TurnProduceCtx* pc);
void turn_produce_lumber(TurnProduceCtx* pc);
void turn_produce_custom_house(TurnProduceCtx* pc);
void turn_produce_education(TurnProduceCtx* pc);
void turn_produce_discover(TurnProduceCtx* pc);
void turn_produce_birth(TurnProduceCtx* pc);
TurnProduceStatus turn_produce_starve(TurnProduceCtx* pc);
void turn_produce_hammers(TurnProduceCtx* pc);
void turn_produce_crumbs(TurnProduceCtx* pc);
void turn_produce_produced_mask(TurnProduceCtx* pc);
void turn_produce_spoilage(TurnProduceCtx* pc);
void turn_produce_cargo_ready(TurnProduceCtx* pc);
void turn_produce_depletion(TurnProduceCtx* pc);
#endif /* COLONIZE_TESTING */

#endif /* COLONIZE_CORE_TURN_INTERNAL_H */
