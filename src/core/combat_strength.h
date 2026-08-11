#ifndef COLONIZE_COMBAT_STRENGTH_H
#define COLONIZE_COMBAT_STRENGTH_H

#include <stdbool.h>
#include <stdint.h>

#include "core/col1_save.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/units.h"

/*
 * DOS FUN_157e_004a / FUN_157e_015e combat strength peels.
 * Flag bits mirror DS 0x8d00/0x8d02 (and fortify high byte) for Combat Analysis.
 * Cite: viceroy_unpacked.c 8914–9051; FUNCTION_CATALOG.
 */

/* Per-side modifier flags (0x8d00 / 0x8d02 shaped). */
#define COMBAT_FLAG_MODE_ATK 0x0001u /* 004a mode!=0 */
#define COMBAT_FLAG_VETERAN 0x0002u
#define COMBAT_FLAG_HOLDS 0x0004u
#define COMBAT_FLAG_COLONY 0x0008u
#define COMBAT_FLAG_STOCKADE 0x0010u /* Stockade+ tier */
#define COMBAT_FLAG_FORTRESS 0x0020u
#define COMBAT_FLAG_VILLAGE 0x0040u
#define COMBAT_FLAG_DRAKE 0x0040u /* high-byte bit6 on 004a Drake path (0x8d01) */
#define COMBAT_FLAG_TERRAIN 0x0080u
#define COMBAT_FLAG_FORTIFY 0x2000u /* 0x8d03 bit5 → expose as line bit */
#define COMBAT_FLAG_ARTILLERY 0x0800u /* open-field >>2 (0x8d01/03 bit3) */
#define COMBAT_FLAG_AMBUSH 0x1000u /* Spanish +50% (0x8d01 bit4) */
#define COMBAT_FLAG_SOL 0x0002u /* a156 SoL band — stored in flags2 */
#define COMBAT_FLAG_REF 0x8000u /* crown/REF +50% (0x8d01 bit7) */
#define COMBAT_FLAG_ARTY_COLONY 0x0001u /* a156 bit0 artillery vs natives */

typedef struct ColonizeCombatSideFlags {
  uint16_t flags; /* low word (0x8d00 / 0x8d02) */
  uint16_t flags_hi; /* high extras: Drake 0x40, fortify 0x20, arty/ambush/REF */
  uint16_t flags2; /* a156-shaped: SoL / arty-colony */
  int base_combat; /* pre-×8 type combat byte (0x8d06) */
  int local_1a; /* 015e multiplier accumulator */
  int terrain_byte; /* DS:0x2f77 when terrain applies */
  int village_n; /* 0..3 settlement probes */
  int holds_occupied; /* subtracted holds (ships) */
  int sol_percent; /* WoI popular-support % applied */
} ColonizeCombatSideFlags;

typedef struct ColonizeCombatStrengthCtx {
  const ColonizeUnitPool* units;
  const ColonizeWorldMap* map;
  const ColonizeColonyPool* colonies;
  const ColonizeCol1Save* col1;
} ColonizeCombatStrengthCtx;

typedef struct ColonizeCombatEngageResult {
  int atk_strength;
  int def_strength;
  bool force_defender_wins; /* Scout vs Artillery */
  ColonizeCombatSideFlags atk_flags;
  ColonizeCombatSideFlags def_flags;
} ColonizeCombatEngageResult;

void combat_side_flags_clear(ColonizeCombatSideFlags* f);

/*
 * FUN_157e_004a: mode 0 → type.defense (5235); mode 1 → type.attack (5236).
 * Returns scaled strength (combat×8 + vet/Drake/holds). out_flags optional.
 */
int combat_unit_base_x8(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int mode,
  ColonizeCombatSideFlags* out_flags
);

/*
 * FUN_157e_015e: engagement strength for unit vs foe (colony/village/terrain/
 * fortify). Calls 004a(unit, 0) for base. out_flags optional.
 */
int combat_engagement_strength(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int foe_id,
  ColonizeCombatSideFlags* out_flags
);

/*
 * FUN_5fef_1b0e peels on top of 157e strengths: artillery, ambush, SoL, REF,
 * difficulty, Scout-vs-Arty forced lose. Fills io strengths+flags in place.
 */
void combat_apply_1b0e_peels(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* io
);

/*
 * Full land engage: 004a(atk)+015e(def)+1b0e peels.
 */
void combat_land_engage(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* out
);

/*
 * Naval engage: 004a both + 1b0e difficulty/REF peels that apply to ships.
 */
void combat_naval_engage(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* out
);

/*
 * Effective toughness for AI scoring: engagement strength when foe_id>=0,
 * else base×8 defense mode. Same numbers as resolve.
 */
int combat_unit_toughness(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int foe_id
);

/* Type helpers used by best-defender / outcome. */
int combat_type_is_artillery_name(const char* name);
int combat_type_is_scout_name(const char* name);
int combat_unit_is_combat_role(const ColonizeUnitPool* pool, int unit_id);

#endif
