#ifndef COLONIZE_VICEROY_TABLES_H
#define COLONIZE_VICEROY_TABLES_H

#include <stdint.h>

/*
 * Static tables extracted from COLONIZE/VICEROY.EXE (DS-relative addresses from viceroy.c).
 * Regenerate with: python3 scripts/extract_viceroy_tables.py
 */

#define VICEROY_TERRAIN_COUNT 29
#define VICEROY_TERRAIN_META_SIZE 52u
#define VICEROY_TILE_DISPLAY_COUNT 29
#define VICEROY_TILE_DISPLAY_SIZE 14u
#define VICEROY_RIVER_TRANSITION_COUNT 28
#define VICEROY_CONNECTIVITY_TRANSITION_COUNT 21

/* DS segment offsets in the shipped DOS executable. */
#define VICEROY_DS_TERRAIN_META 0x543Fu
#define VICEROY_DS_TILE_DISPLAY 0x5234u
#define VICEROY_DS_RIVER_TRANSITION 0x54DEu
#define VICEROY_DS_FEATURE_BASES_A 0x54FCu
#define VICEROY_DS_FEATURE_BASES_B 0x5502u
#define VICEROY_DS_CONNECTIVITY_TRANSITION 0x5599u

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

/*
 * tile_display records are indexed by tile display type (runtime field 0x3146).
 * Bytes 1 and 2 are PHYS0 sprite groups; multiply by 8 for the atlas index.
 */
#define VICEROY_TILE_DISPLAY_OFF_BASE 0u
#define VICEROY_TILE_DISPLAY_OFF_SPRITE_A 1u
#define VICEROY_TILE_DISPLAY_OFF_SPRITE_B 2u
#define VICEROY_TILE_DISPLAY_OFF_MAX_LAYERS 3u

extern const uint32_t viceroy_ds_to_file_offset;
extern const uint8_t viceroy_terrain_meta[VICEROY_TERRAIN_COUNT][VICEROY_TERRAIN_META_SIZE];
extern const uint8_t viceroy_tile_display[VICEROY_TILE_DISPLAY_COUNT][VICEROY_TILE_DISPLAY_SIZE];
extern const uint8_t viceroy_river_transition[VICEROY_RIVER_TRANSITION_COUNT];
extern const uint8_t viceroy_feature_sprite_bases_a[4];
extern const uint8_t viceroy_feature_sprite_bases_b[4];
extern const uint8_t viceroy_connectivity_transition[VICEROY_CONNECTIVITY_TRANSITION_COUNT];

static inline uint8_t viceroy_terrain_class(int terrain_index) {
  if (terrain_index < 0 || terrain_index >= VICEROY_TERRAIN_COUNT) {
    return 0;
  }
  return viceroy_terrain_meta[terrain_index][0];
}

static inline int viceroy_tile_display_sprite_a(int display_type) {
  if (display_type < 0 || display_type >= VICEROY_TILE_DISPLAY_COUNT) {
    return -1;
  }
  return (int)viceroy_tile_display[display_type][VICEROY_TILE_DISPLAY_OFF_SPRITE_A] * 8;
}

static inline int viceroy_tile_display_sprite_b(int display_type) {
  if (display_type < 0 || display_type >= VICEROY_TILE_DISPLAY_COUNT) {
    return -1;
  }
  return (int)viceroy_tile_display[display_type][VICEROY_TILE_DISPLAY_OFF_SPRITE_B] * 8;
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
