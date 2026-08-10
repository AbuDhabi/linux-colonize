/*
 * FUN_521d_20e6 ship band — LAB_521d_3558 skeleton (annotated structure only).
 *
 * Decomp: viceroy_unpacked.c ~89384–89870. Gate: local_34 (type ∈ [0x0d,0x12]).
 * Linux live scorer: ai_euro_ocean_score_step (thin distance + HS bias + combat).
 * Full cargo/colony-probe matrix and −0x6790 deep tables stay OPEN.
 *
 * Cite: move_scoring.md §20e6 band table; FUN_48d3_048e / 015e / 0434.
 */

#include "viceroy_types.h"

/* Prologue: local_34 = type∈[0x0d,0x12]; local_90 = ocean 0x19 / HS 0x1a. */

/*
 * LAB_521d_3558 — ship body entry.
 * Cargo hold scan (0be6/0c68) → goal probes (2a1f_0584/0464) → optional
 * 8-dir land-adj flag word (continent −0x6790 / 0492 / explore bits) used to
 * disembark founders via 06ae (2a1f_04ac sole 20e6 call ~89587).
 * Colony-list score (~89621) picks sail target when cargo wants port.
 */
int euro_ocean_ship_band_skeleton(int unit_index) {
  (void)unit_index;
  /* if (!is_ship_type) return 0; */
  /* scan holds; build local_9c flags; maybe 06ae unload; else score colonies */
  return 0;
}

/*
 * LAB_521d_3fa6 — thunk FUN_291f_02ea → FUN_48d3_015e.
 * Spiral-find nearest High Seas; set sail/goto when ship needs HS egress.
 */
int euro_ocean_spiral_hs_goto(int unit_index) {
  (void)unit_index;
  return 0;
}

/*
 * FUN_48d3_0434 — tile OK for HS place: in-bounds land/water plane, terrain
 * class == 0x1a (high seas), owner empty or own nation.
 * FUN_48d3_048e — expanding spiral from unit goto (landfall) until 0434 hits;
 * then teleport ship + sync cargo (Linux: units_spiral_place_hs_near).
 */
int euro_ocean_hs_place_ok(int x, int y) {
  (void)x;
  (void)y;
  return 0;
}
