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
 * European / Indian / King AI (Full T0/T1 surface).
 *
 * New-game: Col1 template + rival fleets + tribes/Braves.
 * Euro: full dispatcher by default (ai_euro.c). Opt into retired seed-100
 *   early fixture with AI_EURO_EARLY_FIXTURE=1 (bisect only).
 * Indian: growth + quiet pulse + contact/meet/trade/raids (ai_contact.c).
 * King: tax / SoL declare / REF waves (ai_king.c).
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

/* One native nation (4..11): village growth + DOS Brave pulse + contact/raids. */
void ai_indian_nation_turn(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_4cc6_03f8 (via FUN_281f_0316): which European nation this settlement
 * feels most threatened by, and how strongly. Returns the nation 0..3, or -1
 * when nothing scores; *out_score (optional) gets the threat score.
 *
 * Two consumers, as in DOS: the per-turn village tick, which turns the score
 * into settlement alarm, and the map chrome (FUN_112b_0790), which draws
 * `score/4 + 1` exclamation marks over the village. Cheap enough to call per
 * visible village per frame — it walks a 20-tile ring and the colony list.
 */
int ai_indian_village_threat(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int human_nation,
  int tribe_index,
  int* out_score
);

/* King / tax / REF / independence phase (replaces turn_run_king_stub body). */
void ai_king_nation_turn(ColonizeTurnContext* ctx);

/*
 * Cheat: Kill Indians — despawn all units of nation_id (4..11), remove villages,
 * clear map owner nibbles on village tiles, reset indian[N-4] slot.
 * Returns number of villages removed (0 if none / invalid).
 */
int col1_kill_indian_nation(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int nation_id
);

/* FUN_521d_0a60 entry: wipe + restamp the DS:0x9faa coarse plane for this Euro nation. */
void ai_coarse_fog_euro_restamp(
  const ColonizeUnitPool* units, const ColonizeColonyPool* colonies, int nation_id
);

#endif
