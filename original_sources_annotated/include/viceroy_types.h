/*
 * VICEROY runtime layouts used by AI (phase 1).
 *
 * Offsets match the live DS unit/tribe arrays seen in
 * original_sources_decompiled/viceroy_unpacked.c and the Col1 save unit
 * record in src/core/col1_save.h (28-byte / 0x1c stride).
 *
 * Reference only — not compiled into the Linux binary.
 */
#ifndef VICEROY_TYPES_H
#define VICEROY_TYPES_H

#include <stdint.h>

/* ---- Map planes -------------------------------------------------------- */

/* Terrain byte (map plane 0): low 5 = type; bit 0x20 = hill; bit 0x40 = minor river. */
#define VICEROY_TERRAIN_TYPE_MASK   0x1fu
#define VICEROY_TERRAIN_HILL_BIT    0x20u
#define VICEROY_TERRAIN_RIVER_BIT   0x40u
#define VICEROY_TERRAIN_OCEAN       0x19u
#define VICEROY_TERRAIN_HIGH_SEAS   0x1au

/*
 * Layer2 / fa-mask bits used by AI costing (FUN_281f_0754 / FUN_137f_0142).
 * DOS TEST AL,0x0a — tribe (0x02) or Col1 road-like (0x08).
 */
#define VICEROY_LAYER2_PRESENCE     0x01u /* FUN_137f_0314 bit0 gate */
#define VICEROY_LAYER2_TRIBE        0x02u
#define VICEROY_LAYER2_FA_ROAD      0x08u
#define VICEROY_LAYER2_FA_MASK      0x0au

/* Layer3: low nibble = continent id; high nibble = owner (0xf = unowned). */
#define VICEROY_OWNER_UNOWNED       0x0fu

/* ---- Unit record (DS base 0x3144, stride 0x1c) -------------------------- */

#define VICEROY_UNIT_STRIDE         0x1c
#define VICEROY_UNIT_ACT_MAX        0x15  /* per-turn act counter ceiling in 1816 */

/*
 * Layout aligns with ColonizeCol1Unit. Field names that are still RE-soft
 * stay as unk_* / comments.
 *
 * Absolute addresses in VICEROY DS:
 *   units[i].x            → 0x3144 + i*0x1c
 *   units[i].type         → 0x3146 + i*0x1c
 *   units[i].nation_lo    → 0x3147 + i*0x1c  (low nibble)
 *   units[i].moves_spent  → 0x3149 + i*0x1c
 *   units[i].act_counter  → 0x315a + i*0x1c  (Col1 turns_worked)
 */
typedef struct ViceroyUnit {
  uint8_t x;              /* +0x00 */
  uint8_t y;              /* +0x01 */
  uint8_t type;           /* +0x02 — Brave=19, ships 0x0a..0x0c, … */
  uint8_t nation_id : 4;  /* +0x03 low nibble */
  uint8_t unused_hi : 4;  /* +0x03 high nibble */
  uint8_t unk_04;         /* +0x04 — flags (bit 0x80 checked in unit_has_moves) */
  uint8_t moves_spent;    /* +0x05 — compared to max MP allotment */
  uint8_t unk_06[2];      /* +0x06 */
  uint8_t orders;         /* +0x08 */
  uint8_t goto_x;         /* +0x09 */
  uint8_t goto_y;         /* +0x0a */
  uint8_t unk_0b;         /* +0x0b */
  uint8_t holds_occupied; /* +0x0c */
  uint8_t cargo_nibbles[3]; /* +0x0d..0x0f packed cargo types */
  uint8_t cargo_hold[6];  /* +0x10..0x15 */
  uint8_t act_counter;    /* +0x16 — reset each nation pulse; bumps per act */
  uint8_t profession;     /* +0x17 */
  int16_t transport_next; /* +0x18 */
  int16_t transport_prev; /* +0x1a */
} ViceroyUnit;

_Static_assert(sizeof(ViceroyUnit) == VICEROY_UNIT_STRIDE, "ViceroyUnit stride");

/* Unit type constants used by Euro dispatcher ship/land split. */
#define VICEROY_UNIT_TYPE_SOLDIER   0x01
#define VICEROY_UNIT_TYPE_PIONEER   0x02
#define VICEROY_UNIT_TYPE_SHIP_A    0x0a
#define VICEROY_UNIT_TYPE_SHIP_B    0x0b
#define VICEROY_UNIT_TYPE_SHIP_C    0x0c
#define VICEROY_UNIT_TYPE_BRAVE     19

/* ---- Tribe record (DS base 0x54ee, stride 0x12) ------------------------- */

#define VICEROY_TRIBE_STRIDE        0x12

typedef struct ViceroyTribe {
  uint8_t x;
  uint8_t y;
  uint8_t nation_id;      /* compared to g_active_nation_id in 1816 */
  uint8_t state;          /* capital / scouted / … bitfield */
  uint8_t population;
  uint8_t unk_05;
  uint8_t growth_accum;   /* FUN_4d56_152e accumulator; overflow at 0x13 */
  uint8_t unk_07[11];
} ViceroyTribe;

_Static_assert(sizeof(ViceroyTribe) == VICEROY_TRIBE_STRIDE, "ViceroyTribe stride");

/* Village growth threshold in FUN_4d56_152e (`'\x13' < accum`). */
#define VICEROY_VILLAGE_GROWTH_THRESHOLD  19

/* ---- Directions (8-way + stay) ----------------------------------------- */

/* Matching Linux k_ai_dir8_dx/dy in src/core/ai.c. */
enum {
  VICEROY_DIR_N = 0,
  VICEROY_DIR_NE,
  VICEROY_DIR_E,
  VICEROY_DIR_SE,
  VICEROY_DIR_S,
  VICEROY_DIR_SW,
  VICEROY_DIR_W,
  VICEROY_DIR_NW,
  VICEROY_DIR_STAY = 8
};

#endif /* VICEROY_TYPES_H */
