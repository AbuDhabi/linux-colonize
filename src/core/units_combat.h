#ifndef COLONIZE_UNITS_COMBAT_H
#define COLONIZE_UNITS_COMBAT_H

#include "core/units.h"
#include "core/world.h"

/* Combat, promotion, dissolve, capture: split out of units.h for navigability. */

/*
 * Apply pending treasure ransom CHOICE (AI_POPUP_TAG_COMBAT_RANSOM).
 * Accept (choice_id==1) credits payload gold to nation_a; Refuse credits 0.
 * Returns true if the tag was handled.
 */
bool units_combat_apply_ransom_popup(
  ColonizeCol1Save* col1,
  const AiPopupState* popups
);

/*
 * Human-facing colony capture / burn GAME.TXT OKs (@CAPTURED* / @BURNED*).
 * plunder_gold: known cargo sum or 0 (uses CAPTURED3 / CAPTURED2 when 0).
 */
void units_combat_notify_colony_captured(
  const ColonizeCol1Save* col1,
  const ColonizeColony* colony,
  int capturer_nation,
  int plunder_gold
);
void units_combat_notify_colony_burned(
  const ColonizeCol1Save* col1,
  const char* colony_name,
  int victim_nation,
  const char* burner_label
);
/*
 * @BURNED3 "Spies report: …" bystander OK: enqueued for the human when they
 * are neither the burner nor the victim (AI colony burned by natives/rivals
 * while the human watches from elsewhere).
 */
void units_combat_notify_colony_burned_foreign(
  const ColonizeCol1Save* col1,
  const char* colony_name,
  int victim_nation,
  const char* burner_label
);

/*
 * FUN_5fef_0000: pick best defender on tile for engagement toughness.
 * Skips non-combat roles (type.attack==0). Artillery vs Indian attacker ×2.
 * Returns unit id or -1. except_id skips the attacker / mover.
 */
int units_best_defender_at(
  const ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  int x,
  int y,
  int attacker_id,
  int except_id
);

/*
 * Village Attack empty-tile defense (FUN_5fef_1b0e): DOS spawns a temporary
 * Brave (Armed / Mtd. by indian muskets / horse_breeding>24) on the settlement
 * — not by dragging nearby map Braves. Returns temp defender id or -1.
 * Caller must run combat then units_finish_village_temp_defender.
 */
int units_spawn_village_temp_defender(
  ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  int village_x,
  int village_y,
  int indian_nation,
  int attacker_id
);

/*
 * FUN_5fef_31ea conquest treasure gold (×100 from DOS amount byte). Runs for
 * ANY conqueror (bugs.md #381) — Hernan Cortes only removes the difficulty-0/1
 * "did it pay out at all" roll and adds +50% (difficulty 0/1) or +10/+100
 * (difficulty 2/3); at difficulty 2 and 3 everyone gets an amount.
 * rich_capital: stack-local -0xcc ← ColonizeCol1TribeState.capital.
 * Returns 0 if no treasure / no rng. Cite: viceroy_unpacked.c ~101407–101495.
 */
int units_conquest_treasure_gold(
  const ColonizeCol1Save* col1,
  int attacker_nation_id,
  ColonizeDosRng* rng,
  int rich_capital
);

/*
 * Post-win fallout stand-in for FUN_5fef_31ea / inlined FUN_5fef_1b0e:
 * If defender was native on a tribe tile and no other same-nation Braves remain
 * on that tile after win, destroy tribe. Before destroy: subjugated convert-join
 * when tribe.mission low-nibble == attacker (PEDIA Sepulveda / @INDIANSLAVES) —
 * threshold 4|8 (±Spanish/Sepulveda/Las Casas), roll dos_rng_range(0,12).
 * Conquest treasure: gold_amount>0 is used as-is, else the FUN_5fef_31ea peel
 * rolls one (no Cortes gate — see bugs.md #381).
 * May adjust Indian relation via ai_diplo helpers when col1 is set.
 */
bool units_try_native_settlement_fallout_w(
  const ColonizeWorld* w,
  int attacker_nation_id,
  int defender_nation_id,
  int tile_x,
  int tile_y,
  int gold_amount
);


/* bugs.md: combat "bump" animation hook — called once as an engagement is
 * about to resolve (attacker still on its tile, defender at (def_x,def_y)).
 * The game loop renders the attacker nudged toward the defender. */
typedef void (*ColonizeUnitsCombatWatchFn)(
  void* user,
  const ColonizeUnitPool* pool,
  int attacker_id,
  int def_x,
  int def_y
);
void units_set_combat_watch(ColonizeUnitsCombatWatchFn fn, void* user);

/*
 * Fire the combat-watch bump for a non-combat approach (DOS FUN_5bfb_022e
 * animates a visiting Brave sliding into the colony tile before its popup —
 * FUN_281f_0e08/02d0/09ba block). No-op headless (no watch installed).
 */
void units_combat_watch_notify(const ColonizeUnitPool* pool, int unit_id, int x, int y);

/*
 * DOS "fizzle" present (FUN_12d6_0000 behind thunk FUN_281f_03ea): after a
 * combat outcome (FUN_5fef_1b0e tail), a stack-sweep, or an LCR "vanishes"
 * despawn (FUN_65dd_0004 case 5), the redrawn frame is copied to the VGA in
 * 16-bit LFSR order (poly 0xB400, 64000 pixels, duration 8), so a removed
 * unit's sprite pixelates away in place. The hook fires twice per outcome:
 * phase 0 before the pool mutates (snapshot the "before" frame), phase 1
 * after (render the "after" frame and animate the transition). Headless
 * callers leave it unset.
 */
typedef void (*ColonizeUnitsDissolveFn)(void* user, int phase);
void units_set_combat_dissolve(ColonizeUnitsDissolveFn fn, void* user);

/*
 * FUN_5fef_1b0e colony-raid handoff (raw 101142): a native attacker beaten
 * at a European colony runs the full FUN_5fef_0f14 raid resolver instead of
 * just dying. The resolver lives in ai_contact.c, which the small unit-test
 * binaries do not link; ai_contact.c self-registers this hook at load time
 * (constructor), so the handoff is live in every binary that links it and a
 * plain death (the pre-port behavior) everywhere else.
 */
typedef int (*ColonizeUnitsRaidRepelledFn)(
  const ColonizeWorld* w,
  int indian_nation,
  int euro_nation,
  int colony_id,
  int home_tribe_id,
  int forced
);
void units_set_colony_raid_repelled(ColonizeUnitsRaidRepelledFn fn);

/*
 * bugs.md: combat popups are presented per-combat, not hoarded until the AI
 * slice ends. The hook runs a nested modal loop draining the ai_popup queue
 * after each combat resolution; headless callers leave it unset.
 */
typedef void (*ColonizeUnitsPopupPumpFn)(void* user);
void units_set_combat_popup_pump(ColonizeUnitsPopupPumpFn fn, void* user);
/* Present-and-answer queued popups now via the registered pump (no-op when
 * headless). bugs.md #237: popup before animation, each blocking. */
void units_pump_combat_popups(void);

/*
 * DOS entry seizure: apply FUN_5fef_0352's loss outcome to every foreign
 * non-combat unit left standing on (x, y) once the tile's defense is beaten —
 * Colonists / Wagon Trains / Treasure change hands, Pioneers / Missionaries /
 * Scouts are destroyed, Artillery is damaged then destroyed. Callers must
 * already have established that no armed defender remains.
 */
void units_seize_noncombat_at(
  ColonizeUnitPool* pool,
  int winner_id,
  int x,
  int y,
  const ColonizeCol1Save* col1
);

/*
 * FUN_5fef_0f14 kind 3 (Indian colony raid, "unit" outcome): the ship picked
 * out of the colony's port goes through FUN_5fef_0352 as a loser with NO
 * winner (`param_2 = 0xffff`), which forces the damage arm — holds and
 * passengers lost, damaged bit7, repair timer, relocation to the nearest own
 * repair port (GAME.TXT @RAIDSHIP: "{ship} damaged."). Returns 1 if the ship
 * survived damaged, 0 if it went down (0352's WoI no-port sink).
 */
int units_raid_damage_ship(ColonizeUnitPool* pool, int ship_id, const ColonizeCol1Save* col1);

/*
 * Land combat (FUN_157e / FUN_5fef_1b0e peel): attacker base×8 (004a mode 1);
 * defender engagement (015e: colony/village/terrain/fortify). Probability =
 * atk/(atk+def). Optional Combat Analysis presenter after strengths, before
 * roll. Winner stays;
 * loser despawned. Naval / mixed: no fight.
 * When col1 is non-NULL and winner nation owns Washington (PEDIA/wiki George
 * Washington; docs/fandom_col1994.md: non-veteran soldiers/dragoons who win
 * always upgrade), promote winner name/type like 1eca. col1 may be NULL (no FF
 * promote). Returns true if attacker wins.
 */
bool units_resolve_land_combat_ff_w(
  const ColonizeWorld* w,
  int attacker_id,
  int defender_id
);

/*
 * AI/contact wrapper: same as units_resolve_land_combat_ff with col1 from
 * units_set_ff_col1 (g_units_ff_col1). Callers that always passed NULL missed
 * Washington promote; turn_refresh_moves_for_nation sets the global.
 */
bool units_resolve_land_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
);

/*
 * Naval combat: FUN_157e_004a for both sides (damage/holds/Drake). Same roll
 * shape as land; ships only. Winner keeps the tile; loser despawned after hold
 * plunder into winner. Optional Combat Analysis after strengths, before roll.
 */
bool units_resolve_naval_combat_ff_w(
  const ColonizeWorld* w,
  int attacker_id,
  int defender_id
);

/*
 * AI/king wrapper: same as units_resolve_naval_combat_ff with col1 from
 * units_set_ff_col1 (g_units_ff_col1). Callers that always passed NULL missed
 * Drake privateer *3/2; turn_refresh_moves_for_nation sets the global.
 * Cite: PEDIA/wiki Francis Drake; founding_fathers.c FF_FRANCIS_DRAKE.
 */
bool units_resolve_naval_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
);

/*
 * Coastal Fort/Fortress naval fire strength (FUN_364b_03f6).
 * Fort: 4*(1+arty); Fortress: 8*(1+arty). Artillery/Cannon on colony tile
 * (owner nation). Stockade alone → 0. Cite: decomp local_12*local_c*4;
 * fandom Fort/Fortress.
 */
int units_coastal_fort_attack_strength(
  const ColonizeColonyPool* colonies,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units
);

/*
 * EOT pulse: each Fort/Fortress colony fires on adjacent ocean ships that are
 * at war with the colony owner, or Privateers (peace ignored). Fort win →
 * sink ship (no hold plunder). Fort loss → ship stopped (moves_left=0).
 * Optional human status line. Returns ships sunk. Cite: FUN_364b_03f6.
 */
/*
 * FUN_5bfb_3180 naval half, run by units_try_move after every committed
 * ship step: an adjacent foreign warship (rolled, drain by its type 4/6/8)
 * or Fort (+2) / Fortress (dead stop) eats remaining MP unless the pair is
 * at PEACE; a Privateer mover ignores the peace gate. Human mover gets
 * @SHIPSLOW / @SHIPRUN through the combat popup queue. Exposed for tests.
 */
void units_ship_slow_scan_w(
  const ColonizeWorld* w,
  int unit_id
);
int units_coastal_fort_fire_pulse_w(
  const ColonizeWorld* w,
  int human_nation,
  char* status,
  size_t status_size
);

/* After units_try_move: 0 none, 1 attacker won, -1 attacker lost. */
int units_last_combat_outcome(void);


#endif
