#ifndef COLONIZE_AI_H
#define COLONIZE_AI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

/*
 * European / Indian AI actors (Phase 1 + early-AI T2 gate).
 *
 * New-game: Col1 template + rival fleets with landfall goto +
 * TRIBE.TXT / procedural villages with Braves + post-`6a09` native pulse.
 * Turn: sail/unload/found + AI crosses; seed-100 early path (fixture slices);
 * village growth + Brave pulse (seed-100 TURN snaps = R0 debt).
 * Full FUN_521d_* planner / raids remain parked.
 */

#define AI_TRIBE_CAP_AMERICA 84
#define AI_TRIBE_CAP_NEW_WORLD 84
#define AI_VILLAGE_GROWTH_THRESHOLD 19

typedef struct AiNewGameParams {
  ColonizeCol1Save* col1;
  bool* col1_ok;
  ColonizeWorldMap* map;
  ColonizeUnitPool* units;
  EuropeScreen* europe;
  const ColonizeMsgCatalog* names; /* NAMES.TXT @TRIBES / leaders optional */
  const char* data_dir; /* for TRIBE.TXT */
  int human_nation; /* 0..3 */
  int difficulty;
  const char* leader_name;
  bool use_tribe_txt; /* AMERICA / named .MP */
  const char* map_stem; /* e.g. "AMER2" for @SCENARIO landfalls; may be NULL */
  int human_start_x;
  int human_start_y;
  uint32_t rng_seed;
  /* When set, tribe placement continues this DOS LCG (post map_generate). */
  ColonizeDosRng* rng;
} AiNewGameParams;

/* Col1 template, AI fleets, tribes/Braves, fix human unit nation_id. */
bool ai_init_new_game(const AiNewGameParams* params, char* err, size_t err_size);

/* One European AI nation: refresh already done by caller; sail/unload/found + crosses. */
void ai_euro_nation_turn(ColonizeTurnContext* ctx, int nation_id);

/* One native nation (4..11): village growth + DOS Brave pulse. */
void ai_indian_nation_turn(ColonizeTurnContext* ctx, int nation_id);

#endif
