#ifndef COLONIZE_CORE_AI_KING_INTERNAL_H
#define COLONIZE_CORE_AI_KING_INTERNAL_H

/*
 * Stage seams for ai_king.c's FUN_43f7_0982 REF invasion wave
 * (ai_king_0982_*). See core/internal.h for the COLONIZE_INTERNAL /
 * COLONIZE_TESTING pattern this follows.
 */

#include "core/colony.h"
#include "core/internal.h"
#include "core/turn.h"

/* Wave-local state shared by the FUN_43f7_0982 invasion stages. */
struct ai_king_0982_ctx {
  ColonizeTurnContext* ctx;
  int crown;
  int human;
  uint16_t* force;
  int total;
  bool exhaust;
  bool landed;
};

#ifdef COLONIZE_TESTING
int ai_king_0982_garrison_score(const ColonizeTurnContext* ctx, const ColonizeColony* c);
int ai_king_0982_tile_strength(const ColonizeTurnContext* ctx, int x, int y);
void ai_king_0982_purge_tile(ColonizeTurnContext* ctx, int crown, int x, int y);
int ai_king_0982_crown_mow_alive(const ColonizeTurnContext* ctx, int crown);
int ai_king_0982_spawn_pool_unit(ColonizeTurnContext* ctx, int crown, int k, int x, int y);
void ai_king_0982_land_troops(
  ColonizeTurnContext* ctx, int crown, uint16_t* force, const ColonizeColony* c,
  int continent, int garrison_raw, int need, int lx, int ly
);
void ai_king_0982_invasion(struct ai_king_0982_ctx* w);
/* FUN_43f7_0082 unit pick + spawn for the 10f0 intervention (bugs.md #505). */
int ai_king_10f0_spawn_unit(ColonizeTurnContext* ctx, int human, int k, int x, int y);
#endif /* COLONIZE_TESTING */


/* ===== Cross-file seams of the ai_king.c split (2026-09-23) =====
 * ai_king.c was 6.0k lines; it is now split along its banners into
 * ai_king{,_audience,_ref,_offers,_war}.c. The module-wide #defines and the
 * audience-flavour type below used to sit at the top of the single .c, and
 * the declarations that follow are the symbols used across the new files;
 * everything else stayed `static` in its own file. Code moved verbatim —
 * these are the only de-static'd names.
 * ===================================================================== */

#include "core/ai_king.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/popup_msg.h"
#include "core/units.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AI_KING_YEAR_CAP 1850
#define AI_KING_PEACE_YEAR_CAP 1800
#define AI_KING_SOONRETIRE0_YEAR 1790
#define AI_KING_SOONRETIRE1_YEAR 1840
/* DOS 3844_0442: warn at >=80% crown pop share (0x4f < pct), lose at >=90. */
#define AI_KING_WARN3_PCT_MIN 80
#define AI_KING_LOSING3_PCT 90

/*
 * Auto-path (no ai_popups attached) tea-party stand-in thresholds. DOS's
 * Accept/tea-party choice is inherently player-interactive (a single
 * FUN_291f_0182 dialog inside FUN_38fd_3dc8, no NPC/auto answer exists) —
 * this heuristic is invented for the no-UI auto path only, unrelated to
 * the real 38fd_5be8 delta formula it now follows. See ai_king_tax_event.
 */
#define AI_KING_BOYCOTT_TAX_MIN 20
#define AI_KING_BOYCOTT_SOL_MIN 30
#define AI_KING_BOYCOTT_BELLS_MIN 80
/* Village-goods cargo pick is FUN_38fd_3dc8's roulette (stock×price weight)
 * over non-boycotted, Europe-bid-eligible cargos — use
 * ai_king_pick_dump_goods_cargo; do not invent a fixed Sugar/Tobacco pick.
 * Cite: docs/fandom_col1994.md Boycott; viceroy FUN_38fd_3dc8. */
/*
 * FUN_43f7_2022 rebel-branch self-funded troop-gift purchase — real port
 * (2026-08-14, replaces an earlier SoL/300-gold invented stand-in; see
 * king_ref.md "2244/2022 — corrected"). Recurring per-turn 1-in-3 roll
 * while REF is not present or the Artillery backup pool is empty; price
 * = (qty_regular+2) * ((difficulty+3)*2 + roll(0,6)) * 100, paid from the
 * rebel (human) nation's own gold.
 */
#define AI_KING_MERC_ROLL_CHANCE 3 /* 1-in-3 per turn, dos_rng_range(0,2)==0 */
/*
 * FUN_43f7_2564 / fandom Independence: declare when nation SoL ≥ 50%.
 * Human + ai_popups → CHOICE; else auto-declare.
 */
#define AI_KING_DECLARE_SOL_MIN 50
/* Restless chrome band immediately below declare (SoL 40..49 when min=50). */
#define AI_KING_RESTLESS_SOL_MIN 40
/*
 * MoW hold fill uses real ship capacity (units_ship_capacity / type->cargo,
 * capped at COLONIZE_UNIT_CARGO_MAX=6). Cite: fandom REF “man-o-war with 6
 * units”; units_board_stacked. Coastal unload dumps multiple cargo_ids per
 * war_act beat up to min(moves, capacity) (1 MP/pax); full unload with
 * moves left → AI_SAIL next human coast; after that sail step, if still
 * carrying and now adjacent to the next colony → unload same beat.
 * PARK: full embark UI chrome; dump-goods boycott modal
 * CHOICE Done (pick API + Europe bid>0 weight for auto; KING_DUMP_GOODS for
 * human; VGA PARKED).
 */
/* 10f0 (re-read 2026-08-28): the intervention force is the HUMAN's — one
 * Man-O-War on the best water tile by the colony + Cont. Cav. ≤2 /
 * Artillery ≤2 / Cont. Army = 6 − those, pool-capped, Veteran 0x15. The old
 * "dual/third landing by difficulty" shape was a stand-in and is gone. */
/* 0982: second MoW same beat when difficulty >= 2 and force[2] still > 0.
 * REF idle hunt prefers the founding capital when its MD is within 2 of the
 * nearest other human colony. Both numbers are spelled at their one use site;
 * the AI_KING_SECOND_MOW_DIFF / AI_KING_CAPITAL_MD_SLACK names had no
 * references at all and were deleted 2026-09-14. */

/* ai_popup choice_ids (FUN_43f7_38fd_5be8 / 2244 / 2564). */
#define AI_KING_CHOICE_ACCEPT 1
#define AI_KING_CHOICE_REFUSE 2
#define AI_KING_CHOICE_HIRE 1
#define AI_KING_CHOICE_DECLINE 2
#define AI_KING_CHOICE_CONFIRM 1
#define AI_KING_CHOICE_NOT_YET 2
#define AI_KING_CHOICE_THATS_ALL 0
#define AI_KING_CHOICE_KEEP_PLAYING 1
/*
 * FUN_38fd_5be8 does not pick one message: every rung of its ladder names its
 * own GAME.TXT section and (for three of them) a %STRING2 flavour noun. The
 * port used to show an invented English line for cuts and @KINGTAX for the
 * hike, so the King never mentioned his wedding, his war or the Stamp Act.
 *
 * Section per rung (viceroy_unpacked.asm 38fd:5d2d-5e2f, the PUSHed DS tag
 * ids resolved through docs/popup_tag_ids.md):
 *   cut         @KINGVICTORY  0x113f, %STRING2 = @COUNTRIES[war country - 1]
 *   +1          @KINGWIFE     0x1155, %STRING2 = @ORDINAL[wives - 1]
 *   +2          @KINGWAR      0x1166, %STRING2 = @COUNTRIES[new country - 1]
 *   +3..4       @KINGNAVACT   0x1178, no %STRING2
 *   +5..8       @KINGSTAMPACT 0x1183, %STRING2 = player's New World name
 * %STRING0 / %STRING1 are the same for all five: the difficulty title from
 * DS:0x8394[difficulty] and the player's name (38fd:5cdd FUN_281f_0438(0,..)
 * then FUN_281f_0416(1, nation*0x34 + 0x540e)) — the @KINGNEWWAR convention.
 */
typedef struct AiKingAudienceFlavor {
  const char* section;
  char string2[COLONIZE_MSG_LINE_LEN];
  bool has_string2;
} AiKingAudienceFlavor;

/* Cross-file function seams (were `static` in the single ai_king.c). */

void ai_king_10f0_land(
  ColonizeTurnContext* ctx, int target, int paid, const int merc_counts[4]
);
/* Man-O-War holds column REF per-wave landing cap (bugs.md #661) — exposed
 * for test_regress_ai_tables.c. */
int ai_king_0982_max_landing(const ColonizeTurnContext* ctx);
void ai_king_1d42_royal_purse(ColonizeTurnContext* ctx);
void ai_king_apply_dump_goods_choice(ColonizeTurnContext* ctx, int human, int cargo);
int ai_king_colony_sol_at(const ColonizeTurnContext* ctx, int nation_id, int x, int y);
void ai_king_do_declare(ColonizeTurnContext* ctx, int human);
int ai_king_do_merc_hire_at(ColonizeTurnContext* ctx, int human, int hx, int hy,
                                   int qty_a, int extra_flag, int price);
int ai_king_do_merc_peace_hire(
  ColonizeTurnContext* ctx, int human, int regular, int artillery, int price
);
void ai_king_emit_ok(
  ColonizeTurnContext* ctx,
  const char* msg_tag,
  const PopupMsgTokens* tok,
  const char* fallback,
  AiPopupTag popup_tag,
  int nation_a,
  int nation_b,
  int payload,
  int overwrite_status,
  char* out_body,
  size_t out_body_size
);
void ai_king_enqueue_teaparty_ok(ColonizeTurnContext* ctx, int human, int cargo);
int ai_king_force_total(const uint16_t force[4]);
int ai_king_format_boycott_cargos(char* buf, size_t buf_size, uint16_t bitmap);
void ai_king_frigate_accept(ColonizeTurnContext* ctx, int nation);
int ai_king_human_colonies(const ColonizeTurnContext* ctx, int human);
int ai_king_human_popups(const ColonizeTurnContext* ctx);
int ai_king_intervention_nation_slot(
  const ColonizeTurnContext* ctx, int human_nation, int slot_idx
);
int ai_king_is_mow(const ColonizeUnitPool* units, const ColonizeUnit* u);
void ai_king_merc_offer(ColonizeTurnContext* ctx);
void ai_king_merc_payload_parts(int payload, int* out_hx, int* out_hy, int* out_qty_a,
                                       int* out_extra_flag, int* out_price);
void ai_king_merc_peace_payload_parts(
  int payload, int* out_regular, int* out_artillery, int* out_price
);
const char* ai_king_merc_unit_name(
  const ColonizeUnitPool* units, ColonizeUnitKind kind
);
void ai_king_rank_nations_0218(const ColonizeCol1Save* col1, int order[4]);
void ai_king_ref_wave(ColonizeTurnContext* ctx);
void ai_king_seed_backup_force_1a26(ColonizeTurnContext* ctx, int human);
void ai_king_set_boycott(ColonizeCol1Save* col1, int on);
void ai_king_set_independence(ColonizeCol1Save* col1, int on);
void ai_king_set_ref_present(ColonizeCol1Save* col1, int on);
void ai_king_succession(ColonizeTurnContext* ctx);
void ai_king_sync_boycott_refuse(ColonizeCol1Save* col1, int human);
void ai_king_tax_commit(ColonizeTurnContext* ctx, int human, int delta);
void ai_king_tax_event(ColonizeTurnContext* ctx);
void ai_king_tax_hike_apply(
  ColonizeTurnContext* ctx,
  int human,
  int delta,
  const AiKingAudienceFlavor* flavor
);
void ai_king_tax_teaparty(ColonizeTurnContext* ctx, int human, int cargo);
ColonizeColony* ai_king_teaparty_colony(
  const ColonizeTurnContext* ctx,
  int human,
  int cargo
);
void ai_king_teaparty_party_name(
  char* buf,
  size_t buf_size,
  const ColonizeColony* colony,
  int cargo
);
int ai_king_teaparty_payload(int applied, int cargo);
void ai_king_teaparty_payload_parts(int payload, int* out_applied, int* out_cargo);
void ai_king_try_declare(ColonizeTurnContext* ctx);
int ai_king_weakest_port(ColonizeTurnContext* ctx, int nation_id, int* out_x, int* out_y);
void ai_king_write_rival_nation_slots(ColonizeCol1Save* col1, int human);

#endif /* COLONIZE_CORE_AI_KING_INTERNAL_H */
