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
 *   2. Accumulate bells until liberty_bells_total >= 40 * (count + 1).
 *   3. Elect the locked-in next_founding_father; then next = -1 (re-debate).
 * Bells are never decremented — DOS threshold/spend recovery still PARKED.
 *
 * founding_fathers_tick: at most one elect per nation per call —
 * human first, then each AI Euro nation (player.control==1).
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

/* True if nation owns FF index (head owner or nation bitmask). */
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
 * Ownership: founding_fathers_nation_has (head owner or nation bitmask;
 * head.founding_father[i]==-1 when unclaimed). Wired from ai_diplo
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
 * Jan de Witt (docs/fandom_col1994.md Trade):
 * Trade with foreign colonies allowed; FA report more revealing.
 * Ownership gate for foreign-colony trade. FA detailed strength already peeks
 * head.founding_father[4] in reports.c. Cargo: colonies_de_witt_transfer_*;
 * AI wagon/ship trade act in ai_euro (stock only; no gold invent).
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
 * units_cortes_conquest_treasure_gold (FUN_5fef_31ea peel); rich_capital ←
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
 * and has stockpiled muskets → auto-arm a colonist defender.
 * Returns true when the nation owns Revere, has no soldier defender, and
 * muskets_stock >= equip step (UNITS_EQUIP_MUSKETS = 50).
 * Wired from units_try_move when FF col1 context is set (turn_refresh) and the
 * attacker steps onto an empty foreign colony tile — see units.c.
 */
bool founding_fathers_revere_should_auto_arm(
  const ColonizeCol1Save* col1,
  int nation,
  bool colony_has_soldier_defender,
  int muskets_stock
);

/*
 * Paul Revere apply (PEDIA / wiki): eject one colonist as Soldier from colony
 * warehouse muskets. Caller must have passed founding_fathers_revere_should_auto_arm.
 * Returns new defender unit id, or -1 if eject/spawn failed.
 */
int founding_fathers_revere_auto_arm(
  ColonizeColonyPool* colonies,
  ColonizeUnitPool* units,
  int colony_id
);

/* Elect at most one FF per eligible nation when the bells threshold is met. */
void founding_fathers_tick(ColonizeTurnContext* ctx);

/*
 * Apply Continental Congress debate CHOICE (AI_POPUP_TAG_FF_CONGRESS).
 * choice_id = FF index. Cite: FUN_4345_06d2 category debate; ai_popup.
 */
void founding_fathers_apply_popup_result(ColonizeTurnContext* ctx, AiPopupState* popups);

#endif
