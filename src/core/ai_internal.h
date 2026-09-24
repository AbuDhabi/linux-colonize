#ifndef COLONIZE_CORE_AI_INTERNAL_H
#define COLONIZE_CORE_AI_INTERNAL_H

/*
 * Stage seams for ai.c's FUN_4d56_021a native-move scorer (ai_021a_*). See
 * core/internal.h for the COLONIZE_INTERNAL / COLONIZE_TESTING pattern this
 * follows.
 */

#include "core/ai.h"
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
  AiNativeScoreTile score_tiles[9];
  int score_tile_count;
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

/* ===== Cross-file seams of the ai.c split (2026-09-23) =====
 * ai.c was 5081 lines; it is now split along its banners into
 * ai.c (startup RNG / fog / diagnostics / new-game world setup / game init /
 * Euro dispatch), ai_indian.c (152e village worth, growth tick, tile helpers,
 * native pull scoring), ai_native_021a.c (the FUN_4d56_021a structural scorer)
 * and ai_brave.c (pick-dir dispatch, first contact, brave step, nation pulse).
 * The declarations below are the symbols used across those files (plus the
 * module's file-scope result type, which used to sit in the single .c);
 * everything else stayed `static` in its own file. Code moved verbatim —
 * these are the only de-static'd names, and the shared file-scope variables
 * were renamed `s_ai_*` -> `ai_s_*` to carry the module prefix.
 * ========================================================== */

#include "core/turn.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct Ai021aResult {
  int dir; /* 0..7 move, 8 stay */
  int flags; /* DOS [bp-0x82] of the winning dir */
} Ai021aResult;

extern int ai_s_seed100_init_pulse;
extern uint32_t ai_s_init_pulse_seed;
extern int ai_s_seed100_midturn_turn;
extern int ai_s_lcg_in_pick;
extern int ai_s_lcg_pick_burns;
extern uint32_t ai_s_lcg_total_nexts;
extern const ColonizeColonyPool* ai_s_native_colonies;
extern const ColonizeCol1Save* ai_s_native_col1;
extern int ai_s_native_home_dist;
extern const int k_ai_dir8_dx[9];
extern const int k_ai_dir8_dy[9];

int ai_021a_settle_owner(const ColonizeWorldMap* map, int x, int y);

int ai_021a_trace_enabled(void);

int ai_brave_peels_disabled(void);

uint8_t ai_coarse_fog_explore_byte(int x, int y);

uint8_t ai_coarse_fog_tribe_byte(int x, int y);

int ai_coarse_fog_unseen(int x, int y);

int ai_continent_id(const ColonizeWorldMap* map, int x, int y);

int ai_dos_terr_class(const ColonizeWorldMap* map, int x, int y);

void ai_find_home_tribe(
  const ColonizeCol1Save* col1,
  const ColonizeUnit* u,
  int* out_x,
  int* out_y
);

void ai_grow_villages(ColonizeTurnContext* ctx, int nation_id);

int ai_is_ocean_hs(const ColonizeWorldMap* map, int x, int y);

int ai_lab_54f5_gate(
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  int dest_x,
  int dest_y,
  int nation_id
);

uint8_t ai_layer2_at(const ColonizeWorldMap* map, int x, int y);

int ai_lcg_audit_enabled(void);

int ai_mask_fa_flags(const ColonizeWorldMap* map, int x, int y);

void ai_nation_reseed(ColonizeTurnContext* ctx);

int ai_native_021a_tail(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  ColonizeUnit* u,
  int nation_id,
  int dir,
  int flags
);

int ai_native_apply_seed100_peels(
  int nation_id,
  int x,
  int y,
  int best_dir,
  int dump,
  const int* audit_unseen,
  const int* audit_seen,
  int unit_seen_by_any
);

int ai_native_foreign_euro_pull(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, int x, int y, int nation_id,
  int dest_x, int dest_y, int score
);

int ai_native_foreign_euro_pull_open(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, int x, int y, int nation_id,
  int dest_x, int dest_y, int owner
);

typedef enum {
  AI_NATIVE_STEP_MORE = 0, /* the Brave may act again (loop continues) */
  AI_NATIVE_STEP_STOP = 1  /* the Brave is done this turn (was a bare `break;`) */
} AiNativeStepStatus;

#ifdef COLONIZE_TESTING
/* One FUN_1427_13b0 Brave act (ai_brave.c) — test seam, see conventions.md. */
AiNativeStepStatus ai_native_brave_step(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  int nation_id,
  bool seed100_init_burns,
  ColonizeUnit* u,
  int hx,
  int hy,
  int tech,
  int max_mp,
  int brave_index,
  int* steps
);
#endif

void ai_native_nation_pulse(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  int nation_id,
  bool seed100_init_burns
);

int ai_native_pick_dir_021a(
  AiRng* rng,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  const ColonizeUnit* u,
  int nation_id,
  Ai021aResult* out
);

int ai_owner_nibble(const ColonizeWorldMap* map, int x, int y);

int ai_peel_audit_argmax(const int score[8]);

int ai_peel_audit_enabled(void);

int ai_quiet_fog_explore_ex(
  const ColonizeWorldMap* map,
  int score,
  int unit_x,
  int unit_y,
  int dir,
  int nation_id,
  int* out_p8,
  int* out_m2
);

uint16_t ai_rng_next_counted(AiRng* rng);

int ai_rng_range(AiRng* rng, int lo, int hi_inclusive);

int ai_score_at_match(int nation_id, int x, int y);

void ai_set_owner_nibble_move(ColonizeWorldMap* map, int x, int y, int nation);

int ai_step_audit_enabled(void);

int ai_tribe_initial_pop(uint8_t tech);

uint32_t ai_turn_seed(const ColonizeTurnContext* ctx);

int ai_unit_index_on_tile(const ColonizeUnitPool* units, int x, int y);

#endif /* COLONIZE_CORE_AI_INTERNAL_H */
