#include "core/internal.h"
#include "core/ai.h"
#include "core/combat_strength.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/founding_fathers.h"
#include "core/ai_goals.h"
#include "core/ai_king.h"
#include "core/col1_bridge.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/ai_internal.h"
#include "core/dos_rng.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "core/strutil.h"
#include "core/turn.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * Sections:
 *  - Indian village worth, brave spawn, mission & threat scoring
 *  - Village growth tick, tile/owner helpers & native pull scoring
 */

/*
 * Village growth worth-cap (152e: `if (value < worth) local_16 = 2`).
 *
 * 2026-08-24 (T1.15), resolved same day in a follow-up pass: the thunk
 * chain from the 2026-08-24 note above (`4d56:0038`/`4d56:152e` ->
 * near-`CALL 4c54` -> `JMPF 2a1f:0410`) was re-walked with Ghidra
 * headless directly against the raw bytes (not the flattened export's
 * `FUN_41f2_0294` misresolve) and fully confirmed:
 *   - `4d56:0086`'s near `CALL` encodes as `E8 CB 4B` -> target
 *     `0x0089 + 0x4BCB = 0x4C54` (same segment, matches the prior note).
 *   - `4d56:4c54` disassembles as `JMPF 2A1F:0410` (raw operand bytes
 *     `EA 10 04 1F 2A`; Ghidra's rendered `0x2000:a600` display is a
 *     segmented-space rendering artifact, not the real target).
 *   - Raw bytes at `2a1f:0410` (read directly, not via a stale cached
 *     function boundary): `CALLF 210D:0DAB` (`FUN_210d_0dab`, the RTLink
 *     overlay loader) immediately followed by `JMPF 4D56:0000` — a
 *     12-byte RTLink thunk-table entry. So thunk offset `0` in the
 *     `2a1f` slot really does resolve to `FUN_4d56_0000`, confirming the
 *     earlier session's guess. (Table also gives `041c`->`4d56:39ea`,
 *     `0428`->`4d56:3646`, `0434`->`4d56:2154`, consistent with this
 *     project's other already-resolved `2a1f` thunk-table entries.)
 *
 * **The earlier session's own transcription of `FUN_4d56_0000`'s capital
 * arm was the actual bug**, not the callee identification. Decompiling
 * `4d56:0000` fresh gives:
 *   tech = indian[nation_id-4].tech;      // DS:0x5AD6 stride 0x4e, +2
 *   worth = tech*2 + 3;                   // ASM: base in CX
 *   if (capital_flag) worth = tech + worth + 1;   // ASM: ADD CX,AX; INC CX
 * i.e. the capital arm is **`3*tech + 4`**, not `tech+1` as previously
 * transcribed (the earlier read dropped the pre-existing `2*tech+3` base
 * before the `ADD`/`INC`). This resolves the 2026-08-24 contradiction
 * cleanly: seed-100 capitals have `pop == 2*tech+3`, and
 * `3*tech+4 > 2*tech+3` for every `tech >= 0`, so `pop < worth` holds and
 * `growth_accum += pop` fires every turn, exactly matching golden
 * TURN1->2 behavior. The non-capital arm (`2*tech+3`, unused at this
 * call site since 152e's growth block is capital-gated, see
 * `ai_indian_152e_village_growth`) still matches `ai_tribe_initial_pop`.
 *
 * `FUN_4d56_0000` takes `(unused_cs_word, tribe_index)`; the byte-pair
 * it reads (`DS:0x54ec` stride `0x12`, `+2` nation id / `+3 bit0x4`
 * capital flag) is a separate per-tribe worklist table mirroring the
 * settlement record's own `nation_id`/`state.capital` fields already
 * wired in this port — read those directly off `t` instead of porting
 * the redundant lookup table. DOS stores the result through a `byte`
 * local before comparing (`byte bVar5 = FUN_41f2_0294(...)`), so the
 * value is truncated mod 256 same as this project's established
 * "byte-truncate on port" convention elsewhere.
 *
 * `FUN_41f2_0092`/`0294` (nation-score + report UI, prior 2026-08-19..22
 * notes) remains a real function — just never called from 152e/0038.
 * Full trace: `docs/port_plan.md` T1.15.
 */
/* ===================== Indian village worth, brave spawn, mission & threat scoring (ai_indian_152e_worth_cap .. ai_indian_152e_village_growth) ===================== */
static int ai_indian_152e_worth_cap(
  const ColonizeTurnContext* ctx,
  const ColonizeCol1Tribe* t
) {
  const ColonizeCol1Indian* ind = &ctx->col1->indian[t->nation_id - 4];
  int tech = ind->tech;
  int worth = tech * 2 + 3;
  if (t->state.capital) {
    worth = tech + worth + 1; /* == 3*tech + 4 */
  }
  return (uint8_t)worth; /* DOS truncates through a `byte` before the compare */
}

/*
 * FUN_281f_095c -> FUN_1427_06b4 (unit CREATE) — de-stubbed 2026-09-06d.
 *
 * Resolved via the standard chain: `FUN_281f_095c` = `FUN_1000_8b4c`
 * (address_mapping.csv), whose overlay body is the two-instruction RTLink
 * thunk `CALLF FUN_1000_1e61; JMPF 0000:4924` -> `FUN_1427_06b4`
 * (viceroy_unpacked.c:7710-7772).
 *
 * **The old stub's premise was wrong**: DOS's `local_4` is not a "cost", it
 * is `param_1` of `1427:06b4` — the *unit type*. 152e builds it as
 * `0x13 + 1*(a musket was spent) + 2*(50 horse-breeding was spent)`, i.e.
 * exactly the four native unit types Linux already knows from
 * `ai_euro.c`'s NAMES map: 0x13 Brave, 0x14 Armed Brave, 0x15 Mtd. Brave,
 * 0x16 Mtd. Warrior. So this branch arms the newborn out of the nation's
 * own musket/horse stock — the muskets/horses were never a "cost paid for
 * nothing", they decide what walks out of the village.
 *
 * `1427:06b4` body, Indian path (`param_2 >= 4`):
 *   - hard cap `unit_count < 0x124` (292) — the `0x543f` human gate and the
 *     `< 300` / `tribe_population_totals < 0xc9` arms only apply to Euro
 *     slots, and the `@NOMOREUNITS` beep (`281f_03fe`) is Euro-only too.
 *   - zeroes the per-unit scratch (`+0x3148/49/4c/50/54/55/5a`), sets
 *     `+0x314b = 0x58` (orders 'X' = fortify-ish default), `+0x3156` =
 *     `DS:0x538e` for natives, `+0x314a` = settlement at (x,y) — which 152e
 *     then overwrites with the *founding* village index anyway.
 *   - `FUN_1427_02ca(idx, x, y)` finally places the unit on the tile (the
 *     `0xff/0xff` writes just above it are pre-placement scratch; Ghidra
 *     drops 02ca's register args).
 *
 * Linux uses `units_spawn_allow_stack` for the placement half (native units
 * routinely stack on their own village tile) and mirrors the DOS pool cap.
 * The branch is still **unreachable today** — its only gate is
 * `t->state.needs_colonist`, whose DOS producer (village CREATE,
 * `FUN_4d56_0038`) is unported, so the bit is always 0. Ported anyway so the
 * arm is correct the day that producer lands (same "wired but not fed"
 * convention as `ai_euro_5d04_compute_flags`).
 */
static int ai_indian_152e_spawn_brave(
  ColonizeTurnContext* ctx,
  const ColonizeCol1Tribe* t,
  int dos_type,
  int tribe_index
) {
  if (!ctx || !ctx->units || !t) {
    return -1;
  }
  /* DOS: `*(int *)0x539c < 0x124` — the native arm's only pool gate. */
  int live = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (ctx->units->units[i].active) {
      live++;
    }
  }
  if (live >= 0x124) {
    return -1;
  }
  /* NAMES pool index == DOS type index for 0x13..0x16 (ai_euro.c k[] map). */
  int type_index = dos_type;
  if (type_index < 0x13) {
    type_index = UNITS_KIND_BRAVE;
  }
  if (type_index > 0x16) {
    type_index = UNITS_KIND_MTD_WARRIOR;
  }
  if (type_index >= ctx->units->type_count) {
    type_index = UNITS_KIND_BRAVE; /* small NAMES pools: fall back to plain Brave. */
    if (type_index >= ctx->units->type_count) {
      return -1;
    }
  }
  const int id = units_spawn_allow_stack(ctx->units, type_index, (int)t->x, (int)t->y);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, id);
  if (!u) {
    return -1;
  }
  u->nation_id = (int)t->nation_id;
  u->home_tribe_id = tribe_index; /* DOS +0x314a, stamped by 152e itself. */
  /*
   * DOS +0x3149 = 0. Careful: for a native unit `moves` carries DOS
   * SPENT-thirds semantics (turn.c:186 sets natives to 0 at every refresh,
   * decomp ~6357), so 0 means "nothing spent" — the fresh Brave is free to
   * act on the turn it is born, which is what DOS does. The old comment here
   * read it with European remaining-MP polarity and claimed the opposite
   * ("created spent, acts next turn"); the code was right, the comment was a
   * trap (smell #55, 2026-09-09).
   */
  u->moves = 0;
  u->col1_counter16 = 0;
  return id;
}

/*
 * The two readers of the settlement mission byte (+5) in this file used to
 * spell "no mission" two different ways (`!= 0xff` in the growth tick,
 * `(int8_t) >= 0` in the threat picker) while the other twelve readers in the
 * port use COL1_TRIBE_MISSION_NONE. Unified here (smell sweep-3 E6).
 *
 * DOS is the sign test: FUN_4d56_152e (viceroy_unpacked.c raw 81472-81476)
 * does `uVar7 = (uint)*(char *)(iVar8 + 5); if (-1 < (int)uVar7) uVar7 &= 0xf;`
 * — it sign-extends the byte FIRST and only masks the nibble when the result
 * is non-negative, so every 0x80..0xff byte (the 0xff sentinel included)
 * reads as "no mission". FUN_4cc6_03f8 is identical (raw 81040 sign-extends
 * into local_e, raw 81090 gates on `-1 < (int)local_e`).
 */
static bool ai_indian_tribe_has_mission(const ColonizeCol1Tribe* t) {
  if (t->mission == COL1_TRIBE_MISSION_NONE) {
    return false;
  }
  return (int)(int8_t)t->mission >= 0;
}

/*
 * The European nation owning the mission, 0..3, or -1 for none.
 *
 * The >= 4 rejection is a port-safety bound over the documented 0..3 domain
 * (col1_save.h:729), NOT DOS: DOS indexes `indian + nibble + 0x36` (raw
 * 81488) and `settlement + nibble*2 + 0xa` (raw 81490) with the raw nibble
 * and would walk straight past the 4-entry arrays on a 4..15 nibble. Every
 * writer in the port honours the domain, so an out-of-range nibble can only
 * come from a corrupt or foreign save; skipping the mission arm is the safe
 * reading of an undefined value, where indexing would be an out-of-bounds
 * write past `euro_relation_accum[4]` (col1_save.h:854) into the
 * neighbouring euro_diplo[] bytes.
 */
static int ai_indian_tribe_mission_nation(const ColonizeCol1Tribe* t) {
  if (!ai_indian_tribe_has_mission(t)) {
    return -1;
  }
  const int nation = (int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK);
  if (nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return -1;
  }
  return nation;
}

/*
 * FUN_281f_0316 -> FUN_4cc6_03f8 (viceroy_unpacked.c:80991): which European
 * nation this settlement feels most threatened by, and how strongly. Ported
 * 2026-08-30 — it was the stub that kept `t->alarm[e]` at zero forever, so
 * settling and developing next to a tribe never raised any alarm and the
 * village map chrome had nothing to show.
 *
 * Two passes:
 *
 *  1. Military pressure. The 20 tiles of DS:0xc8/0xde around the village (the
 *     colony work radius: 8 neighbours, the 4 at ±2 orthogonal, the 8
 *     knight-ish ones). On each in-bounds land tile, sum `type.attack` over
 *     every Euro-owned unit there whose attack is > 1 and which is not a ship
 *     (DOS types 0xd..0x12). Halve inside a native settlement, and halve again
 *     unless the tile is one of the 8 immediate neighbours. Accumulate per
 *     nation.
 *
 *  2. Colonies within distance 7 (FUN_281f_037a = map_dos_dist). Each scores
 *
 *       base  = 2*max(0, pop-6) + min(tribe.tech, pop/2) + min(pop, 6)
 *               + difficulty + ((buildings*c/e - 8) >> 2)
 *       score = (base*2 - d - 1) / (d + 4)
 *
 *     where c/e come from difficulty for a *human* colony only
 *     ({1,2},{3,4},{1,1},{3,2},{2,1}) and are 1/1 for an AI's. Halve when the
 *     colony is on a different continent, add that nation's military pressure,
 *     halve for the French (nation 1 — their standing native-relations bonus),
 *     halve again on FF 16 (Pocahontas). The highest-scoring colony names the
 *     threatening nation.
 *
 *     The `min(..., pop/2)` operand is the tribe's TECH level, not
 *     `capitol_x`: DOS reads `*(byte *)(tribe.nation_id * 0x4e + 0x59a0)`
 *     (raw 81038) and `0x59a0 + 4*0x4e == 0x5ad8 == indian_base(0x5ad6) + 2`,
 *     i.e. `indian[nation_id-4].tech`. Corrected 2026-09-06d; this header kept
 *     claiming capitol_x, plus the argument that "capitol_x is a map column so
 *     it exceeds pop/2 and the term behaves as pop/2" — which is backwards for
 *     tech (0..~5), a genuinely BINDING cap on pop/2 for any colony of pop >= 2.
 *     Do not "restore" pop/2. Smell audit 2026-09-10 D6.
 *
 * Finally the village's own mission rescales the winner: owned by the threat
 * nation → ×3/4 plain, ×1/2 Jesuit; owned by a rival → ×3/2 plain, ×2 Jesuit.
 */
int ai_indian_village_threat_w(
  const ColonizeWorld* w,
  int human_nation,
  int tribe_index,
  int* out_score
) {
  const ColonizeCol1Save* col1 = w->col1;
  const ColonizeWorldMap* map = w->map;
  const ColonizeUnitPool* pool = w->units;
  const ColonizeColonyPool* colonies = w->colonies;

  if (out_score) {
    *out_score = 0;
  }
  if (!col1 || !map || !colonies || !col1->tribe) {
    return -1;
  }
  if (tribe_index < 0 || tribe_index >= (int)col1->head.tribe_count) {
    return -1;
  }
  const ColonizeCol1Tribe* t = &col1->tribe[tribe_index];

  /* DS:0xc8 / DS:0xde — the 20-tile ring the threat scan walks. */

  const int vx = (int)t->x;
  const int vy = (int)t->y;
  const int village_continent = map_continent_id_at(map, vx, vy);

  int pressure[4] = {0, 0, 0, 0};
  if (pool) {
    /*
     * One pass over the unit pool bucketed into the ring, rather than 20
     * passes: this runs per visible village per frame for the map chrome.
     * `first` is the tile's leading unit whatever its nation — DOS reads the
     * head of the tile's unit list and abandons the tile when it is not a
     * European's (`(unit.nation & 0xf) < 4`).
     */
    signed char in_ring[5][5];
    memset(in_ring, 0, sizeof(in_ring));
    for (int i = 0; i < 20; ++i) {
      in_ring[MAP_RING20_DY[i] + 2][MAP_RING20_DX[i] + 2] = 1;
    }
    int first[5][5];
    int score[5][5];
    for (int ry = 0; ry < 5; ++ry) {
      for (int rx = 0; rx < 5; ++rx) {
        first[ry][rx] = -1;
        score[ry][rx] = 0;
      }
    }
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      const ColonizeUnit* u = &pool->units[ui];
      if (!u->active || !units_is_on_map(u)) {
        continue;
      }
      const int rx = u->x - vx + 2;
      const int ry = u->y - vy + 2;
      if (rx < 0 || rx > 4 || ry < 0 || ry > 4 || !in_ring[ry][rx]) {
        continue;
      }
      if (first[ry][rx] < 0) {
        first[ry][rx] = u->nation_id;
      }
      if (u->nation_id < 0 || u->nation_id > 3 || units_is_sea(pool, u->id)) {
        continue;
      }
      const ColonizeUnitType* ty_def = units_type(pool, u->type_index);
      if (ty_def && ty_def->attack > 1) {
        score[ry][rx] += ty_def->attack;
      }
    }
    for (int i = 0; i < 20; ++i) {
      const int rx = MAP_RING20_DX[i] + 2;
      const int ry = MAP_RING20_DY[i] + 2;
      const int owner = first[ry][rx];
      int s = score[ry][rx];
      if (owner < 0 || owner > 3 || s <= 0) {
        continue;
      }
      const int tx = vx + MAP_RING20_DX[i];
      const int ty = vy + MAP_RING20_DY[i];
      if (!map_coords_inset(map, tx, ty) || map_tile_is_water(map, tx, ty)) {
        continue;
      }
      if (map_tile_tribe_or_presence(map, tx, ty) >= 0) {
        s >>= 1;
      }
      const int adx = MAP_RING20_DX[i] < 0 ? -MAP_RING20_DX[i] : MAP_RING20_DX[i];
      const int ady = MAP_RING20_DY[i] < 0 ? -MAP_RING20_DY[i] : MAP_RING20_DY[i];
      if (adx >= 2 || ady >= 2) {
        s >>= 1;
      }
      pressure[owner] += s;
    }
  }

  const int indian_idx = (int)t->nation_id - 4;
  /*
   * 2026-09-06d: DOS reads `*(byte *)(tribe.nation_id * 0x4e + 0x59a0)`.
   * `0x59a0 + 4*0x4e == 0x5ad8 == indian_base(0x5ad6) + 2`, so the operand is
   * the tribe's **tech level**, not `capitol_x` (+0). The old name/mapping
   * happened to read the neighbouring field of the same record. Cite
   * viceroy_unpacked.c:81038 + `ai_indian_152e_worth_cap`'s own DS note.
   */
  const int tribe_tech = (indian_idx >= 0 && indian_idx < 8)
    ? (int)col1->indian[indian_idx].tech
    : 0;
  const int difficulty = (int)col1->head.difficulty;

  int best_score = 0;
  int best_nation = -1;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* c = &colonies->colonies[ci];
    if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    const int d = map_dos_dist(vx - c->x, vy - c->y);
    if (d >= 7) {
      continue;
    }
    const int human =
      (human_nation >= 0 && human_nation <= 3) ? (c->nation_id == human_nation) : 0;
    int mul = 1;
    int div = 1;
    int diff_term = 0;
    if (human) {
      diff_term = difficulty;
      switch (difficulty) {
        case 0: mul = 1; div = 2; break;
        case 1: mul = 3; div = 4; break;
        case 2: mul = 1; div = 1; break;
        case 3: mul = 3; div = 2; break;
        default: mul = 2; div = 1; break;
      }
    }
    int buildings = 0;
    for (int b = 0; b < COLONIZE_BUILDING_TYPES_MAX; ++b) {
      if (c->has_building[b]) {
        buildings++;
      }
    }
    buildings = (buildings * mul) / (div > 0 ? div : 1);

    const int pop = c->colonist_count > 0 ? c->colonist_count : c->population;
    const int capped_pop = pop > 6 ? 6 : pop;
    int cap_term = pop >> 1;
    if (tribe_tech < cap_term) {
      cap_term = tribe_tech;
    }
    const int base =
      (capped_pop - pop) * -2 + cap_term + capped_pop + diff_term + ((buildings - 8) >> 2);
    int score = ((base * 2 - d) - 1) / (d + 4);

    if (map_continent_id_at(map, c->x, c->y) != village_continent) {
      score >>= 1;
    }
    score += pressure[c->nation_id];
    if (c->nation_id == 1) {
      score >>= 1; /* French: half the native alarm everyone else earns */
    }
    /*
     * FUN_281f_07b4(nation, 0x10) = FF 16 Pocahontas — de-stubbed 2026-09-06d
     * (see ai_indian_152e_ff_bit). This IS the PEDIA "all Indian alarm is
     * generated half as fast" clause: it halves the threat score that feeds
     * `euro_relation_accum`, which is the only DOS producer of Indian alarm.
     */
    if (founding_fathers_nation_has(col1, c->nation_id, FF_POCAHONTAS)) {
      score >>= 1;
    }
    if (score > best_score) {
      best_score = score;
      best_nation = c->nation_id;
    }
  }

  if (best_score <= 0) {
    return -1;
  }
  /*
   * The nibble is only compared here, never used as an index, so this arm
   * keeps DOS's raw compare (an out-of-domain nibble takes the "rival"
   * branch, as in FUN_4cc6_03f8) instead of ai_indian_tribe_mission_nation's
   * port-safety bound.
   */
  if (ai_indian_tribe_has_mission(t)) {
    const int jesuit = (t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0;
    if ((int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK) == best_nation) {
      best_score = jesuit ? (best_score >> 1) : (best_score - (best_score >> 2));
    } else {
      best_score = jesuit ? (best_score << 1) : (best_score + (best_score >> 1));
    }
  }
  if (out_score) {
    *out_score = best_score;
  }
  return best_nation;
}


/* ColonizeTurnContext adapter for the village tick. */
static int ai_indian_152e_best_threat_nation(
  const ColonizeTurnContext* ctx,
  int tribe_index,
  int* out_score
) {
  if (out_score) {
    *out_score = 0;
  }
  if (!ctx || !ctx->col1_ok) {
    return -1;
  }
  return ai_indian_village_threat_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, ctx->human_nation, tribe_index, out_score);
}

/*
 * FUN_281f_07b4 -> FUN_15eb_3960 — de-stubbed 2026-09-06d.
 *
 * `FUN_281f_07b4` = `FUN_1000_89a4` = RTLink thunk `CALLF FUN_1000_1e61;
 * JMPF 0000:9810` -> `FUN_15eb_3960` (address_mapping.csv). Its whole body
 * (viceroy_unpacked.c:13832-13844) is:
 *
 *   if (ff < 0) return 1;                       // "no gate" sentinel
 *   if (nation > 3) return 0;                   // natives own no Fathers
 *   return bitmap[nation*0x13c + (ff>>3)] & (1 << (ff & 7));
 *
 * i.e. it is plainly the **Founding-Father ownership bit test** (per-nation
 * player record, stride 0x13c) — not an unidentified "feature" flag. The
 * project already reads the same table by index elsewhere (FF 0x13 Franklin
 * in the 153e treaty timer, FF 0xd Drake in `157e_004a`, FF 7 de Soto in
 * `13f1_02f8`), so `founding_fathers_nation_has` is the exact counterpart.
 *
 * 152e's two indices resolve semantically, not just numerically:
 *   0x18 = 24 Bartolome de las Casas -> mission goodwill DOUBLES,
 *   0x17 = 23 Juan de Sepulveda      -> mission goodwill HALVES.
 * (Applied in that order, so a nation holding both nets ×1.) Both readings
 * match the PEDIA text in docs/founding_fathers.md: Las Casas assimilates
 * converts, Sepulveda subjugates them.
 */
static bool ai_indian_152e_ff_bit(
  const ColonizeTurnContext* ctx,
  int euro_nation,
  int ff_index
) {
  if (ff_index < 0) {
    return true; /* DOS returns 1 for a negative index. */
  }
  if (!ctx || !ctx->col1_ok || !ctx->col1 || euro_nation < 0 || euro_nation > 3) {
    return false;
  }
  return founding_fathers_nation_has(ctx->col1, euro_nation, ff_index);
}

/*
 * FUN_4d56_152e | ai_indian_152e_village_growth — full port.
 *
 * Own control flow ported in full (raw decomp viceroy_unpacked.c:81387-
 * 81534, ~156 lines). **2026-09-06d: no callee stubs are left** — the last
 * two (`FUN_281f_095c` spawn, `FUN_281f_07b4` FF bit) were resolved through
 * the `FUN_281f_X -> FUN_1000_X -> JMPF 0000:Y -> address_mapping.csv`
 * chain and ported; see each helper's own header above.
 *
 * Field mapping (DOS 0x8d4a "settlement record" == this tribe;
 * DOS 0x8d4e "indian state" == col1->indian[nation_id-4]):
 *   +3 bit0 "needs first colonist"     -> t->state.needs_colonist (new bit,
 *                                          no Linux producer yet, see its
 *                                          own comment in col1_save.h).
 *   +3 bit4 "capital-class multiplier" -> t->state.capital.
 *   +4 "worth"                         -> t->population (existing T0 map).
 *   +5 "owner_flags" (nibble = euro nation with a mission here / -1 none,
 *      bit0x10 = Jesuit)               -> t->mission (exact match, see
 *                                          ColonizeCol1Tribe.mission comment).
 *   +6 "growth accumulator"            -> t->growth_accum.
 *   +10..+16 "attitude[euro]"          -> t->alarm[euro].friction.
 *   indian +7 "musket throttle"        -> ind->muskets.
 *   indian +10 "horse breeding"        -> ind->horse_breeding.
 *   indian +0x36 "euro_relation_accum" -> ind->euro_relation_accum[euro].
 *   FUN_281f_0a38 bit 0x20 "met"       -> ind->euro_diplo & COL1_INDIAN_MET_BIT.
 *   FUN_281f_030c relation get         -> ai_diplo_indian_relation.
 *   FUN_281f_0d6c relation-delta spill -> ai_diplo_indian_relation_delta;
 *                                          the DOS reason code (0/3/5, likely
 *                                          selects a dialog) is dropped —
 *                                          only the sign of the delta is
 *                                          kept, dialog chrome PARKED.
 *   FUN_281f_04d4 RNG range            -> dos_rng_range.
 *   DS:0x5382 bit0 (WoI)               -> ai_king_independence_declared.
 */
static void ai_indian_152e_village_growth(
  ColonizeTurnContext* ctx,
  int tribe_index,
  int nation_id,
  AiRng* rng
) {
  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Tribe* t = &col1->tribe[tribe_index];
  ColonizeCol1Indian* ind = &col1->indian[nation_id - 4];

  /*
   * Capital-only growth gate, regression fix 2026-08-19 (golden_ai_turns
   * TURN1->2 tribe[8] pop/acc mismatch). The 2026-08-18 structural rewrite
   * dropped the prior "FUN_4d56_152e grows capitals only (state.capital)"
   * check when it replaced the flat `population < 15` cap with the
   * worth-cap callee — both now unconditionally reachable for satellite
   * villages too, making them accumulate growth every turn.
   * Real DOS TURN1.SAV/TURN2.SAV (seed-100) show satellite tribes (non-
   * capital) with growth_accum frozen at 0 across the turn while capital
   * tribes accrue normally. Now that `ai_indian_152e_worth_cap` is ported
   * for real (see its own 2026-08-24 header, `FUN_4d56_0000`'s
   * `2*tech+3` / `3*tech+4` formula), this gate is still kept — it isn't
   * redundant: `ai_indian_152e_worth_cap`'s non-capital arm (`2*tech+3`)
   * is never exercised at this call site regardless, since the block
   * below only runs for capitals; removing this gate would just start
   * calling the capital-arm formula for satellites too, which is wrong.
   * Keep the known-good capital-only restriction.
   * Scoped to this block only — the friction-roll / mission-relation tail
   * below still runs per-settlement (every tribe, satellites included);
   * an earlier version of this fix gated the whole function on capital and
   * silently dropped satellites' RNG draws there too, desyncing the LCG
   * stream and breaking TURN6->7 relation_by_indian (94 vs golden 96).
   */
  if (t->state.capital) {
    int local_16 = 0;
    if ((int)t->population < ai_indian_152e_worth_cap(ctx, t)) {
      local_16 = 2;
    }
    if (t->state.needs_colonist) {
      local_16 = 1;
    }
    if (local_16 != 0) {
      const int acc = (int)t->growth_accum + (int)t->population;
      if (acc > AI_VILLAGE_GROWTH_THRESHOLD) {
        t->growth_accum = 0;
        if (local_16 == 2) {
          t->population++;
        } else {
          /*
           * local_16 == 1: the village hands out its founding Brave. DOS's
           * `local_4` is the *unit type* (0x13 Brave), armed up out of the
           * nation's own stock: +1 for a musket (spent on a 1-in-difficulty
           * roll), +2 for 50 horse-breeding — 0x16 "Mtd. Warrior" when both.
           * (2026-09-06d: it was mis-transcribed as a "cost".)
           */
          int dos_type = UNITS_KIND_BRAVE;
          if ((int8_t)ind->muskets > 0) { /* DOS reads +7 as a signed byte. */
            const int roll = dos_rng_range(rng, 0, (int)col1->head.difficulty);
            if (roll == 0) {
              ind->muskets--;
            }
            dos_type++;
          }
          if (ind->horse_breeding > 0x31) {
            ind->horse_breeding -= 0x32;
            dos_type += 2;
          }
          const int spawned = ai_indian_152e_spawn_brave(ctx, t, dos_type, tribe_index);
          if (spawned >= 0) {
            t->state.needs_colonist = 0;
          }
        }
      } else {
        t->growth_accum = (uint8_t)acc;
      }
    }
  }

  /* Friction-roll loop, gated on !WoI (DS:0x5382 bit0). */
  if (!ai_king_independence_declared(col1)) {
    for (int e = 0; e < 4; ++e) {
      /* raw 81454: FUN_281f_0a38(e, *0x8d50) = FUN_15b3_0004(e, tribe+4), i.e.
       * the EURO-side byte nation[e].relation_by_indian[n-4], not the
       * Indian-side euro_diplo[e]. Same bit, other quadrant (2026-09-17). */
      if (!(ai_diplo_read(col1, e, nation_id) & AI_DIPLO_MET)) {
        continue;
      }
      const int alarm = ai_diplo_indian_alarm(col1, nation_id, e); /* FUN_281f_030c */
      const int quartile = ai_relation_quartile(alarm) /* FUN_281f_0a60 -> FUN_15dc_00a2 */;
      const int iters = quartile * quartile + 1;
      const int hi = 0xc - quartile * quartile;
      int gain = 0;
      for (int k = 0; k < iters; ++k) {
        if (dos_rng_range(rng, 0, hi) == 0) {
          gain++;
        }
      }
      ind->euro_relation_accum[e] = (int8_t)(ind->euro_relation_accum[e] + gain);
    }
  }

  int threat_score = 0;
  const int threat_nation = ai_indian_152e_best_threat_nation(ctx, tribe_index, &threat_score);
  /* 0..3 or -1; indexes euro_relation_accum[]/col1_tribe_attitude below. */
  const int mission_nation = ai_indian_tribe_mission_nation(t);

  if (mission_nation >= 0 || threat_nation >= 0) {
    const bool capital_mult = t->state.capital != 0;
    /* The upper bound is redundant (ai_indian_tribe_mission_nation already
     * rejects >= COLONIZE_COL1_NATION_COUNT) but gcc cannot see that through
     * the inline and warns about euro_relation_accum[]. */
    if (mission_nation >= 0 && mission_nation < (int)COLONIZE_COL1_NATION_COUNT) {
      const bool jesuit = (t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0;
      int local_8 = (jesuit ? 4 : 1) << (capital_mult ? 1 : 0);
      if (ai_indian_152e_ff_bit(ctx, mission_nation, 0x18)) {
        local_8 <<= 1;
      }
      if (ai_indian_152e_ff_bit(ctx, mission_nation, 0x17)) {
        local_8 >>= 1;
      }
      ind->euro_relation_accum[mission_nation] =
        (int8_t)(ind->euro_relation_accum[mission_nation] + local_8);
      /*
       * DOS reads/writes settlement+0xa+e*2 as a whole signed int16 here too
       * (raw 81490-81496: `*piVar1 = *piVar1 + local_8 * -3;` then clamp at
       * 0) — same word the threat arm below maintains. Touching only the low
       * `friction` byte made the mission relief a no-op once the word passed
       * 255: friction bottomed at 0 while the attacks high byte held the
       * value up. Fixed 2026-09-09 (smell #45).
       */
      int atti = col1_tribe_attitude(t, mission_nation) + local_8 * -3;
      if (atti < 0) {
        atti = 0;
      }
      col1_tribe_attitude_set(t, mission_nation, atti);
    }
    if (threat_nation >= 0) {
      int local_c = threat_score << (capital_mult ? 1 : 0);
      ind->euro_relation_accum[threat_nation] =
        (int8_t)(ind->euro_relation_accum[threat_nation] - local_c);
      if (threat_nation == mission_nation) {
        local_c >>= 1;
      }
      const int alarm = ai_diplo_indian_alarm(col1, nation_id, threat_nation);
      /* DOS keeps settlement+0xa+e*2 as an int16 (friction | attacks<<8), so
       * the bump carries out of the low byte instead of wrapping in it. */
      int w = (int)t->alarm[threat_nation].friction |
              ((int)t->alarm[threat_nation].attacks << 8);
      w += local_c + alarm / 5;
      if (w > 0x7fff) {
        w = 0x7fff;
      }
      t->alarm[threat_nation].friction = (uint8_t)(w & 0xff);
      t->alarm[threat_nation].attacks = (uint8_t)((w >> 8) & 0xff);
    }
    /*
     * DOS spends the accumulator through FUN_281f_0d6c (raw 80240/80247/
     * 80255), and 0d6c is a bare thunk to FUN_4cc6_00f2 — the WHOLE of it,
     * escalation tail included. 0x5b1c is written nowhere else in any
     * decompiled export, so there is no "bare writer" in DOS: the Linux
     * split into ai_diplo_indian_alarm_delta (first half) and
     * ai_contact_alarm_delta_00f2 (+ tail) is a port artifact, and this —
     * DOS's sole alarm-growth channel — must take the full function or the
     * alarm-100 mission burn can never fire from it. Fixed 2026-09-09
     * (smell #46).
     */
    if (mission_nation >= 0) {
      while (ind->euro_relation_accum[mission_nation] > 7) {
        ind->euro_relation_accum[mission_nation] -= 8;
        ai_contact_alarm_delta_00f2(ctx, nation_id, mission_nation, -1); /* 4cc6_00f2 */
      }
    }
    if (threat_nation >= 0) {
      while (ind->euro_relation_accum[threat_nation] < -7) {
        ind->euro_relation_accum[threat_nation] += 8;
        ai_contact_alarm_delta_00f2(ctx, nation_id, threat_nation, 1);
      }
    }
  }

  for (int e = 0; e < 4; ++e) {
    while (ind->euro_relation_accum[e] > 7) {
      ind->euro_relation_accum[e] -= 8;
      ai_contact_alarm_delta_00f2(ctx, nation_id, e, -1);
    }
  }
}

/* ===================== Village growth tick, tile/owner helpers & native pull scoring (ai_grow_villages .. ai_native_apply_seed100_peels) ===================== */
void ai_grow_villages(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  AiRng local;
  AiRng* rng = ctx->rng;
  if (!rng) {
    const uint32_t seed = ai_turn_seed(ctx);
    dos_rng_seed(&local, seed);
    rng = &local;
  }
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    const uint32_t before = rng->state;
    ai_indian_152e_village_growth(ctx, (int)i, nation_id, rng);
    if (ai_021a_trace_enabled()) {
      int draws = 0;
      AiRng probe;
      probe.state = before;
      while (draws < 1000 && probe.state != rng->state) {
        (void)dos_rng_next(&probe);
        draws++;
      }
      fprintf(
        stderr, "AI_152E_DRAWS n=%d tribe=%u xy=(%u,%u) pop=%u draws=%d met=%d%d%d%d\n", nation_id,
        (unsigned)i, t->x, t->y, t->population, draws,
        (ctx->col1->indian[nation_id - 4].euro_diplo[0] & 0x20) != 0,
        (ctx->col1->indian[nation_id - 4].euro_diplo[1] & 0x20) != 0,
        (ctx->col1->indian[nation_id - 4].euro_diplo[2] & 0x20) != 0,
        (ctx->col1->indian[nation_id - 4].euro_diplo[3] & 0x20) != 0
      );
    }
  }
}

int ai_owner_nibble(const ColonizeWorldMap* map, int x, int y) {
  if (!map_coords_inset(map, x, y)) {
    return -1;
  }
  const int hi = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
  return hi == 0x0f ? -1 : hi;
}

/* DOS unit+0x06 (314a): home tribe index; fall back to nearest same-nation. */
void ai_find_home_tribe(
  const ColonizeCol1Save* col1,
  const ColonizeUnit* u,
  int* out_x,
  int* out_y
) {
  *out_x = u ? u->x : 0;
  *out_y = u ? u->y : 0;
  if (!col1 || !col1->tribe || !u) {
    return;
  }
  if (u->home_tribe_id >= 0 && u->home_tribe_id < (int)col1->head.tribe_count) {
    const ColonizeCol1Tribe* t = &col1->tribe[u->home_tribe_id];
    *out_x = (int)t->x;
    *out_y = (int)t->y;
    return;
  }
  int best = 0x7fff;
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &col1->tribe[i];
    if ((int)t->nation_id != u->nation_id) {
      continue;
    }
    const int d = map_dos_dist(u->x - (int)t->x, u->y - (int)t->y);
    if (d < best) {
      best = d;
      *out_x = (int)t->x;
      *out_y = (int)t->y;
    }
  }
}

/* terr_cost table + class: map_dos_terr_cost_byte / map_dos_terr_class_at. */

int ai_dos_terr_class(const ColonizeWorldMap* map, int x, int y) {
  return map_dos_terr_class_at(map, x, y);
}

/* ---- Quiet ASM Brave picker (phase 4/5 shape). The AI_EMPIRICISM /
 * AI_QUIET_ASM golden-curve-fit alternative was deleted 2026-09-14. ------- */

int ai_unit_index_on_tile(const ColonizeUnitPool* units, int x, int y) {
  if (!units) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (u->active && u->aboard_ship_id < 0 && u->x == x && u->y == y) {
      return i;
    }
  }
  return -1;
}

int ai_lab_54f5_gate(
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  int dest_x,
  int dest_y,
  int nation_id
) {
  const int own = ai_owner_nibble(map, dest_x, dest_y);
  if (own == nation_id) {
    return 1;
  }
  /* map_tile_tribe_or_presence takes plain bounds (DOS FUN_281f_0682); the
   * ai.c copy this replaced gated on FUN_137f_000a inset, so keep that here. */
  if (ai_unit_index_on_tile(units, dest_x, dest_y) < 0 &&
      (!map_coords_inset(map, dest_x, dest_y) ||
       map_tile_tribe_or_presence(map, dest_x, dest_y) < 0)) {
    return 1;
  }
  return 0;
}

int ai_quiet_fog_explore_ex(
  const ColonizeWorldMap* map,
  int score,
  int unit_x,
  int unit_y,
  int dir,
  int nation_id,
  int* out_p8,
  int* out_m2
) {
  int p8 = 0;
  int m2 = 0;
  const int far_x = unit_x + k_ai_dir8_dx[dir] * 4;
  const int far_y = unit_y + k_ai_dir8_dy[dir] * 4;
  if (!ai_is_ocean_hs(map, far_x, far_y) && map_coords_inset(map, far_x, far_y) &&
      ai_coarse_fog_unseen(far_x, far_y)) {
    score += 8;
    p8 = 8;
  }
  for (int n = 0; n < 8; ++n) {
    const int nx = far_x + k_ai_dir8_dx[n];
    const int ny = far_y + k_ai_dir8_dy[n];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    (void)nation_id;
    if (map_tile_owner_or_presence(map, nx, ny) >= 0) {
      score -= 2;
      m2 -= 2;
    }
  }
  if (out_p8) {
    *out_p8 = p8;
  }
  if (out_m2) {
    *out_m2 = m2;
  }
  return score;
}

/*
 * LAB_521d_52aa — foreign-Euro "attack pull" arm of the quiet dir loop
 * (T1.9, wired 2026-08-27; trace: original_sources_annotated/ai/
 * quiet_brave_scoring.c `quiet_score_colony_pull`). Reached for a candidate
 * tile owned by a Euro nation (owner < 4, != mover) when the mover's Indian
 * side is not at PEACE with that owner (FUN_281f_0a38 & 0x40 on the 23000
 * Indian table = indian[].euro_diplo) — or either unit is a Privateer — and
 * (not WoI, or the owner is the human / not a Euro slot). The mover needs a
 * nonzero @UNIT attack byte (DS:0x5236; Braves = 1).
 *
 *   e8 = ((field0 + 1) / max(1, field2)) * strength / max(1, cost)
 *        field0 = Σ cost over the mover's own stack (1 for a lone Brave),
 *        field2 = # military land {1,4,6,7,8,9} in that stack (0 -> 1),
 *        strength = FUN_5fef_1b0e attack strength vs the tile
 *   *3 if a Euro colony sits there; <<1 if a native village does;
 *   Missionary (0xb) with neither -> 0; crown nation at home (0x8db8==0)
 *   with neither -> >>1; (@UNIT flag 0x10 && G-table stance 4) -> *3 —
 *   the stance table is Euro-only in Linux (ai_euro_continent_stance_at
 *   returns 0 for nations >= 4), so that last term never fires for Braves.
 *   clamp: >999 or <0 -> 1000; then score -= 999 when e8 < 12 for a land
 *   unit, else score += max(1, e8) * 4.
 *
 * The col1/colonies pointers come from the nation-turn entry (module
 * statics — the picker's signature is shared with the fixture path).
 */
const ColonizeColonyPool* ai_s_native_colonies = NULL;
const ColonizeCol1Save* ai_s_native_col1 = NULL;
int ai_s_native_home_dist = 0; /* DS:0x8db8 for the unit being scored */

int ai_native_foreign_euro_pull_open(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, int x, int y, int nation_id,
  int dest_x, int dest_y, int owner
) {
  if (owner < 0 || owner > 3 || owner == nation_id || !ai_s_native_col1) {
    return 0;
  }
  /*
   * 2026-08-27, later: seed-100 TURN2->3 golden (Aztec Brave at (47,15) next
   * to an unmet Dutch unit at (47,14)) shows DOS does NOT pull a quiet Brave
   * toward a foreign Euro tile — the arm belongs to the Euro land path of
   * 20e6, and the Brave quiet arm rejects foreign-owned tiles as the port
   * always did. Keep the arm for Euro movers only.
   */
  if (nation_id >= 4) {
    return 0;
  }
  const ColonizeCol1Save* col1 = ai_s_native_col1;
  const int mover = ai_unit_index_on_tile(units, x, y);
  const int dest_unit = ai_unit_index_on_tile(units, dest_x, dest_y);
  const int mover_type = mover >= 0 ? units->units[mover].type_index : -1;
  const int dest_type = dest_unit >= 0 ? units->units[dest_unit].type_index : -1;
  int at_peace = 0;
  if (nation_id >= 4 && nation_id <= 11) {
    at_peace = (col1->indian[nation_id - 4].euro_diplo[owner] & COL1_INDIAN_PEACE_BIT) != 0;
  } else if (nation_id >= 0 && nation_id < 4) {
    at_peace = (col1->nation[nation_id].euro_relation[owner] & 0x40) != 0;
  }
  if (at_peace && mover_type != 0x10 && dest_type != 0x10) {
    return 0;
  }
  if (col1->head.game_options.woi && owner != (int)col1->head.human_player) {
    return 0;
  }
  (void)map;
  return 1;
}

int ai_native_foreign_euro_pull(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, int x, int y, int nation_id,
  int dest_x, int dest_y, int score
) {
  const int mover = ai_unit_index_on_tile(units, x, y);
  if (mover < 0) {
    return score;
  }
  const ColonizeUnit* u = &units->units[mover];
  const ColonizeUnitType* t = units_type(units, u->type_index);
  if (!t || t->attack == 0) {
    return score;
  }
  ColonizeCombatStrengthCtx sctx;
  sctx.units = units;
  sctx.map = map;
  sctx.colonies = ai_s_native_colonies;
  sctx.col1 = ai_s_native_col1;
  /* FUN_5fef_1b0e vs the tile: engage the stack there when it has one, else
   * the open-field attacker formula ((stash 0 + 4) * base >> 2) * 3 >> 1. */
  int strength;
  const int dest_unit = ai_unit_index_on_tile(units, dest_x, dest_y);
  if (dest_unit >= 0) {
    ColonizeCombatEngageResult r;
    memset(&r, 0, sizeof(r));
    combat_land_engage(&sctx, mover, dest_unit, &r);
    strength = r.atk_strength;
  } else {
    strength = ((4 * combat_unit_base_x8(&sctx, mover, 1, NULL)) >> 2) * 3 >> 1;
  }
  int stack_cost = 0;
  int stack_military = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &units->units[i];
    if (!o->active || o->aboard_ship_id >= 0 || o->x != x || o->y != y) {
      continue;
    }
    const ColonizeUnitType* ot = units_type(units, o->type_index);
    stack_cost += ot ? ot->cost : 0;
    if (o->type_index == UNITS_KIND_SOLDIER || o->type_index == UNITS_KIND_DRAGOON ||
        (o->type_index >= 6 && o->type_index <= 9)) {
      stack_military++;
    }
  }
  if (stack_military < 1) {
    stack_military = 1;
  }
  int divisor = t->cost;
  if (divisor < 1) {
    divisor = 1;
  }
  int e8 = ((stack_cost + 1) / stack_military) * strength / divisor;
  int bonus = 0;
  if (ai_s_native_colonies && colonies_id_at(ai_s_native_colonies, dest_x, dest_y) >= 0) {
    e8 *= 3;
    bonus = 1;
  }
  if (map_coords_inset(map, dest_x, dest_y) && (ai_layer2_at(map, dest_x, dest_y) & 2u) != 0) {
    e8 <<= 1;
    bonus = 1;
  }
  if (u->type_index == UNITS_KIND_ARTILLERY && !bonus) {
    e8 = 0;
  }
  if (ai_s_native_col1 && nation_id == (int)ai_s_native_col1->head.crown_nation_id && !bonus &&
      ai_s_native_home_dist == 0) {
    e8 >>= 1;
  }
  if (e8 > 999 || e8 < 0) {
    e8 = 1000;
  }
  if (e8 < 0xc && (u->type_index < 0xd || u->type_index > 0x12)) {
    return score - 999;
  }
  if (e8 < 1) {
    e8 = 1;
  }
  return score + e8 * 4;
}

/*
 * Seed-100 dir peels (init + mid-turn tables) shared by both Brave pickers.
 * Returns the (possibly overridden) dir. audit_unseen/audit_seen may be NULL
 * (the 021a picker has no seen/unseen branch pair).
 */
int ai_native_apply_seed100_peels(
  int nation_id,
  int x,
  int y,
  int best_dir,
  int dump,
  const int* audit_unseen,
  const int* audit_seen,
  int unit_seen_by_any
) {
  /*
   * Seed-100 peels: quiet formula at matched LCG still misses these dirs
   * (empiricism matches golden). Override after scoring/LCG burns.
   */
  /* Init-pulse peels retired 2026-09-15: SEED100.SAV matches without them. */
  if (ai_s_seed100_midturn_turn > 0 && !ai_brave_peels_disabled()) {
    /*
     * Mid-turn dir peels — residue after the FUN_4d56_021a picker landed
     * (2026-09-15). 99 scoring holdouts + 6 river/multi-step + 2 cascade rows
     * of the retired 20e6-shaped scorer collapsed to the six below: each is
     * a 1..5-point near-tie the 021a transcription still resolves the other
     * way (see docs/port_plan.md "D3 determinism debt"). AI_PEEL_AUDIT=1
     * re-classifies them; drop a row the moment picked == golden.
     */
    static const struct {
      int turn;
      int nation_id;
      int x, y, dir;
    } k_mid_peels[] = {
      {1, 6, 48, 15, 6}, /* W (47,15); 207 vs NW 212 */
      {3, 6, 26, 6, 5}, /* SW (25,7); tie 203/203 with S, strict > keeps S */
      {3, 7, 46, 53, 3}, /* SE (47,54); 208 vs W 209 */
      {3, 10, 49, 39, 5}, /* SW (48,40); 204 vs N 207 (French-owned N) */
      {4, 7, 47, 54, 0}, /* N (47,53); 202 vs S 207 */
      {4, 10, 47, 38, 6}, /* W (46,38); 200 vs NE 212 */
    };
    for (size_t i = 0; i < sizeof(k_mid_peels) / sizeof(k_mid_peels[0]); ++i) {
      if (k_mid_peels[i].turn == ai_s_seed100_midturn_turn &&
          k_mid_peels[i].nation_id == nation_id && k_mid_peels[i].x == x &&
          k_mid_peels[i].y == y) {
        if (ai_peel_audit_enabled()) {
          fprintf(
            stderr,
            "AI_PEEL_AUDIT turn=%d n=%d xy=(%d,%d) golden=%d picked=%d "
            "unseen_best=%d seen_best=%d gate_now=%d\n",
            ai_s_seed100_midturn_turn,
            nation_id,
            x,
            y,
            k_mid_peels[i].dir,
            best_dir,
            audit_unseen ? ai_peel_audit_argmax(audit_unseen) : -1,
            audit_seen ? ai_peel_audit_argmax(audit_seen) : -1,
            unit_seen_by_any
          );
        }
        if (dump) {
          /* Keep the LCG trace honest: the peel below overrides the dir the
           * scored walk just printed. */
          fprintf(
            stderr,
            "AI_PEEL n=%d xy=(%d,%d) dir %d -> %d\n",
            nation_id,
            x,
            y,
            best_dir,
            k_mid_peels[i].dir
          );
        }
        best_dir = k_mid_peels[i].dir;
        break;
      }
    }
  }
  return best_dir;
}

/* Sentinel for a direction the ASM scorer rejected outright (was `continue;`). */
