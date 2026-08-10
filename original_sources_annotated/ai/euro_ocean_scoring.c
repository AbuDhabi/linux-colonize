/*
 * FUN_521d_20e6 ship band — LAB_521d_3558 + follow-ons (annotated structure).
 *
 * Decomp: viceroy_unpacked.c ~89384–90224.
 *   3558 ship body ~89384–89870; 3fa6 HS spiral; 4393–47b9 cargo/wagon.
 * Linux:  ai_euro_ocean_score_step (thin). Full −0x6790 / local_9c matrix OPEN.
 *
 * Section map: ai/move_scoring_ship.md
 * Cite: move_scoring.md band table; FUN_48d3_015e / 0434 / 048e; 06ae via 04ac.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include "viceroy_types.h"

/* Prologue gates (set earlier in 20e6):
 *   local_34 = type ∈ [0x0d,0x12]
 *   local_90 = tile ocean 0x19 / HS 0x1a
 *   local_88/94 = unit x/y; uVar11 = nation
 */

/* ---- Hold / stack queries ---------------------------------------------- */

/*
 * Scan cargo holds: FUN_281f_0be6 (slot type) / 0c68 (amount).
 * Builds local_c8[0x10] amounts and local_44 = max interesting type
 * (type>0xc or type==8 Treasure).
 */
void euro_ocean_scan_holds(int unit_index) {
  (void)unit_index;
}

/*
 * FUN_281f_08bc stack queries:
 *   mode 2 → local_a8 (free-ish / pax capacity remnant)
 *   mode 3 → local_4a (founders / settle cargo)
 *   mode 4 → local_48 (military)
 *   mode 5 → local_16
 *   mode 6 → local_46
 *   mode 0xc (wartime) folds into local_48
 * free_slots local_82 = −48 − (16 − a8) − 4a
 */
void euro_ocean_stack_queries(int unit_index) {
  (void)unit_index;
}

/* ---- Goal probes + land-adj flag word ---------------------------------- */

/*
 * Probes: 2a1f_0584(nation,x,y,7|1), 2a1f_0464(unit), 053c/0494 urgency.
 * Then 8-dir loop builds local_9c from continent tables −0x6790 / −0x6b1a /
 * −0x6b5a / −0x6a0e and bitmaps DS:0x173c / 0x173e.
 *
 * Bits: 0x40 founder unload, 0x20 trade, 0x10 military, 0xffff goto-match.
 * Cleared entirely when DS:0x1740 != 0.
 */
unsigned euro_ocean_build_land_adj_flags(int unit_index, int nation_id, int x, int y) {
  (void)unit_index;
  (void)nation_id;
  (void)x;
  (void)y;
  return 0;
}

/*
 * LAB_521d_3558 unload loop (~89566–89609).
 * For each land unit on ship stack matching (type_table_0x523d & local_9c):
 *   dir = thunk_FUN_2a1f_04ac → FUN_521d_06ae (sole 20e6 founding call ~89587)
 *   step + exhaust; stop when unit left ship tile.
 */
int euro_ocean_try_unload_via_06ae(int ship_index, unsigned land_adj_flags) {
  (void)ship_index;
  (void)land_adj_flags;
  return 0;
}

/*
 * Colony sail pick (~89614–89711) → best colony xy → LAB_521d_27f5 goto.
 * Peace vs war score arms differ (see move_scoring_ship.md).
 */
int euro_ocean_pick_colony_sail_target(int ship_index, int nation_id) {
  (void)ship_index;
  (void)nation_id;
  return -1;
}

/*
 * LAB_521d_3558 — ship body entry (orchestration stub).
 */
int euro_ocean_ship_band_skeleton(int unit_index) {
  (void)unit_index;
  /* if (!local_34) return 0; */
  euro_ocean_scan_holds(unit_index);
  euro_ocean_stack_queries(unit_index);
  /* probes; flags = euro_ocean_build_land_adj_flags(...); */
  /* if (flags) euro_ocean_try_unload_via_06ae(...); */
  /* else / also: euro_ocean_pick_colony_sail_target(...); */
  /* empty → LAB_521d_3fa6 or LAB_521d_4393 */
  return 0;
}

/* ---- HS spiral + cargo follow-ons -------------------------------------- */

/*
 * LAB_521d_3fa6 — thunk FUN_291f_02ea → FUN_48d3_015e.
 * Spiral-find nearest High Seas; set sail/goto when ship needs HS egress.
 */
int euro_ocean_spiral_hs_goto(int unit_index) {
  (void)unit_index;
  return 0;
}

/*
 * FUN_48d3_0434 — tile OK for HS place: in-bounds, terrain class == 0x1a,
 * owner empty or own nation.
 * FUN_48d3_048e — expanding spiral from landfall goto until 0434 hits;
 * teleport ship + sync cargo (Linux: units_spiral_place_hs_near).
 */
int euro_ocean_hs_place_ok(int x, int y) {
  (void)x;
  (void)y;
  return 0;
}

/*
 * LAB_521d_4393 — score AI work queue (−0x5f24, 16×6) for ship haul target.
 * Distance-normalize; update queue counts; bind colony → 27f5.
 */
int euro_ocean_work_queue_haul_pick(int ship_index) {
  (void)ship_index;
  return -1;
}

/*
 * LAB_521d_457e / 47b9 — empty-ship HS revisit or destroy wagon/treasure dead
 * ends (281f_0808). Type 0x0c / 0x0a / 0x03 follow-ons: see move_scoring_ship.md.
 */
void euro_ocean_followon_dead_end(int unit_index) {
  (void)unit_index;
}
