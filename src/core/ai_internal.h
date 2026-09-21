#ifndef COLONIZE_CORE_AI_INTERNAL_H
#define COLONIZE_CORE_AI_INTERNAL_H

/*
 * Stage seams for ai.c's FUN_4d56_021a native-move scorer (ai_021a_*). See
 * core/internal.h for the COLONIZE_INTERNAL / COLONIZE_TESTING pattern this
 * follows.
 */

#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/internal.h"
#include "core/map.h"
#include "core/units.h"

/* ai.c's local alias for ColonizeDosRng, mirrored here for the ctx struct. */
typedef ColonizeDosRng AiRng;

/* Per-direction and per-act state shared by the FUN_4d56_021a scorer stages. */
struct ai_021a_ctx {
  AiRng* rng;
  const ColonizeWorldMap* map;
  const ColonizeUnitPool* units;
  const ColonizeCol1Save* col1;
  const ColonizeColonyPool* colonies;
  const ColonizeUnit* u;
  int nation_id, x, y, indian, turn_w, cool, unit_fa, unit_river, home, dump;
  int adj_foreign, adj_nation, continent, col_dist, col_idx;
  const ColonizeColony* col;
  const ColonizeCol1Tribe* village;
  int vx, vy, home_reach, encroach, threat, threat_nation, angry, visit_turn;
  int self_stack, lone;
  const ColonizeCol1Indian* ind;
  int tech, facing;
  /* carried across directions */
  int grudge, best, best_dir, best_flags;
  /* per-direction */
  int d, nx, ny, score, flags, terr, owner, presence, settle, dfa, driver, dres;
  int hostile, att, occ, visit_nation, visit_val, vdist, attack_intent, alarm, upg;
};

typedef enum {
  AI_021A_DIR_OK = 0,  /* stage fell through — run the next one */
  AI_021A_DIR_SKIP = 1 /* direction rejected (was a bare `continue;`) */
} Ai021aDirStatus;

#ifdef COLONIZE_TESTING
Ai021aDirStatus ai_021a_dir_tile(struct ai_021a_ctx* c);
Ai021aDirStatus ai_021a_dir_occupant(struct ai_021a_ctx* c);
Ai021aDirStatus ai_021a_dir_terrain(struct ai_021a_ctx* c);
Ai021aDirStatus ai_021a_dir_angry(struct ai_021a_ctx* c);
void ai_021a_score_dir(struct ai_021a_ctx* c);
int ai_465b_dest_owner(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, int x, int y
);
#endif /* COLONIZE_TESTING */

#endif /* COLONIZE_CORE_AI_INTERNAL_H */
