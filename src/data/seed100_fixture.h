#ifndef COLONIZE_SEED100_FIXTURE_H
#define COLONIZE_SEED100_FIXTURE_H

/*
 * Locked constants for golden_mapgen_seed100 vs test-saves-mapgen/SEED100.SAV.
 * These are expected-value anchors only — runtime NEW WORLD never special-cases
 * seed 100; DOS LCG + FUN_684c_08c0 / FUN_6a09 must reproduce them for any seed.
 */

#define SEED100_MAP_W 58
#define SEED100_MAP_H 72
#define SEED100_TRIBE_COUNT 34
#define SEED100_UNIT_COUNT 46
#define SEED100_SEED 100u

/* DOS LCG: seed(100) → five FUN_19ef_0032(0,3) = NEW WORLD axes (not 0..2). */
#define SEED100_LAND_MASS 0
#define SEED100_LAND_FORM 0
#define SEED100_TEMPERATURE 0
#define SEED100_CLIMATE 2
#define SEED100_FOREST_EXTRA 2

/* Human England stack tile in the golden save. */
#define SEED100_HUMAN_X 43
#define SEED100_HUMAN_Y 28

#endif
