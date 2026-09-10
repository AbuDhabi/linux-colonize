#ifndef COLONIZE_COLONY_YIELD_H
#define COLONIZE_COLONY_YIELD_H

#include "core/colony.h"
#include "core/map.h"

/*
 * Terrain yield stubs from NAMES.TXT @UNFORESTED / @FORESTED / @OTHER
 * (Farmer … Fisherman columns) plus light @RESOURCE bonuses.
 */

/* Cargo produced by a field @JOB (Farmer/Fisherman → food). Returns -1 if invalid. */
int colony_yield_job_cargo(int field_job);

/*
 * The Fisherman docks gate, answered once for the whole port.
 *
 * DOS has no `has_docks` parameter at all: FUN_15eb_18ec ends with
 * `if ((7 < local_14) && (iVar3 = FUN_15eb_038e(6), iVar3 == 0)) local_26 = 0;`
 * (viceroy_unpacked.c:11967-11969) — job 8+ (Fisherman) yields 0 unless the
 * colony owns @BUILDING group 6. FUN_15eb_038e is just
 * `FUN_15eb_035e(*(int *)0x8dc6, group)`, i.e. it reads the *current colony*
 * global, so in DOS the scorer, the picker and the tick physically cannot
 * disagree about this. The port had six hand-copied answers instead, one of
 * which (the AI plot scorer) also accepted bare coastal placement — see the
 * 2026-09-10 smell audit, Area E #2.
 *
 * DOS's group 6 is one building slot upgraded in place; the port models the
 * three tiers as separate building types, hence the three names. Coastal
 * placement alone is NOT enough — the building has to be there.
 */
bool colony_yield_colony_has_docks(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
);

/*
 * Base + resource yield for working (x,y) as field_job, no worker context
 * (no expert/convert bonus, never docks-gated) — used by AI/job-suggestion
 * callers that don't have a specific colonist. 0 if impossible. Thin
 * wrapper over colony_yield_for_worker's full pipeline with profession=-1,
 * sol_bonus=0, has_docks=true.
 *
 * The ungated `has_docks=true` is only right for callers with no colony:
 * founding-site scoring, the map/terrain probes and the tests. A caller that
 * DOES have a colony and could ask about Fisherman must pass the real answer
 * — use colony_yield_for_tile_in_colony below, which is the DOS shape.
 */
int colony_yield_for_tile(const ColonizeWorldMap* map, int x, int y, int field_job);

/*
 * colony_yield_for_tile for a caller that has the colony: identical pipeline,
 * except the Fisherman gate is derived from the colony via
 * colony_yield_colony_has_docks instead of being assumed open. This is DOS's
 * own shape (18ec reads the colony global), so a scorer using it cannot
 * disagree with the tick.
 */
int colony_yield_for_tile_in_colony(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job
);

/*
 * Tile yield for colonist `profession` on `field_job` — the full DOS
 * pipeline (FUN_15eb_18ec): base terrain, Fisherman coastal distance mod
 * (any skill), positive sol_bonus fold, plow/road/river, special resource
 * (deferred for a matching Farmer/Fisherman expert — see below), then the
 * expert multiplier: flat ×2 for every field expert *except* a matching
 * Farmer/Fisherman, which instead gets flat +2 plus the colony's SoL latch
 * bits re-added a second time (`colony_flags`, asm-confirmed 2026-08-18 —
 * see docs/terrain_yields.md "Field Farmer/Fisherman expert formula"), and
 * only then its own resource bonus (doubled if expert). Lumberjack's
 * unconditional ×2 and negative sol_bonus (not amplified by any of the
 * above) apply last. `has_docks`: Fisherman yields 0 without it, matching
 * DOS — always source it from colony_yield_colony_has_docks above rather
 * than open-coding another building scan, and never widen it to bare
 * coastal placement. `sol_bonus`: colony_prod_sol_bonus_field (signed; 0 to
 * skip).
 * `colony_flags`: the colony's ColonizeColony.colony_flags (SOL_50/
 * SOL_100 latch bits; 0 if not a matching Farmer/Fisherman expert, or if
 * the caller has no colony context — colony_yield_for_tile passes 0).
 * `has_hudson`: whether the colony's OWNER nation has Henry Hudson (@FF 8);
 * doubles a Fur Trapper yield at DOS's own position (after the improvement
 * stack, BEFORE Convert +1 and before the Tory subtraction — 11970-11973),
 * which is why it is a pipeline argument and not a caller-side `*= 2` (smell
 * audit #60). See colony_yield_pipeline's comment in colony_yield.c and
 * docs/terrain_yields.md for the full step order and its player-data
 * derivation.
 */
int colony_yield_for_worker(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job,
  int profession,
  bool has_docks,
  int sol_bonus,
  uint8_t colony_flags,
  bool has_hudson
);

/*
 * Raw DOS `DS:0x2f7b + job` base-yield byte for a terrain CLASS — the
 * stride-16 terrain record's `+5..+13` columns, which are exactly the
 * NAMES.TXT @UNFORESTED/@FORESTED/@OTHER field-yield grid (see
 * docs/terrain_yields.md "DS:0x2f76 terrain-class record"). Class numbering is
 * map_dos_terr_class_at / map_pedia_terrain_index_at (0-7 cleared, 8-23
 * forested via &7, 24-28 arctic/ocean/sea lane/mountains/hills).
 *
 * NO tile context: no resource, plow, road, river, SoL or expert folding.
 * Anything scoring a real tile wants colony_yield_for_tile /
 * colony_yield_for_worker instead. This exists for the DOS AI arms that
 * literally index the table by class — FUN_5952_035e's colony ring scan
 * (`*(byte*)(class*0x10 + 0x2f7b)`, raw 94090-94113). 0 out of range.
 */
int colony_yield_terrain_class_base(int terr_class, int field_job);

/* Display name for field @JOB (static string). */
const char* colony_yield_job_name(int field_job);

/*
 * Town commons (colony center): always food + one other commodity.
 * Food = the FUN_15eb_1f72 class split + difficulty handout + plow + special
 * + the SoL *latch* bits. Secondary = terrain base, no flat road add, folds
 * the same `colony_flags` SoL latch bits only (+1 SOL_50, +1 SOL_100 — up to
 * +2). Neither side reads the general signed sol_bonus (it was a dead
 * parameter here until 2026-09-09, smell audit #69). Both asm-confirmed 2026-08-18
 * against FUN_15eb_1f72 (viceroy_unpacked.c ~12474, the real town-commons
 * composer, previously undiscovered — "peel pending" in terrain_yields.md):
 * secondary is `table_lookup(pedia,job) + resource_effect + river(0/1/2,
 * unscaled) + (SOL_50 latch ? +1) + (SOL_100 latch ? +1)`, no plow, no
 * flat "+1 road"; a difficulty term (`+1` only on the easiest setting) is
 * omitted here since every fixture this project's data comes from is
 * Viceroy (hardest), where it's always 0. Player-confirmed via
 * colony_prod02's New Holland (Savannah, no plow/river, needs base+2) vs
 * golden_colony_prod01's Guadeloupe (Swamp, plowed, needs base+2 too —
 * the plow contributes nothing; both colonies just have both SoL latch
 * bits set). Plow → food only; river → food + secondary; Game/Oasis/Wheat
 * → +2 food; matching specials → +2 secondary, or x2 for a DOUBLE-type
 * match (Prime Timber excluded from both).
 * See docs/terrain_yields.md.
 */
typedef struct ColonizeTownCommonsYield {
  int food;
  int secondary_job;   /* COLONIZE_JOB_* or -1 */
  int secondary_cargo; /* COLONIZE_CARGO_* or -1 */
  int secondary_amount;
} ColonizeTownCommonsYield;

void colony_yield_town_commons(
  const ColonizeWorldMap* map,
  int x,
  int y,
  uint8_t colony_flags,
  int difficulty,
  ColonizeTownCommonsYield* out
);

#endif
