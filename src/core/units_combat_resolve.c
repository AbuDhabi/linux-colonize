#include "core/units.h"

/*
 * Land/naval combat resolution, coastal fort fire, capture setup.
 *
 * Sections:
 *  - Land & naval combat resolution (FF-aware) (units_resolve_land_combat .. units_resolve_naval_combat_ff)
 *  - Coastal fort fire, ship-slowing & foreign-colony capture setup (units_coastal_fort_attack_strength .. units_capture_claim_ring)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/sound.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/village_trade_intel.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"
#include "core/units_internal.h"

/* ===================== Land & naval combat resolution (FF-aware) (units_resolve_land_combat .. units_resolve_naval_combat_ff) ===================== */


bool units_resolve_land_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
) {
  /*
   * Use g_units_ff_col1 so AI/king/contact callers (ai_euro_try_attack,
   * units_resolve_land_combat) get Washington promote when
   * turn_refresh_moves_for_nation → units_set_ff_col1 has run.
   * Cite: PEDIA/wiki George Washington; docs/fandom_col1994.md; FF elect
   * comment in founding_fathers.c (FF_GEORGE_WASHINGTON).
   */
  return units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .col1=(ColonizeCol1Save*)(g_units_ff_col1), .col1_ok=((g_units_ff_col1) != NULL), .rng=(ColonizeDosRng*)(rng)}, attacker_id, defender_id);
}

/*
 * bugs.md: mounted units (Dragoons, Scouts, Cavalry, Cont. Cav., Mtd.
 * Braves/Warriors) get one attack per turn — any combat drains all their
 * remaining moves, win or lose.
 */
static void units_mounted_attack_spend_all(ColonizeUnitPool* pool, int attacker_id) {
  ColonizeUnit* a = units_get(pool, attacker_id);
  if (!a || !a->active) {
    return;
  }
  const ColonizeUnitType* t = units_type(pool, a->type_index);
  const bool mounted = a->horses > 0 || units_type_is_mounted(t);
  if (mounted) {
    /* Natives keep the SPENT byte: drain = spent := max, not 0 (audit). */
    units_mp_exhaust(pool, a);
  }
}

/*
 * DOS 1b0e's `local_ca` (viceroy_unpacked.c 100573-100577, immediately after
 * the FUN_281f_04d4 roll):
 *
 *   bVar8 = iVar23 <= local_92;
 *   if (3 < uVar16 && uVar15 < 4 && *(char *)(uVar15 * 0x34 + 0x543f) == 0 &&
 *       uVar19 == 0x13 && uVar20 == 0xb) { bVar8 = false; local_ca = 1; }
 *
 * A **plain Brave** (@UNIT type 0x13 — not Armed Braves 0x14 / Mtd. 0x15 /
 * Mtd. Warriors 0x16) attacking the Artillery (@UNIT type 0xb) of a
 * HUMAN-controlled European (nation < 4, DS:0x543f + nation*0x34 control byte
 * == 0) never wins, whatever the roll said. The same predicate is DOS's
 * one and only write of `local_ca`, which it later hands FUN_5fef_0f14 as
 * param_4 (the raid resolver's walls-check bypass) at raw 101142 — so the
 * auto-loss and the raid flag are the same latch, computed once.
 *
 * @UNIT type ids are the NAMES.TXT @UNIT line order (Colonists 0 … Artillery
 * 0xb … Braves 0x13), but a Linux POOL INDEX is not a DOS @UNIT id — synthetic
 * fixtures and modded rosters place types at arbitrary slots, and the raw
 * `type_index == 0x13/0x0b` test silently no-opped there (smell audit
 * 2026-09-09 #8). Match by type-name family, the rule combat_strength.c
 * already states for the veteran gate (`combat_type_is_soldier_or_dragoon`,
 * combat_strength.c:78-81): plain "Braves" only — NOT Armed Braves 0x14 /
 * Mtd. Braves 0x15 / Mtd. Warriors 0x16 — versus the artillery family.
 * On the stock roster the two spellings are identical.
 */
static int units_combat_type_is_plain_brave(const ColonizeUnitType* t) {
  return units_type_kind(t) == UNITS_KIND_BRAVE ? 1 : 0;
}

static int units_combat_brave_vs_human_arty(
  const ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  const ColonizeUnit* atk,
  const ColonizeUnit* def
) {
  if (!col1 || !atk || !def) {
    return 0;
  }
  if (atk->nation_id <= 3 || def->nation_id < 0 || def->nation_id > 3) {
    return 0;
  }
  if (col1->player[def->nation_id].control != 0) {
    return 0;
  }
  const ColonizeUnitType* at = units_type(pool, atk->type_index);
  const ColonizeUnitType* dt = units_type(pool, def->type_index);
  /* A Braves body that picked up muskets/horses in the port's kit model is
   * the DOS Armed/Mtd. type, not the plain 0x13 the latch demands. */
  if (atk->muskets > 0 || atk->horses > 0) {
    return 0;
  }
  return units_combat_type_is_plain_brave(at) && dt &&
         combat_type_is_artillery(dt);
}

/*
 * Shared pre-roll of units_resolve_land_combat_ff / units_resolve_naval_combat_ff
 * (UN-8): Combat Analysis (strengths known, outcome not decided) -> the attack
 * bump -> the DOS 1b0e difficulty-handicap group (raw 100534-100556, which runs
 * after the param_5==0 early return so the panel shows the RAW odds while the
 * roll uses the handicapped attacker; raw 100563's defender bump feeds the
 * 100571 total) -> the roll itself. Writes atk_strength / def_strength / roll /
 * atk_wins into eng.
 */
static void units_combat_present_and_roll(
  ColonizeUnitPool* pool,
  ColonizeCombatStrengthCtx* sctx,
  ColonizeCombatEngagement* eng,
  ColonizeCombatEngageResult* er,
  int attacker_id,
  int defender_id,
  int atk_nation,
  int def_nation,
  int def_x,
  int def_y,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1
) {
  units_combat_maybe_present_analysis(col1, eng, atk_nation, def_nation);
  /* bugs.md #233: the attack "bump" plays AFTER the analysis is dismissed —
   * analysis → bump → outcome popups, each blocking, per combat. */
  if (g_units_combat_watch) {
    g_units_combat_watch(g_units_combat_watch_user, pool, attacker_id, def_x, def_y);
  }
  combat_apply_1b0e_resolve_handicaps(sctx, attacker_id, defender_id, er);
  eng->atk_strength = er->atk_strength;
  eng->def_strength = er->def_strength;

  const int total = eng->atk_strength + eng->def_strength;
  if (total <= 0) {
    eng->atk_wins = true;
    eng->roll = 0;
  } else if (!rng) {
    eng->atk_wins = eng->atk_strength >= eng->def_strength;
    eng->roll = eng->atk_wins ? eng->atk_strength : eng->atk_strength + 1;
  } else {
    eng->roll = dos_rng_range(rng, 1, total);
    eng->atk_wins = eng->roll <= eng->atk_strength;
  }
}

bool units_resolve_land_combat_ff_w(
  const ColonizeWorld* w,
  int attacker_id,
  int defender_id
) {
  ColonizeUnitPool* pool = w->units;
  ColonizeDosRng* rng = w->rng;
  const ColonizeCol1Save* col1 = w->col1;

  g_units_last_combat = 0;
  ColonizeUnit* atk = units_get(pool, attacker_id);
  ColonizeUnit* def = units_get(pool, defender_id);
  if (!atk || !def || !atk->active || !def->active) {
    return false;
  }
  if (units_is_sea(pool, attacker_id) || units_is_sea(pool, defender_id)) {
    return false;
  }
  const ColonizeUnitType* at = units_type(pool, atk->type_index);
  const ColonizeUnitType* dt = units_type(pool, def->type_index);
  if (!at || !dt) {
    return false;
  }
  const bool combat_audible = units_combat_is_visible(pool, attacker_id, defender_id);
  if (combat_audible) {
    units_combat_music_sting();
    /* FUN_5fef_1b0e 5fef:2271: a human attacker on an Indian (nation ≥ 4)
     * pushes `0x3b + attacker unit type` — Regulars 0x41, Cont. Cav. 0x42,
     * Cavalry 0x43, Cont. Army 0x44, Artillery 0x46, Braves 0x4e… — so the
     * "unit-class variants" are just the type index. Ids below 0x40 would
     * land in the BGM range; DOS lets them through, the port keeps the
     * generic fire for those. Other pairings use the plain 0x40. */
    int fire_id = UNITS_SFX_ATTACK_FIRE;
    if (def->nation_id >= 4 && atk->nation_id >= 0 && atk->nation_id <= 3) {
      /* bugs.md #765: the DOS operand is the @UNIT ROW, not a pool slot.
       * units_type_dos_code resolves the row from the catalog and returns -1
       * for a hand-built / synthetic type table, where the typed variant is
       * simply skipped. */
      int dos_row = units_type_dos_code(at);
      if (dos_row < 0 && pool->type_count >= 19 && atk->type_index >= 0 &&
          atk->type_index < pool->type_count) {
        dos_row = atk->type_index; /* NAMES-loaded pool: index == @UNIT row */
      }
      const int typed = dos_row >= 0 ? 0x3b + dos_row : -1;
      if (typed >= 0x40 && typed <= 0x5c) {
        fire_id = typed;
      }
    }
    units_play_event_sound(fire_id);
  }

  /*
   * FUN_5fef_1b0e / FUN_157e: attacker 004a(mode=1); defender 015e + peels.
   * Cite: viceroy_unpacked.c FUN_157e_004a / 015e / 5fef_1b0e; combat_strength.c.
   */
  ColonizeCombatStrengthCtx sctx = units_combat_strength_ctx(col1);
  sctx.units = pool;
  ColonizeCombatEngageResult er;
  combat_land_engage(&sctx, attacker_id, defender_id, &er);

  ColonizeCombatEngagement eng;
  memset(&eng, 0, sizeof(eng));
  eng.attacker_id = attacker_id;
  eng.defender_id = defender_id;
  eng.is_naval = false;
  eng.atk_strength = er.atk_strength;
  eng.def_strength = er.def_strength;
  eng.atk_flags = er.atk_flags;
  eng.def_flags = er.def_flags;
  /*
   * DOS 1b0e undefended-colony arm (viceroy_unpacked.c:100418-100431, asm
   * 5fef:1d2f-1d5f): the token defender is built with `OR [0x8d03],2`, and
   * when the colony's owner holds Founding Father 12 (Paul Revere,
   * FUN_281f_07b4(owner, 0xc)) AND the colony record's Muskets word (+0xb8)
   * is > 0x31, DOS swaps the defender graphic to 0x4b, adds +1 to its base
   * combat level, and sets `OR [0x8d03],4` — bit 0x400 of the DEFENDER
   * analysis word 0x8d02, i.e. COMBAT_FLAG_MUSKETS, the panel's Muskets row.
   * The strength half of that override rides on the phantom's unit type,
   * which units_spawn_colony_temp_defender picks as "Soldiers" (2/2) instead
   * of "Colonists" (defense 1) for the Revere arm — DOS's scratch row 0x17 is
   * written with the same 2/2 and graphic 0x4b = sprite 74
   * (UNITS_ICON_SOLDIER). Only the analysis bit needs this latch.
   */
  if (g_units_revere_muskets_latch) {
    eng.def_flags.flags |= COMBAT_FLAG_MUSKETS;
  }
  g_units_revere_muskets_latch = 0;

  /* The colony-tile tail inside the handicap group (raw 100557-100564) also
   * moves the DEFENDER — `local_a8 = local_a8 + (0x53a6 - 4) * -4` — and raw
   * 100571 rolls FUN_281f_04d4(1, local_a8 + local_92) off the bumped value,
   * so both sides come back. (local_a8 stays bumped for the promotion calls
   * at raw 100728 / 100758 too.) */
  units_combat_present_and_roll(
    pool, &sctx, &eng, &er, attacker_id, defender_id, atk->nation_id, def->nation_id, def->x,
    def->y, rng, col1
  );
  /*
   * DOS 1b0e raw 100573-100577, kept in DOS's own place: the roll is drawn
   * first (RNG stream unchanged), then a plain Brave attacking a
   * human-controlled European's Artillery has `bVar8` forced false and
   * `local_ca` latched. DOS leaves `iVar23` (eng.roll) alone here, so the
   * analysis log keeps the roll that was actually drawn.
   */
  const int brave_vs_human_arty = units_combat_brave_vs_human_arty(pool, col1, atk, def);
  if (brave_vs_human_arty) {
    eng.atk_wins = false;
  }
  combat_analysis_log_engagement(pool, &eng, true);

  const int ambush = (er.atk_flags.flags & COMBAT_FLAG_AMBUSH) != 0;

  /* DOS 1b0e tail: outcome redraw is presented via the fizzle (FUN_281f_03ea
   * duration 8) — snapshot the "before" frame while both pieces stand. */
  units_dissolve_notify(0);

  if (eng.atk_wins) {
    const int def_x = def->x;
    const int def_y = def->y;
    const int def_nation = def->nation_id;
    const int atk_nation = atk->nation_id;
    /* bugs.md #660: a beaten Treasure Train CHANGES HANDS in DOS
     * (FUN_5fef_0352 raw 99392-99413) — see the capture arm in
     * units_apply_land_loss_outcome. The ransom CHOICE and the gold
     * credit that stood here were port inventions; 0352 credits no gold.
     */
    /* bugs.md #240: loss outcome (demote/damage/capture popups) FIRST, then
     * @EUROPEWIN — DOS 1b0e order. Snapshots keep labels past a despawn. */
    {
      const ColonizeUnit win_snap = *atk;
      const ColonizeUnit lose_snap = *def;
      /*
       * DOS raw 100636-100639 deletes the bVar28 phantom before the outcome
       * branch, and the `if (bVar28)` win limb runs the colony/village
       * consequences instead of FUN_5fef_0352 — so the scratch 0x17 row is
       * never captured, never demoted, and shows neither popup. The caller
       * (units_revere_defend_colony_tile / units_finish_village_temp_defender)
       * still evaporates it, exactly as FUN_291f_0a06 does.
       */
      if (!units_defender_is_dos_scratch_row()) {
        (void)units_apply_land_loss_outcome(pool, defender_id, attacker_id, col1, 1, rng);
      }
      units_combat_outcome_popups(
        pool, &win_snap, &lose_snap, 1, atk_nation, def_nation, 0, ambush, col1
      );
    }
    /*
     * DOS 5fef land tail (~0x2532): on a DEFENDER loss the 0ec0 stack sweep
     * runs only when the attacker's type attack byte is 0 or a ship (type
     * 0xd..0x12) is party to the fight — ships never reach this land-only
     * resolver, so a normal land attack touches ONLY the picked defender.
     * Stackmates / colony bystanders are not captured while the colony still
     * stands; they fall with the colony (entry seizure) instead. The
     * attacker-loss sweep below stays unconditional, as in DOS.
     *
     * bugs.md: the raw type byte is not the faithful test in THIS port. DOS
     * stores a colony-armed colonist as @UNIT type 1 "Soldiers" (attack 2);
     * the port keeps the colonist body and hangs muskets/horses on it, so its
     * type byte is 0. Reading `at->attack` alone therefore made every armed
     * colonist (including a Tory one that changed hands with a captured port)
     * sweep and capture the whole defending stack — wagon trains and all —
     * on a single won attack, while armed defenders were still standing.
     * Test the DOS combat role instead: attack 0 AND no carried kit — which
     * is exactly `units_is_combat_role` (:6092, the BODY predicate of the two
     * documented there). Spelled through the helper rather than open-coded,
     * so the type-byte-only reading cannot creep back (audit 2026-09-10 A6).
     */
    if (!units_is_combat_role(pool, atk)) {
      units_sweep_stack_after_loss(pool, def_x, def_y, def_nation, attacker_id, defender_id, col1);
    }
    atk = units_get(pool, attacker_id);
    if (atk) {
      (void)units_promote_on_win(pool, atk, col1, eng.atk_strength, eng.def_strength, rng);
    }
    /* DS:0x54f6 discharge, DOS 1b0e site 1 (raw 101039-101041). */
    units_indian_attack_tension_clear(
      col1,
      g_units_combat_colonies,
      atk_nation,
      atk ? atk->home_tribe_id : -1,
      def_nation,
      def_x,
      def_y,
      1
    );
    /*
     * `local_a6` = difficulty/2 − 5, DOS's "!bVar28 && bVar8" row: a native
     * that wins an ordinary field fight against a European. When the defender
     * was the auto-spawned stand-in for an undefended colony (DOS bVar28) the
     * row is a different one — difficulty − 10, or −50 if the town fell — and
     * belongs to the walk-in that follows, in
     * units_try_capture_foreign_colony's Indian arm.
     */
    if (atk_nation >= 4 && atk_nation <= 11 && def_nation >= 0 && def_nation <= 3 &&
        !g_units_colony_autodefender) {
      units_indian_attack_alarm_vent(
        col1, atk_nation, def_nation, units_indian_attack_alarm_vent_amount(col1, def_nation, 5, 1)
      );
      g_units_indian_combat_vent_done = true;
    }
    /*
     * Village destroy / pop drain is NOT "no Brave left on tile". DOS only
     * drains dwelling population when the empty-village temp Brave arm wins
     * (units_finish_village_temp_defender). Killing a map Brave on the tile
     * leaves the dwelling intact. Cite: FUN_5fef_1b0e bVar28 path.
     */
    units_mounted_attack_spend_all(pool, attacker_id);
    g_units_last_combat = 1;
    units_dissolve_notify(1);
    units_combat_pump_popups();
    return true;
  }
  {
    const int atk_nation = atk->nation_id;
    const int def_nation = def->nation_id;
    const ColonizeUnit win_snap = *def;
    const ColonizeUnit lose_snap = *atk;
    /* bugs.md #240: loss outcome popups first, then @EUROPELOSE (DOS order). */
    (void)units_apply_land_loss_outcome(pool, attacker_id, defender_id, col1, 1, rng);
    units_combat_outcome_popups(
      pool, &win_snap, &lose_snap, 0, atk_nation, def_nation, 0, ambush, col1
    );
    /*
     * bugs.md #473: NO sweep of the attacker's origin tile. DOS 1b0e does run
     * 0ec0 on the attacker after a loss (raw 100759-100760), but before the
     * roll it has already lifted the attacker off its tile: raw 100568
     * FUN_281f_0916(param_1) = FUN_1427_12f6, which for a land unit is
     * FUN_1427_0362(unit, -2, -2) — unlink from the tile stack (1427_023a)
     * and relink at the off-map park (1427_02ca). 0ec0 walks the stack the
     * loser is IN (02ee/02e4 over +0x315c/+0x315e), so the origin tile —
     * a colony's docked ships, wagons and colonists — is never touched;
     * the survivor is put back at the saved (uVar21, uVar22) by 0948 at raw
     * 100764-100768. The old port sweep here sent a Galleon berthed in
     * Isabella to Seville for repairs after an Artillery sortie lost to a
     * Scout. (The land resolver has no ship attacker; the naval sweep keeps
     * its own attacker-loss walk, since 12f6 does not park a hull.)
     */
    /* DS:0x54f6 discharge, DOS 1b0e site 1 (raw 101039-101041). A native
     * attacker that LOSES at a colony is DOS's raid handoff and is skipped
     * here — the tension clear rides FUN_5fef_0f14 on that limb. */
    units_indian_attack_tension_clear(
      col1,
      g_units_combat_colonies,
      atk_nation,
      lose_snap.home_tribe_id,
      def_nation,
      win_snap.x,
      win_snap.y,
      0
    );
    /*
     * DOS's colony-raid handoff (raw 101142). The `else` limb of the
     * alarm/tension block: a native attacker (nation >= 4) beaten by a
     * European (nation < 4) whose tile carries a colony is NOT simply dead —
     * 1b0e skips its whole alarm/tension block and calls
     * `thunk_FUN_2a1f_06c8(indian_nation, colony, home_tribe, local_ca,
     * attacker_type)` = FUN_5fef_0f14, the full raid resolver, which brings
     * its own loot roll and its own DS:0x54f6 clear. `local_a6` stays 0 on
     * this limb, so there is no alarm vent here either.
     *
     * `local_ca` (0f14's param_4, bypasses the walls check) is the
     * Brave-versus-human-Artillery latch set at the roll above (raw
     * 100573-100577) — the same predicate that forced this loss, computed
     * once as DOS does.
     */
    if (atk_nation >= 4 && atk_nation <= 11 && def_nation >= 0 && def_nation <= 3 &&
        g_units_combat_colonies) {
      const int raid_cid = colonies_id_at(g_units_combat_colonies, win_snap.x, win_snap.y);
      if (raid_cid >= 0) {
        const int forced = brave_vs_human_arty;
        if (g_units_raid_repelled) {
          ColonizeWorld raid_w = world_make(
            pool,
            (ColonizeColonyPool*)g_units_combat_colonies,
            units_occupancy_map ? units_occupancy_map : g_units_fallout_map,
            (ColonizeCol1Save*)col1,
            col1 != NULL,
            rng,
            NULL
          );
          (void)g_units_raid_repelled(
            &raid_w,
            atk_nation,
            def_nation,
            raid_cid,
            lose_snap.home_tribe_id,
            forced
          );
        }
      }
    }
  }
  def = units_get(pool, defender_id);
  /*
   * FUN_5fef_172c's first gate is `unit type byte == 1 || == 4`; the phantom
   * carries the scratch type 0x17 (and DOS has already deleted its row by
   * this point anyway), so it returns before the FUN_281f_04d4 promotion
   * roll — no promote, and no RNG draw to shift the stream.
   */
  if (def && !units_defender_is_dos_scratch_row()) {
    (void)units_promote_on_win(pool, def, col1, eng.def_strength, eng.atk_strength, rng);
  }
  units_mounted_attack_spend_all(pool, attacker_id);
  g_units_last_combat = -1;
  units_dissolve_notify(1);
  units_combat_pump_popups();
  return false;
}


int units_plunder_ship_holds(ColonizeUnitPool* pool, int winner_id, int loser_id) {
  if (!pool || winner_id < 0 || loser_id < 0 || winner_id == loser_id) {
    return 0;
  }
  ColonizeUnit* win = units_get(pool, winner_id);
  ColonizeUnit* lose = units_get(pool, loser_id);
  if (!win || !lose || !win->active || !lose->active) {
    return 0;
  }
  if (!units_is_sea(pool, winner_id) || !units_is_sea(pool, loser_id)) {
    return 0;
  }
  /*
   * FUN_5fef_016c-shaped: move commodity holds from loser into winner capacity.
   * Passengers stay with the sinking ship (despawned with loser).
   */
  const int n = units_goods_hold_count(pool, loser_id);
  int moved = 0;
  for (int i = 0; i < n; ++i) {
    const int amt = units_unit_hold_amount(lose, i);
    const int ctype = lose->hold_goods_type[i];
    if (amt <= 0 || ctype < 0 || ctype >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    const int got = units_load_goods(pool, winner_id, ctype, amt);
    if (got > 0) {
      moved += got;
      if (got >= amt) {
        lose->hold_goods_amount[i] = 0;
        lose->hold_goods_type[i] = 0;
      } else {
        lose->hold_goods_amount[i] = amt - got;
      }
    }
  }
  return moved;
}

bool units_resolve_naval_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
) {
  /*
   * Use g_units_ff_col1 so AI/king callers (ai_euro / ai_king naval attack)
   * get Drake privateer *3/2 when turn_refresh_moves_for_nation →
   * units_set_ff_col1 has run. Cite: PEDIA/wiki Francis Drake (+50%);
   * founding_fathers.c FF_FRANCIS_DRAKE (*3/2).
   */
  return units_resolve_naval_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .col1=(ColonizeCol1Save*)(g_units_ff_col1), .col1_ok=((g_units_ff_col1) != NULL), .rng=(ColonizeDosRng*)(rng)}, attacker_id, defender_id);
}

bool units_resolve_naval_combat_ff_w(
  const ColonizeWorld* w,
  int attacker_id,
  int defender_id
) {
  ColonizeUnitPool* pool = w->units;
  ColonizeDosRng* rng = w->rng;
  const ColonizeCol1Save* col1 = w->col1;

  g_units_last_combat = 0;
  ColonizeUnit* atk = units_get(pool, attacker_id);
  ColonizeUnit* def = units_get(pool, defender_id);
  if (!atk || !def || !atk->active || !def->active) {
    return false;
  }
  if (!units_is_sea(pool, attacker_id) || !units_is_sea(pool, defender_id)) {
    return false;
  }
  const ColonizeUnitType* at = units_type(pool, atk->type_index);
  const ColonizeUnitType* dt = units_type(pool, def->type_index);
  if (!at || !dt) {
    return false;
  }
  const bool combat_audible = units_combat_is_visible(pool, attacker_id, defender_id);
  if (combat_audible) {
    units_combat_music_sting();
    units_play_event_sound(UNITS_SFX_ATTACK_FIRE);
  }

  /* FUN_157e_004a + 1b0e difficulty peels for both sides. */
  ColonizeCombatStrengthCtx sctx = units_combat_strength_ctx(col1);
  sctx.units = pool;
  ColonizeCombatEngageResult er;
  combat_naval_engage(&sctx, attacker_id, defender_id, &er);

  ColonizeCombatEngagement eng;
  memset(&eng, 0, sizeof(eng));
  eng.attacker_id = attacker_id;
  eng.defender_id = defender_id;
  eng.is_naval = true;
  eng.atk_strength = er.atk_strength;
  eng.def_strength = er.def_strength;
  eng.atk_flags = er.atk_flags;
  eng.def_flags = er.def_flags;

  /* Same 1b0e handicap group as land — DOS's single resolver covers naval,
   * defender bump included (raw 100563 feeds the 100571 roll total). */
  units_combat_present_and_roll(
    pool, &sctx, &eng, &er, attacker_id, defender_id, atk->nation_id, def->nation_id, def->x,
    def->y, rng, col1
  );
  combat_analysis_log_engagement(pool, &eng, true);

  /*
   * Evasion (DOS 1b0e 5fef:~2532, after the main roll and preempting its
   * outcome): a defender whose type attack is BELOW the attacker's may slip
   * away — power per FUN_5bfb_312e (movement+3, Privateer ×2, Galleon +3,
   * −4 per occupied hold, min 1); it escapes on roll(1, atk+def) <= def.
   * No outcome is applied; @EVASIVE popup. The attacker's MP surcharge is
   * charged by the caller like any failed attack.
   */
  if (dt->attack < at->attack) {
    const int ap = units_naval_evade_power(pool, attacker_id);
    const int dp = units_naval_evade_power(pool, defender_id);
    int evades;
    if (rng) {
      evades = dos_rng_range(rng, 1, ap + dp) <= dp;
    } else {
      evades = 0; /* no-RNG (deterministic test) path: always fight */
    }
    if (evades) {
      if (units_combat_human_involved(col1, atk->nation_id, def->nation_id)) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, def->nation_id);
        tok.string1 = dt->name[0] ? dt->name : "Ship";
        tok.string2 = units_combat_nation_label(col1, atk->nation_id);
        tok.string3 = at->name[0] ? at->name : "Ship";
        char fb[AI_POPUP_BODY_LEN];
        snprintf(
          fb, sizeof(fb), "%s %s evades %s %s.",
          tok.string0, tok.string1, tok.string2, tok.string3
        );
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_SHIP, "EVASIVE", def->nation_id, atk->nation_id, 0, &tok, fb
        );
      }
      units_combat_pump_popups();
      return false;
    }
  }

  /* DOS 1b0e tail fizzle (FUN_281f_03ea, duration 8) — sunk/seized ships
   * pixelate away like land losers do. Snapshot before the outcome. */
  units_dissolve_notify(0);

  if (eng.atk_wins) {
    units_combat_outcome_popups(
      pool, atk, def, 1, atk->nation_id, def->nation_id, 1, 0, col1
    );
    const int def_x = def->x;
    const int def_y = def->y;
    const int def_nation = def->nation_id;
    const int atk_nation = atk->nation_id;
    const int def_alive = units_apply_naval_loss_outcome(
      pool, defender_id, attacker_id, eng.def_strength, eng.atk_strength, 1, col1, rng
    );
    /* DOS 0ec0 sweep: every stackmate on the loser tile takes its own loss. */
    units_sweep_naval_stack_after_loss(
      pool, def_x, def_y, def_nation, attacker_id, defender_id, col1, rng
    );
    {
      const ColonizeUnitType* wt = units_type(pool, atk->type_index);
      const int is_priv = units_type_is_privateer(wt);
      const int human = units_combat_human_involved(col1, atk_nation, def_nation);
      /*
       * bugs.md: the "captured/seized" cue belongs to a Privateer (or the
       * Royal Navy) taking an UNARMED transport as a prize — and only when
       * the prize is actually TAKEN (sunk/seized), not when it escapes
       * damaged. A warship defeating a warship is damage-or-sink only.
       */
      const int def_transport = dt && dt->attack == 0;
      if (human && def_transport && !def_alive) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = dt && dt->name[0] ? dt->name : "Ship";
        if (is_priv) {
          /*
           * Privateer prize — not Crown. GAME.TXT @SEIZURE* is Royal Navy /
           * Army wording, so it doesn't fit; no catalog section covers a
           * Privateer capture at all. Port-authored short notice.
           */
          char body[AI_POPUP_BODY_LEN];
          snprintf(
            body,
            sizeof(body),
            "%s taken as a prize.",
            tok.string0
          );
          if (g_units_combat_popups) {
            ai_popup_enqueue_ok_ctx(
              g_units_combat_popups,
              AI_POPUP_TAG_COMBAT_SEIZURE,
              atk_nation,
              def_nation,
              0,
              NULL,
              body
            );
          }
        } else {
          /* Crown / warship seizure → Royal Navy @SEIZURESEA. */
          units_combat_enqueue_tok(
            AI_POPUP_TAG_COMBAT_SEIZURE,
            "SEIZURESEA",
            atk_nation,
            def_nation,
            0,
            &tok,
            "");
        }
      }
    }
    g_units_last_combat = 1;
    units_dissolve_notify(1);
    units_combat_pump_popups();
    return true;
  }
  units_combat_outcome_popups(
    pool, def, atk, 0, atk->nation_id, def->nation_id, 1, 0, col1
  );
  {
    const int atk_x = atk->x;
    const int atk_y = atk->y;
    const int atk_nation = atk->nation_id;
    (void)units_apply_naval_loss_outcome(
      pool, attacker_id, defender_id, eng.atk_strength, eng.def_strength, 1, col1, rng
    );
    /* DOS 0ec0 sweep on the attacker's own tile stack, unconditional. */
    units_sweep_naval_stack_after_loss(
      pool, atk_x, atk_y, atk_nation, defender_id, attacker_id, col1, rng
    );
  }
  g_units_last_combat = -1;
  units_dissolve_notify(1);
  units_combat_pump_popups();
  return false;
}

/* ===================== Coastal fort fire, ship-slowing & foreign-colony capture setup (units_coastal_fort_attack_strength .. units_capture_claim_ring) ===================== */


int units_coastal_fort_attack_strength(
  const ColonizeColonyPool* colonies,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units
) {
  if (!colonies || !colony || !colony->active || !units) {
    return 0;
  }
  /*
   * DOS-LITERAL FUN_364b_03f6 raw 57040-57058 (bugs.md #764): the tier is a
   * SUM of two independent stockade-level probes, not an either/or —
   *   local_c  = (FUN_281f_09fc(1) != 0);            // Fort level
   *   if (FUN_281f_09fc(2) != 0) local_c++;          // Fortress level
   * A Fortress colony answers YES to both probes (FUN_157e_0008 raw 8895-8908
   * sums all three level probes), so local_c reaches 2 there and 1 for a plain
   * Fort. The port stores the two buildings as separate flags and an upgraded
   * colony may carry only the Fortress flag, so the Fort probe is spelled as
   * "has at least Fort level".
   */
  const int fortress_row = colonies_building_row(colonies, COLONY_BUILDING_FORTRESS);
  const int fort_row = colonies_building_row(colonies, COLONY_BUILDING_FORT);
  const int has_fortress =
    fortress_row >= 0 && fortress_row < COLONIZE_BUILDING_TYPES_MAX &&
    colony->has_building[fortress_row];
  const int has_fort =
    (fort_row >= 0 && fort_row < COLONIZE_BUILDING_TYPES_MAX &&
     colony->has_building[fort_row]) ||
    has_fortress;
  const int tier = (has_fort ? 1 : 0) + (has_fortress ? 1 : 0);
  if (tier <= 0) {
    return 0;
  }
  int arty = 0;
  int slot_a = 0;
  /* DOS-LITERAL raw 57053-57057: the chain walk counts EVERY unit on the tile
   * with type byte 0x0b; there is no nation test and no damaged-bit test. The
   * port-side `u->nation_id != colony->nation_id` filter was removed
   * 2026-09-23 (bugs.md #764). */
  for (const ColonizeUnit* u = units_next_on_tile_const(units, colony->x, colony->y, &slot_a);
       u != NULL; u = units_next_on_tile_const(units, colony->x, colony->y, &slot_a)) {
    const ColonizeUnitType* t = units_type(units, u->type_index);
    if (!t) {
      continue;
    }
    if (units_type_is_artillery(t)) {
      arty++;
    }
  }
  /* DOS-LITERAL raw 57058: `local_12 * local_c * 4`, local_12 starting at 1
   * → 4 * tier * (1 + arty). */
  return 4 * tier * (1 + arty);
}

/*
 * bugs.md #465. DOS-LITERAL FUN_364b_03f6 raw 57082-57083:
 *
 *     rel = FUN_281f_0a38(colony_nation, ship_nation);
 *     if ((rel & 0x40) == 0 || ship_type == 0x10) { fire; }
 *
 * "the PEACE bit is clear, OR the target is a Privateer". The F8 Foreign
 * Affairs report reads the very same byte and bit (reports.c
 * REPORTS_FOREIGN_PEACE_BIT), so what the player sees as Peace/War there is
 * exactly what the battery obeys. The earlier port spelling gated on the
 * Linux WAR bit (0x02) instead: a treaty writer that set PEACE without
 * clearing WAR left a pair reading "peace" in F8 while the fort still fired
 * (user-observed: Spanish Caravel shot at while F8 showed Spain at peace).
 */
static int units_fort_fire_is_hostile(
  const ColonizeCol1Save* col1,
  int owner_nation,
  const ColonizeUnit* ship,
  const ColonizeUnitType* st
) {
  if (!ship || ship->nation_id == owner_nation) {
    return 0;
  }
  if (units_type_is_privateer(st)) {
    return 1;
  }
  if (!col1 || owner_nation < 0 || owner_nation > 3) {
    return 0;
  }
  if (ship->nation_id >= 0 && ship->nation_id <= 3) {
    return (ai_diplo_read(col1, owner_nation, ship->nation_id) & AI_DIPLO_PEACE) == 0;
  }
  if (ship->nation_id >= 4 && ship->nation_id <= 11) {
    return ai_diplo_indian_at_war(col1, owner_nation, ship->nation_id - 4);
  }
  return 0;
}

/*
 * Fort battery vs one ship: attack strength vs ship defense (Drake scales
 * Privateer defense). Winner sink only — no temp attacker to despawn/plunder.
 */
bool units_fort_vs_ship(
  ColonizeUnitPool* pool,
  int attack_str,
  int fort_nation,
  int defender_id,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1,
  const char* atk_label
) {
  ColonizeUnit* def = units_get(pool, defender_id);
  if (!def || !def->active || !units_is_sea(pool, defender_id) || attack_str <= 0) {
    return false;
  }
  const ColonizeUnitType* dt = units_type(pool, def->type_index);
  if (!dt) {
    return false;
  }
  int defense = dt->defense;
  if (defense < 0) {
    defense = 0;
  }
  defense = units_drake_scale_strength(pool, def, defense, col1);
  /* bugs.md #261: DOS engages through the real resolver (temp attacker →
   * FUN_5fef_1b0e), so a fort firing on a human-visible ship shows the
   * Combat Analysis dialog before the roll, same as any naval fight. */
  {
    ColonizeCombatEngagement eng;
    memset(&eng, 0, sizeof(eng));
    eng.attacker_id = -1;
    eng.defender_id = defender_id;
    eng.is_naval = true;
    eng.atk_strength = attack_str;
    eng.def_strength = defense;
    eng.atk_flags.base_combat = attack_str;
    eng.def_flags.base_combat = dt->defense > 0 ? dt->defense : 0;
    if (defense > eng.def_flags.base_combat) {
      eng.def_flags.flags_hi |= COMBAT_FLAG_DRAKE;
    }
    /* NAMES.TXT @BUILDING tier 1 ("Fort"): only caller always passes its own
     * fort_label, this is the defensive default for a NULL/empty one. */
    snprintf(
      eng.atk_label, sizeof(eng.atk_label), "%s",
      atk_label && atk_label[0] ? atk_label : reports_fort_tier_name(1)
    );
    units_combat_maybe_present_analysis(col1, &eng, fort_nation, def->nation_id);
  }
  /* attack_str >= 1 (gated above) and defense >= 0, so total >= 1 always. */
  const int total = attack_str + defense;
  bool atk_wins = false;
  if (!rng) {
    atk_wins = attack_str >= defense;
  } else {
    const int roll = dos_rng_range(rng, 1, total);
    atk_wins = roll <= attack_str;
  }
  if (atk_wins) {
    /*
     * bugs.md #249: DOS engages via the REAL naval resolver (temp attacker →
     * FUN_5fef_1b0e), so a fort win lands the same outcomes as any naval
     * fight: holds lost, damage-vs-sink roll (fort strength as "guns"),
     * damaged ship relocates to the repair port with the DOS repair timer
     * (0352 doubles the repair bill when the winner is not a ship), popups.
     */
    const int human = units_combat_human_involved(col1, def->nation_id, fort_nation);
    units_ship_lose_holds(pool, defender_id);
    const int lhull = dt->hull > 0 ? dt->hull : 0;
    /* Same DOS 0352 roll as the naval path — fort strength stands in for the
     * winner's @UNIT guns column. See units_ship_damage_vs_sink. */
    int damaged = units_ship_damage_vs_sink(rng, attack_str, lhull);
    /* bugs.md #867: DOS reaches 0352 through the 1b0e temp attacker, so the
     * raw 99527-99570 gate applies here too. `wt` NULL = the winner is not a
     * unit type (only the Frigate-vs-Frigate clause reads it). */
    damaged = units_naval_damage_gate(pool, col1, def, NULL, fort_nation, damaged);
    const ColonizeColony* home = NULL;
    if (damaged) {
      home = units_nearest_own_drydock_colony(
        g_units_combat_colonies, def->nation_id, def->x, def->y
      );
      if (!home && col1 && col1->head.game_options.woi &&
          def->nation_id == (int)col1->head.human_player) {
        damaged = 0; /* WoI human with no drydock port: she goes down */
      }
    }
    /* bugs.md #464: 364b_03f6 resolves through the real combat resolver
     * (FUN_291f_0a14 = FUN_5fef_1b0e, raw 57095), so the fort's kill gets
     * 1b0e's own outcome redraw — the FUN_281f_03ea fizzle. Snapshot the
     * "before" frame while the ship is still on its tile. */
    units_dissolve_notify(0);
    if (damaged) {
      /* One repair path for both call sites: the fort has no unit of its own,
       * so DOS's "winner is not a ship" doubling of the @UNIT combat column
       * (0352 raw 99626-99628) is applied to the battery strength here. */
      units_ship_enter_repair(pool, def, attack_str << 1, home, col1, human, fort_nation);
      units_dissolve_notify(1);
      return false; /* ship survives damaged */
    }
    if (human) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = units_combat_nation_label(col1, def->nation_id);
      tok.string1 = dt->name;
      tok.string2 = "coastal";
      tok.string3 = "fortifications";
      char fb[AI_POPUP_BODY_LEN];
      snprintf(fb, sizeof(fb), "%s %s sunk by coastal fortifications!", tok.string0, tok.string1);
      units_combat_enqueue_tok(
        AI_POPUP_TAG_COMBAT_SHIP, "SHIPSUNK", def->nation_id, -1, 0, &tok, fb
      );
      units_play_event_sound(0x57);
    }
    /* FUN_5fef_0352 raw 99653-99690: Royal-flagged hull lost → tax cut (bugs.md #659). */
    (void)units_royal_loss_tax_cut((ColonizeCol1Save*)col1, pool, def);
    units_despawn(pool, defender_id);
    units_dissolve_notify(1);
    return true;
  }
  /* bugs.md #249: fort loses the exchange → NOTHING happens (DOS undoes the
   * temp attacker and the ship sails on; the old ship-slow was invented). */
  return false;
}

/*
 * FUN_5bfb_3180 ship-slow (decomp 98519-98624), the naval half of the
 * post-step 8-neighbour scan that 465b's commit tail runs for every mover,
 * human or AI, per STEP. Two independent branches, each gated on: mover is
 * a ship (type 0x0d..0x12), mover still has MP (`spent < 090c max`), and the
 * pair is not at PEACE (`0a38 & 0x40 == 0`) OR the mover is a Privateer
 * (0x10) — so a Privateer is slowed by everyone and, via the neighbour's
 * own scan, slows everyone.
 *
 *   A. Foreign ship on an adjacent WATER tile (0768 != 0; a ship docked in
 *      a colony is on land and falls to B): for each unit in that tile's
 *      stack that is a ship, drain = by the NEIGHBOUR's type — Privateer 4,
 *      Frigate 6, Man-O-War 8; other hulls 0 (skipped). Roll
 *      `04d4(1, 312e(mover) + 312e(neighbour) + 2)`: roll < mover power →
 *      no slow (+ @SHIPRUN "slips past" if either side is human), roll ==
 *      power → half drain, else full. Slow → `spent += drain`, @SHIPSLOW
 *      (0x1a51) when the mover is human. Loop stops once the mover is out
 *      of MP.
 *   B. Foreign colony adjacent (07be >= 0): no roll. Fortress (09fc(2)) →
 *      `spent += 50` (dead stop), else Fort (09fc(1)) → `spent += 2`,
 *      Stockade/none → nothing. @SHIPSLOW (0x1a5a) with the building name
 *      when the mover is human. Independent of fort fire (that is the
 *      end-of-turn 5fef_1b0e temp-attacker path; bugs.md #249).
 *
 * 312e power = max MP thirds + 3, ×2 for a Privateer (0x10), +3 for a
 * Galleon (0x0f), −4 per hold in use, floor 1. Type ids: 13 Caravel .. 18
 * Man-O-War (docs/units.md). Port keeps Euro ships' countdown in
 * moves, so `spent += n` is `moves -= n` floored at 0.
 */
/* bugs.md #868/#869: 312e has exactly ONE port spelling —
 * units_naval_evade_power (units_combat.c). The copy that used to live here
 * subtracted 4 per PASSENGER (`cargo_count`) where DOS raw 98448 reads
 * +0x3150, the goods-hold count. */
static int units_ship_slow_power(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  return units_naval_evade_power(pool, u->id);
}

static int units_ship_slow_drain_for(ColonizeUnitKind k) {
  switch (k) {
    case UNITS_KIND_PRIVATEER: return 4;
    case UNITS_KIND_FRIGATE: return 6;
    case UNITS_KIND_MAN_O_WAR: return 8;
    default: return 0;
  }
}

static void units_ship_slow_popup(
  const ColonizeUnitPool* pool,
  const ColonizeUnit* mover,
  const char* tag,
  int other_nation,
  const char* other_thing,
  const char* other_thing2
) {
  if (!g_units_combat_popups || !g_units_combat_game_txt) {
    return;
  }
  const ColonizeCol1Save* col1 = g_units_fallout_col1;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  char body[AI_POPUP_BODY_LEN];
  body[0] = '\0';
  const char* mover_name = units_display_name(pool, mover);
  const char* other_adj = units_combat_nation_label(col1, other_nation);
  if (strcmp(tag, "SHIPRUN") == 0) {
    /* {%STRING2 %STRING0} slips past {%STRING1 %STRING3}! */
    tok.string0 = mover_name;
    tok.string1 = other_adj;
    tok.string2 = units_combat_nation_label(col1, mover->nation_id);
    tok.string3 = other_thing;
  } else {
    /* {%STRING0}'s progress slowed by presence of {%STRING1 %STRING2}. */
    tok.string0 = mover_name;
    tok.string1 = other_adj;
    tok.string2 = other_thing;
  }
  (void)other_thing2;
  popup_msg_fill(g_units_combat_game_txt, tag, &tok, "", body, sizeof(body));
  if (body[0]) {
    ai_popup_enqueue_ok(g_units_combat_popups, AI_POPUP_TAG_INFO, NULL, body);
  }
}

static int units_ship_slow_gate(const ColonizeCol1Save* col1, int mover_nation, int other_nation,
                                ColonizeUnitKind mover_kind) {
  if (mover_kind == UNITS_KIND_PRIVATEER) {
    return 1;
  }
  if (!col1) {
    return 1; /* no relation table: nobody is at PEACE (bit 0x40 clear) */
  }
  return (ai_diplo_read(col1, mover_nation, other_nation) & AI_DIPLO_PEACE) == 0;
}

void units_ship_slow_scan_w(
  const ColonizeWorld* w,
  int unit_id
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;
  ColonizeDosRng* rng = w->rng;

  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !map || !units_is_on_map(u) || !units_is_sea(pool, unit_id) ||
      u->nation_id < 0 || u->nation_id > 3) {
    return;
  }
  /* local_36: branch A needs the mover itself on water; B does not. */
  const int mover_on_water = map_tile_is_water(map, u->x, u->y);
  const ColonizeCol1Save* col1 = g_units_fallout_col1;
  const ColonizeUnitKind mover_kind = units_type_kind(units_type(pool, u->type_index));
  const int mover_human = units_combat_human_involved(col1, u->nation_id, -1);
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
      continue;
    }
    /* Branch A: foreign ship stack on an adjacent water tile. */
    const int top = units_id_at(pool, nx, ny);
    const ColonizeUnit* topu = top >= 0 ? units_get_const(pool, top) : NULL;
    if (mover_on_water && topu && topu->active && topu->nation_id != u->nation_id &&
        map_tile_is_water(map, nx, ny) &&
        units_ship_slow_gate(col1, u->nation_id, topu->nation_id, mover_kind)) {
      const int other_nation = topu->nation_id;
      /*
       * DOS-LITERAL FUN_5bfb_3180 raw 98530-98588: the inner walk is the
       * TILE CHAIN (`local_44 = FUN_281f_02e4(...)`) filtered only on type
       * 0x0d..0x12. `local_46` (the top unit's nation) is read for the gate
       * and the popup, never as a per-link filter — bugs.md #878(i). The
       * port used to skip stackmates of a different nation than the tile's
       * top unit.
       */
      for (int i = 0; i < COLONIZE_UNITS_MAX && u->moves > 0; ++i) {
        const ColonizeUnit* f = &pool->units[i];
        if (!f->active || f->x != nx || f->y != ny || !units_is_sea(pool, f->id)) {
          continue;
        }
        const char* fname = units_display_name(pool, f);
        int drain = units_ship_slow_drain_for(units_type_kind(units_type(pool, f->type_index)));
        if (drain == 0) {
          continue;
        }
        const int self_power = units_ship_slow_power(pool, u);
        const int foe_power = units_ship_slow_power(pool, f) + 2;
        const int roll = rng ? dos_rng_range(rng, 1, self_power + foe_power) : self_power + foe_power;
        if (roll < self_power) {
          drain = 0;
        } else if (roll == self_power) {
          drain >>= 1;
        }
        if (drain == 0) {
          if (mover_human || units_combat_human_involved(col1, other_nation, -1)) {
            units_ship_slow_popup(pool, u, "SHIPRUN", other_nation, fname, NULL);
          }
          continue;
        }
        u->moves -= drain;
        if (u->moves < 0) {
          u->moves = 0;
        }
        if (diag_info_enabled()) {
          diag_info("SHIPSLOW unit %d by %s (%d) drain %d -> %d", unit_id, fname, f->id, drain, u->moves);
        }
        if (mover_human) {
          units_ship_slow_popup(pool, u, "SHIPSLOW", other_nation, fname, NULL);
        }
      }
    }
    /* Branch B: foreign colony with a Fort / Fortress. */
    const int cid = colonies ? colonies_id_at(colonies, nx, ny) : -1;
    const ColonizeColony* col = cid >= 0 ? colonies_get(colonies, cid) : NULL;
    if (col && col->active && col->nation_id != u->nation_id && u->moves > 0 &&
        units_ship_slow_gate(col1, u->nation_id, col->nation_id, mover_kind)) {
      const int fortress = colonies_building_row(colonies, COLONY_BUILDING_FORTRESS);
      const int fort = colonies_building_row(colonies, COLONY_BUILDING_FORT);
      const char* bname = NULL;
      int drain = 0;
      if (fortress >= 0 && col->has_building[fortress]) {
        drain = 50;
        /* Live @BUILDING name (NAMES.TXT); "" fallback if unset. */
        bname = colonies->building_types[fortress].name[0]
          ? colonies->building_types[fortress].name
          : "";
      } else if (fort >= 0 && col->has_building[fort]) {
        drain = 2;
        bname = colonies->building_types[fort].name[0]
          ? colonies->building_types[fort].name
          : "";
      }
      if (drain > 0) {
        u->moves -= drain;
        if (u->moves < 0) {
          u->moves = 0;
        }
        if (diag_info_enabled()) {
          diag_info("SHIPSLOW unit %d by %s %s drain %d -> %d", unit_id, col->name, bname, drain, u->moves);
        }
        if (mover_human) {
          units_ship_slow_popup(pool, u, "SHIPSLOW", col->nation_id, bname, NULL);
        }
      }
    }
  }
}

/*
 * FUN_5bfb_3180 sentry-wake half (decomp 98628-98646) — the third independent
 * branch of the post-step 8-neighbour scan that 465b's commit tail runs for
 * EVERY mover, human or AI, once per STEP (the naval half is
 * units_ship_slow_scan_w above).
 *
 * Rule: for each of the 8 neighbours of the mover's destination whose unit
 * stack belongs to another nation (`local_46 != uVar7`, raw 98521 — there is
 * NO war/treaty gate and no sight radius; plain adjacency of a foreign unit),
 * every SENTRY unit in that stack has its order byte cleared
 * (`*(undefined1 *)(local_44 * 0x1c + 0x314c) = 0`, raw 98640). Only order 1
 * is touched; Fortify/Fortified stay put (their own wake is FUN_5bfb_12d0,
 * ai_diplo_wake_border_garrisons).
 *
 * Two DOS gates, both literal:
 *   - domain (raw 98628-98630): the mover must stand on a Euro colony tile
 *     (`local_34` = FUN_281f_0696(dest) >= 0), OR the neighbour tile must
 *     carry a settlement (`local_42` = FUN_281f_06be(n) >= 0, rebound to a
 *     boolean at raw 98591), OR the neighbour's water-ness must equal the
 *     destination's (`FUN_281f_0768(n) == local_36`). So a ship sailing past
 *     open coast does not wake the sentries ashore and a land march does not
 *     wake anchored ships, unless a settlement is involved.
 *   - aboard (raw 98635-98638): a NON-ship type (outside 0x0d..0x12) standing
 *     on a water tile is a passenger in a hold and is left asleep.
 *
 * DOS writes the order byte only; the port routes through units_wake so the
 * park_nights MP-refund discriminator stays the single owner of that rule.
 * bugs.md #539 (REF landfall left the garrison's Sentry units asleep).
 */
void units_sentry_wake_scan(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int unit_id
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!pool || !map || !u || !u->active || !units_is_on_map(u) || u->nation_id < 0) {
    return;
  }
  const ColonizeCol1Save* col1 = g_units_fallout_col1;
  const int mover_nation = u->nation_id;
  const int dest_x = u->x;
  const int dest_y = u->y;
  /* local_36 / local_34 (raw 98502-98504). */
  const bool dest_water = map_tile_is_water(map, dest_x, dest_y);
  const bool mover_in_colony = colonies && colonies_id_at(colonies, dest_x, dest_y) >= 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = dest_x + MAP_DIR8_DX[d];
    const int ny = dest_y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
      continue;
    }
    /* local_44 = FUN_281f_07e0(n) stack head, local_46 = its nation nibble. */
    const int top = units_id_at(pool, nx, ny);
    const ColonizeUnit* topu = top >= 0 ? units_get_const(pool, top) : NULL;
    if (!topu || !topu->active || topu->nation_id < 0 || topu->nation_id == mover_nation) {
      continue;
    }
    const bool nb_settlement =
      (colonies && colonies_id_at(colonies, nx, ny) >= 0) ||
      (col1 && col1_save_tribe_at(col1, nx, ny) != NULL);
    if (!mover_in_colony && !nb_settlement && map_tile_is_water(map, nx, ny) != dest_water) {
      continue;
    }
    int slot = 0;
    for (ColonizeUnit* f = units_next_on_tile(pool, nx, ny, &slot); f != NULL;
         f = units_next_on_tile(pool, nx, ny, &slot)) {
      if (f->orders != UNITS_ORDER_SENTRY) {
        continue;
      }
      if (!units_is_sea(pool, f->id) && map_tile_is_water(map, f->x, f->y)) {
        continue; /* land unit on water: riding in a hold (raw 98635-98638) */
      }
      units_wake(pool, f->id);
    }
  }
}

int units_coastal_fort_fire_pulse_w(
  const ColonizeWorld* w,
  int human_nation,
  char* status,
  size_t status_size
) {
  ColonizeUnitPool* units = w->units;
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeWorldMap* map = w->map;
  const ColonizeCol1Save* col1 = w->col1;
  ColonizeDosRng* rng = w->rng;

  if (!units || !colonies || !map) {
    return 0;
  }
  int sunk = 0;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* col = &colonies->colonies[ci];
    if (!col->active) {
      continue;
    }
    const int atk = units_coastal_fort_attack_strength(colonies, col, units);
    if (atk <= 0) {
      continue;
    }
    /* bugs.md #261: analysis header for the unit-less attacker. */
    char fort_label[40];
    {
      const int fortress = colonies_building_row(colonies, COLONY_BUILDING_FORTRESS);
      const int is_fortress = fortress >= 0 && fortress < COLONIZE_BUILDING_TYPES_MAX &&
        col->has_building[fortress];
      /* Live @BUILDING name (NAMES.TXT); "" fallback if unset. */
      const int fort = colonies_building_row(colonies, COLONY_BUILDING_FORT);
      const char* bname = is_fortress
        ? (colonies->building_types[fortress].name[0] ? colonies->building_types[fortress].name : "")
        : (fort >= 0 && colonies->building_types[fort].name[0] ? colonies->building_types[fort].name : "");
      snprintf(
        fort_label, sizeof(fort_label), "%s %s",
        col->name[0] ? col->name : "", bname
      );
    }
    for (int d = 0; d < 8; ++d) {
      const int nx = col->x + MAP_DIR8_DX[d];
      const int ny = col->y + MAP_DIR8_DY[d];
      if (!map_tile_is_water(map, nx, ny)) {
        continue;
      }
      /*
       * bugs.md #465: ONE shot per neighbour tile, at the first SHIP in that
       * tile's stack — FUN_364b_03f6 (raw 57068-57076) walks the stack with
       * FUN_281f_02e4 only until the type byte lands in 0x0d..0x12, then
       * tests that one unit's nation nibble and fires. A tile whose stack
       * head is a ship of the colony's own nation is skipped entirely; the
       * port used to snapshot every hostile hull on the tile and fire at all
       * of them, so a fleet took one salvo per hull per turn.
       */
      int targets[1];
      int n_tg = 0;
      int slot_b = 0;
      for (const ColonizeUnit* u = units_next_on_tile_const(units, nx, ny, &slot_b); u != NULL;
           u = units_next_on_tile_const(units, nx, ny, &slot_b)) {
        if (!units_is_sea(units, u->id)) {
          continue;
        }
        const ColonizeUnitType* st = units_type(units, u->type_index);
        if (units_fort_fire_is_hostile(col1, col->nation_id, u, st)) {
          targets[n_tg++] = u->id;
        }
        break; /* first ship in the stack, hostile or not */
      }
      for (int t = 0; t < n_tg; ++t) {
        const ColonizeUnit* before = units_get(units, targets[t]);
        const int ship_nation = before ? before->nation_id : -1;
        const int human_chrome =
          status && status_size > 0 &&
          (col->nation_id == human_nation || ship_nation == human_nation);
        if (human_chrome) {
          /* GAME.TXT @FORTFIRE: "{%STRING0} at {%STRING1} opens fire on
           * {%STRING2 %STRING3}!" — announced whenever the fort/fortress
           * fires on a hostile ship, hit or miss (FUN_364b_03f6 tail). */
          const ColonizeUnitType* ship_type = before ? units_type(units, before->type_index) : NULL;
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          const int fortress = colonies_building_row(colonies, COLONY_BUILDING_FORTRESS);
          const int is_fortress = fortress >= 0 && fortress < COLONIZE_BUILDING_TYPES_MAX &&
            col->has_building[fortress];
          /* Live @BUILDING name (NAMES.TXT); "" fallback if unset. */
          const int fort = colonies_building_row(colonies, COLONY_BUILDING_FORT);
          tok.string0 = is_fortress
            ? (colonies->building_types[fortress].name[0] ? colonies->building_types[fortress].name : "")
            : (fort >= 0 && colonies->building_types[fort].name[0] ? colonies->building_types[fort].name : "");
          tok.string1 = col->name[0] ? col->name : "";
          tok.string2 = units_combat_nation_label(col1, ship_nation);
          tok.string3 = ship_type && ship_type->name[0] ? ship_type->name : "";
          units_combat_enqueue_tok(
            AI_POPUP_TAG_COMBAT_SHIP, "FORTFIRE", col->nation_id, ship_nation, 0, &tok, ""
          );
          /*
           * bugs.md #535: DOS announces and THEN resolves, one fort at a
           * time - raw 57104-57105 fires the blocking @FORTFIRE dialog
           * (FUN_281f_0652, tag 0xd7f) and only once it is answered calls
           * FUN_291f_0a14 (= FUN_5fef_1b0e), which draws the Combat
           * Analysis and rolls. The port's announcement goes to the
           * deferred ai_popup queue while the analysis is a nested modal,
           * so with several forts firing the two channels batched apart.
           * Pump here, as every other combat site does, so the
           * announcement is answered before this fort's analysis opens.
           */
          units_combat_pump_popups();
        }
        if (units_fort_vs_ship(units, atk, col->nation_id, targets[t], rng, col1, fort_label)) {
          sunk++;
          if (human_chrome) {
            snprintf(status, status_size, "Coastal fort sank a ship.");
          }
        }
        /* Drain this fort's own outcome chrome (@SHIPSUNK / repair notice)
         * before the next fort fires - the pump this pulse was missing
         * (bugs.md #535); units_fort_vs_ship has none of its own. */
        units_combat_pump_popups();
        /* bugs.md #249: fort loss/miss → nothing happens, no chrome. */
      }
    }
  }
  return sunk;
}


/*
 * TWO combat-role predicates, two different questions (smell audit
 * 2026-09-09 #12 — they were being picked by name similarity):
 *
 *   combat_strength.c `combat_unit_is_combat_role(pool, id)` — "does this
 *     unit's @UNIT TYPE row carry the combat flag?", i.e. the literal
 *     DS:0x5236 column read (`type[*0xe + 0x5236] != 0`, spelled
 *     `type->attack > 0` here). That is the byte FUN_5fef_0000 skips on.
 *     Use it whenever DOS reads the type table AND the port's body model
 *     cannot disagree with it.
 *     (FUN_5fef_0352's WINNER-may-capture gate, viceroy_unpacked.c 99378-80,
 *     reads the same DOS byte but is spelled with the BODY predicate below —
 *     smell audit 2026-09-10 #5: DOS's armed colonist is type 1 Soldiers, so
 *     the type read is the wrong half of the split for that unit.)
 *
 *   units.c `units_is_combat_role(pool, u)` (this one) — "can this BODY
 *     fight?", the same question asked of a port unit. DOS stores a
 *     colony-armed colonist as @UNIT type 1 "Soldiers" (attack 2); this port
 *     keeps the Colonists body and hangs muskets/horses on it, so its type
 *     byte is 0 and the type-table read alone answers NO for a unit DOS
 *     would have called a soldier. Carried kit therefore counts. Use it for
 *     "is this unit a combatant" gameplay gates (move bounce, defender
 *     ranking); the two agree on every stock typed military unit.
 *
 * Non-combat movers bounce off foreign stacks instead of fighting.
 */
bool units_is_combat_role(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  if (!pool || !u) {
    return false;
  }
  if (u->muskets > 0 || u->horses > 0) {
    return true;
  }
  const ColonizeUnitType* t = units_type(pool, u->type_index);
  return t && t->attack > 0;
}

static bool units_at_war_for_move_target(int a, int b, const ColonizeUnit* target) {
  if (a < 0 || b < 0 || a == b) {
    return false;
  }
  /*
   * bugs.md: an INDIAN mover walking into a Euro must not open combat on a
   * wander — that made Indians attack units with zero alarm. A native
   * attack now needs real hostility: the Indian×Euro war flag, or alarm at
   * the "already hostile" cut (0x4b). No Treasure exception: DOS has no
   * `type == 0x0a` term in the move/attack chain; the treasure pull is the
   * FUN_4d56_021a scorer's +0x10 tile weight (ai.c:3520). A Euro attacking a
   * native stays
   * always fightable (the @WHACKINDIANS confirm gates the human side).
   */
  if (a >= 4 && a <= 11 && b >= 0 && b <= 3) {
    if (!g_units_ff_col1) {
      return true;
    }
    (void)target;
    if (ai_diplo_indian_at_war(g_units_ff_col1, b, a - 4)) {
      return true;
    }
    const ColonizeCol1Indian* ind = &g_units_ff_col1->indian[a - 4];
    return (int)ind->alarm_by_player[b] >= 0x4b;
  }
  /* Euro mover vs native, or native vs native: always fightable. */
  if (a >= 4 || b >= 4) {
    return true;
  }
  if (!g_units_ff_col1) {
    return true; /* tests / no diplo: allow combat */
  }
  /*
   * Smell #106: DOS FUN_465b_0000 has NO war-state test on the Euro×Euro
   * attack path — the only relation gate anywhere is the human-only
   * @HAVETREATY prompt on the signed-treaty bit 0x40 (viceroy 75545-75551),
   * and war is declared as a side effect of the attack (75567-75593). The
   * port's at-war requirement made AI nations essentially unable to open a
   * Euro war (@SNEAK near-dead). AI movers now fight unless a signed treaty
   * stands; the human mover keeps the at-war test because game_loop's
   * confirm chain (declare-then-move) runs before the probe.
   */
  if (ai_diplo_at_war(g_units_ff_col1, a, b)) {
    return true;
  }
  if (a == g_units_combat_human_nation) {
    return false;
  }
  return (ai_diplo_read(g_units_ff_col1, a, b) & AI_DIPLO_PEACE) == 0;
}

bool units_at_war_for_move(int a, int b) {
  return units_at_war_for_move_target(a, b, NULL);
}

/*
 * bugs.md #439: refusing an Indian demand writes +0x80 into the VILLAGE's own
 * attitude word (LAB_5bfb_0ff2), and DOS treats a village grudge over 0x7f as
 * hostility in its own right (the same `0x7f <` test FUN_521d_0906 uses on
 * DS:0x54f6). The nation-level gate above never sees that word, so a refused
 * demand could never be answered with an attack. A native mover with a home
 * village holding such a grudge fights the offender.
 */
bool units_native_village_grudge(const ColonizeUnit* mover, int euro_nation) {
  if (!mover || !g_units_ff_col1 || !g_units_ff_col1->tribe || euro_nation < 0 ||
      euro_nation > 3) {
    return false;
  }
  const int home = mover->home_tribe_id;
  if (home < 0 || home >= (int)g_units_ff_col1->head.tribe_count) {
    return false;
  }
  return col1_tribe_attitude(&g_units_ff_col1->tribe[home], euro_nation) > 0x7f;
}

bool units_village_squat_illegal(
  const ColonizeUnitPool* pool,
  const ColonizeUnitType* type,
  const ColonizeUnit* mover,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int mover_nation,
  const ColonizeColonyPool* colonies
) {
  if (!pool || !type || !map || !map->layer2 || mover_nation < 0 || mover_nation >= 4) {
    return false;
  }
  const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
  if (idx >= (size_t)map->width * (size_t)map->height) {
    return false;
  }
  if ((map->layer2[idx] & MAP_OCCUPANCY_HAS_CITY) == 0) {
    return false;
  }
  const int cid = colonies ? colonies_id_at(colonies, x, y) : -1;
  if (cid >= 0) {
    return false;
  }
  const ColonizeUnitKind k = units_type_kind(type);
  const int missionary = k == UNITS_KIND_MISSIONARY;
  /* Old set was {Soldier, Scout, Dragoon, Regular, Army, Cavalry, Artillery};
   * units_kind_is_military adds Cont. Cav., which the type->attack > 0 term
   * below already claimed (@UNIT Cont. Cav. attack 5), so the fold is inert. */
  const int combatish =
    (mover && (mover->muskets > 0 || mover->horses > 0)) ||
    units_kind_is_military(k) || k == UNITS_KIND_SCOUT || (type->attack > 0);
  return !missionary && !combatish;
}

int units_colony_plunder_stock_sum(const ColonizeColony* col) {
  if (!col) {
    return 0;
  }
  int sum = 0;
  for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
    if (col->stock[i] > 0) {
      sum += col->stock[i];
    }
  }
  return sum;
}

/*
 * FUN_5fef_1b0e capture arm, raw viceroy_unpacked.c:100937-100948 — the ring
 * the prize brings with it. For each of the 8 neighbours (DS:0xb4 / DS:0xbe
 * dir8 tables):
 *
 *   if (FUN_281f_06d2(x, y) < 0) FUN_281f_0704(x, y, new_owner);
 *
 * `06d2` = `FUN_137f_0428` = `03e4` (layer2 settlement bit 0x02 → its owner
 * nibble) falling back to `0314` (layer2 unit bit 0x01 → its owner nibble),
 * so the test reads "no settlement and no unit stands here"; `0704` =
 * `FUN_137f_0228` is the owner-nibble stamp (units_map_set_owner_nibble).
 * Same idiom ai.c already carries for FUN_4d56_4528's worked-tile claim.
 *
 * `0228`'s own @SEIZURE arm cannot fire from here: it needs a native
 * settlement on the tile (`FUN_137f_0392`), which the `06d2` gate has already
 * excluded. The centre tile's own stamp (raw 100901) is not repeated — the
 * captor's step onto the colony square already ran it through
 * units_occupancy_refresh_tile → units_claim_tile_owner_from_stack.
 *
 * NOT gated on DOS's `param_4` (the animate/interactive flag): this is map
 * state, and an AI capture stamps the ring exactly the same way.
 */
void units_capture_claim_ring(ColonizeWorldMap* map, int x, int y, int new_owner) {
  if (!map || !map->layer2 || !map->layer3) {
    return;
  }
  for (int d = 0; d < 8; ++d) {
    const int tx = x + MAP_DIR8_DX[d];
    const int ty = y + MAP_DIR8_DY[d];
    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height) {
      continue;
    }
    const uint8_t l2 = map->layer2[(size_t)ty * (size_t)map->width + (size_t)tx];
    if ((l2 & (MAP_OCCUPANCY_HAS_UNIT | MAP_OCCUPANCY_HAS_CITY)) != 0) {
      continue; /* FUN_137f_0428 returned an owner — DOS leaves the tile alone */
    }
    map_set_owner_nibble(map, tx, ty, new_owner);
  }
}
