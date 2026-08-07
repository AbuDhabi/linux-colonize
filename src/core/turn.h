#ifndef COLONIZE_TURN_H
#define COLONIZE_TURN_H

#include <stdbool.h>
#include <stdint.h>

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
 *   Indian AI → King stub → refresh human MP / select next unit
 */

#define TURN_START_YEAR 1492
#define TURN_BIANNUAL_YEAR 1600
#define TURN_FOOD_PER_COLONIST 2
#define TURN_DEFAULT_NEEDED_CROSSES 8

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
   * (FUN_281f_04ca ← DS:0x83a6 timer word; VR_SEED locks this to 100). */
  ColonizeDosRng* rng;
  uint32_t rng_seed;
  char* status;
  size_t status_size;
} ColonizeTurnContext;

typedef struct ColonizeTurnResult {
  bool advanced;
  bool request_autosave_turn;   /* slot 9 when autosave option set */
  bool request_autosave_decade; /* slot 8 when year % 10 == 0 */
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
  TURN_PROC_EURO,   /* one European AI nation per advance */
  TURN_PROC_INDIAN, /* one native nation per advance */
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

/* Production for every active colony (used by turn_end). map/col1 may be NULL. */
void turn_run_colony_production(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeTurnResult* out
);

/* Crosses → dock immigrant; liberty bells counters (human nation + Col1). */
void turn_run_nation_ticks(ColonizeTurnContext* ctx, ColonizeTurnResult* out);

/* EN→FR→SP→DU AI nations (skip human); calls ai_euro_nation_turn. */
void turn_run_european_ai_stubs(ColonizeTurnContext* ctx);

/* Indian AI (growth + Brave pulse + contact) / King phase (tax/REF). */
void turn_run_indian_stub(ColonizeTurnContext* ctx);
void turn_run_king_stub(ColonizeTurnContext* ctx);

/*
 * Refresh moves for units of one nation (4..11 = natives when nation >= 4).
 * When col1 is set and the nation owns Magellan, sea units get +1 movement.
 */
void turn_refresh_moves_for_nation(
  ColonizeUnitPool* pool,
  int nation_id,
  const ColonizeCol1Save* col1
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
