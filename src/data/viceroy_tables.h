#ifndef COLONIZE_VICEROY_TABLES_H
#define COLONIZE_VICEROY_TABLES_H

#include <stdint.h>

/*
 * Static tables extracted from COLONIZE/VICEROY.EXE (DS-relative addresses from
 * original_sources_decompiled/viceroy_unpacked.c).
 * Regenerate with: python3 scripts/extract_viceroy_tables.py
 */

#define VICEROY_TERRAIN_COUNT 29
#define VICEROY_TERRAIN_META_SIZE 52u

/* DS segment offsets in the shipped DOS executable. */
#define VICEROY_DS_TERRAIN_META 0x543Fu

/*
 * terrain_meta[class_flag] (byte 0) values used by the DOS renderer:
 *   0   = cleared land / ocean / sea lane (TERRAIN.SS base path)
 *   1   = composited water layer (coast / ocean animation)
 *   2   = river overlay path
 *   7   = hills (PEDIA TERRAIN28)
 * 255  = mountains (PEDIA TERRAIN27)
 */
#define VICEROY_TERRAIN_CLASS_LAND 0u
#define VICEROY_TERRAIN_CLASS_WATER_LAYER 1u
#define VICEROY_TERRAIN_CLASS_RIVER 2u
#define VICEROY_TERRAIN_CLASS_HILLS 7u
#define VICEROY_TERRAIN_CLASS_MOUNTAIN 255u

extern const uint8_t viceroy_terrain_meta[VICEROY_TERRAIN_COUNT][VICEROY_TERRAIN_META_SIZE];

static inline uint8_t viceroy_terrain_class(int terrain_index) {
  if (terrain_index < 0 || terrain_index >= VICEROY_TERRAIN_COUNT) {
    return 0;
  }
  return viceroy_terrain_meta[terrain_index][0];
}

/*
 * PHYS0 canopy for forest terrain (map index & 7). Scrub (1) is TERRAIN-only.
 * Canopies are PHYS0.SS mixed-forest sprites 64–79 (not mountain 32–47 or
 * resource 96–99). Sprite 40 is a mountain variant; 99 is timber.
 */
static inline int viceroy_forest_phys0_sprite(int forest_type) {
  switch (forest_type) {
    case 0:
      return 70; /* boreal */
    case 1:
      return -1;
    case 2:
      return 64;
    case 3:
      return 65;
    case 4:
      return 66;
    case 5:
      return 69; /* tropical */
    case 6:
      return 67;
    case 7:
      return 68;
    default:
      return -1;
  }
}

#endif
