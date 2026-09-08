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
#define COMBAT_FLAG_TORIES 0x0002u /* a156 bit1 — crown attacker uses 100−SoL */
#define COMBAT_FLAG_REBELS 0x0004u /* a156 bit2 — rebel attacker uses SoL */
#define COMBAT_FLAG_SOL COMBAT_FLAG_REBELS /* legacy alias */
#define COMBAT_FLAG_REF 0x8000u /* 0x8d01 bit7 — colony WoI +50% (crown or ref_present) */
#define COMBAT_FLAG_ARTY_COLONY 0x0001u /* a156 bit0 artillery vs natives */
/* Fatigue: attacker below one whole movement point (DOS 1b0e ~100374). */
#define COMBAT_FLAG_FATIGUE_33 0x0100u /* 0x8d01 bit0 — 2 thirds left, ×2/3 */
#define COMBAT_FLAG_FATIGUE_66 0x0008u /* a156 bit3 — 1 third left, ×1/3 */
/*
 * Capital village doubling marker. DOS carries it as 8d02 bit5, but the port's
 * flags2 word is a156-shaped (bits 0..3 = arty-colony / Tories / Rebels /
 * fatigue-66), so parking it on 0x0008 aliased a156 bit3 and made every attack
 * on a capital village print a phantom "Fatigue −66%" row. Relocated to the
 * first free flags2 bit; it is a render-time hint only (nothing serializes
 * flags2 into a save), so the bit value is port-internal.
 */
#define COMBAT_FLAG_VILLAGE_CAPITAL 0x0010u /* flags2 free bit (DOS 8d02 bit5) */

typedef struct ColonizeCombatSideFlags {
  uint16_t flags; /* low word (0x8d00 / 0x8d02) */
  uint16_t flags_hi; /* high extras: Drake 0x40, fortify 0x20, arty/ambush/REF */
  uint16_t flags2; /* a156-shaped: SoL / arty-colony */
  int base_combat; /* pre-×8 type combat byte (0x8d06) */
  int local_1a; /* 015e multiplier accumulator */
  int terrain_byte; /* DS:0x2f77 when terrain applies (to this side) */
  int terrain_stash; /* 015e→0x8d04: denied to defender, applied to attacker */
  int village_n; /* village defender: tribe tech level 0..3 (DOS indian+2) */
  int holds_occupied; /* subtracted holds (ships) */
  int sol_percent; /* WoI popular-support % applied */
  /*
   * Combat Analysis row chrome (FUN_636c_0000 draw pass). DOS re-derives these
   * inside the dialog from the colony / village / tile it is handed; the port
   * snapshots them here because 636c's caller context is gone by render time.
   *   fort_tier      FUN_157e_0008 — how many of Stockade/Fort/Fortress the
   *                  colony has (0..3). 636c colony row prints (tier+1)*50%
   *                  and labels itself with FUN_15eb_0434(0)'s building name.
   *   colony_icon    ICONS.SS settlement marker drawn beside that row
   *                  (FUN_112b_0c64 at scale 100); -1 = no row icon.
   *   colony_nation  nation whose flag that marker is recolored with.
   *   terrain_sprite TERRAIN.SS tile drawn beside the Ambush/Terrain row
   *                  (FUN_1baa_0006); -1 = none.
   *   bombard_icon   ICONS.SS icon on the WoI Bombard row: Man-O-War
   *                  (DS:0x532e = @UNIT 18 icon) over a coastal colony,
   *                  else Artillery (DS:0x52cc = @UNIT 11 icon); -1 = none.
   */
  int fort_tier;
  int colony_icon;
  int colony_nation;
  int terrain_sprite;
  int bombard_icon;
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
 * DOS `bVar28`: the engagement about to be resolved has a defender 1b0e
 * auto-spawned for an undefended settlement (colony militia / Paul Revere
 * phantom, or the empty-dwelling Brave), not a unit that stood on the map.
 * units.c raises this around the phantom's engage call; the 1b0e peels read
 * it for the Discoverer beginner shield (raw 100544).
 */
void combat_set_auto_defender(bool on);
bool combat_auto_defender(void);

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
 * FUN_5fef_1b0e peels on top of 157e strengths: artillery, Spanish ambush,
 * WoI colony REF +50% / Tory|Rebel %, crown open-field difficulty/20,
 * difficulty, Scout-vs-Arty forced lose. Fills io strengths+flags in place.
 */
void combat_apply_1b0e_peels(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* io
);

/*
 * FUN_5fef_1b0e difficulty-handicap group (raw 100534-100556): Discoverer/
 * Explorer attacker dampers vs a human European (+ the beginner shield via
 * combat_set_auto_defender) and the diff-0 human-attacker doubling. Sits
 * after DOS's `param_5 == 0` early return, so it must be applied only by the
 * real resolvers, AFTER Combat Analysis is presented — never by AI scoring.
 */
void combat_apply_1b0e_resolve_handicaps(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* io
);

/*
 * Full land engage: 004a(atk) scaled by ((8d04+4)*atk>>2)*3>>1 (always +50%
 * attack factor; 8d04 = terrain stash from 015e), 015e(def), then 1b0e peels.
 * Terrain stash (Indian vs Euro / human vs AI-Euro under WoI): defender gets
 * no site terrain; attacker strength absorbs 0x8d04 instead.
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
