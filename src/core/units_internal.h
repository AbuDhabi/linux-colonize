#ifndef COLONIZE_CORE_UNITS_INTERNAL_H
#define COLONIZE_CORE_UNITS_INTERNAL_H

/* ===== Cross-file seams of the units.c split (2026-09-23) =====
 * units.c was 13.5k lines; it is now split into units{,_map,_combat,
 * _combat_resolve,_capture,_move,_pioneer,_cargo}.c. The declarations below
 * are the symbols used across those files; everything else stayed `static`
 * in its own file. Code moved verbatim — these are the only de-static'd
 * names, and they keep the `units_` file-scope prefix.
 * ===================================================================== */

#include <stdbool.h>

#include "core/internal.h"
#include "core/map.h"
#include "core/units.h"
#include "core/world.h"
#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/units_cargo.h"
#include "core/units_move.h"

/* DOS event ids (segment 5fef / 2b5a, `mov ax,N; callf FUN_281f_04c0`): the
 * GSOUND handler for each plays a COLDIG.BIN sample plus a short MIDI sting. */
enum {
  /* FUN_5fef_1b0e (asm 5fef:2271, 232e-23a7): the typed cue is
   * `0x3b + attacker @UNIT row` for a native attacker on a human Euro defender;
   * a separate generic cue follows, selected by both unit types. */
  UNITS_SFX_ATTACK_FIRE = 0x40,
  UNITS_SFX_COMBAT_WON = 0x4a,  /* 0x4b when natives are involved */
  UNITS_SFX_ORDER_FORTIFY = 0x58,
  UNITS_SFX_SHIP_SUNK = 0x57, /* FUN_5fef_0352 (COLDIG 16 sinking) */
};

/* --- hand-listed seams (de-static'd in the first split pass) --- */
extern ColonizeWorldMap* units_occupancy_map;
void units_occupancy_refresh_tile(ColonizeUnitPool* pool, int x, int y, int except_id);
void units_sync_equip_after_type_change(ColonizeUnit* u, const ColonizeUnitType* t);
int units_unit_hold_amount(const ColonizeUnit* u, int hold);
int units_plunder_ship_holds(ColonizeUnitPool* pool, int winner_id, int loser_id);
void units_mp_charge(const ColonizeUnitPool* pool, ColonizeUnit* u, int cost);
void units_mp_exhaust(const ColonizeUnitPool* pool, ColonizeUnit* u);
void units_mp_restore(const ColonizeUnitPool* pool, ColonizeUnit* u);
bool units_is_combat_role(const ColonizeUnitPool* pool, const ColonizeUnit* u);

/* --- owned by units.c --- */
extern int g_units_combat_human_nation;
extern char g_units_homeport[4][24];
extern char g_units_levels[5][24];
extern char g_units_nationality[4][24];
bool units_move_crosses_shore( const ColonizeWorldMap* map, const ColonizeColonyPool* colonies, int from_x, int from_y, int to_x, int to_y );
ColonizeUnit* units_slot(ColonizeUnitPool* pool);
void units_slot_reset_defaults( ColonizeUnitPool* pool, ColonizeUnit* slot, const ColonizeUnitType* type, int type_index, int x, int y );
bool units_type_is_brave_named(const ColonizeUnitType* t);

/* --- owned by units_map.c --- */
extern const ColonizeColonyPool* g_units_combat_colonies;
extern EuropeScreen* g_units_combat_europe;
extern const ColonizeMsgCatalog* g_units_combat_game_txt;
extern AiPopupState* g_units_combat_popups;
extern ColonizeUnitsCombatWatchFn g_units_combat_watch;
extern void* g_units_combat_watch_user;
extern ColonizeUnitsDissolveFn g_units_dissolve;
extern void* g_units_dissolve_user;
extern ColonizeCol1Save* g_units_fallout_col1;
extern ColonizeWorldMap* g_units_fallout_map;
extern const ColonizeCol1Save* g_units_ff_col1;
extern int g_units_last_combat;
extern ColonizeEnterReason g_units_last_enter_reason;
extern ColonizeUnitsMoveWatchFn g_units_move_watch;
extern void* g_units_move_watch_user;
extern int g_units_native_chrome_owned;
extern int g_units_native_gear_armed;
extern int g_units_native_gear_mounted;
extern ColonizeUnitsPopupPumpFn g_units_popup_pump;
extern void* g_units_popup_pump_user;
extern ColonizeUnitsRaidRepelledFn g_units_raid_repelled;
extern int g_units_revere_muskets_latch;
void units_claim_tile_owner_from_stack( ColonizeUnitPool* pool, ColonizeWorldMap* map, int x, int y, int except_id );
void units_combat_maybe_present_analysis( const ColonizeCol1Save* col1, const ColonizeCombatEngagement* eng, int atk_nation, int def_nation );
void units_combat_pump_popups(void);
ColonizeCombatStrengthCtx units_combat_strength_ctx(const ColonizeCol1Save* col1);
bool units_defender_is_dos_scratch_row(void);
void units_dissolve_notify(int phase);
void units_log_ident( const ColonizeUnitPool* pool, int unit_id, char* out, size_t out_size );
int units_tile_is_ocean_or_hs(const ColonizeCol1Save* col1, int x, int y);
bool units_unit_is_sea(const ColonizeUnitPool* pool, const ColonizeUnit* u);

/* --- owned by units_combat.c --- */
extern bool g_units_colony_autodefender;
extern bool g_units_indian_combat_vent_done;
int units_apply_land_loss_outcome( ColonizeUnitPool* pool, int loser_id, int winner_id, const ColonizeCol1Save* col1, int show_popups, ColonizeDosRng* rng );
int units_apply_naval_loss_outcome( ColonizeUnitPool* pool, int loser_id, int winner_id, int loser_str, int winner_str, int show_popups, const ColonizeCol1Save* col1, ColonizeDosRng* rng );
void units_combat_enqueue_tok( AiPopupTag tag, const char* section, int nation_a, int nation_b, int payload, const PopupMsgTokens* tok, const char* fallback );
int units_combat_human_involved(const ColonizeCol1Save* col1, int nat_a, int nat_b);
bool units_combat_is_visible(const ColonizeUnitPool* pool, int a_id, int b_id);
void units_combat_music_sting(void);
const char* units_combat_nation_label(const ColonizeCol1Save* col1, int nation_id);
void units_combat_outcome_popups( const ColonizeUnitPool* pool, const ColonizeUnit* win, const ColonizeUnit* lose, int atk_wins, int atk_nation, int def_nation, int is_naval, int ambush, const ColonizeCol1Save* col1 );
int units_domain_blocker_at( const ColonizeUnitPool* pool, int x, int y, int mover_id, int mover_nation );
int units_drake_scale_strength( const ColonizeUnitPool* pool, const ColonizeUnit* unit, int strength, const ColonizeCol1Save* col1 );
void units_finish_village_temp_defender( ColonizeUnitPool* pool, ColonizeCol1Save* col1, ColonizeWorldMap* map, int temp_id, int attacker_won, int attacker_nation, int village_x, int village_y, ColonizeDosRng* rng );
const char* units_home_port_name(const ColonizeCol1Save* col1, int nation_id);
void units_indian_attack_alarm_vent( const ColonizeCol1Save* col1, int atk_nation, int def_nation, int delta );
int units_indian_attack_alarm_vent_amount( const ColonizeCol1Save* col1, int def_nation, int base, int difficulty_shift );
void units_indian_attack_tension_clear( const ColonizeCol1Save* col1, const ColonizeColonyPool* colonies, int atk_nation, int atk_home_tribe, int def_nation, int def_x, int def_y, int atk_wins );
void units_indian_tension_clear( const ColonizeCol1Save* col1, int tribe_index, int euro_nation );
int units_naval_evade_power(const ColonizeUnitPool* pool, int unit_id);
const ColonizeColony* units_nearest_own_drydock_colony( const ColonizeColonyPool* colonies, int nation_id, int x, int y );
void units_play_event_sound(int id);
int units_promote_on_win( ColonizeUnitPool* pool, ColonizeUnit* winner, const ColonizeCol1Save* col1, int winner_str, int loser_str, ColonizeDosRng* rng );
void units_set_bgm_pool(int pool);
int units_ship_damage_vs_sink( ColonizeDosRng* rng, int winner_guns, int loser_hull );
/* FUN_5fef_0352 raw 99527-99570 damage-vs-sink gate (bugs.md #867). */
int units_naval_damage_gate( const ColonizeUnitPool* pool, const ColonizeCol1Save* col1, const ColonizeUnit* lose, const ColonizeUnitType* wt, int winner_nation, int damaged );
void units_ship_enter_repair( ColonizeUnitPool* pool, ColonizeUnit* lose, int wstr, const ColonizeColony* home, const ColonizeCol1Save* col1, int human, int winner_nation );
void units_ship_lose_holds(ColonizeUnitPool* pool, int ship_id);
void units_sweep_naval_stack_after_loss( ColonizeUnitPool* pool, int x, int y, int loser_nation, int winner_id, int primary_loser_id, const ColonizeCol1Save* col1, ColonizeDosRng* rng );
void units_sweep_stack_after_loss( ColonizeUnitPool* pool, int x, int y, int loser_nation, int winner_id, int primary_loser_id, const ColonizeCol1Save* col1 );
int units_tribe_nation_at(const ColonizeCol1Save* col1, int x, int y);

/* --- owned by units_combat_resolve.c --- */
bool units_at_war_for_move(int a, int b);
void units_capture_claim_ring(ColonizeWorldMap* map, int x, int y, int new_owner);
int units_colony_plunder_stock_sum(const ColonizeColony* col);
bool units_fort_vs_ship( ColonizeUnitPool* pool, int attack_str, int fort_nation, int defender_id, ColonizeDosRng* rng, const ColonizeCol1Save* col1, const char* atk_label );
bool units_native_village_grudge(const ColonizeUnit* mover, int euro_nation);
void units_sentry_wake_scan( ColonizeUnitPool* pool, const ColonizeWorldMap* map, const ColonizeColonyPool* colonies, int unit_id );
bool units_village_squat_illegal( const ColonizeUnitPool* pool, const ColonizeUnitType* type, const ColonizeUnit* mover, const ColonizeWorldMap* map, int x, int y, int mover_nation, const ColonizeColonyPool* colonies );

/* --- owned by units_capture.c --- */
bool units_can_afford_move_cost(const ColonizeUnitPool* pool, int unit_id, int cost);
bool units_revere_defend_colony_tile( ColonizeUnitPool* pool, ColonizeColonyPool* colonies, int attacker_id, int dest_x, int dest_y, ColonizeDosRng* rng );
int units_spawn_colony_temp_defender( ColonizeUnitPool* pool, const ColonizeColony* col, bool revere_armed );
void units_try_capture_foreign_colony( ColonizeUnitPool* pool, ColonizeColonyPool* colonies, int unit_id );

/* --- owned by units_move.c --- */
const char* units_order_name(int orders);

/* --- owned by units_pioneer.c --- */
bool units_adjacent(int ax, int ay, int bx, int by);

/* --- owned by units_cargo.c --- */
int units_spawn_aboard(ColonizeUnitPool* pool, int type_index, ColonizeUnit* ship);

#endif /* COLONIZE_CORE_UNITS_INTERNAL_H */
