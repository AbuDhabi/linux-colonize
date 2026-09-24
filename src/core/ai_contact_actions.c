/*
 * Indian contact — DOS village action handlers (overlay 13 / FUN_4d56_4528 @ACTIONS) & menu dispatch
 *
 * Split out of ai_contact.c (2026-09-23) verbatim; shared symbols are
 * declared in ai_contact_internal.h. See ai_contact.c for the module
 * prologue and the DOS provenance notes.
 *
 * Sections:
 *   DOS village action handlers (overlay 13 thunks behind the @ACTIONS switch)
 *   Menu actions: live-among-natives, speak-with-chief, demand-tribute, denounce/mission & popup-result dispatch
 */

#include "core/internal.h"
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"

#include "core/ai.h"
#include "core/ai_diplo.h"
#include "core/sound.h"
#include "core/woodcut.h"
#include "core/ai_king.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/strutil.h"
#include "core/units.h"
#include "core/village_trade_intel.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================================================
 * DOS village action handlers (overlay 13 thunks behind FUN_4d56_4528's
 * @ACTIONS switch). Static port 2026-08-28 from viceroy_overlays.c /
 * viceroy_overlays.asm; see original_sources_annotated/ai/indian_actions_menu.md.
 * ====================================================================== */

static ColonizeDosRng* ai_contact_action_rng(ColonizeTurnContext* ctx, int nation_id, ColonizeDosRng* local) {
  if (ctx && ctx->rng) {
    return ctx->rng;
  }
  ai_contact_local_rng(ctx, nation_id, local);
  return local;
}

/* The acting unit of a village menu result (payload bits 2.. = id + 1). */
static ColonizeUnit* ai_contact_menu_unit(ColonizeTurnContext* ctx, const AiPopupState* popup, int e) {
  if (!ctx || !ctx->units || !popup) {
    return NULL;
  }
  const int uid = ai_contact_meet_payload_unit(popup->result_payload);
  if (uid < 0) {
    return NULL;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (!u || !u->active || u->nation_id != e) {
    return NULL;
  }
  return u;
}

/* The village the unit is acting on: adjacent (or same tile) village of the tribe. */
ColonizeCol1Tribe* ai_contact_menu_village(ColonizeTurnContext* ctx, int nation_id, const ColonizeUnit* u) {
  if (!ctx || !ctx->col1 || !ctx->col1->tribe) {
    return NULL;
  }
  ColonizeCol1Tribe* first = NULL;
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    if (!first) {
      first = t;
    }
    if (u && map_chebyshev(t->x, t->y, u->x, u->y) <= 1) {
      return t;
    }
  }
  return first;
}

/* NAMES.TXT @JOB column 1 (DS:0x8ea4 + job*8): "Expert Farmers" … */
static const char* ai_contact_job_expert_name(const ColonizeTurnContext* ctx, int job) {
  static char live[40];
  if (ctx && assets_msg_row_field(ctx->names, "JOB", job, 1, live, sizeof(live))) {
    return live;
  }
  if (job >= 0 && job < COLONIZE_FIELD_JOB_COUNT) {
    return colony_yield_job_name(job);
  }
  if (job == UNITS_JOB_SCOUT) {
    return reports_job_display_name(job);
  }
  return ""; /* catalog miss = empty string (bugs.md #775) */
}

/* NAMES.TXT @JOB column 0 (DS:0x8ea2 + job*8): "Farmer" … / "Scout". */
static const char* ai_contact_job_name(int job) {
  if (job >= 0 && job < COLONIZE_FIELD_JOB_COUNT) {
    return colony_yield_job_name(job);
  }
  /* Every other @JOB id by its NAMES.TXT row: villages also teach factory
   * skills (Fur Trader 12, Weaver 11, Tobacconist 10), which used to fall
   * through to a typed "colonist" (bugs.md #547). Empty on a miss. */
  return reports_job_short_name(job);
}

/* DS:0x8394 difficulty titles (%STRING0 of the @EXTORT* bodies). */
static const char* ai_contact_difficulty_title(const ColonizeCol1Save* col1) {
  unsigned d = col1 ? (unsigned)col1->head.difficulty : 0u;
  if (d > 4u) {
    d = 4u;
  }
  return reports_difficulty_title((int)d);
}

/* FUN_1000_8804 → FUN_15eb_0142: nearest colony of `e` (continent -1 = any). */
static int ai_contact_nearest_own_colony(
  const ColonizeTurnContext* ctx,
  int e,
  int x,
  int y,
  int continent
) {
  return ai_contact_nearest_colony(ctx, e, x, y, continent, -1, 9999, NULL);
}

/*
 * Live recompute of the FUN_4962_0018 / FUN_4962_06b6 census bytes the
 * tribute roll reads: -0x6a4e (Euro exposed land combat on a continent),
 * -0x6be4 (Euro land combat total), -0x6e34 (Brave combat on a continent),
 * -0x6e7c (Brave combat total). Byte tables in DOS — capped at 255; the
 * nation total is a word. combat value = FUN_157e_004a(unit, mode 1).
 * `exposed_only` reproduces DOS's 0x942c/0x95b2 gate — see the loop body.
 */
/*
 * FUN_281f_06be → FUN_137f_03e4 (viceroy_unpacked.c:6838-6860): owner byte of
 * ANY settlement standing on the tile — Euro colony (0..3) or Indian village
 * (>= 4) — and −1 for an empty or off-map tile. Twin of
 * col1_stuff_census_settlement_at (static there; a 20-line pure helper is
 * cheaper to repeat than to export across link units).
 *
 * Both branches return the absolute Col1 nation id. DOS reads a single owner
 * nibble off the tile (FUN_137f_0200 = FUN_137f_01ac >> 4 & 0xf, 0xf = none)
 * that already holds 0..3 for a European colony and 4..11 for an Indian
 * village — which is why the sibling FUN_137f_03c2 can filter villages out
 * with a bare `if (owner < 4) return -1`. `ColonizeCol1Tribe.nation_id` lives
 * in that same absolute space (consumers index `col1->indian[]` with
 * `nation_id - 4`), so the village branch returns it as stored. Adding 4 here
 * — as this helper and both of its clones did until 2026-09-10 — reported
 * villages as 8..15.
 */
static int ai_contact_settlement_owner_at_pools(
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int x,
  int y
) {
  if (colonies) {
    const int cid = colonies_id_at(colonies, x, y);
    const ColonizeColony* c = colonies_get(colonies, cid);
    if (c && c->active) {
      return c->nation_id >= 0 ? c->nation_id : 0;
    }
  }
  /* Unfiltered walk — col1_save_tribe_at is the shared FUN_137f_03e4 port. */
  const ColonizeCol1Tribe* t = col1_save_tribe_at(col1, x, y);
  return t ? (int)t->nation_id : -1; /* nation_id is already 4..11 */
}

/*
 * DOS 0x942c / exposed-row garrison gate, verbatim (4962:022f-026e):
 *
 *   settlement = FUN_281f_06be(u.x, u.y)
 *   if (settlement >= 0) {
 *     if (nation < 4 && control[nation] == 0) skip;   // human never counts
 *     if (ai_plan == 'A' || ai_plan == 'G') skip;
 *   }
 *
 * +0x314b is `ai_plan`, the FUN_521d_0a60 garrison-assignment letter, NOT the
 * orders byte at +0x314c. DOS parks a passenger off-map at (-2,-2), where
 * 06be returns -1 and the unit always counts; the port rides passengers at
 * the carrier's tile, so the aboard short-circuit is explicit. Exported
 * because col1_stuff_census.c's per-nation twin asks exactly this (audit
 * AC-9) — the gate used to be hand-synced in both files.
 */
int ai_contact_unit_counts_as_field(
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  const ColonizeUnit* u
) {
  if (!u) {
    return 1;
  }
  const int settlement = u->aboard_ship_id >= 0
                           ? -1
                           : ai_contact_settlement_owner_at_pools(colonies, col1, u->x, u->y);
  if (settlement < 0) {
    return 1;
  }
  const int n = u->nation_id;
  const int human_slot = col1 && n >= 0 && n < (int)COLONIZE_COL1_NATION_COUNT &&
                         col1->player[n].control == 0;
  if (human_slot || u->col1_ai_plan == 0x41u || u->col1_ai_plan == 0x47u) {
    return 0;
  }
  return 1;
}

int ai_contact_land_combat_sum(
  const ColonizeTurnContext* ctx,
  int nation,
  int continent,
  int exposed_only,
  int cap
) {
  if (!ctx || !ctx->units) {
    return 0;
  }
  const ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  int sum = 0;
  /*
   * Slot walk, `u->id` to every id-taking accessor. `units_get_const`,
   * `units_is_sea` and `combat_unit_base_x8` all take a unit ID; ids are
   * handed out monotonically from 1 and never recycled (units.c:337,
   * `units_reset` next_id = 1), so an `i`-as-id walk over
   * COLONIZE_UNITS_MAX silently dropped every unit with id >= 256 in a long
   * game plus the highest slot in a short one. DOS walks the unit ARRAY in
   * record order (raw 78159: `for (local_1a = 0; local_1a < *(int *)0x539c;
   * ++local_1a)` indexing `0x3144 + local_1a * 0x1c`), which is exactly a
   * slot walk. Same idiom as col1_stuff_census's tally (ai_diplo's own copy
   * was folded onto this helper by the 2026-09-14 audit, AC-10). Fixed
   * 2026-09-10 (audit Leads item 1).
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation || units_is_sea(ctx->units, u->id)) {
      continue;
    }
    /* DOS parks a ship's passengers off-map at (−2,−2), where FUN_281f_081c
     * reports no continent: they reach the nation-wide word (-0x6be4) but no
     * per-continent row. The port rides passengers at the ship's own tile, so
     * the exclusion has to be explicit — and only for continent rows. */
    if (u->aboard_ship_id >= 0 && continent >= 0) {
      continue;
    }
    if (continent >= 0 && ctx->map && map_continent_id_at(ctx->map, u->x, u->y) != continent) {
      continue;
    }
    /* Shared 4962:022f-026e garrison gate; see ai_contact_unit_counts_as_field. */
    if (exposed_only &&
        !ai_contact_unit_counts_as_field(
          ctx->colonies, ctx->col1_ok ? ctx->col1 : NULL, u
        )) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, u->id, 1, NULL);
    if (sum >= cap) {
      return cap;
    }
  }
  return sum;
}

/*
 * DS:0x95f2[cont] `continent_presence_flags` — the single port of
 * FUN_4962_0018's presence writer (raw 78149-78312), shared by BOTH halves of
 * the FUN_5952_035e colony tick that read it: the Indian war-declare block
 * below (`ai_contact_colony_tick_war_5952`) and the expansion-appetite cap in
 * ai_euro.c (`ai_euro_5952_continent_presence`, the same DOS body ~6 lines
 * apart). Until 2026-09-10 those were two copies that disagreed about the
 * aboard-ship filter and the owner-nibble mask, so a passenger aboard a docked
 * ship set bit 2 in one and not the other (audit C7).
 *
 * The port keeps no mirror of the byte: it is ZEROED at the top of every
 * per-nation call (raw 78149-78150, `for (local_14 = 0; local_14 < 0x10;
 * ++local_14) *(undefined1 *)(local_14 + -0x6a0e) = 0;`, inside
 * FUN_4962_0018 whose `param_1` IS the nation), so it always describes the
 * nation currently being censused and can be recomputed from live state for
 * one (nation, continent) pair. docs/save_format_map.md row 156's "not
 * cleared between nations, accumulates across the full per-turn pass" is
 * refuted by that raw (audit C7, 2026-09-10).
 *
 * The three modelled bits, verbatim:
 *
 *   bit 1 — raw 78306-78312, the settlement loop: EVERY Indian settlement
 *     record whose tile has a continent, no nation filter at all
 *     (`FUN_281f_0a4c(i); iVar6 = FUN_281f_0722(*0x8d4a, [1]);
 *      if (-1 < iVar6) -0x6a0e[iVar6] |= 1;`).
 *
 *   bit 2 — raw 78234-78235, the unit loop's else-arm (i.e. the arm taken
 *     when the unit is NOT this nation's), verbatim:
 *
 *       else if ((-1 < iVar6) && ((unit[+0x3147] & 0xf) < 4))
 *         -0x6a0e[cont] |= 2;
 *
 *     Three DOS details:
 *      - The owner byte is masked to its low NIBBLE, both here and in the
 *        own-nation compare at raw 78162 (`(bVar5 & 0xf) == param_1`), and
 *        the arm then demands `< 4`, i.e. a EUROPEAN nation only. The port
 *        stores `nation_id` as a clean 0..11 int so the mask is a no-op; it
 *        is kept literal so the code reads like the raw.
 *      - No unit-domain filter: a foreign SHIP counts. Ships normally sit on
 *        water, where `FUN_281f_081c` (→ `FUN_1427_0f0e` → tile continent)
 *        reports −1 and the `-1 < iVar6` gate drops them; a ship docked in a
 *        colony stands on a land tile and does set the bit.
 *      - Passengers are excluded, but only IMPLICITLY: DOS parks a unit in a
 *        hold at the sentinel (−2,−2) (`FUN_1427_10be` boards via
 *        `FUN_1427_0362(unit, 0xfffe, 0xfffe)`), so its tile lookup also
 *        returns −1. The port rides passengers at the carrier's own tile
 *        (units.c:10282-10283), so the exclusion has to be spelled out.
 *        `units_is_on_map` already folds in `aboard_ship_id < 0`
 *        (units.c:1214-1216); it is written out for the reader.
 *
 *   bit 4 — raw 78301-78302: `else if (-1 < iVar6) -0x6a0e[cont] |= 4;`
 *     after `if (colony[+0x1a] == param_1)`. No mask and no range test on the
 *     colony owner byte here (unlike the unit arm) — every colony that is not
 *     this nation's counts. Port colonies are always 0..3, so the `< 0` guard
 *     is uninitialised-fixture defence, not a DOS filter.
 *
 *   bit 8 — raw 78167-78180, the own-nation arm of the same unit loop, i.e.
 *     "this nation has a dug-in field force on the continent":
 *
 *       if ((type < 0xd) || (0x12 < type))                  // non-naval
 *         if ((1 < *(byte *)(type * 0xe + 0x5235)) &&       // combat row > 1
 *             ((orders == 5 || orders == 6) &&              // +0x314c fortify/-ied
 *              (FUN_281f_0696(x, y) < 0)) &&                // NOT in a Euro colony
 *             (-1 < iVar6))
 *           -0x6a0e[cont] |= 8;
 *
 *     `FUN_281f_0696` is the EURO-colony owner probe (clamps any owner above 3
 *     to −1), not the settlement probe `FUN_281f_06be` the exposed-row gate
 *     uses — a unit parked on a village tile still sets the bit. DS:0x5235 is
 *     the NAMES @UNIT combat column, `ColonizeUnitType.defense` in the port
 *     (units.c:533). +0x314c is `orders`, UNITS_ORDER_FORTIFY / FORTIFIED.
 *
 *     Neither 5952 arm reads bit 8; its one reader is the DS:0xa89c tally
 *     (raw 93110-93115, `for (i = 0; i < 0x10; ++i) if (-0x6a0e[i] & 8)
 *     ++*(char *)0xa89c;`) at the top of the per-nation AI pass, consumed by
 *     FUN_521d_20e6's war-cargo colony scorer — see
 *     `ai_contact_continent_war_count_a89c` below. Modelled since 2026-09-10
 *     (third-fix-wave lead 1); before that the scorer stood in
 *     `head.difficulty` for the tally.
 *
 * Slot walk, not an id walk: `units_get_const` takes a unit ID and ids are
 * handed out monotonically from 1 and never recycled (units.c:337), so an
 * `i`-as-id form drops every unit above COLONIZE_UNITS_MAX as well as the
 * highest slot. DOS walks the unit ARRAY in record order (raw 78159,
 * `local_1a` indexing `0x3144 + local_1a * 0x1c`), which is what a slot walk
 * reproduces.
 */
/*
 * The bit-8 predicate of the writer above, factored out so the DS:0xa89c tally
 * below walks the same test rather than a second copy of it. Returns 1 for an
 * own, on-map, non-naval, combat-row->1 unit that is fortifying/fortified and
 * is NOT standing on a Euro colony. Continent validity is the caller's job.
 */
static int ai_contact_4962_unit_sets_bit8(
  const ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation_id
) {
  if (!u->active || !units_is_on_map(u) || u->aboard_ship_id >= 0) {
    return 0;
  }
  if ((u->nation_id & 0xf) != nation_id) {
    return 0;
  }
  if (u->type_index >= 0xd && u->type_index <= 0x12) {
    return 0; /* naval band: DOS `type < 0xd || 0x12 < type` */
  }
  const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
  if (!ty || ty->defense <= 1) {
    return 0; /* `1 < *(byte *)(type * 0xe + 0x5235)` */
  }
  if (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED) {
    return 0; /* +0x314c == 5 || == 6 */
  }
  if (ctx->colonies) {
    const ColonizeColony* c = colonies_get(ctx->colonies, colonies_id_at(ctx->colonies, u->x, u->y));
    if (c && c->active) {
      return 0; /* FUN_281f_0696(x, y) < 0 */
    }
  }
  return 1;
}

int ai_contact_continent_presence_4962(
  const ColonizeTurnContext* ctx, int nation_id, int cont
) {
  if (!ctx || !ctx->map || cont < 0 || cont >= 16) {
    return 0;
  }
  int presence = 0;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  if (col1 && col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if (map_continent_id_at(ctx->map, (int)t->x, (int)t->y) == cont) {
        presence |= 1;
        break;
      }
    }
  }
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX && (presence & 0xa) != 0xa; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || !units_is_on_map(u) || u->aboard_ship_id >= 0) {
        continue;
      }
      const int owner = u->nation_id & 0xf;
      if (owner == nation_id) {
        /* own-nation arm: bit 8 */
        if ((presence & 8) == 0 && ai_contact_4962_unit_sets_bit8(ctx, u, nation_id) &&
            map_continent_id_at(ctx->map, u->x, u->y) == cont) {
          presence |= 8;
        }
        continue;
      }
      if (owner >= 4) {
        continue;
      }
      if (map_continent_id_at(ctx->map, u->x, u->y) == cont) {
        presence |= 2;
      }
    }
  }
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX && (presence & 4) == 0; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id < 0 || c->nation_id == nation_id) {
        continue;
      }
      if (map_continent_id_at(ctx->map, c->x, c->y) == cont) {
        presence |= 4;
      }
    }
  }
  return presence;
}

/*
 * DS:0xa89c, raw 93110-93115 — the head of the per-nation AI pass, immediately
 * after FUN_4962_0018 has refilled DS:0x95f2 for that nation:
 *
 *   *(undefined1 *)0xa89c = 0;
 *   local_12 = 0;
 *   do {
 *     if ((*(byte *)(local_12 + -0x6a0e) & 8) != 0)
 *       *(char *)0xa89c = *(char *)0xa89c + '\x01';
 *     local_12 = local_12 + 1;
 *   } while (local_12 < 0x10);
 *
 * i.e. the COUNT of continents (0..15) on which this nation has a dug-in field
 * force, 0..16. Recomputed live from the same bit-8 predicate the presence
 * writer uses, in one unit walk instead of sixteen presence calls; the port
 * keeps no mirror of either byte for the same reason (the array is zeroed at
 * the top of every per-nation FUN_4962_0018 call).
 *
 * Sole reader: FUN_521d_20e6's war-cargo colony scorer (raw 89660-89662,
 * `if ((*(char *)0xa89c != '\0') && (1 < local_48)) local_28 += (uint)*(byte
 * *)0xa89c * local_48 * -8;`) — ai_euro.c's ai_euro_20e6_colony_sail_pick,
 * which stood `head.difficulty` in for it until 2026-09-10.
 */
int ai_contact_continent_war_count_a89c(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map) {
    return 0;
  }
  unsigned mask = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!ai_contact_4962_unit_sets_bit8(ctx, u, nation_id)) {
      continue;
    }
    const int cont = map_continent_id_at(ctx->map, u->x, u->y);
    if (cont >= 0 && cont < 16) {
      mask |= 1u << (unsigned)cont;
    }
  }
  int n = 0;
  for (int c = 0; c < 16; ++c) {
    if (mask & (1u << (unsigned)c)) {
      ++n;
    }
  }
  return n;
}

/*
 * FUN_5952_035e's Indian war-declare block — the AI colony tick's one and only
 * production writer of COL1_INDIAN_WAR_BIT (viceroy_unpacked.c:94170-94190;
 * clean OVL15 body in original_sources_annotated/ai/colony_tick_5952_035e.md
 * lines 513-538, where it reads `FUN_1000_8bf6(nation, DS:0x8d50, 2)`).
 *
 * Ghidra dropped every argument of the far call in the canonical export, so
 * the call was recovered from the raw listing (viceroy_unpacked.asm, label
 * LAB_5952_0ac6):
 *
 *   6a02        PUSH 0x2                  ; mask = WAR (0x02)
 *   ff36508d    PUSH word [0x8d50]        ; the tribe's nation id (tribe + 4)
 *   ffb652fe    PUSH word [BP+local_1b0]  ; this colony's Euro nation
 *   9a060a1f28  CALLF switchD_2000:da9f::caseD_10   ; = FUN_15b3_0066 or_both
 *
 * i.e. `or_both(nation, tribe + 4, 2)`, which sets the bit on BOTH sides of
 * the 12x12 matrix: nation[nation].relation_by_indian[tribe] and
 * indian[tribe].euro_diplo[nation]. Static sweep 2026-09-09: this and
 * FUN_5bfb_13b0's paid "smite" arm (raw 98378, ported in ai_diplo.c's
 * AI_TALK_ST_ALLY_PAY) are the only two sites in the whole game that ever OR
 * bit 2 into an Indian row — FUN_15b3_0032 has no thunk of its own, so
 * or_both/clear_both are the sole mutation channel, and FUN_4cc6_00f2's
 * cool-below-75 clear is the only other toucher.
 *
 * DOS body, verbatim (`presence` = DS:0x95f2[cont], computed by the shared
 * `ai_contact_continent_presence_4962` above — see its header for the
 * FUN_4962_0018 writer and for why the byte always describes the nation
 * being censused):
 *
 *   if ((presence & 1) == 0)  -> nothing (no natives on this continent)
 *   if ((presence & 6) != 0 && nation != 2) -> else-arm: only caps the
 *                                              expansion appetite, no war
 *   if (exposed[nation][cont] <= 1) -> nothing
 *   lim_a = land_combat_strength[nation] << (nation == 2 ? 2 : 1)
 *   lim_b = exposed[nation][cont]        << (nation == 2 ? 3 : 2)
 *   if (brave_total[tribe] > lim_a) -> nothing
 *   if (brave_on_cont[tribe][cont] >= lim_b) -> nothing
 *   if (alarm(tribe, nation) <= 0x19 && nation != 2) -> nothing
 *   or_both(nation, tribe + 4, 2)
 *
 * `tribe` is DS:0x8d52, left behind by the tick's earlier
 * `FUN_281f_0d84(colony x, y, -1, cont)` = `FUN_4cc6_0356` nearest-village
 * scan (raw 93998-94006) — the tribe owning the village nearest THIS colony
 * on its own continent, ties going to the later record (DOS `<=`).
 *
 * Spain (nation 2) doubles both strength allowances and skips both the
 * foreign-presence gate and the alarm floor, the same `nation == 2`
 * special-casing the surrounding block carries.
 *
 * The four census tables (-0x6a4e / -0x6be4 / -0x6e34 / -0x6e7c) are
 * recomputed live through ai_contact_land_combat_sum, exactly as the
 * Demand-Tribute roll below does: the port never refreshes the DS:0x95b2 /
 * 0x91cc mirrors for a Linux-started game.
 */
void ai_contact_colony_tick_war_5952(ColonizeTurnContext* ctx, int nation_id, int cx, int cy) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->map || nation_id < 0 || nation_id > 3) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  const int cont = map_continent_id_at(ctx->map, cx, cy);
  if (cont < 0 || cont >= 16) {
    return;
  }
  /*
   * DS:0x95f2[cont], via the shared writer above. DOS computes all three bits
   * for every nation in FUN_4962_0018 and only reads them here; the two early
   * returns below are the port's own short-circuit, not a DOS ordering.
   */
  const int presence = ai_contact_continent_presence_4962(ctx, nation_id, cont);
  if ((presence & 1) == 0) {
    return;
  }
  if (nation_id != 2 && (presence & 6) != 0) {
    return;
  }
  const int exposed = ai_contact_land_combat_sum(ctx, nation_id, cont, 1, 255);
  if (exposed <= 1) {
    return;
  }
  const int total = ai_contact_land_combat_sum(ctx, nation_id, -1, 0, 0xffff);
  const int lim_a = nation_id == 2 ? (total << 2) : (total << 1);
  const int lim_b = nation_id == 2 ? (exposed << 3) : (exposed << 2);
  /* FUN_4cc6_0356(cx, cy, -1, cont): nearest village on the colony's own
   * continent; DOS's `iVar1 <= local_4` makes a tie pick the later record. */
  int tribe_nation = -1;
  if (col1->tribe) {
    int best = 9999;
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      const int tc = map_continent_id_at(ctx->map, (int)t->x, (int)t->y);
      if (tc >= 0 && tc != cont) {
        continue;
      }
      const int dx = abs(cx - (int)t->x);
      const int dy = abs(cy - (int)t->y);
      const int d = dy < dx ? ((dy >> 1) + dx) : ((dx >> 1) + dy);
      if (d <= best) {
        best = d;
        tribe_nation = (int)t->nation_id;
      }
    }
  }
  if (tribe_nation < 4 || tribe_nation > 11) {
    return;
  }
  if (ai_contact_land_combat_sum(ctx, tribe_nation, -1, 0, 255) > lim_a) {
    return;
  }
  if (ai_contact_land_combat_sum(ctx, tribe_nation, cont, 0, 255) >= lim_b) {
    return;
  }
  if (nation_id != 2 && ai_diplo_indian_alarm(col1, tribe_nation, nation_id) <= 0x19) {
    return;
  }
  ai_diplo_or_both(col1, nation_id, tribe_nation, COL1_INDIAN_WAR_BIT);
}

/* DS:0xc8 / DS:0xde — the 20-tile colony work ring (5x5 minus centre and corners). */

/*
 * thunk_FUN_1000_a618 skill pick (the "what does this village teach" half,
 * param_5 = 1 preview). Weights = the FUN_4d56_2154 bid[] table (DS:0x9e78,
 * ai_contact_meet_economics_2154) with fisherman zeroed, then tech trims:
 * tech<1 zero FurTrader/OreMiner, Farmer/2; tech<2 zero Weaver/Tobacconist/
 * SilverMiner, Farmer -= Farmer/4; tech<3 zero Distiller; tech==3 Silver
 * += Silver/2. Weighted draw rand(1,Σ). FurTrapper(4) with (x+y)%3==0 →
 * Seasoned Scout (0x16). Farmer(0) → Fisherman(8) when rand(1,20) < ocean
 * tiles in the 20-ring. DOS reseeds the RNG from the village position
 * (FUN_1000_86ba(y*256+x + DS:0x8d80)) so the answer is stable per village
 * per game; DS:0x8d80 = post_map.boot_timer (saved/restored, bugs.md #564).
 */
int ai_contact_a618_skill(ColonizeTurnContext* ctx, int nation_id, const ColonizeCol1Tribe* t) {
  if (!ctx || !ctx->col1 || !t) {
    return COLONIZE_JOB_FARMER;
  }
  AiContactMeetEcon2154 econ;
  int w[16];
  if (ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
    for (int i = 0; i < 16; ++i) {
      w[i] = (int)econ.bid[i];
    }
  } else {
    /* No economics record to build DS:0x9e78 from: fall back to the one
     * skill DOS's own weight table makes the default, @JOB 0 Expert Farmer
     * (highest weight at tech >= 2). The old fallback was a hand-written
     * tribe/cargo table with no DOS counterpart — bugs.md #573. */
    memset(w, 0, sizeof(w));
    w[0] = 1;
  }
  const unsigned tech = (unsigned)ctx->col1->indian[nation_id - 4].tech;
  w[8] = 0;
  if (tech < 1u) {
    w[12] = 0;
    w[6] = 0;
    w[0] >>= 1;
  }
  if (tech < 2u) {
    w[11] = 0;
    w[10] = 0;
    w[7] = 0;
    w[0] -= w[0] >> 2;
  }
  if (tech < 3u) {
    w[9] = 0;
  }
  if (tech == 3u) {
    w[7] += w[7] >> 1;
  }
  ColonizeDosRng rng;
  /*
   * DOS-LITERAL FUN_1000_86ba seed, overlays.c:77726-77731 (raw 77726):
   * seed32 = (village.y * 0x100 + village.x) + DS:0x8d80 (32-bit, high word
   * at 0x8d82), fed to srand which keeps the low 15 bits (dos_rng_seed
   * masks). DS:0x8d80 is the BIOS tick at 0040:006C sampled once per program
   * launch (FUN_75c2_2d46 raw 121990 <- FUN_281f_0e72 -> FUN_1c0c_0012 =
   * _DAT_0000_046c) AND it is written to / restored from the save file
   * (post_map tail @608: FUN_2a1f_0c9c write, FUN_2a1f_0cb4 read at raw
   * 120429), so it is a stable per-game value: post_map.boot_timer.
   * bugs.md #564.
   */
  dos_rng_seed(&rng,
               (uint32_t)((int)t->y * 256 + (int)t->x) + ctx->col1->post_map.boot_timer);
  int sum = 0;
  for (int i = 0; i < 16; ++i) {
    sum += w[i];
  }
  int skill = 0;
  if (sum > 0) {
    int r = dos_rng_range(&rng, 1, sum);
    skill = -1;
    do {
      skill++;
      r -= w[skill];
    } while (r > 0 && skill < 15);
  }
  if (skill == 4 && (((int)t->x + (int)t->y) % 3) == 0) {
    skill = UNITS_JOB_SCOUT; /* 0x16 */
  }
  /*
   * bugs.md #601 — deliberate divergence, documented not "fixed". DOS
   * `thunk_FUN_1000_a618` (viceroy_overlays.c:77768-77780) walks the same
   * 20-plot ring and calls `FUN_281f_0768` on raw village x+-2 / y+-2 with NO
   * bounds test: at the map edge it reads whatever byte sits past the row and
   * can count it as ocean. The port keeps `map_coords_inset` because reading
   * outside the map here is undefined behaviour on a heap-allocated plane, not
   * a wrap into a neighbouring row as it was in DOS's flat segment. Effect is
   * confined to villages within 2 tiles of the map border, which stock maps
   * do not place (the playable interior starts at x/y 1), so the Farmer ->
   * Fisherman substitution odds match DOS everywhere a real save reaches.
   */
  if (skill == 0 && ctx->map) {
    int ocean = 0;
    for (int k = 0; k < 20; ++k) {
      const int ox = (int)t->x + MAP_RING20_DX[k];
      const int oy = (int)t->y + MAP_RING20_DY[k];
      if (map_coords_inset(ctx->map, ox, oy) && map_tile_is_water(ctx->map, ox, oy)) {
        ocean++;
      }
    }
    if (dos_rng_range(&rng, 1, 20) < ocean) {
      skill = 8;
    }
  }
  return skill;
}
/* ===================== Menu actions: live-among-natives, speak-with-chief, demand-tribute, denounce/mission & popup-result dispatch (ai_contact_learnstay_apply .. ai_contact_ai_live_among_village) ===================== */


/* thunk_FUN_1000_a618 LAB_398c "DONE": profession = skill, village learned bit, @LEARNDONE. */
static void ai_contact_learnstay_apply(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t,
  int skill
) {
  if (!ctx || !u || !t) {
    return;
  }
  u->profession = skill;
  t->state.learned = 1;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_tribe_name(nation_id);
  tok.string1 = ai_contact_job_name(skill);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "LEARNDONE", &tok, "", body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body);
}

/*
 * thunk_FUN_1000_a618 (param_5 = 0) — "Live Among The Natives". Alarm
 * quartile ≥ 2 → @LEARNMAD (+3 alarm; silent when met-but-no-peace, rel &
 * 0x60 == 0x20). Petty Criminal → @LEARNCRIMINAL. Indian Convert →
 * @TEACHCONVERT. Any skilled profession → @LEARNMASTER. Free Colonist /
 * Indentured Servant: village already taught && !capital → @LEARNALREADY;
 * quartile 1 && rand(1,1000) < 200*difficulty+100 → @LEARNSLOW; human →
 * @LEARNSTAY CHOICE (Yes → DONE, No → @LEARNLATER); AI → DONE at once.
 */
void ai_contact_live_among_natives(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t) {
    return;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  const int human = ai_contact_euro_is_human(ctx, e);
  /*
   * DOS-LITERAL thunk_FUN_1000_a618 head (overlays.c 77693-77718, asm
   * OVL13:0x3650-0x36ab) — bugs.md #729. For a human-controlled actor
   * (param_3 < 4 && DS:0x543f+param_3*0x34 == 0) the village visit opens its
   * own tune on a 1-in-4 roll: FUN_1000_86c4(0,3) == 0 plays FUN_1000_8688(5)
   * (Natives), plus 7 when DS:0x8d52 == 0 (Inca) and 6 when it == 1 (Aztec) —
   * the same tribe SLOT / tune triple as the first-meet cue above.
   *
   * The roll is drawn off a private stream, not ctx->rng: in DOS the very next
   * thing 0x36c8 does is FUN_1000_8f80 (reseed from the village record), so
   * the draw cannot shift any later draw. Spending a shared-stream draw here
   * would.
   */
  if (human) {
    ColonizeDosRng bgm_rng;
    ai_contact_local_rng(ctx, nation_id, &bgm_rng);
    if (dos_rng_range(&bgm_rng, 0, 3) == 0) {
      const int tribe_slot = nation_id - 4;
      sound_set_bgm(5);
      if (tribe_slot == 0) {
        sound_set_bgm(7);
      } else if (tribe_slot == 1) {
        sound_set_bgm(6);
      }
    }
  }
  const int skill = ai_contact_a618_skill(ctx, nation_id, t);
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
  const int band = ai_relation_quartile(alarm);
  const char* tribe = ai_contact_tribe_name(nation_id);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = tribe;
  tok.string1 = ai_contact_job_name(skill);
  const char* section = NULL;
  const char* fb = NULL;

  if (band > 1) {
    ai_contact_alarm_delta_00f2(ctx, nation_id, e, 3);
    const uint8_t rel = ctx->col1->indian[nation_id - 4].euro_diplo[e];
    if ((rel & 0x60u) == 0x20u) {
      return; /* met, no peace → DOS returns before the popup */
    }
    section = "LEARNMAD";
    fb = "";
  } else if (ai_contact_is_petty_criminal(ctx->units, u)) {
    section = "LEARNCRIMINAL";
    fb = "";
  } else {
    const int is_convert = u->profession == COLONIZE_PROF_CONVERT;
    /* Profession byte, not the display name (see is_petty_criminal). */
    const int is_indentured = u->profession == UNITS_JOB_SERVANT;
    if (is_convert) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "TEACHCONVERT", NULL, "", body, sizeof(body));
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body);
      return;
    }
    /* @JOB 19 ("Colonist") is the port's free-colonist alias (newborns,
     * Europe recruit pool) — a learner like NONE, not a skilled master. */
    if (u->profession != UNITS_JOB_NONE && u->profession != UNITS_JOB_COLONIST &&
        !is_indentured) {
      tok.string1 = ai_contact_learner_skill_name(ctx->units, u);
      section = "LEARNMASTER";
      fb = "";
    } else if (t->state.learned && !t->state.capital) {
      section = "LEARNALREADY";
      fb = "";
    } else {
      int slow = 0;
      if (band > 0) {
        const int roll = dos_rng_range(rng, 1, 1000);
        if (roll < 200 * (int)ctx->col1->head.difficulty + 100) {
          slow = 1;
        }
      }
      if (slow) {
        section = "LEARNSLOW";
        fb = "";
      } else if (human && ctx->ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(ctx->messages, "LEARNSTAY", &tok, "", body, sizeof(body));
        /* @LEARNSTAY row 2 ("Not right now, thanks.") carries no token, so
         * running it through the shared tokeniser leaves it byte for byte —
         * checked against COLONIZE/GAME.TXT:1475-1476 (audit AC-28). */
        char label_buf[2][POPUP_MSG_CHOICE_LEN];
        const char* labels[2];
        popup_msg_section_labels(
          ctx->messages,
          "LEARNSTAY",
          &tok,
          "",
          "",
          label_buf,
          labels
        );
        const int ids[2] = {AI_CONTACT_LEARNSTAY_YES, AI_CONTACT_LEARNSTAY_NO};
        const int payload = (u->id & 0xffff) | (skill << 16);
        if (ai_popup_enqueue_choice_ctx(
              ctx->ai_popups, AI_POPUP_TAG_CONTACT_LEARNSTAY, e, nation_id, payload, NULL, body, labels, ids, 2
            )) {
          ai_contact_set_status(ctx, body);
          return;
        }
        ai_contact_learnstay_apply(ctx, e, nation_id, u, t, skill);
        return;
      } else {
        ai_contact_learnstay_apply(ctx, e, nation_id, u, t, skill);
        return;
      }
    }
  }
  char fbs[AI_POPUP_BODY_LEN];
  if (strstr(fb, "%s")) {
    snprintf(fbs, sizeof(fbs), fb, strcmp(section, "LEARNMASTER") == 0 ? tok.string1 : tribe);
  } else {
    snprintf(fbs, sizeof(fbs), "%s", fb);
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, section, &tok, fbs, body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body);
}

/*
 * thunk_FUN_1000_a60c — "Ask to Speak With Chief" (Scouts only). seasoned =
 * profession 0x16. alarm < 75: thr = rand(0, seasoned ? 140 : 100); good
 * branch when alarm < 25 || alarm/4 < thr: Arawak (slot 2) rand(0,
 * (8-difficulty) << seasoned) == 0 → kill; @CHIEFHOWDY (village skill +
 * three most-wanted goods); then alarm < thr && !scouted → scouted, rand(1,3):
 * 1 plain Scout → Seasoned + @CHIEFGUIDES/@WELLSEASONED (seasoned → tales);
 * 2 → @CHIEFAREA + fog reveal radius 6; 3 → @CHIEFGIFT gold = (tech+1) *
 * rand(1,6) * (Σ3 rand(1,10-difficulty)) * 4. Else @CHIEFBORED. Kill path
 * (alarm ≥ 75 / bad roll): FF 6 (Coronado) owned → bored instead; else
 * @CHIEFKILL + unit destroyed.
 */
static void ai_contact_speak_with_chief(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t) {
    return;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  ColonizeCol1Save* col1 = ctx->col1;
  /* bugs.md #491: no music/RNG draw here. thunk_FUN_1000_a60c (overlay 13,
   * OVL13:0x3a00..) opens straight on FUN_1000_84fc(alarm) — the village tune
   * pool is switched once by the FUN_4d56_4528 menu head (alarm >= 0x32), which
   * this file already does in ai_contact_enqueue_village_meet. */
  ColonizeCol1Indian* ind = &col1->indian[nation_id - 4];
  const int human = ai_contact_euro_is_human(ctx, e);
  const int seasoned = u->profession == UNITS_JOB_SCOUT;
  const int alarm = ai_diplo_indian_alarm(col1, nation_id, e);
  const int diff = (int)col1->head.difficulty;
  const char* tribe = ai_contact_tribe_name(nation_id);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = tribe;
  char body[AI_POPUP_BODY_LEN];
  char fb[AI_POPUP_BODY_LEN];
  int kill = 1;

  if (alarm < 0x4b) {
    const int thr = dos_rng_range(rng, 0, seasoned ? 140 : 100);
    if (alarm < 0x19 || (alarm >> 2) < thr) {
      kill = 0;
      if (nation_id == 6) {
        const int hi = (8 - diff) << (seasoned ? 1 : 0);
        if (dos_rng_range(rng, 0, hi) == 0) {
          kill = 1;
        }
      }
      if (!kill) {
        if (human) {
          /* @CHIEFHOWDY: village skill + the three most-wanted goods (ask[] top). */
          const int skill = ai_contact_a618_skill(ctx, nation_id, t);
          AiContactMeetEcon2154 econ;
          int want[3] = {-1, -1, -1};
          if (ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
            int ask[16];
            int order[16];
            for (int i = 0; i < 16; ++i) {
              ask[i] = (int)econ.ask[i];
              order[i] = i;
            }
            if (t->last_bought < 16) {
              ask[t->last_bought] = 0;
            }
            if (t->last_sold < 16) {
              ask[t->last_sold] = 0;
            }
            for (int i = 1; i < 16; ++i) {
              const int v = order[i];
              int j = i - 1;
              while (j >= 0 && ask[order[j]] > ask[v]) {
                order[j + 1] = order[j];
                j--;
              }
              order[j + 1] = v;
            }
            want[0] = order[15];
            want[1] = order[14];
            want[2] = order[13];
          }
          PopupMsgTokens ht;
          memset(&ht, 0, sizeof(ht));
          ht.string0 = ai_contact_job_expert_name(ctx, skill);
          ht.string1 = want[0] >= 0 ? ai_contact_cargo_name(want[0])
                                    : ai_contact_cargo_name(COLONIZE_CARGO_TRADE_GOODS);
          ht.string2 = want[1] >= 0 ? ai_contact_cargo_name(want[1])
                                    : ai_contact_cargo_name(COLONIZE_CARGO_TOOLS);
          ht.string3 = want[2] >= 0 ? ai_contact_cargo_name(want[2])
                                    : ai_contact_cargo_name(COLONIZE_CARGO_MUSKETS);
          fb[0] = '\0';
          popup_msg_fill(ctx->messages, "CHIEFHOWDY", &ht, fb, body, sizeof(body));
          ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
          if (want[0] >= 0) {
            village_trade_intel_note_buys(e, t->x, t->y, want, 3);
          }
        }
        if (alarm < thr && !t->state.scouted) {
          t->state.scouted = 1;
          int r = dos_rng_range(rng, 1, 3);
          if (r == 1 && !seasoned) {
            tok.string1 = ai_contact_level_noun(ctx, (int)ind->tech);
            u->profession = UNITS_JOB_SCOUT;
            if (human) {
              fb[0] = '\0';
              popup_msg_fill(ctx->messages, "CHIEFGUIDES", &tok, fb, body, sizeof(body));
              ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
              /* OVL13:0x003c2b `PUSH 1 / CALLF FUN_1000_900c` between
               * @CHIEFGUIDES and @WELLSEASONED is FUN_281f_0e1c →
               * FUN_6b7e_00c0, the map-viewport repaint — not a sound cue
               * (bugs.md #502). The port repaints every frame; nothing to do. */
              popup_msg_fill(ctx->messages, "WELLSEASONED", NULL, "", body, sizeof(body));
              ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
            }
            return;
          }
          if (r == 3) {
            const int r1 = dos_rng_range(rng, 1, 10 - diff);
            const int r2 = dos_rng_range(rng, 1, 10 - diff);
            const int r3 = dos_rng_range(rng, 1, 10 - diff);
            const int r6 = dos_rng_range(rng, 1, 6);
            const int gold = ((int)ind->tech + 1) * r6 * (r1 + r2 + r3) * 4;
            tok.string1 = ai_contact_euro_name(e);
            tok.number0 = gold;
            tok.has_number0 = true;
            if (human) {
              fb[0] = '\0';
              popup_msg_fill(ctx->messages, "CHIEFGIFT", &tok, fb, body, sizeof(body));
              ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
            }
            /* bugs.md: the human's live treasury is EuropeScreen.gold; the
             * accessor picks the store by nation (audit G3). */
            europe_nation_gold_add(ctx->europe, col1, e, (long)gold);
            return;
          }
          /* r == 2, or a seasoned scout rolling 1: tales of nearby lands. */
          if (human) {
            popup_msg_fill(ctx->messages, "CHIEFAREA", &tok, "", body, sizeof(body));
            ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
          }
          if (ctx->map) {
            /*
             * DOS-LITERAL FUN_13f1_0158 raw 7133-7199 (reached via
             * FUN_1000_8986 → FUN_13f1_02b4(unit, DX = 6)): the inner 3x3
             * ring (|dx| <= 1 && |dy| <= 1, local_c == 0) is revealed
             * unconditionally; every outer tile is revealed only when
             * FUN_13e4_0074 (continent id) matches the unit's own — so ocean
             * and other landmasses stay dark. map_reveal_sight is that walk.
             */
            map_reveal_sight(ctx->map, u->x, u->y, e, 6, false);
          }
          return;
        }
        /* bored */
        tok.string1 = ai_contact_euro_name(e);
        fb[0] = '\0';
        popup_msg_fill(ctx->messages, "CHIEFBORED", &tok, fb, body, sizeof(body));
        ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
        return;
      }
    }
  }
  /* LAB_3a22: kill unless FF 6 (Coronado) is owned. */
  if (!founding_fathers_nation_has(col1, e, FF_FRANCISCO_CORONADO)) {
    if (human) {
      /* OVL13:0x003dc9 `MOV AX,0x55 / CALLF FUN_1000_86b0` (= FUN_281f_04c0 →
       * FUN_12d8_000e, the sound dispatcher) fires only on the human branch,
       * immediately before @CHIEFKILL (DS:0x1668). bugs.md #502. */
      sound_play(0x55);
    }
    fb[0] = '\0';
    popup_msg_fill(ctx->messages, "CHIEFKILL", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
    units_despawn(ctx->units, u->id);
    return;
  }
  tok.string1 = ai_contact_euro_name(e);
  fb[0] = '\0';
  popup_msg_fill(ctx->messages, "CHIEFBORED", &tok, fb, body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
}

/*
 * thunk_FUN_1000_a5f4 — "Demand Tribute". euro = exposed land combat on the
 * unit's continent + (land combat total >> 1); Spanish ×1.5; Cortes (FF 10)
 * ×1.5. indian = (Brave combat on continent + (Brave total >> 1)) * 2 +
 * alarm/2. win = rand(0,indian) < rand(0,euro) && an own colony exists on
 * the continent. alarm bump = human ? difficulty+1 : 1. (win || indian <
 * euro) && alarm < 75: (win || alarm < 50): village not yet extorted && win →
 * bit 0x10, bump ×2, @EXTORTSTUFF: 10 of the village's top bid[] good into
 * the nearest colony; else @EXTORTPOOR, bump 0. alarm 50..74 → @EXTORTNO.
 * Otherwise @EXTORTLAUGH. Ends with FUN_4cc6_00f2(+bump).
 */
static void ai_contact_demand_tribute(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t) {
    return;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  ColonizeCol1Save* col1 = ctx->col1;
  const int human = ai_contact_euro_is_human(ctx, e);
  const int continent = ctx->map ? map_continent_id_at(ctx->map, u->x, u->y) : -1;
  const int alarm = ai_diplo_indian_alarm(col1, nation_id, e);
  const char* tribe = ai_contact_tribe_name(nation_id);

  int euro = ai_contact_land_combat_sum(ctx, e, continent, 1, 255) +
             (ai_contact_land_combat_sum(ctx, e, -1, 0, 0xffff) >> 1);
  if (e == 2) {
    euro += euro >> 1;
  }
  if (founding_fathers_nation_has(col1, e, FF_HERNAN_CORTES)) {
    euro += euro >> 1;
  }
  const int indian =
    (ai_contact_land_combat_sum(ctx, nation_id, continent, 0, 255) +
     (ai_contact_land_combat_sum(ctx, nation_id, -1, 0, 255) >> 1)) * 2 + (alarm >> 1);
  int bump = human ? (int)col1->head.difficulty + 1 : 1;
  const int r_e = dos_rng_range(rng, 0, euro);
  const int r_i = dos_rng_range(rng, 0, indian);
  const int cid = ai_contact_nearest_own_colony(ctx, e, u->x, u->y, continent);
  const int win = cid >= 0 && r_i < r_e;

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  char body[AI_POPUP_BODY_LEN];
  char fb[AI_POPUP_BODY_LEN];
  const char* title = ai_contact_difficulty_title(col1);
  if ((win || indian < euro) && alarm < 0x4b) {
    if (win || alarm < 0x32) {
      if (!t->state.tribute_paid && win) {
        bump <<= 1;
        t->state.tribute_paid = 1;
        int good = 13;
        int top_bid = 0;
        AiContactMeetEcon2154 econ;
        if (ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
          int order[16];
          for (int i = 0; i < 16; ++i) {
            order[i] = i;
          }
          for (int i = 1; i < 16; ++i) {
            const int v = order[i];
            int j = i - 1;
            while (j >= 0 && econ.bid[order[j]] > econ.bid[v]) {
              order[j + 1] = order[j];
              j--;
            }
            order[j + 1] = v;
          }
          good = order[15];
          top_bid = (int)econ.bid[15]; /* DS:0x9e96 literal — the muskets slot, always 0 after 2154 */
        }
        ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
        int qty = 10;
        if (c) {
          const int cap = colonies_warehouse_capacity(ctx->colonies, c, good);
          int room = cap - c->stock[good];
          int lim = top_bid * 3 + 10;
          if (lim > 100) {
            lim = 100;
          }
          if (lim < room) {
            room = lim;
          }
          if (room < 10) {
            room = 10;
          }
          qty = room;
          c->stock[good] += qty;
        }
        tok.string0 = title;
        tok.string1 = tribe;
        tok.number0 = qty;
        tok.has_number0 = true;
        tok.string2 = ai_contact_cargo_name(good);
        tok.string3 = c ? c->name : "your colony";
        fb[0] = '\0';
        popup_msg_fill(ctx->messages, "EXTORTSTUFF", &tok, fb, body, sizeof(body));
        ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Tribute", body);
      } else {
        tok.string0 = title;
        tok.string1 = tribe;
        fb[0] = '\0';
        popup_msg_fill(ctx->messages, "EXTORTPOOR", &tok, fb, body, sizeof(body));
        ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Tribute", body);
        bump = 0;
      }
    } else {
      tok.string0 = title;
      tok.string1 = col1->player[e].name;
      tok.string2 = tribe;
      fb[0] = '\0';
      popup_msg_fill(ctx->messages, "EXTORTNO", &tok, fb, body, sizeof(body));
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Tribute", body);
    }
  } else {
    tok.string0 = tribe;
    fb[0] = '\0';
    popup_msg_fill(ctx->messages, "EXTORTLAUGH", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Tribute", body);
  }
  if (bump != 0) {
    ai_contact_alarm_delta_00f2(ctx, nation_id, e, bump); /* full 4cc6_00f2 */
  }
}

/*
 * FUN_4cc6_03f8 — strongest nearby Euro presence for a village: returns the
 * owning nation of the best-scoring colony within DOS distance < 7 (or -1)
 * and its score. Ring pass: for the 20 work-ring tiles, sum the attack
 * column of that tile's land units with attack > 1 into threat[owner];
 * halved when the tile holds a Euro settlement, halved again on the outer
 * ring (|dx| ≥ 2 or |dy| ≥ 2). Colony pass: human owner → (building weight,
 * divisor) by difficulty {1/2, 3/4, 1/1, 3/2, 2/1}, AI → (1,1);
 * built = Σ built-building bits × weight / divisor; pop6 = min(6, pop);
 * score = ((2*max(0,pop-6) + min(tech, pop>>1) + pop6 + difficulty +
 * ((built-8)>>2)) * 2 - dist - 1) / (dist + 4); halved if the colony sits
 * on another continent; += threat[owner]; halved for the French; halved
 * when the owner has Pocahontas. Tail: with a mission in the village, the
 * best score is scaled by who owns it: same nation → Jesuit ×1/2, plain
 * ×3/4; other nation → Jesuit ×2, plain ×3/2.
 */
static int ai_contact_4cc6_03f8(
  ColonizeTurnContext* ctx,
  int nation_id,
  const ColonizeCol1Tribe* v,
  int* out_score
) {
  *out_score = 0;
  if (!ctx || !ctx->col1 || !v) {
    return -1;
  }
  const ColonizeCol1Save* col1 = ctx->col1;
  const int vx = (int)v->x;
  const int vy = (int)v->y;
  const int vcont = ctx->map ? map_continent_id_at(ctx->map, vx, vy) : -1;
  const int tech = (int)col1->indian[nation_id - 4].tech;
  int threat[4] = {0, 0, 0, 0};
  if (ctx->units && ctx->map) {
    for (int k = 0; k < 20; ++k) {
      const int tx = vx + MAP_RING20_DX[k];
      const int ty = vy + MAP_RING20_DY[k];
      if (!map_coords_inset(ctx->map, tx, ty) || map_tile_is_water(ctx->map, tx, ty)) {
        continue;
      }
      int owner = -1;
      int sum = 0;
      /* Slot walk (DOS record order, and `i` is not a unit id) — see
       * ai_contact_land_combat_sum's note. Fixed 2026-09-10. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->x != tx || u->y != ty || u->aboard_ship_id >= 0) {
          continue;
        }
        if (owner < 0) {
          owner = u->nation_id;
        }
        /* DOS-LITERAL FUN_4cc6_03f8 raw 81047-81057: the owner is read from the
         * FIRST unit of the tile chain only; the sum then walks the whole chain
         * with no per-unit nation re-test — only the 0xd..0x12 hull-range test
         * (units_is_sea) and attack > 1. Port-side nation re-filter removed
         * 2026-09-23 (bugs.md #768); differs only on a mixed-nation stack. */
        if (units_is_sea(ctx->units, u->id)) {
          continue;
        }
        const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
        if (t && t->attack > 1) {
          sum += t->attack;
        }
      }
      if (owner < 0 || owner > 3) {
        continue;
      }
      if (ctx->colonies && colonies_id_at(ctx->colonies, tx, ty) >= 0) {
        sum >>= 1;
      }
      if (abs(MAP_RING20_DX[k]) >= 2 || abs(MAP_RING20_DY[k]) >= 2) {
        sum >>= 1;
      }
      threat[owner] += sum;
    }
  }
  int best = -1;
  int best_score = 0;
  if (ctx->colonies) {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      const ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      const int d = map_chebyshev(vx, vy, c->x, c->y);
      if (d >= 7) {
        continue;
      }
      const int owner = c->nation_id;
      int weight = 1;
      int divisor = 1;
      int diff = 0;
      if (ai_contact_euro_is_human(ctx, owner)) {
        diff = (int)col1->head.difficulty;
        static const int k_w[5] = {1, 3, 1, 3, 2};
        static const int k_d[5] = {2, 4, 1, 2, 1};
        if (diff >= 0 && diff <= 4) {
          weight = k_w[diff];
          divisor = k_d[diff];
        }
      }
      int built = 0;
      for (int b = 0; b < COLONIZE_BUILDING_TYPES_MAX && b < 48; ++b) {
        if (c->has_building[b]) {
          built += weight;
        }
      }
      built /= divisor;
      const int pop = c->population;
      const int pop6 = pop < 6 ? pop : 6;
      int half = pop >> 1;
      if (tech < half) {
        half = tech;
      }
      int score = (((pop6 - pop) * -2 + half + pop6 + diff + ((built - 8) >> 2)) * 2 - d - 1) / (d + 4);
      if (ctx->map && map_continent_id_at(ctx->map, c->x, c->y) != vcont) {
        score >>= 1;
      }
      score += threat[owner];
      if (owner == 1) {
        score >>= 1;
      }
      if (founding_fathers_nation_has(col1, owner, FF_POCAHONTAS)) {
        score >>= 1;
      }
      if (best_score < score) {
        best_score = score;
        best = owner;
      }
    }
  }
  if (best_score > 0 && best >= 0 && v->mission != COL1_TRIBE_MISSION_NONE) {
    const int owner = (int)(v->mission & COL1_TRIBE_MISSION_NATION_MASK);
    const int jesuit = (v->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0;
    if (owner == best) {
      best_score = jesuit ? (best_score >> 1) : (best_score - (best_score >> 2));
    } else {
      best_score = jesuit ? (best_score << 1) : (best_score + (best_score >> 1));
    }
  }
  *out_score = best_score;
  return best;
}

/*
 * thunk_FUN_1000_a594 — "Denounce Heresy of {rival}'s Mission". Per village
 * of the tribe: (n, s) = FUN_4cc6_03f8 (nearby Euro presence): n == rival →
 * pro_me += s; n == me → mine += s; else s seeds that village's mission
 * weight; mission weight += population, ×2 Jesuit, ×2 capital, credited to
 * the mission owner's side (mine → mine, else pro_me). Then pro_me += alarm[foreign] << (capital ? 4 : 0),
 * mine += alarm[me] >> (capital ? 29 : 1); capital: both += rand(1,20) and
 * both deltas ×2; denouncer profession == 3 (DOS typo, see below):
 * pro_me ×2, delta ×2; rival Jesuit: mine ×2,
 * delta ×2. rand(1, mine+pro_me) > pro_me → @HERESY1 (missionary burned,
 * rival's alarm −delta, mine +delta); else @HERESY0 (mission flips to me,
 * mine −delta, rival's +delta). The missionary unit is consumed either way.
 */
static void ai_contact_denounce_heresy(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t || t->mission == COL1_TRIBE_MISSION_NONE) {
    return;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  ColonizeCol1Save* col1 = ctx->col1;
  const int foreign = (int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK);
  if (foreign < 0 || foreign > 3 || foreign == e) {
    return;
  }
  int mine = 0;
  int pro_me = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* v = &col1->tribe[ti];
      if ((int)v->nation_id != nation_id) {
        continue;
      }
      int c = 0;
      int sc = 0;
      const int n = ai_contact_4cc6_03f8(ctx, nation_id, v, &sc);
      if (n == foreign) {
        pro_me += sc;
      } else if (n == e) {
        mine += sc;
      } else {
        c = sc;
      }
      if (v->mission == COL1_TRIBE_MISSION_NONE) {
        continue;
      }
      c += (int)v->population;
      if (v->mission & COL1_TRIBE_MISSION_JESUIT_BIT) {
        c *= 2;
      }
      if (v->state.capital) {
        c <<= 1;
      }
      if ((int)(v->mission & COL1_TRIBE_MISSION_NATION_MASK) == e) {
        mine += c;
      } else {
        pro_me += c;
      }
    }
  }
  /*
   * DOS-LITERAL thunk_FUN_1000_a594 viceroy_overlays.asm 126507:
   * IMUL BX,[BP+6],0x1c / CMP byte ptr [BX+0x315b],0x3 / MOV AX,1 —
   * the denouncing unit's PROFESSION byte is tested against @JOB row 3
   * (Cotton Planter), not the Missionary/Jesuit row: an apparent DOS typo
   * that makes this bonus effectively dead. There is NO Brebeuf term here;
   * only the establish body (thunk_FUN_1000_a5dc) tests 0x18 / FF 0x16.
   */
  static const int k_a594_heresy_prof_quirk = 3; /* @JOB 3 Cotton Planter */
  const int jesuit_me = (int)u->profession == k_a594_heresy_prof_quirk;
  const int rival_jesuit = (t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0;
  const unsigned cap_shift = t->state.capital ? 4u : 0u;
  pro_me += ai_diplo_indian_alarm(col1, nation_id, foreign) << cap_shift;
  mine += ai_diplo_indian_alarm(col1, nation_id, e) >> ((1u - cap_shift) & 0x1fu);
  int d_me = ai_relation_quartile(mine) + 1;
  int d_them = ai_relation_quartile(pro_me) + 1;
  if (t->state.capital) {
    pro_me += dos_rng_range(rng, 1, 20);
    mine += dos_rng_range(rng, 1, 20);
    d_them *= 2;
    d_me *= 2;
  }
  if (jesuit_me) {
    pro_me <<= 1;
    d_them <<= 1;
  }
  if (rival_jesuit) {
    mine <<= 1;
    d_me <<= 1;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_euro_name(e);
  tok.string1 = ai_contact_euro_name(foreign);
  tok.string2 = ai_contact_tribe_name(nation_id);
  char body[AI_POPUP_BODY_LEN];
  char fb[AI_POPUP_BODY_LEN];
  const int roll = dos_rng_range(rng, 1, mine + pro_me > 0 ? mine + pro_me : 1);
  if (pro_me < roll) {
    /* a594 / @HERESY1: human missionary is burned (asm 126622-126626). */
    if (ai_contact_euro_is_human(ctx, e)) {
      sound_play(0x53);
    }
    fb[0] = '\0';
    popup_msg_fill(ctx->messages, "HERESY1", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "", body);
    d_them = -d_them;
  } else {
    /* a594 / @HERESY0: human mission takes over (asm 126605-126609). */
    if (ai_contact_euro_is_human(ctx, e)) {
      sound_play(0x8024);
    }
    fb[0] = '\0';
    popup_msg_fill(ctx->messages, "HERESY0", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "", body);
    t->mission = (uint8_t)(jesuit_me ? ((unsigned)e | COL1_TRIBE_MISSION_JESUIT_BIT) : (unsigned)e);
    d_me = -d_me;
  }
  units_despawn(ctx->units, u->id);
  /* DOS routes both through FUN_281f_0d6c = the whole of FUN_4cc6_00f2. */
  ai_contact_alarm_delta_00f2(ctx, nation_id, foreign, d_them);
  ai_contact_alarm_delta_00f2(ctx, nation_id, e, d_me);
}

/*
 * thunk_FUN_1000_a5dc — "Establish Mission". count = own missions with the
 * tribe; Sepulveda (FF 23) ×2; Las Casas (FF 24) >>1; Pocahontas (FF 16)
 * >>1; French >>1. base = count*8 − {25,15,10,5}[alarm quartile]; capital:
 * base += sign(base)*8. Text = "MISSION" + n, n = quartile raised to 1 when
 * base > −6, 2 when base > 0, 3 when base > 9. Mission owner = me; Jesuit
 * bit when profession 0x18 (Jesuit) or Brebeuf (FF 22). Unit consumed;
 * FUN_4cc6_00f2(+base).
 */
static void ai_contact_establish_mission(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t || t->mission != COL1_TRIBE_MISSION_NONE) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  int count = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* v = &col1->tribe[ti];
      if ((int)v->nation_id == nation_id && v->mission != COL1_TRIBE_MISSION_NONE &&
          (int)(v->mission & COL1_TRIBE_MISSION_NATION_MASK) == e) {
        count++;
      }
    }
  }
  if (founding_fathers_nation_has(col1, e, FF_JUAN_DE_SEPULVEDA)) {
    count <<= 1;
  }
  if (founding_fathers_nation_has(col1, e, FF_BARTOLOME_DE_LAS_CASAS)) {
    count >>= 1;
  }
  if (founding_fathers_nation_has(col1, e, FF_POCAHONTAS)) {
    count >>= 1;
  }
  if (e == 1) {
    count >>= 1;
  }
  const int alarm = ai_diplo_indian_alarm(col1, nation_id, e);
  int band = ai_relation_quartile(alarm);
  static const int k_sub[4] = {0x19, 0xf, 10, 5};
  int base = count * 8 - k_sub[band];
  if (t->state.capital) {
    base += (base > 0 ? 1 : (base < 0 ? -1 : 0)) * 8;
  }
  if (base > -6 && band < 1) {
    band = 1;
  }
  if (base > 0 && band < 2) {
    band = 2;
  }
  if (base > 9) {
    band = 3;
  }
  char section[16];
  snprintf(section, sizeof(section), "MISSION%d", band);
  const int cid = ai_contact_nearest_own_colony(ctx, e, t->x, t->y, -1);
  const ColonizeColony* c = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_euro_name(e);
  tok.string1 = c ? c->name : col1->player[e].country_name;
  /* NAMES.TXT @SEASONS rows 0/1; reports_season_name is the shared live
     lookup, same Spring/Autumn literals kept as its fallback. */
  tok.string2 = reports_season_name(ctx->game_autumn && *ctx->game_autumn != 0);
  tok.number0 = ctx->game_year ? (int)*ctx->game_year : 0;
  tok.has_number0 = true;
  tok.string3 = ai_contact_tribe_name(nation_id);
  char fb[AI_POPUP_BODY_LEN];
  snprintf(fb, sizeof(fb), "%s %s mission founded in %s, %d.", tok.string0, tok.string1, tok.string2, tok.number0);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, section, &tok, fb, body, sizeof(body));
  /* a5dc mission founded, gated on a human actor (asm 126365-126375). */
  if (ai_contact_euro_is_human(ctx, e)) {
    sound_play(0x8024);
  }
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "", body);
  t->mission = (uint8_t)e;
  if (u->profession == UNITS_JOB_MISSIONARY || founding_fathers_nation_has(col1, e, FF_JEAN_DE_BREBEUF)) {
    t->mission = (uint8_t)(t->mission | COL1_TRIBE_MISSION_JESUIT_BIT);
  }
  units_despawn(ctx->units, u->id);
  ai_contact_alarm_delta_00f2(ctx, nation_id, e, base); /* full 4cc6_00f2 */
}

/*
 * FUN_4d56_4528 non-human (AI) branch, unit-type switch at OVL13 0x476d,
 * caseD_3 = Missionary (raw: asm OVL13_L0000 0x4650-0x46ee). The AI arm
 * picks a switch code into [BP-0x54] and falls into the SAME tail switch
 * (0x4bdb) the human @ACTIONS menu uses:
 *   code 7 (0x46c6) — incite the village against the human, when the five
 *     gates hold (ai_contact_ai_incite_human, thunk_FUN_1000_a5b8 = 417e
 *     Mode 2);
 *   code 3 (0x46d8) — village has no mission -> Establish Mission
 *     (thunk_FUN_1000_a5dc);
 *   code 4 (0x46ee) — village carries a foreign mission -> Denounce Heresy
 *     (thunk_FUN_1000_a594);
 *   own mission -> nothing.
 * DOS reaches this off a real village-tile entry; entering a village tile is
 * an attack in this port, so — exactly as for the AI Scout visit and the
 * wagon errand — the entry is resolved from the adjacent tile by the caller
 * (ai_euro_20e6_village_arm). Returns 1 when an arm consumed the unit's act.
 */
int ai_contact_ai_missionary_village(
  ColonizeTurnContext* ctx, int e, int tribe_index, int unit_id
) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || e < 0 || e > 3) {
    return 0;
  }
  if (tribe_index < 0 || tribe_index >= (int)ctx->col1->head.tribe_count) {
    return 0;
  }
  if (ai_contact_euro_is_human(ctx, e)) {
    return 0; /* the human arm is the @ACTIONS menu, not this branch */
  }
  ColonizeUnit* u = units_get(ctx->units, unit_id);
  if (!u || !u->active || u->nation_id != e || !units_is_missionary(ctx->units, u)) {
    return 0;
  }
  ColonizeCol1Tribe* t = &ctx->col1->tribe[tribe_index];
  const int nation_id = (int)t->nation_id;
  if (nation_id < 4 || nation_id > 11) {
    return 0;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  /*
   * FUN_4d56_417e raw 83579-83580: the -1500 incite discount tests the
   * acting unit's PROFESSION byte (+0x315b == 0x18 = Jesuit Missionary),
   * not its @UNIT type — a blessed non-Jesuit colonist pays full price.
   * bugs.md #587.
   */
  if (ai_contact_ai_incite_human(
        ctx, ind, t, nation_id, e, u->profession == UNITS_JOB_MISSIONARY
      )) {
    return 1; /* case 7 */
  }
  if (t->mission == COL1_TRIBE_MISSION_NONE) {
    ai_contact_establish_mission(ctx, e, nation_id, u, t); /* case 3 */
    return 1;
  }
  if ((int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK) != e) {
    ai_contact_denounce_heresy(ctx, e, nation_id, u, t); /* case 4 */
    return 1;
  }
  return 0; /* own mission: the switch does nothing */
}


/*
 * thunk_FUN_1000_a5e8 — "Enter Hostile Village" (wagon / ship, alarm ≥ 75).
 * r = rand(0,500): r ≤ alarm → @KILLWAGONS + unit destroyed; r ≤ 2·alarm →
 * @MADATWAGONS; else @GRUDGEWAGONS and the trade runs. Returns 1 when the
 * caller should continue into the Trade arm.
 */
int ai_contact_enter_hostile_village(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->col1 || !u) {
    return 0;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
  const int r = dos_rng_range(rng, 0, 500);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_tribe_name(nation_id);
  char body[AI_POPUP_BODY_LEN];
  if (r <= alarm) {
    popup_msg_fill(ctx->messages, "KILLWAGONS", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
    units_despawn(ctx->units, u->id);
    return 0;
  }
  if (r <= alarm * 2) {
    popup_msg_fill(ctx->messages, "MADATWAGONS", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
    return 0;
  }
  popup_msg_fill(ctx->messages, "GRUDGEWAGONS", &tok, "", body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "", body);
  return 1;
}


/*
 * Non-menu popup-result arms (WELCOME / REPARATIONS / GIFT / trade picks /
 * DEMAND / BEGFOOD / INCITE / LEARNSTAY). Extracted verbatim from
 * ai_contact_apply_popup_result; each arm that used to `return;` now
 * reports AI_CONTACT_POPUP_DONE.
 */
static AiContactPopupStatus ai_contact_apply_popup_result_tags(
  ColonizeTurnContext* ctx, const AiPopupState* popup, ColonizeCol1Indian* ind,
  int e, int nation_id
) {
  /*
   * FUN_5bfb_022e @INDIANWELCOME: Yes → FUN_5bfb_0182 peace; No/cancel →
   * FUN_4cc6_00f2 hostility + @INDIANSHUN. Cite: GAME.TXT; indian_contact.md.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_WELCOME) {
    if (popup->result_cancelled || popup->result_choice_id == AI_CONTACT_WELCOME_NO) {
      ai_contact_apply_welcome_reject(ctx, ind, nation_id, e);
    } else if (popup->result_choice_id == AI_CONTACT_WELCOME_YES) {
      ai_contact_apply_welcome_accept(ctx, ind, nation_id, e);
    }
    return AI_CONTACT_POPUP_DONE;
  }

  /*
   * @INDIANCITY / @INDIANWAGONS reparations (FUN_5bfb_022e LAB_5bfb_0def).
   * result_payload is the flavor, and the accepting row id differs per
   * flavor because the two GAME.TXT sections print their rows in opposite
   * order (DOS reads `local_c != 2` at 0x1866 and `local_c == 1` at 0x1871).
   * Refusing runs LAB_5bfb_0ff2: village attitude word += 0x80, no goods.
   * A dismissal (Esc) is neither — it drops the offer without moving goods
   * and without the 0x80, so an accidental Esc cannot latch a village
   * hostile; it only clears the pending record.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_REPARATIONS) {
    const int flavor = popup->result_payload;
    if (popup->result_cancelled) {
      if (e >= 0 && e <= 3) {
        ai_contact_s_reparations[e].active = 0;
      }
      return AI_CONTACT_POPUP_DONE;
    }
    const int accept_row = (flavor == AI_CONTACT_REPARATIONS_WAGONS)
                             ? AI_CONTACT_REPARATIONS_ROW1
                             : AI_CONTACT_REPARATIONS_ROW2;
    ai_contact_apply_reparations(
      ctx, ind, nation_id, e, flavor, popup->result_choice_id == accept_row
    );
    return AI_CONTACT_POPUP_DONE;
  }

  if (popup->result_cancelled) {
    return AI_CONTACT_POPUP_DONE;
  }

  /*
   * Gift amount CHOICE (FUN_5bfb_102a stand-in): Small −5 / Large −10.
   * Cite: indian_contact.md gift amount widget.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_GIFT) {
    if (popup->result_choice_id == AI_CONTACT_GIFT_SMALL) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 5u, 1);
    } else if (popup->result_choice_id == AI_CONTACT_GIFT_LARGE) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 10u, 2);
    } else if (popup->result_choice_id == AI_CONTACT_GIFT_GENEROUS) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 20u, 3);
    }
    return AI_CONTACT_POPUP_DONE;
  }

  /*
   * Trade buy-offer CHOICE (FUN_4d56_2820 LAB_002e92 human branch): Accept
   * applies the locked price via ai_contact_apply_trade_offer; Decline (or
   * cancel) is a no-op pass. Cite: indian_trade_2820.md.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_TRADE_PICK) {
    ai_contact_apply_trade_pick(ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id);
    return AI_CONTACT_POPUP_DONE;
  }
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_BUYWHICH) {
    if (popup->result_choice_id > 0) {
      ai_contact_apply_buywhich(ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id - 1);
    } else if (e >= 0 && e <= 3) {
      ai_contact_s_2820[e].active = 0; /* "Nothing right now" / cancel ends the visit */
    }
    return AI_CONTACT_POPUP_DONE;
  }
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_BUY0) {
    ai_contact_apply_buy0(ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id);
    return AI_CONTACT_POPUP_DONE;
  }
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_TRADE_OFFER) {
    ai_contact_apply_trade_offer(ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id);
    return AI_CONTACT_POPUP_DONE;
  }

  /*
   * Demand amount CHOICE (FUN_5bfb_102a / 1092 stand-in): tools vs gold.
   * Cite: indian_contact.md mid demand amount widget.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_DEMAND) {
    int near_x = 0;
    int near_y = 0;
    ColonizeUnit* other =
      ai_contact_find_adjacent_euro(ctx, nation_id, e, &near_x, &near_y);
    if (!other && ctx->col1->tribe) {
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id == nation_id) {
          near_x = t->x;
          near_y = t->y;
          break;
        }
      }
    }
    if (popup->result_choice_id == AI_CONTACT_DEMAND_TOOLS) {
      ai_contact_apply_demand_tools(ctx, ind, nation_id, e, other, near_x, near_y);
    } else if (popup->result_choice_id == AI_CONTACT_DEMAND_GOLD) {
      ai_contact_apply_demand_gold(ctx, ind, nation_id, e);
    }
    return AI_CONTACT_POPUP_DONE;
  }

  /*
   * @INDIANBEGFOOD Give/Refuse (FUN_5bfb_022e already-met adjacency —
   * see ai_contact_try_village_beg_food's own header comment). Payload
   * carries the offer-time colony id in the low word and the VISITING
   * Brave's home settlement index + 1 in the high word (captured at offer
   * time, same discipline as ai_king_merc's landing tile — the colony could
   * theoretically change hands between offer and apply). choice_id 2 =
   * accept/give (label[1]), 1 = decline/refuse (label[0]).
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_BEGFOOD) {
    ai_contact_apply_beg_food(
      ctx, ind, nation_id, e, popup->result_payload & 0xffff,
      ((popup->result_payload >> 16) & 0xffff) - 1, popup->result_choice_id == 2
    );
    return AI_CONTACT_POPUP_DONE;
  }

  /*
   * Incite Indians target picked (FUN_4d56_417e tail): result_choice_id is
   * the target Euro nation (0-3), set by ai_contact_enqueue_incite_target_choice.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_INCITE) {
    /* Unpack the offer-time discount flags packed by
     * ai_contact_enqueue_incite_target_choice (bit0=is_missionary,
     * bit1=is_capital); bit2 marks the @INDIANWARPATH2 pay confirm, whose
     * target rides in bits 3-4. */
    const int is_missionary = popup->result_payload & 1;
    const int is_capital = (popup->result_payload >> 1) & 1;
    if (popup->result_payload & AI_CONTACT_INCITE_STAGE_CONFIRM) {
      const int target = (popup->result_payload >> 3) & 3;
      if (!popup->result_cancelled &&
          popup->result_choice_id == AI_CONTACT_INCITE_PAY) {
        ai_contact_apply_incite(
          ctx, ind, nation_id, e, target, is_missionary, is_capital
        );
      }
      return AI_CONTACT_POPUP_DONE;
    }
    if (!popup->result_cancelled) {
      ai_contact_enqueue_incite_confirm(
        ctx, ind, nation_id, e, popup->result_choice_id, is_missionary, is_capital
      );
    }
    return AI_CONTACT_POPUP_DONE;
  }

  /* @LEARNSTAY (thunk_FUN_1000_a618): Yes → DONE; No → @LEARNLATER. */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_LEARNSTAY) {
    const int uid = popup->result_payload & 0xffff;
    const int skill = popup->result_payload >> 16;
    ColonizeUnit* lu = ctx->units ? units_get(ctx->units, uid) : NULL;
    ColonizeCol1Tribe* lt = ai_contact_menu_village(ctx, nation_id, lu);
    if (popup->result_choice_id == AI_CONTACT_LEARNSTAY_YES && lu && lu->active && lt) {
      ai_contact_learnstay_apply(ctx, e, nation_id, lu, lt, skill);
    } else {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "LEARNLATER", NULL, "", body, sizeof(body));
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body);
    }
    return AI_CONTACT_POPUP_DONE;
  }
  return AI_CONTACT_POPUP_CONTINUE;
}

/*
 * Village action menu result (@ACTIONS, FUN_4d56_4528 human arm). Extracted
 * verbatim from ai_contact_apply_popup_result.
 */
static void ai_contact_apply_popup_result_menu(
  ColonizeTurnContext* ctx, const AiPopupState* popup, ColonizeCol1Indian* ind,
  int e, int nation_id
) {
  /*
   * Village action menu result (NAMES.TXT @ACTIONS, FUN_4d56_4528 human arm).
   * Trade keeps the 2820 port; the unit-scoped DOS actions dispatch on the
   * acting unit carried in the payload. Attack Village is committed by
   * game_loop (it moves the unit); Leave dismisses.
   */
  if (popup->result_tag != AI_POPUP_TAG_CONTACT_MEET) {
    return;
  }
  ColonizeUnit* menu_unit = ai_contact_menu_unit(ctx, popup, e);
  ColonizeCol1Tribe* menu_village = ai_contact_menu_village(ctx, nation_id, menu_unit);
  int near_x = 0;
  int near_y = 0;
  ColonizeUnit* other = ai_contact_find_adjacent_euro(ctx, nation_id, e, &near_x, &near_y);
  if (menu_unit) {
    other = menu_unit;
    near_x = menu_unit->x;
    near_y = menu_unit->y;
  }
  if (!other && ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id == nation_id) {
        near_x = t->x;
        near_y = t->y;
        break;
      }
    }
  }

  /*
   * bugs.md 2026-09-04 (user-observed DOS, then confirmed in the ASM): a unit
   * that interacts with an Indian village — scout, armed unit, colonist
   * alike — forfeits its remaining movement for the turn.
   *
   * Cite: FUN_4d56_4528's common tail, viceroy_overlays.asm OVL13:0x4c0a —
   *   if ([BP-0x58] == 1) FUN_1000_8b24(unit);   ; then RETF [BP-0x58]
   * [BP-0x58] is the function's return code and FUN_1000_8b24 resolves (the
   * FUN_1000_X = 281f_(X-0x81f0) resident-stub rule: 0x8b24-0x81f0 = 0x934)
   * to FUN_281f_0934 → FUN_1427_155e → `moves_spent = FUN_1427_065a(unit)`,
   * i.e. spent := the unit's own max MP — a full forfeit, not a "stat cache
   * refresh" (indian_settlement_4528.md's old reading of 8b24, corrected).
   *
   * Two DOS exceptions, both reproduced here:
   *  - Attack Village (switch case 9) sets the code to 0, so no forfeit — the
   *    deferred move onto the village tile is committed by game_loop and
   *    consumes MP through the move/combat itself.
   *  - Establish Mission (case 3) sets the code to 2 unconditionally, so no
   *    forfeit either; on the success path the missionary is consumed by the
   *    village anyway, and on a refusal DOS really does leave its MP alone.
   * Every other outcome — Trade, Denounce Heresy, Live Among The Natives,
   * Speak With Chief, Incite, Demand Tribute, and plain Leave (which falls to
   * the switch default) — returns 1 and forfeits.
   */
  if (menu_unit && popup->result_choice_id != AI_CONTACT_CHOICE_ATTACK_VILLAGE &&
      popup->result_choice_id != AI_CONTACT_CHOICE_MISSION) {
    menu_unit->moves = 0;
  }

  switch (popup->result_choice_id) {
  case AI_CONTACT_CHOICE_LEAVE:
    /*
     * DOS OVL13 0x4bd2 (`DEC AX; CMP AX,8; JA default`): choice 10 falls to
     * the switch default, which shows nothing — only the MP forfeit above
     * runs. bugs.md #799.
     */
    break;
  case AI_CONTACT_CHOICE_ENTER_HOSTILE:
    /* thunk_FUN_1000_a5e8: survive the roll → the Trade arm (a63c) runs. */
    if (!menu_unit || !ai_contact_enter_hostile_village(ctx, e, nation_id, menu_unit)) {
      break;
    }
    /* FALLTHROUGH */
  case AI_CONTACT_CHOICE_TRADE:
    /* FUN_4d56_2820 shell (ai_contact_2820_begin): tables, hold pick, sell loop, buy loop. */
    if (!other || !ai_contact_2820_begin(ctx, ind, nation_id, e, other)) {
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "", "Trade concluded.");
    }
    break;
  case AI_CONTACT_CHOICE_GIFT: {
    /*
     * Gift-band + human popups → Small/Large amount CHOICE (FUN_5bfb_102a).
     * Else thin gift_or_demand (auto Large / demand / refuse). Cite:
     * indian_contact.md gift amount widget.
     */
    const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
    /* pair_friction dominates alarm_by_player[e] (it is seeded from it and
     * only raised), so the old `&& friction < 55 && alarm_by_player[e] < 55`
     * conjuncts were both dead under `< 40` (smell #54 / audit D10). */
    const int gift_band = friction < 40;
    if (gift_band && ctx->ai_popups && ai_contact_euro_is_human(ctx, e)) {
      if (ai_contact_enqueue_gift_amount_choice(ctx, e, nation_id)) {
        break;
      }
    }
    if (other) {
      ai_contact_gift_or_demand(ctx, ind, nation_id, e, other, near_x, near_y);
    }
    break;
  }
  case AI_CONTACT_CHOICE_DEMAND: {
    /* "Demand Tribute" — thunk_FUN_1000_a5f4 on the acting unit. */
    if (menu_unit && menu_village) {
      ai_contact_demand_tribute(ctx, e, nation_id, menu_unit, menu_village);
      break;
    }
    /*
     * Mid-band + human popups → tools/gold amount CHOICE (FUN_5bfb_102a).
     * Alarmed (≥55) → gift_or_demand refuse OK "The %s refuse demands."
     * (CONTACT_DEMAND; no amount CHOICE / no drain). Cite:
     * indian_contact.md demand amount widget / alarmed refuse.
     */
    const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
    /* `alarm_by_player[e] < 55` dropped: pair_friction is seeded from it and
     * only raised, so `friction < 55` already implies it (smell #54 / D10). */
    const int demand_band = friction >= 40 && friction < 55;
    if (demand_band && ctx->ai_popups && ai_contact_euro_is_human(ctx, e)) {
      if (ai_contact_enqueue_demand_amount_choice(
            ctx, e, nation_id, other, near_x, near_y
          )) {
        break;
      }
    }
    if (other) {
      ai_contact_gift_or_demand(ctx, ind, nation_id, e, other, near_x, near_y);
      /* friction >= 55 alone: pair_friction dominates alarm_by_player[e]
       * (seeded from it, only raised) — smell #54 / audit D10. */
    } else if (friction >= 55) {
      /* No adjacent Euro unit — still show alarmed refuse chrome. */
      ai_contact_refuse_chrome(ctx, e, nation_id, AI_POPUP_TAG_CONTACT_DEMAND, "", "demands");
    }
    break;
  }
  case AI_CONTACT_CHOICE_TEACH:
    /* "Live Among The Natives" — thunk_FUN_1000_a618 on the acting unit. */
    /* No acting unit / village => not a real DOS pick; nothing happens
     * (bugs.md #573 retired the invented adjacency pulse that used to run
     * here). */
    if (menu_unit && menu_village) {
      ai_contact_live_among_natives(ctx, e, nation_id, menu_unit, menu_village);
    }
    break;
  case AI_CONTACT_CHOICE_CHIEF:
    if (menu_unit && menu_village) {
      ai_contact_speak_with_chief(ctx, e, nation_id, menu_unit, menu_village);
    }
    break;
  case AI_CONTACT_CHOICE_HERESY:
    if (menu_unit && menu_village) {
      ai_contact_denounce_heresy(ctx, e, nation_id, menu_unit, menu_village);
    }
    break;
  case AI_CONTACT_CHOICE_MISSION:
    if (menu_unit && menu_village) {
      ai_contact_establish_mission(ctx, e, nation_id, menu_unit, menu_village);
    }
    break;
  case AI_CONTACT_CHOICE_ATTACK_VILLAGE:
    /* Committed by game_loop (needs the move engine); nothing to do here. */
    break;

  case AI_CONTACT_CHOICE_INCITE: {
    /*
     * FUN_4d56_417e Mode 1: show the "whom would you like us to attack"
     * target-nation CHOICE. No eligible target (every other Euro nation is
     * the Crown, or the inciter itself) → refuse OK; affordability is NOT a
     * row filter in DOS, it is the @UNFORTUNATE answer after the confirm.
     * Re-unpack the same is_missionary/is_capital bits the Meet CHOICE
     * itself was enqueued with (ai_contact_enqueue_village_meet) and carry
     * them into the target-choice's own payload.
     */
    const int is_missionary = popup->result_payload & 1;
    const int is_capital = (popup->result_payload >> 1) & 1;
    if (!ai_contact_enqueue_incite_target_choice(ctx, e, nation_id, is_missionary, is_capital)) {
      /* Port-authored: no GAME.TXT section covers this no-eligible-target
       * refusal (distinct from @UNFORTUNATE, the affordability case). */
      char refuse_fb[AI_POPUP_BODY_LEN];
      snprintf(
        refuse_fb,
        sizeof(refuse_fb),
        "No other nation is available for the %s to incite.",
        ai_contact_tribe_name(nation_id)
      );
      ai_contact_human_chrome(
        ctx, e, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", refuse_fb
      );
    }
    break;
  }
  default:
    break;
  }
}

void ai_contact_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup) {
  if (!ctx || !popup || !popup->has_result) {
    return;
  }
  ai_contact_bind_names(ctx);
  const int e = popup->result_nation_a;
  const int nation_id = popup->result_nation_b;
  if (e < 0 || e > 3 || nation_id < 4 || nation_id > 11) {
    return;
  }
  if (!ctx->col1_ok || !ctx->col1) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  if (ai_contact_apply_popup_result_tags(ctx, popup, ind, e, nation_id) ==
      AI_CONTACT_POPUP_DONE) {
    return;
  }

  ai_contact_apply_popup_result_menu(ctx, popup, ind, e, nation_id);
}

/*
 * FUN_521d_20e6 0x4c village arms — AI-side entry points (header comment).
 * Shared resolve: valid tribe record with a live village, unit of Euro
 * nation e adjacent to it.
 */
static ColonizeCol1Tribe* ai_contact_ai_visit_resolve(
  ColonizeTurnContext* ctx, int e, int tribe_index, int unit_id, ColonizeUnit** out_u
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || e < 0 || e > 3 ||
      tribe_index < 0 || tribe_index >= (int)ctx->col1->head.tribe_count) {
    return NULL;
  }
  ColonizeCol1Tribe* t = &ctx->col1->tribe[tribe_index];
  if (t->nation_id < 4 || t->nation_id > 11 || t->x >= 200 || t->y >= 200) {
    return NULL;
  }
  ColonizeUnit* u = units_get(ctx->units, unit_id);
  if (!u || !u->active || u->nation_id != e) {
    return NULL;
  }
  const int dx = u->x - (int)t->x;
  const int dy = u->y - (int)t->y;
  if (dx < -1 || dx > 1 || dy < -1 || dy > 1 || (dx == 0 && dy == 0)) {
    return NULL;
  }
  *out_u = u;
  return t;
}

int ai_contact_ai_scout_visit_village(ColonizeTurnContext* ctx, int e, int tribe_index, int unit_id) {
  ColonizeUnit* u = NULL;
  ColonizeCol1Tribe* t = ai_contact_ai_visit_resolve(ctx, e, tribe_index, unit_id, &u);
  if (!t) {
    return 0;
  }
  ai_contact_speak_with_chief(ctx, e, (int)t->nation_id, u, t);
  return 1;
}

int ai_contact_ai_live_among_village(ColonizeTurnContext* ctx, int e, int tribe_index, int unit_id) {
  ColonizeUnit* u = NULL;
  ColonizeCol1Tribe* t = ai_contact_ai_visit_resolve(ctx, e, tribe_index, unit_id, &u);
  if (!t) {
    return 0;
  }
  ai_contact_live_among_natives(ctx, e, (int)t->nation_id, u, t);
  return 1;
}
