#ifndef COLONIZE_CORE_AI_KING_INTERNAL_H
#define COLONIZE_CORE_AI_KING_INTERNAL_H

/*
 * Stage seams for ai_king.c's FUN_43f7_0982 REF invasion wave
 * (ai_king_0982_*). See core/internal.h for the COLONIZE_INTERNAL /
 * COLONIZE_TESTING pattern this follows.
 */

#include "core/colony.h"
#include "core/internal.h"
#include "core/turn.h"

/* Wave-local state shared by the FUN_43f7_0982 invasion stages. */
struct ai_king_0982_ctx {
  ColonizeTurnContext* ctx;
  int crown;
  int human;
  uint16_t* force;
  int total;
  bool exhaust;
  bool landed;
};

#ifdef COLONIZE_TESTING
int ai_king_0982_garrison_score(const ColonizeTurnContext* ctx, const ColonizeColony* c);
int ai_king_0982_tile_strength(const ColonizeTurnContext* ctx, int x, int y);
void ai_king_0982_purge_tile(ColonizeTurnContext* ctx, int crown, int x, int y);
int ai_king_0982_crown_mow_alive(const ColonizeTurnContext* ctx, int crown);
int ai_king_0982_spawn_pool_unit(ColonizeTurnContext* ctx, int crown, int k, int x, int y);
void ai_king_0982_land_troops(
  ColonizeTurnContext* ctx, int crown, uint16_t* force, const ColonizeColony* c,
  int continent, int garrison_raw, int need, int lx, int ly
);
void ai_king_0982_invasion(struct ai_king_0982_ctx* w);
#endif /* COLONIZE_TESTING */

#endif /* COLONIZE_CORE_AI_KING_INTERNAL_H */
