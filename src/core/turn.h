#ifndef COLONIZE_TURN_H
#define COLONIZE_TURN_H

#include <stdbool.h>
#include <stdint.h>

#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * End-of-turn orchestrator (Colonization calendar + simulation scaffold).
 *
 * Calendar (@TIMECHANGE / GAME.TXT): one turn per year until 1600; thereafter
 * Spring then Autumn each year (autumn != 0 means Autumn).
 *
 * Pipeline after the human ends their turn:
 *   calendar → colony production → nation ticks → European AI →
 *   Indian AI → King/REF → refresh human MP / select next unit
 */

#define TURN_START_YEAR 1492
#define TURN_BIANNUAL_YEAR 1600
#define TURN_FOOD_PER_COLONIST 2
#define TURN_DEFAULT_NEEDED_CROSSES 9
/* AI Euro immigrant threshold seed (ai_euro_nation_turn / Col1 rivals). */
#define TURN_AI_DEFAULT_NEEDED_CROSSES 14

typedef struct ColonizeTurnContext {
  uint32_t* turn_number;
  uint16_t* game_year;
  uint16_t* game_autumn;
  int human_nation; /* 0..3 */
  int* active_turn_nation; /* optional; set during AI/Indian phases */
  ColonizeUnitPool* units;
  ColonizeColonyPool* colonies;
  EuropeScreen* europe;
  ColonizeWorldMap* map; /* optional; AI sailing / Brave wander */
  ColonizeCol1Save* col1; /* optional; updated in place when non-NULL */
  bool col1_ok;
  /* DOS LCG for AI / partial-overspend. Nation turns reseed from rng_seed
   * (FUN_281f_04ca ← DS:0x83a6 timer word; VR_SEED locks this to 100).
   * rng_seed_set distinguishes an explicit 0 from "not provided". */
  ColonizeDosRng* rng;
  uint32_t rng_seed;
  bool rng_seed_set;
  char* status;
  size_t status_size;
  /* Optional map AI popup queue (human-facing OK / choice dialogs). */
  AiPopupState* ai_popups;
  /* Optional GAME.TXT catalog for @INDIAN* contact copy. */
  const ColonizeMsgCatalog* messages;
  /* Optional NAMES.TXT for @TRIBES flavor-good live parse (contact trade). */
  const ColonizeMsgCatalog* names;
  /*
   * FUN_5bfb_00f8 inverse rank filled in TURN_PROC_SETUP (0 = strongest).
   * euro_power_rank_ok set when turn_rank_euro_nations ran this EOT.
   */
  uint8_t euro_power_rank[4];
  bool euro_power_rank_ok;
  /*
   * FUN_4962_0606 profession histogram per Euro nation (SETUP). Indices are
   * @JOB / profession ids 0..31; counts saturate at 255.
   */
  uint8_t profession_tally[4][32];
  bool profession_tally_ok;
} ColonizeTurnContext;

typedef struct ColonizeTurnResult {
  bool advanced;
  bool request_autosave_turn;   /* slot 9 when autosave option set */
  bool request_autosave_decade; /* slot 8 when year % 10 == 0 */
  bool year_end_defeat; /* FUN_3844_0442 B: year≥1600, no human colonies, peacetime */
  bool year_end_victory; /* FUN_3844_0442 C1 thin: WoI + no crown colonies */
  bool request_europe_open; /* ship-build ready off-colony (DS:0x14c stand-in) */
  int colonies_produced;
  int food_shortages;
  int immigrants_arrived;
  int buildings_completed;
} ColonizeTurnResult;

/* Per-colony last production tick (for colony-screen deltas). */
typedef struct ColonizeColonyProdDelta {
  int food_net;
  int lumber;
  int ore;
  int hammers_added;
  bool building_completed;
  /* Net change per @CARGO (field harvest, craft consume/produce, lumber use). */
  int goods[COLONIZE_CARGO_COUNT];
} ColonizeColonyProdDelta;

/* Frame-stepped end-of-turn (indicator only while a nation turn runs). */
typedef enum ColonizeTurnProcStep {
  TURN_PROC_IDLE = 0,
  TURN_PROC_SETUP,  /* calendar + production + nation ticks (no indicator) */
  TURN_PROC_EURO,   /* one European AI nation per advance; slots above the human run before INDIAN, slots below after (DOS 130d order) */
  TURN_PROC_INDIAN, /* one native nation per advance (4d56_1b3a mid-pass → 1816) */
  TURN_PROC_FINISH  /* king stub + human refresh (no indicator) */
} ColonizeTurnProcStep;

typedef struct ColonizeTurnProcessor {
  ColonizeTurnProcStep step;
  int nation_cursor; /* next nation to process in EURO/INDIAN */
  uint16_t year_before;
  bool show_indicator;
  ColonizeTurnResult result;
} ColonizeTurnProcessor;

/* Advance year / autumn / turn counter per @TIMECHANGE. */
void turn_advance_calendar(uint16_t* year, uint16_t* autumn, uint32_t* turn_number);

/* "Spring, 1492" / "Autumn, 1600" from live calendar fields. */
void turn_format_date(uint16_t year, uint16_t autumn, char* out, size_t out_size);

/* Full end-of-turn pipeline (human finished orders). Synchronous; for tests. */
ColonizeTurnResult turn_end(ColonizeTurnContext* ctx);

/* Interactive EOT: start then call turn_processor_advance once per frame. */
void turn_processor_start(ColonizeTurnProcessor* proc);
bool turn_processor_active(const ColonizeTurnProcessor* proc);
bool turn_processor_show_indicator(const ColonizeTurnProcessor* proc);
/* One slice; returns true if still active. */
bool turn_processor_advance(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx);

/* Colony Space cheat: one production cycle without advancing world time.
 * If out_delta is non-NULL, fills last-tick nets for UI. map may be NULL. */
void turn_colony_free_production(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const ColonizeWorldMap* map,
  ColonizeTurnResult* out,
  ColonizeColonyProdDelta* out_delta
);

/* Production for every active colony (used by turn_end). map/col1/europe may be NULL. */
/* Hand the unit pool in so births land ON the colony tile (NULL = old
 * join-colony fallback for headless callers). */
void turn_set_birth_units_pool(ColonizeUnitPool* units);
void turn_run_colony_production(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  ColonizeTurnResult* out,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  ColonizeDosRng* rng
);

/*
 * Coastal Fort/Fortress naval fire (FUN_364b_03f6). Call after colony
 * production in EOT SETUP. Returns ships sunk (0 if ctx incomplete).
 */
int turn_run_coastal_fort_fire(ColonizeTurnContext* ctx);

/*
 * Unit-type construction completion (Artillery — colonies_unit_build_info)
 * for every active colony. Call after turn_run_colony_production in EOT
 * SETUP; needs ctx->units, which colony production itself doesn't have
 * access to, so colonies_try_complete_unit_construction can't be reached
 * from inside turn_produce_one_colony the way colonies_try_complete_
 * building is. No-op (not an error) for colonies not building a unit.
 */
void turn_run_colony_unit_construction(ColonizeTurnContext* ctx);

/*
 * Real-building construction completion for every active colony whose
 * banked hammers/tools already meet the current project's cost — including
 * a project a BUY topped up but didn't complete (colonies_buy_construction
 * only tops hammers/tools; this is where the actual completion happens,
 * once per turn, unconditionally). Call alongside
 * turn_run_colony_unit_construction in EOT SETUP. Safe to call even when
 * turn_produce_one_colony's own inline completion already fired this turn
 * (colonies_try_complete_building's has_building[] guard no-ops the retry).
 */
void turn_run_colony_building_completion(ColonizeTurnContext* ctx);

/* Crosses → dock immigrant; liberty bells counters (human + AI Euro Col1). */
void turn_run_nation_ticks(ColonizeTurnContext* ctx, ColonizeTurnResult* out);

/*
 * FUN_5bfb_00f8 — rank Euro nations by gold/100 + 2*colonies + pop + land combat.
 * Writes inverse rank into out_rank[nation] (0 = strongest). Returns 0 on success.
 * Cite: viceroy_unpacked.c ~96506–96531; turn/mid_pass_indian_rank.md.
 */
int turn_rank_euro_nations(
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  uint8_t out_rank[4]
);

/*
 * FUN_4962_0606 thin — profession histogram for one Euro nation from colony
 * colonist jobs + map units' profession. out[32] saturates at 255.
 * Cite: turn/census_tally.md; viceroy_unpacked.c ~78332–78373.
 */
void turn_tally_professions(
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  int nation_id,
  uint8_t out_hist[32]
);

/* EN→FR→SP→DU AI nations (skip human); calls ai_euro_nation_turn. */
void turn_run_european_ai_stubs(ColonizeTurnContext* ctx);

/* Indian AI (growth + Brave pulse + contact) / King phase (tax/REF). */
void turn_run_indian_stub(ColonizeTurnContext* ctx);
void turn_run_king_stub(ColonizeTurnContext* ctx);

/*
 * FUN_3844_0442 section B thin: year≥1600 && human colonies==0 && !WoI →
 * set out->year_end_defeat + status. Full chrome PARKED.
 * Cite: turn/year_end_chrome.md.
 */
void turn_run_year_end_chrome(ColonizeTurnContext* ctx, ColonizeTurnResult* out);

/*
 * Refresh moves for units of one nation (4..11 = natives when nation >= 4).
 * When col1 is set and the nation owns Magellan, sea units get +1 movement.
 * map may be NULL; when non-NULL with col1, arms native settlement fallout
 * (FUN_5fef_31ea-shaped; conquest gold unknown → -1, no invent).
 */
void turn_refresh_moves_for_nation(
  ColonizeUnitPool* pool,
  int nation_id,
  const ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/* Select next human unit with moves_left > 0; centers not done here. */
bool turn_select_next_unit(ColonizeUnitPool* pool, int human_nation);

/* True when no on-map human unit still has movement. */
bool turn_human_units_exhausted(const ColonizeUnitPool* pool, int human_nation);

/* Read Col1 end_of_turn / autosave option bits (false if no save). */
bool turn_option_end_of_turn(const ColonizeCol1Save* col1, bool col1_ok);
bool turn_option_autosave(const ColonizeCol1Save* col1, bool col1_ok);

/*
 * Turn-owner indicator (FUN_1984_00aa / FUN_281f_0590): 5×3 fill at bottom-right
 * (0x13b, 0xc5) overlaid on the 320×200 framebuffer. Colors from NAMES.TXT
 * @COUNTRY / DS:0x848 (Europeans) and @TRIBES / DS:0x84c (natives 4..11).
 */
#define TURN_OWNER_INDICATOR_X 0x13b
#define TURN_OWNER_INDICATOR_Y 0xc5
#define TURN_OWNER_INDICATOR_W 5
#define TURN_OWNER_INDICATOR_H 3

uint8_t turn_nation_color(int nation_id);
void turn_draw_owner_indicator(ColonizeFramebuffer8* framebuffer, int nation_id);

#endif
