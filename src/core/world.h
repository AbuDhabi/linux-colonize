#ifndef COLONIZE_WORLD_H
#define COLONIZE_WORLD_H

#include <stdbool.h>

/*
 * Forward declarations only: world.h sits *under* every pool header, so any of
 * them can include it to declare a ColonizeWorld-taking entry point without a
 * circular include. Consumers that dereference a pool include its own header.
 */
typedef struct ColonizeUnitPool ColonizeUnitPool;
typedef struct ColonizeColonyPool ColonizeColonyPool;
typedef struct ColonizeWorldMap ColonizeWorldMap;
typedef struct ColonizeCol1Save ColonizeCol1Save;
typedef struct ColonizeDosRng ColonizeDosRng;
typedef struct EuropeScreen EuropeScreen;

/*
 * ColonizeWorld — the simulation's context object (2026-09-16).
 *
 * Every long simulation chain used to thread the same 4-6 pointers by hand
 * (units, colonies, map, col1, rng, europe), so adding one more piece of state
 * meant editing dozens of prototypes and every call site. ColonizeWorld bundles
 * them once. New sim entry points take `const ColonizeWorld* w` and read
 * `w->units` instead of growing another parameter.
 *
 * It is a **view struct**: it owns no storage and never frees anything. Build
 * one on the stack (world_make / world_from_turn_ctx / the fixture's fx_world)
 * and pass its address. The struct itself is never mutated by callees — hence
 * `const ColonizeWorld*` in signatures — while the pools it points at are.
 *
 * Field names deliberately match ColonizeTurnContext's, so world_from_turn_ctx
 * is a field-for-field copy and migrated bodies keep their original spelling
 * via a local alias (`ColonizeUnitPool* pool = w->units;`).
 *
 * Any member may be NULL where the old parameter was nullable; migrated
 * functions keep exactly the same NULL handling they had before.
 */
typedef struct ColonizeWorld {
  ColonizeUnitPool* units;
  ColonizeColonyPool* colonies;
  ColonizeWorldMap* map;
  ColonizeCol1Save* col1;
  bool col1_ok; /* mirrors ColonizeTurnContext.col1_ok (col1 loaded/usable) */
  ColonizeDosRng* rng;
  EuropeScreen* europe;
  /* FUN_281f_04ca timer word for the 465b overspend reseed (raw 75649):
   * game_loop passes ColonizeGameState.ai_rng_seed; unset = no reseed
   * (tests / headless callers keep the plain stream). bugs.md #842. */
  uint32_t rng_reseed;
  bool rng_reseed_set;
} ColonizeWorld;

/*
 * Build a view from loose pointers (call sites that have no turn context:
 * game_dialogs, tests, UI screens). Casts away const on the read-only pools —
 * the view is shared by const and non-const users alike and callees keep the
 * constness they always had on their own locals.
 */
static inline ColonizeWorld world_make(
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  bool col1_ok,
  const ColonizeDosRng* rng,
  const EuropeScreen* europe
) {
  ColonizeWorld w;
  w.units = (ColonizeUnitPool*)units;
  w.colonies = (ColonizeColonyPool*)colonies;
  w.map = (ColonizeWorldMap*)map;
  w.col1 = (ColonizeCol1Save*)col1;
  w.col1_ok = col1_ok;
  w.rng = (ColonizeDosRng*)rng;
  w.europe = (EuropeScreen*)europe;
  return w;
}

#endif /* COLONIZE_WORLD_H */
