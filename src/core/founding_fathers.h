#ifndef COLONIZE_FOUNDING_FATHERS_H
#define COLONIZE_FOUNDING_FATHERS_H

#include <stdbool.h>

#include "core/col1_save.h"
#include "core/turn.h"

/*
 * Founding-father election from liberty bells (manual / wiki effect table).
 *
 * Flow (DOS FUN_4345_0a22 / 06d2 / 0342):
 *   1. After liberty bells exist, if next_founding_father < 0 → Congress
 *      debate (one unclaimed Father per category) or AI auto-pick into next.
 *   2. Accumulate bells until liberty_bells_pool >= 40 * (count + 1).
 *   3. Elect the locked-in next_founding_father; then next = -1 (re-debate).
 * During WoI (0x5382&1): no Congress debate; bell pool spends on foreign
 * intervention / REF arrival (FUN_4345_0a22 wartime branch) instead of elect.
 *
 * 2026-08-19..2026-09-24: the pool used to live in a nation-indexed side
 * table in founding_fathers.c, stashed into +0xe at save time behind an
 * unknown21_pad marker and reconstructed from a spent-sum heuristic on load.
 * bugs.md #933 deleted all of that: +0xc (`liberty_bells_pool`) is the pool,
 * added to each turn, zeroed on elect and at the declaration of independence,
 * and +0xe (`liberty_bells_last_turn`) is this turn's bells only. Consumers
 * that want "bells" for SoL fallback / boycott-refusal / score read the pool
 * directly, exactly as DOS does.
 *
 * founding_fathers_tick: final elect pass of the turn for each AI Euro
 * nation (player.control==1); the human's runs in
 * founding_fathers_tick_human_elect. Both loop while the pool still clears
 * the next threshold (bugs.md #934).
 *
 * Effects follow Colonization.pdf + docs/fandom_col1994.md (+ NAMES/decomp).
 * "Rough" means incomplete UI/wiring — not invented gold/crosses stand-ins.
 * Missing hooks: elect ownership only + PARKED comment naming the real effect.
 */

/* FF indices (NAMES / COL1 order). */
#define FF_ADAM_SMITH 0
#define FF_JAKOB_FUGGER 1
#define FF_PETER_MINUIT 2
#define FF_PETER_STUYVESANT 3
#define FF_JAN_DE_WITT 4
#define FF_FERDINAND_MAGELLAN 5
#define FF_FRANCISCO_CORONADO 6
#define FF_HERNANDO_DE_SOTO 7
#define FF_HENRY_HUDSON 8
#define FF_SIEUR_DE_LA_SALLE 9
#define FF_HERNAN_CORTES 10
#define FF_GEORGE_WASHINGTON 11
#define FF_PAUL_REVERE 12
#define FF_FRANCIS_DRAKE 13
#define FF_JOHN_PAUL_JONES 14
#define FF_THOMAS_JEFFERSON 15
#define FF_POCAHONTAS 16
#define FF_THOMAS_PAINE 17
#define FF_SIMON_BOLIVAR 18
#define FF_BENJAMIN_FRANKLIN 19
#define FF_WILLIAM_BREWSTER 20
#define FF_WILLIAM_PENN 21
#define FF_JEAN_DE_BREBEUF 22
#define FF_JUAN_DE_SEPULVEDA 23
#define FF_BARTOLOME_DE_LAS_CASAS 24

/*
 * Bells required to elect the next FF (FUN_4345_0982).
 * Difficulty / human-vs-AI / year band / WoI override. elected_count from
 * nation.founding_father_count. Fallback linear 40*(n+1) if col1 NULL.
 */
unsigned founding_fathers_bells_needed(const ColonizeCol1Save* col1, int nation);

/*
 * DOS nation+0xc IS the FF bell pool: `liberty_bells_pool` in the save record.
 * FUN_4345_0a22 raw 73341 adds this turn's bells, raw 73370 zeroes it on a
 * successful elect; FUN_43f7_1a26 raw 74738 zeroes it at the declaration of
 * independence. (bugs.md #933 — the old side table / save-time stash into
 * +0xe / spent-sum reconstruction are gone.)
 */
unsigned founding_fathers_bells_pool(const ColonizeCol1Save* col1, int nation_id);

/* Zero the pool (col1 record + the human's Europe mirror). */
void founding_fathers_reset_bells_pool(ColonizeTurnContext* ctx, int nation_id);

/*
 * One elect attempt for a nation; true when a Founding Father was elected.
 * bugs.md #934: DOS runs FUN_4345_0a22 once per colony (sole call site
 * FUN_364b_0688 raw 57231), so callers loop while this keeps returning true.
 */
bool founding_fathers_try_elect(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_4345_0a22 phase 3 (thin): status while WoI bell pool grows toward
 * intervention threshold. Called from turn.c during WoI EOT.
 */
void founding_fathers_woi_intervention_chrome(
  ColonizeTurnContext* ctx,
  int nation_id,
  unsigned pool,
  unsigned needed
);

/*
 * After ai_king_spend_woi_bell_pool succeeds: zero the bell pool.
 * DOS keeps no intervention counter — FUN_41f2_0092's total is exactly
 * early-revolution + congress + villages + treasury + rebel + bells/100 +
 * citizens (viceroy_unpacked.c:71068-71413), so nothing is scored here.
 */
void founding_fathers_consume_woi_bell_pool(ColonizeTurnContext* ctx, int nation_id);

/* Reset per-session FF scratch (the persistent Congress slate) — new game. */
void founding_fathers_reset(void);

/*
 * True if nation owns FF index. Per-nation bitmask ONLY (DOS FUN_15eb_3960);
 * head.founding_father[] is a write-once first-claimer record the DOS binary
 * never reads back — see the definition's comment (smell audit #83).
 */
bool founding_fathers_nation_has(const ColonizeCol1Save* col1, int nation, int ff_index);

/*
 * FUN_15eb_0274 Bolivar display boost: +20 SoL when FF held and nation is
 * human (player.control == 0). Else 0. Cap applied by caller.
 */
int founding_fathers_bolivar_sol_bonus(const ColonizeCol1Save* col1, int nation);

/*
 * Benjamin Franklin gate (docs/fandom_col1994.md / PEDIA):
 * King's European wars no longer affect New World relations; Europeans in the
 * New World always offer peace in negotiations.
 * Ownership: founding_fathers_nation_has (per-nation bitmask). Wired from ai_diplo
 * euro_balance / declare_war (war-hit side effects).
 */
bool founding_fathers_franklin_keeps_nw_peace(const ColonizeCol1Save* col1, int nation);

/*
 * Father Jean de Brebeuf gate (docs/fandom_col1994.md):
 * All missionaries function as experts (Jesuit-grade). Ownership only —
 * no elect crosses fiction. Wired from ai_contact mid-band convert
 * (plain Missionary treated as PEDIA @JOB24 Jesuit when owned).
 */
bool founding_fathers_brebeuf_missionaries_are_experts(
  const ColonizeCol1Save* col1,
  int nation
);

/*
 * Bartolome de las Casas (PEDIA @FATHER24 / docs/fandom_col1994.md):
 * Existing Indian converts assimilate as free colonists.
 * Elect + ownership tick in founding_fathers_tick: NAMES @JOB Convert (27)
 * → Free Colonists (19) on owned colony colonists and map units. No gold /
 * crosses fiction.
 */

/*
 * Juan de Sepulveda (docs/fandom_col1994.md Religious / PEDIA @FATHER23):
 * Higher chance subjugated Indians “convert” and join a colony.
 * Ownership gate; convert-join peel in units_try_native_settlement_fallout
 * (FUN_5fef_31ea / 1b0e — not missionary pulse, not 2820). Threshold +4 when
 * owned. Cite: COLONIZE/PEDIA.TXT @FATHER23; GAME.TXT @INDIANSLAVES.
 */
bool founding_fathers_sepulveda_convert_join_bonus(
  const ColonizeCol1Save* col1,
  int nation
);

/*
 * Hernando de Soto (docs/fandom_col1994.md Exploration; Colonization.pdf FF):
 * Lost City Rumors always positive (+ extended sight already on elect).
 * Resolve: units_resolve_lcr_rumour (thin positive reveal). Full FUN_65dd_0004
 * RNG table PARKED (no invented treasure / Fountain of Youth).
 */
bool founding_fathers_de_soto_lcr_always_positive(
  const ColonizeCol1Save* col1,
  int nation
);

/*
 * Jan de Witt (FF 4). DOS reads the bit in two places only: FUN_5f7a_020e
 * (raw 98928, human foreign-colony trade menu; @TRADEMERCANTILISM refusal
 * without him -- docs/foreign_colony_trade.md) and the Foreign Affairs
 * report FUN_3f41_2548. The AI never trades this way.
 */
bool founding_fathers_de_witt_allows_foreign_colony_trade(
  const ColonizeCol1Save* col1,
  int nation
);

/*
 * Hernan Cortes gates (docs/fandom_col1994.md / Colonization.pdf FF):
 * conquered native settlements always yield more treasure; king's galleons
 * transport treasure free. Ownership only — no invented gold amounts.
 * Spawn API: units_spawn_treasure_train. Fallout hook:
 * units_try_native_settlement_fallout (wired from units_resolve_land_combat_ff
 * when units_set_native_fallout_context is set). Amount: gold_amount>0 or
 * units_conquest_treasure_gold (FUN_5fef_31ea peel); rich_capital ←
 * tribe.state.capital. Fallout also increments nation.villages_burned
 * (col1_save.h; reports.c villages_penalty).
 */
bool founding_fathers_cortes_guarantees_conquest_treasure(
  const ColonizeCol1Save* col1,
  int nation
);
bool founding_fathers_cortes_free_king_galleon(const ColonizeCol1Save* col1, int nation);

/*
 * Paul Revere gate (PEDIA / wiki): colony with no standing soldiers is attacked
 * and has stockpiled muskets → the militia phantom turns out armed.
 * Returns true when the nation owns Revere, has no soldier defender, and
 * muskets_stock >= equip step (UNITS_EQUIP_MUSKETS = 50). This is DOS's
 * `FUN_281f_07b4(owner, 0xc) && colony.Muskets(+0xb8) > 0x31` at
 * viceroy_unpacked.c 100424, byte-identical.
 *
 * There is no apply half: Revere only *overrides* the phantom defender DOS
 * spawns anyway (graphic 0x4b, base combat +1) — it never ejects a real
 * Soldier and never spends the muskets. Wired from units_try_move when FF
 * col1 context is set (turn_refresh) and the attacker steps onto an empty
 * foreign colony tile — see units_revere_defend_colony_tile in units.c.
 */
bool founding_fathers_revere_should_auto_arm(
  const ColonizeCol1Save* col1,
  int nation,
  bool colony_has_soldier_defender,
  int muskets_stock
);

/* Elect at most one FF per eligible nation when the bells threshold is met. */
void founding_fathers_tick(ColonizeTurnContext* ctx);

/* bugs.md #434: the human's Congress election check, run in TURN_PROC_FINISH
 * (start of the player's turn) — DOS puts it in the human's own 3844_00f2
 * pass, right before Move Pieces. founding_fathers_tick no longer elects for
 * the human. */
void founding_fathers_tick_human_elect(ColonizeTurnContext* ctx);

/*
 * La Salle immediate hook: call right when a colony's population changes
 * (join/admit/birth) so a colony reaching pop 3 gets its free Stockade the
 * same moment it happens, not only at the next founding_fathers_tick.
 * No-op (returns 0) when col1 is NULL or the nation doesn't own La Salle.
 */
int founding_fathers_la_salle_check(
  ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id
);

/*
 * Apply Continental Congress debate CHOICE (AI_POPUP_TAG_FF_CONGRESS).
 * choice_id = FF index. Cite: FUN_4345_06d2 category debate; ai_popup.
 */
void founding_fathers_apply_popup_result(ColonizeTurnContext* ctx, AiPopupState* popups);

#endif
