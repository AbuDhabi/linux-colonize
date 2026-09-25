#include "core/internal.h"
#include "core/ai_king.h"
#include "core/ai_king_internal.h"
#include "core/ai_diplo.h"
#include "core/sound.h"

#include "core/assets.h"
#include "core/colony.h"
#include "core/combat_strength.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/strutil.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * FUN_43f7_* King/REF/independence — partial structural port.
 * Thin map: original_sources_annotated/ai/king_ref.md
 *
 * WoI: the latch is head.game_options.woi (DOS 0x5382 bit0, mapped in col1_save.h);
 *   ai_king_latch_set(AI_KING_WOI_BYTE) writes that same field. The old
 *   market_demand_pool_raw[] mirror is gone (market_demand_pool_raw[0..5] alias market_demand_pool on DOS
 *   saves, so it was never a safe home).
 * REF-present: port-only pad latch AI_KING_REF_PRESENT_BYTE. (0x5382 bit
 * 0x02 is DOS's intervention-announce latch, bugs.md #865.)
 * Tax audience (ported 2026-08-19, real formula — see ai_king_audience_roll /
 *   ai_king_audience_apply_delta / ai_king_tax_event): FUN_38fd_5be8 rolls a
 *   signed delta off a turn-interval-gated favor-score ladder (cut, +1, +2,
 *   +3-4, +5-8) and FUN_38fd_3dc8 applies it UNCONDITIONALLY, clamped to
 *   0..75%. Only a genuine positive applied delta can trigger the
 *   village-goods popup (Accept "kiss the ring" keeps it / Refuse "tea
 *   party" REVERTS the just-applied hike + boycotts one tonnage-walk-picked
 *   cargo) — there is no DOS gate on whether the hike itself happens.
 *   head.market_demand_pool_raw[2] is now presentation-only (boycott-active flag +
 *   Fugger sync), no longer gates the audience interval.
 *   Follow-up OK is GAME.TXT @TEAPARTY (KING_TAX; thin 3dc8 stock dump +
 *   tokens). Cargo freeze: nation.boycott_bitmap. Fugger/diplo bitmap
 *   clear → drop market_demand_pool_raw[2] when bitmap==0 (king sync; do not touch FF).
 * Rebel troop-gift purchase (FUN_43f7_2022 rebel branch, real port
 *   2026-08-14): recurring per-turn 1-in-3 roll while REF absent or
 *   Artillery backup pool empty; ai_popup CHOICE Hire/Decline when
 *   ctx->ai_popups (auto-accept when NULL); unaffordable → silently
 *   skipped (no DOS status/dialog). No once-per-war flag — head.market_demand_pool_raw[3]
 *   is unused for this now (was an invented gate, see king_ref.md).
 * 160a: signing cinematic only (core/declaration.c, armed from game_loop on
 *   the KING_LETTER popup). No country rename — "United Colonies" was a port
 *   invention (bugs.md #239); WoI faction labels are Rebels/Tory via
 *   units_combat_nation_label. market_demand_pool_raw[4] endgame latch: 0/1 won/2 lost.
 *   @HOWTOWIN fires at first rebel recapture (units.c), not at declare
 *   (bugs.md #236).
 * Congress confirm: head.market_demand_pool_raw[5] + thin 2564 (ai_popup CHOICE from
 *   GAME.TXT @DECLARE Never/Yes when ctx->ai_popups; auto-declare when NULL;
 *   same-turn 1528 may overwrite status).
 * Mid-war @WARN%d: ONE digit-patch selector per turn (raw 58506-58534, twin
 *   of @LOSING%d), precedence colonies<3 (2) > share ≥80% (3) > ports<3 (1);
 *   the selected warn keeps a port-side episode latch — market_demand_pool_raw[6]/[7]/[10]
 *   for @WARN1/2/3 — each cleared when its own band is left, so a relapse
 *   re-fires. @LOSING3 takes the turn instead when share ≥90%.
 * Calendar @SOONRETIRING0 (1790 spring peacetime): head.market_demand_pool_raw[8] once.
 * Calendar @SOONRETIRING1 (1840 WoI): head.market_demand_pool_raw[9] once.
 * Revolution end (raw 58470-58556): lose on one @LOSING%d selector, DOS
 *   precedence 0 colonies (2) > share ≥90% (3) > 0 coastal ports (1); win on
 *   the C1 triple with NO year gate; @RETIRING2 on year 1850 alone.
 * SoL restless chrome (40..49): status only (no invented wood OK).
 * backup_force: DOS 0x53e2…0x53e8 foreign-intervention pools, seeded on
 *   declare by the ported FUN_43f7_1a26 body and drained by 10f0.
 * Crown nation_id: non-human Euro slot (1 if human==0 else 0).
 */

/*
 * Sections:
 * Sections (this file; the rest of the module lives in ai_king_audience.c,
 * ai_king_ref.c, ai_king_offers.c and ai_king_war.c — see
 * ai_king_internal.h "Cross-file seams"):
 *  - Crown/boycott messaging & dump-goods helpers
 *  - Tea-party enqueue & intervention force selection
 *  - REF force bookkeeping, royal purse & SoL/independence state
 */

/* ===== Crown/boycott messaging & dump-goods helpers (ai_king_crown_nation .. ai_king_teaparty_party_name) ===== */
int ai_king_crown_nation(int human_nation) {
  return (human_nation == 0) ? 1 : 0;
}

int ai_king_pick_dump_goods_cargo(
  uint16_t boycott_bitmap,
  uint16_t candidate_mask,
  ColonizeDosRng* rng,
  const int* cargo_weight
) {
  /*
   * DOS-LITERAL FUN_38fd_3dc8 raw 64176-64200. Eligible = candidate_mask &
   * ~boycott_bitmap (DOS skips bits already set in the nation's
   * boycott_bitmap / local_a6 when summing local_80). The weights are the
   * caller's local_7a[] (tonnage-derived, see
   * ai_king_teaparty_candidate_mask). DOS sums the sign-extended 16-bit
   * weights into a 32-bit total, calls FUN_281f_04d4(1, LOWORD(total)), but
   * never reads that call's result: asm 38fd:3f63-3fbc subtracts each
   * eligible weight from the original total and picks the first remainder
   * <= 0. This odd deterministic walk is observable when the truncated
   * weights sum to zero or negative (as in campaign4/COLONY08.SAV), so do
   * not turn the unused draw into a roulette. When cargo_weight is NULL,
   * pick uniformly (no Col1 nation record: tests / synthetic fixtures).
   */
  if (!rng) {
    return -1;
  }
  const uint16_t eligible = (uint16_t)(candidate_mask & (uint16_t)~boycott_bitmap);
  if (eligible == 0) {
    return -1;
  }
  int idxs[COLONIZE_CARGO_COUNT];
  int n = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if ((eligible & (uint16_t)(1u << c)) == 0) {
      continue;
    }
    idxs[n++] = c;
  }
  if (n <= 0) {
    return -1;
  }
  if (!cargo_weight) {
    const int pick = dos_rng_range(rng, 0, n - 1);
    if (pick < 0 || pick >= n) {
      return -1;
    }
    return idxs[pick];
  }
  int32_t remaining = 0;
  for (int i = 0; i < n; ++i) {
    remaining += cargo_weight[idxs[i]];
  }
  /* DOS stores but never reads this result; preserve the RNG-stream burn.
   * The call receives only local_80, the low word of the 32-bit total. */
  (void)dos_rng_range(rng, 1, (int)(uint16_t)remaining);
  for (int i = 0; i < n; ++i) {
    remaining -= cargo_weight[idxs[i]];
    if (remaining <= 0) {
      return idxs[i];
    }
  }
  return -1;
}

/*
 * Comma-separated @CARGO names set in boycott_bitmap (presentation only).
 * Returns 1 if any bit set. Cite: king_ref refuse/holds chrome; Fugger partial
 * clear may leave a subset of bits while market_demand_pool_raw[2] still holds.
 */
int ai_king_format_boycott_cargos(char* buf, size_t buf_size, uint16_t bitmap) {
  if (!buf || buf_size == 0) {
    return 0;
  }
  buf[0] = '\0';
  size_t pos = 0;
  int any = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if ((bitmap & (uint16_t)(1u << c)) == 0) {
      continue;
    }
    if (any) {
      if (pos + 2 >= buf_size) {
        break;
      }
      pos += (size_t)snprintf(buf + pos, buf_size - pos, ", ");
    }
    pos += (size_t)snprintf(buf + pos, buf_size - pos, "%s", reports_cargo_display_name(c));
    any = 1;
  }
  return any;
}

/* Human-facing map popup queue attached (game_loop); AI/auto path when NULL. */
int ai_king_human_popups(const ColonizeTurnContext* ctx) {
  return (ctx && ctx->ai_popups) ? 1 : 0;
}

/*
 * The King chrome emitter (2026-09-14 duplication audit AK-26). Roughly two
 * dozen sites repeated the same tail verbatim: popup_msg_fill(msg_tag) into a
 * local body, write that body to ctx->status, and — only when a human popup
 * queue is attached — enqueue an OK carrying the same body.
 *
 * `overwrite_status` keeps the one real difference between them: the endgame
 * and audience lines overwrite ctx->status unconditionally, while the mid-war
 * warns and the retirement notices only claim it when it is still empty (so a
 * same-turn @INVASION / merc line is not clobbered). `out_body` is optional
 * and is filled with the same text the popup got, for callers that reuse it.
 */
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
) {
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, msg_tag, tok, fallback, body, sizeof(body));
  if (ctx->status && ctx->status_size && (overwrite_status || ctx->status[0] == '\0')) {
    snprintf(ctx->status, ctx->status_size, "%s", body);
  }
  if (ai_king_human_popups(ctx)) {
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, popup_tag, nation_a, nation_b, payload, NULL, body
    );
  }
  if (out_body && out_body_size) {
    snprintf(out_body, out_body_size, "%s", body);
  }
}

/*
 * FUN_38fd_3dc8 aiStack_a4[]: for each cargo, the human COASTAL colony holding
 * the most of it (raw 64160-64175, colony +0x1c bit 0x40). Used both to name
 * the party and to seize the stock.
 */
ColonizeColony* ai_king_teaparty_colony(
  const ColonizeTurnContext* ctx,
  int human,
  int cargo
) {
  if (!ctx || !ctx->colonies || human < 0 || human >= 4) {
    return NULL;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return NULL;
  }
  ColonizeColony* best = NULL;
  int best_stock = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    /* DOS-LITERAL FUN_38fd_3dc8 raw 64160-64175: `(colony[+0x1c] & 0x40)` —
     * only COASTAL colonies fill aiStack_a4[], so an inland colony can never
     * be the named party (bugs.md #904). Same decode as ai_king_ref.c. */
    if ((c->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) == 0) {
      continue;
    }
    if (c->stock[cargo] > best_stock) {
      best_stock = c->stock[cargo];
      best = c;
    }
  }
  return best;
}

/*
 * GAME.TXT %STRING3 for @TAXOPTIONS / @TEAPARTY: DOS builds it at
 * FUN_38fd_3dc8 (strcpy colony name, strcat " ", strcat cargo name), so the
 * rendered headline is "<Colony> <Cargo> Party!" — a literal "Tea Party" never
 * occurs, since Colonization has no Tea cargo.
 */
void ai_king_teaparty_party_name(
  char* buf,
  size_t buf_size,
  const ColonizeColony* colony,
  int cargo
) {
  if (!buf || buf_size == 0) {
    return;
  }
  const char* cargo_nm = reports_cargo_display_name(cargo);
  if (colony && colony->name[0]) {
    snprintf(buf, buf_size, "%s %s", colony->name, cargo_nm);
  } else {
    snprintf(buf, buf_size, "%s", cargo_nm);
  }
}

/*
 * GAME.TXT @TEAPARTY follow-up OK after refuse / dump-goods apply.
 * Thin FUN_38fd_3dc8: dump min(100, stock) from richest human colony of cargo,
 * then enqueue KING_TAX OK with authentic tokens. VGA chrome PARKED.
 * Cite: GAME.TXT @TEAPARTY; popup_audit MissingWire → Done thin.
 */
/* ===== Tea-party enqueue & intervention force selection (ai_king_enqueue_teaparty_ok .. ai_king_write_rival_nation_slots) ===== */
void ai_king_enqueue_teaparty_ok(ColonizeTurnContext* ctx, int human, int cargo) {
  if (!ctx || !ai_king_human_popups(ctx) || human < 0 || human >= 4) {
    return;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }

  ColonizeColony* best = ai_king_teaparty_colony(ctx, human, cargo);

  int tons = 0;
  if (best) {
    tons = best->stock[cargo] > 100 ? 100 : best->stock[cargo];
    best->stock[cargo] -= tons;
  }

  /* reports_cargo_display_name returns reports.c's shared NAMES scratch, so
   * copy it out before any other catalog lookup runs. */
  char cargo_nm[32];
  str_copy_trunc(cargo_nm, sizeof(cargo_nm), reports_cargo_display_name(cargo));
  const char* colony_nm =
    (best && best->name[0]) ? best->name : "a colonial warehouse";
  char party[96];
  ai_king_teaparty_party_name(party, sizeof(party), best, cargo);

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cargo_nm;
  tok.string1 = colony_nm;
  /* DOS supplies this market noun from executable code. Use port-authored
   * wording instead of compiling a duplicate of the catalog's place name. */
  tok.string2 = "the home market";
  tok.string3 = party;
  tok.number0 = tons;
  tok.has_number0 = true;

  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "TEAPARTY", &tok, "", body, sizeof(body));
  sound_play(0x56); /* FUN_38fd_3dc8 tea party (COLDIG 9 cheering) */
  (void)ai_popup_enqueue_ok_ctx(
    ctx->ai_popups,
    AI_POPUP_TAG_KING_TAX,
    human,
    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
    ctx->col1 && ctx->col1_ok ? (int)ctx->col1->nation[human].tax_rate : 0,
    NULL,
    body
  );
}

/* Active colony count for a Euro nation (10f0 intervene nation pick). */
/*
 * Crown-hostile Euro slot for 10f0 landings (not human, not crown).
 * After declare, prefer head.rival_nation_slot_1 cached at 1a26 (DOS 0x53d4);
 * else most colonies + land-unit tie-break.
 */
static bool ai_king_valid_intervention_slot(
  const ColonizeCol1Save* col1, int human_nation, int slot
) {
  if (!col1 || slot < 0 || slot >= 4) {
    return false;
  }
  const int crown = ai_king_crown_nation_col1(col1, human_nation);
  /* WoI foreign landings: eliminated Euros (control==2) still intervene via 10f0. */
  return slot != human_nation && slot != crown;
}

static int ai_king_intervention_nation(const ColonizeTurnContext* ctx, int human_nation) {
  if (ctx && ctx->col1_ok && ctx->col1 &&
      ai_king_independence_declared(ctx->col1)) {
    const int slot = (int)ctx->col1->head.rival_nation_slot_1;
    if (ai_king_valid_intervention_slot(ctx->col1, human_nation, slot)) {
      return slot;
    }
  }
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human_nation);
  int best = -1;
  int best_colonies = -1;
  int best_force = -1;
  for (int n = 0; n < 4; ++n) {
    if (n == human_nation || n == crown) {
      continue;
    }
    const int cols = ctx && ctx->colonies ? colonies_count_for_nation(ctx->colonies, n) : 0;
    int force = 0;
    if (ctx && ctx->units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != n) {
          continue;
        }
        if (units_is_sea(ctx->units, u->id)) {
          continue;
        }
        force++;
      }
    }
    if (best < 0 || cols > best_colonies ||
        (cols == best_colonies && force > best_force)) {
      best = n;
      best_colonies = cols;
      best_force = force;
    }
  }
  return best >= 0 ? best : crown;
}

int ai_king_intervention_nation_slot(
  const ColonizeTurnContext* ctx, int human_nation, int slot_idx
) {
  if (ctx && ctx->col1_ok && ctx->col1 &&
      ai_king_independence_declared(ctx->col1)) {
    const int slot = slot_idx == 0 ? (int)ctx->col1->head.rival_nation_slot_1
                                   : (int)ctx->col1->head.rival_nation_slot_2;
    if (ai_king_valid_intervention_slot(ctx->col1, human_nation, slot)) {
      return slot;
    }
  }
  return ai_king_intervention_nation(ctx, human_nation);
}

/*
 * FUN_43f7_0218 / FUN_43f7_1a26 shared nation rank (both build the same
 * table before FUN_291f_0ed0's ascending insertion sort):
 *   score[n] = ship_counts[n]*3 + colony_counts[n]*2 + census_pop_proxy[n]
 * (DS:-0x6be8 = 0x9418 ship_counts, -0x6d68 = 0x9298 colony_counts,
 * -0x6bf0 = 0x9410 census_pop_proxy — save_format_map.md rows 241/243/247;
 * the port's live FUN_4962_0018 census keeps all three fresh per turn).
 * Was an invented per-colony pop score before 2026-09-06.
 */
void ai_king_rank_nations_0218(const ColonizeCol1Save* col1, int order[4]) {
  int score[4] = {0, 0, 0, 0};
  for (int n = 0; n < 4; ++n) {
    order[n] = n;
    if (col1) {
      score[n] = 3 * (int)col1->stuff.ship_counts[n] + 2 * (int)col1->stuff.colony_counts[n] +
                 (int)col1->stuff.census_pop_proxy[n];
    }
  }
  for (int a = 1; a < 4; ++a) {
    for (int b = a; b > 0 && score[order[b]] < score[order[b - 1]]; --b) {
      const int t = order[b];
      order[b] = order[b - 1];
      order[b - 1] = t;
    }
  }
}

/*
 * FUN_43f7_1a26: cache two non-human/non-crown Euro slots (DOS 0x53d4/0x53d6).
 * DOS ranks all four by ships*3 + colonies*2 + census (see
 * ai_king_rank_nations_0218) and the sort is ASCENDING — so the intervention
 * ally (0x53d4) is the WEAKER of the two remaining powers, the second
 * (0x53d6) the stronger. (Same sorted list 43f7_0218 picks the crown from:
 * first non-human = the weakest power, which is withdrawn-by-merger to free
 * its slot for the King.)
 */
void ai_king_write_rival_nation_slots(ColonizeCol1Save* col1, int human) {
  if (!col1 || human < 0 || human >= 4) {
    return;
  }
  const int crown = ai_king_crown_nation_col1(col1, human);
  int order[4];
  ai_king_rank_nations_0218(col1, order);
  col1->head.rival_nation_slot_1 = -1;
  col1->head.rival_nation_slot_2 = -1;
  int w = 0;
  for (int i = 0; i < 4 && w < 2; ++i) {
    const int n = order[i];
    if (n == human || n == crown) {
      continue;
    }
    if (w == 0) {
      col1->head.rival_nation_slot_1 = (int16_t)n;
    } else {
      col1->head.rival_nation_slot_2 = (int16_t)n;
    }
    w++;
  }
}

/*
 * FUN_43f7_1a26 foreign-intervention pool seed (DOS 0x53e2…0x53e8 → backup_force).
 * Uses intervention-nation stats where DOS nation bytes are mapped to Col1 fields.
 *
 * Index is a literal mirror of the DOS addresses (backup_force[i] ↔
 * 0x53e2+i*2), with the formula-to-address mapping read off
 * viceroy_unpacked.c:74765-74799 and the unit types off :74424
 * (`thunk_FUN_2a1f_0070` type lookup,
 * FUN_43f7_0082 @73519: param_1==2 → unit type 0x12 Man-O-War, param_1==3 →
 * type 0xb Artillery — both cross-checked against NAMES.TXT @UNIT row order):
 *   [0] founding-father-count-based (0x53e2) — Regular land-troop pool
 *   [1] rebel-sentiment-based       (0x53e4) — Dragoon land-troop pool
 *   [2] colony-count-based          (0x53e6) — Man-O-War pool; FUN_43f7_2022's
 *       rebel-gift gate reads this address directly (line 75007) — gates
 *       ai_king_merc_offer, NOT spent as a land unit (FUN_43f7_10f0 skips
 *       `local_52==2` in its land-troop loop, line 74418/74448)
 *   [3] liberty-bells-based         (0x53e8) — Artillery land-troop pool
 * (was previously stored index-swapped — [2]<->[3] — which fed the wrong
 * formula into ai_king_merc_offer's gate and into the Artillery land-troop
 * drain; fixed together with that call site below.)
 */
/* ===== REF force bookkeeping, royal purse & SoL/independence state (ai_king_seed_backup_force_1a26 .. ai_king_teaparty_payload_parts) ===== */
void ai_king_seed_backup_force_1a26(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int diff = (int)ctx->col1->head.difficulty;
  const int ally = ai_king_intervention_nation(ctx, human);
  if (ally < 0 || ally >= 4) {
    return;
  }
  /*
   * Real DOS operands resolved 2026-09-06 (were mapped to unrelated nation
   * fields before): all four are FUN_4962_0018 census tables for the ally
   * (DOS 0x53d4 rival slot):
   *   -0x6bf0 = 0x9410 census_pop_proxy;
   *   -0x6bd4 = 0x942c field_combat_totals (land combat units in the field);
   *   -0x6be4 word = 0x941c land_combat_strength (u16);
   *   local_4 = unit_type_counts[ally][0x10] + [0x11] (DS:-0x6da4/-0x6da3,
   *     stride 0x13 = Privateer + Frigate counts — the ally's warships).
   */
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int local_4 =
    (int)stuff->unit_type_counts[ally][16] + (int)stuff->unit_type_counts[ally][17];
  const int n6bf0 = (int)stuff->census_pop_proxy[ally];
  const int n6bd4 = (int)stuff->field_combat_totals[ally];
  const int n6be4 = (int)stuff->land_combat_strength[ally];

  /*
   * DOS halves the PRE-store accumulators, not the stored pool (raw
   * 74769-74784): `iVar4 = pop/10 - diff; *0x53e2 = iVar4 + 8; …
   * iVar5 = (iVar4 + 9) / 2; *0x53e2 = iVar5`. The +8/+1/+3/+3 stores are
   * dead — overwritten by the halved value before anything reads them — so
   * folding them into the halving inflates every pool (pool0 by +4 Regulars,
   * and a fatter pool2 widens the x2/x6 clamps below and the merc-offer gate).
   */
  const int iVar4 = (n6bf0 / 10) - diff;         /* 0x53e2 accumulator */
  const int iVar7 = (4 - diff) / 2;
  const int iVar9 = ((n6bd4 + 1) >> 4) + iVar7;  /* 0x53e4 accumulator */
  const int iVar8 = iVar7 + ((n6be4 + 1) >> 5);  /* 0x53e8 accumulator */

  int pool0 = (iVar4 + 9) / 2;
  int pool1 = (iVar9 + 2) / 2;
  int pool3 = (iVar8 + 4) / 2;
  int pool2 = (iVar7 + local_4 + 4) / 2;

  const int cap2x = pool2 * 2;
  if (pool3 > cap2x) {
    pool3 = cap2x;
  }
  if (pool1 > cap2x) {
    pool1 = cap2x;
  }
  {
    const int rem = pool2 * 6 - pool3 - pool1;
    if (pool0 > rem) {
      pool0 = rem;
    }
  }

  ctx->col1->head.backup_force[0] = (uint16_t)(pool0 > 0 ? pool0 : 0);
  ctx->col1->head.backup_force[1] = (uint16_t)(pool1 > 0 ? pool1 : 0);
  ctx->col1->head.backup_force[2] = (uint16_t)(pool2 > 0 ? pool2 : 0); /* 0x53e6 MoW pool */
  ctx->col1->head.backup_force[3] = (uint16_t)(pool3 > 0 ? pool3 : 0); /* 0x53e8 Artillery pool */
}

/* True if name looks like a Man-O-War (Galleon fallback used as REF ship). */
/*
 * Crown Man-O-War test. The private copy this replaced also accepted
 * "Galleon"; DOS does not — both call sites are the @UNIT-type-0x12 tests
 * (king_ref.md:841 refill gate `unit_type_counts[crown][0x12]`, king_ref.md:870
 * `unit+0x3146 != 0x12`), so the Galleon arm was a port-side invention.
 */
int ai_king_is_mow(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u || !units_is_sea(units, u->id)) {
    return 0;
  }
  return units_type_is_man_o_war(units_type(units, u->type_index)) ? 1 : 0;
}

int ai_king_force_total(const uint16_t force[4]) {
  if (!force) {
    return 0;
  }
  return (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];
}

/*
 * The bare `ai_king_spawn_landing` helper that used to sit here (spawn one
 * land unit at a caller-chosen tile, no terrain test) is GONE as of
 * 2026-09-10: audit D6 folded the paid mercenary hire into
 * ai_king_10f0_land, and third-wave lead 2 folded FUN_43f7_2244's twin in
 * after it. Every King landing now goes through 10f0's own colony
 * roulette + water-tile scoring + Man-O-War transport, which is what DOS
 * does; a caller-chosen `(hx, hy+1)` could drop troops in the ocean
 * (bugs.md #255). Do not reintroduce it.
 */

void ai_king_set_ref_present(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  /* bugs.md #865: port-only "crown force in the New World" latch (pad bit).
   * It must NOT touch DS:0x5382 bit 0x02, which is the DOS intervention
   * ANNOUNCE latch (FUN_43f7_1528 raw 74493) — see ai_king.h. */
  ai_king_latch_set(col1, AI_KING_REF_PRESENT_BYTE, on ? 1 : 0);
}

void ai_king_set_boycott(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  ai_king_latch_set(col1, AI_KING_BOYCOTT_BYTE, on ? 1 : 0);
}

/*
 * Sync tax-refuse stand-in when cargo boycotts were cleared externally
 * (Jakob Fugger / diplo peace lift — do not touch FF here).
 * Source: fandom Jakob Fugger “all boycotts forgiven”; king_ref refuse +
 * nation.boycott_bitmap. When bitmap==0, clear market_demand_pool_raw[2] so tax may resume.
 */
void ai_king_sync_boycott_refuse(ColonizeCol1Save* col1, int human) {
  if (!col1 || human < 0 || human >= 4) {
    return;
  }
  if (ai_king_latch_get(col1, AI_KING_BOYCOTT_BYTE) == 0) {
    return;
  }
  if (col1->nation[human].boycott_bitmap == 0) {
    ai_king_latch_set(col1, AI_KING_BOYCOTT_BYTE, 0);
  }
}

/*
 * FUN_43f7_1d42 — the real royal REF budget tick (viceroy_unpacked.c
 * 74846-74907 + OVL07 asm at file offset 0x2c62, re-read 2026-09-06).
 * Runs each PEACETIME turn from FUN_43f7_2424 for the human slot; the asm's
 * first test (`TEST [0x5382],1 -> JMP 2de6` = RETF) makes the whole function
 * a no-op once WoI is declared — the Ghidra-visible @KINGMOBILIZE (0x1320)
 * "wartime" arm at OVL07:2d8a is DEAD CODE in the shipped binary (its only
 * inbound jump sits after that early return; the two (*) xrefs into
 * LAB_2da8 are data refs, not calls).
 *
 *   growth = difficulty*8 + 10, doubled at year>=1600, >=1700, >=1750;
 *   nation.royal_money += growth  (nation+0x22 — the same 32-bit purse the
 *     Europe sales tax, Custom House tax and treasure Crown-share already
 *     credit in this port; 1d42 is its consumer);
 *   when >= 0x708 (1800): buy exactly ONE pool unit this turn:
 *     k=0 Regulars; k=1 if (reg+2)/3 > cavalry; k=3 if reg/4 > artillery;
 *     k=2 if (reg+cavalry+artillery+5)/10 > man-o-war (last write wins);
 *   expeditionary_force[k]++ (0x53da/dc/de/e0);
 *   @KINGBUY (0x1318) with %STRING0 = FUN_43f7_0082(k, crown) unit name —
 *     the crown's 0x543f control byte is never 0, so the crown map applies:
 *     6 Regulars / 8 Cavalry / 0x12 Man-O-War / 0xb Artillery;
 *   royal_money -= 1800.
 *
 * DOS tail (asm 43f7:1e58-1e61, `MOV SI,[BP+param]; MOV AL,[SI+0x9408];
 * SUB AH,AH; ADD [BX+0xe],AX`) — ported 2026-09-06d. `nation+0xe` decoded:
 * the nation record is `n*0x13c + DS:-0x77f8` (nation_flags +0, tax_rate +1,
 * recruit[3] +2, tax_hike_count +5, recruit_count +6, founding_fathers[4] +7,
 * unknown21_pad +0xb, liberty_bells_pool +0xc) so +0xe is
 * `liberty_bells_last_turn` — and the DOS write set proves the name: zeroed
 * at each nation's own turn start (FUN_3844_00f2, :58382), zeroed by new-game
 * init (FUN_38fd_6024, :68684), and ADDed alongside +0xc by the bell-spend
 * FUN_4345_0a22 (:73343). Those five sites are the ONLY touches of +0xe in
 * all three decompiled exports (scan of every `<var> = *(int*)0x84fc` alias
 * plus the array form `n*0x13c-0x77ea`, zero hits) — there is no in-game
 * reader, so the bump is observable only in the saved word.
 * `DS:0x9408` = `stuff.free_colonist_counts` (save_format_map row 242,
 * type==0 units). Phase order matches DOS: turn_run_nation_ticks (bells,
 * TURN_PROC_SETUP) runs before ai_king_nation_turn (TURN_PROC_KING), so the
 * bump lands last, exactly as 2424's EOT position does in DOS.
 * (The save-time "pool stash" that used to overwrite this word was deleted
 * by bugs.md #933; the bumped value now always reaches disk.)
 *
 * (The pre-2026-09-06 stand-in grew pools by tax band on every audience
 * event and set ref_present from a peacetime pool — both invented; removed.)
 */
void ai_king_1d42_royal_purse(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(col1)) {
    return; /* asm 43f7:1d54: wartime = full no-op */
  }
  ColonizeCol1Nation* nat = &col1->nation[human];
  const int year = (int)col1->head.year;
  int growth = (int)col1->head.difficulty * 8 + 10;
  if (year >= 1600) {
    growth <<= 1;
  }
  if (year >= 1700) {
    growth <<= 1;
  }
  if (year >= 1750) {
    growth <<= 1;
  }
  nat->royal_money += growth;
  /*
   * bugs.md #932 (verified, no change needed): the `< 0x708` early exit is
   * DOS's own `if (+0x24 > 0 || (+0x24 >= 0 && +0x22 > 0x707))` gate
   * (FUN_43f7_1d42, viceroy_unpacked.c:74875-74876 / _2.c:73600). Everything
   * below — the force pick, the @KINGBUY announce, the `-0x708` and the
   * `ADD [BX+0xe],AX` bells bump at asm 43f7:1e58 — sits INSIDE that gate
   * (raw 74886-74891), so a purse under the threshold correctly skips the
   * bump. The bump is not reached on any other path.
   */
  if (nat->royal_money < 0x708) {
    return;
  }
  uint16_t* f = col1->head.expeditionary_force;
  int k = 0;
  if (((int)f[0] + 2) / 3 > (int)f[1]) {
    k = 1; /* Cavalry short of a third of the Regulars */
  }
  if ((int)f[0] / 4 > (int)f[3]) {
    k = 3; /* Artillery short of a quarter */
  }
  if (((int)f[0] + (int)f[1] + (int)f[3] + 5) / 10 > (int)f[2]) {
    k = 2; /* Man-O-War short of a tenth of the land force */
  }
  f[k]++;
  static const ColonizeUnitKind k_pool_kind[4] = {
    UNITS_KIND_REGULAR, UNITS_KIND_CAVALRY, UNITS_KIND_MAN_O_WAR, UNITS_KIND_ARTILLERY
  };
  /* Display text is the catalog's own @UNIT name for that row. */
  const char* k_pool_display = ai_king_merc_unit_name(ctx->units, k_pool_kind[k]);
  if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
    PopupMsgTokens status_tok;
    memset(&status_tok, 0, sizeof(status_tok));
    status_tok.string0 = k_pool_display;
    popup_msg_fill(ctx->messages, "KINGBUY", &status_tok, "", ctx->status, ctx->status_size);
    popup_msg_strip_markup(ctx->status);
  }
  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = k_pool_display;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "KINGBUY",
      &tok,
      "",
      body,
      sizeof(body)
    );
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_INFO, human,
      ai_king_crown_nation_col1(col1, human), k, NULL, body
    );
  }
  nat->royal_money -= 0x708;
  /* asm 43f7:1e58 tail — 16-bit ADD, wraps like DOS. */
  nat->liberty_bells_last_turn =
    (uint16_t)(nat->liberty_bells_last_turn + (uint16_t)col1->stuff.free_colonist_counts[human]);
}

int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  /*
   * FUN_43f7_0004 (viceroy_unpacked_2.c:72202-72239) verbatim: walk every
   * colony record, and for the ones owned by `nation_id` accumulate
   * `pop` (colony +0x1f) and `pop * FUN_15eb_0274()` (the colony SoL%), then
   * divide the second by the first. No colonies (or a zero pop sum) returns
   * the untouched accumulator, i.e. 0.
   *
   * NO `liberty_bells_pool / 4` STAND-IN (bugs.md #424, "rival monarchs
   * considering granting independence ... in 1530"). That number is not a
   * percentage at all — it is the nation's lifetime bell total, which real
   * DOS saves carry in the MILLIONS (original_saves/valid-lategame-saves/
   * COLONY00.SAV: nation 3 = 34,605,631), so every hit of the old fallback
   * clamped straight to 100% rebel sentiment. Section D of the year end then
   * read `rebel_sentiment * census_pop_proxy / 100` as the rival's rebel
   * count and fired @OTHERMIGHT as soon as the census passed the band floor.
   * DOS's FUN_15eb_0274 (viceroy_unpacked_2.c:8167-8190) returns 0 when the
   * rebel divisor is 0 ("nothing has accumulated"), and 43f7_0004 has no
   * nation-level fallback whatsoever; colony_prod_sol_percent already
   * deleted the identical stand-in from the colony-screen path.
   *
   * DOS calibration for the same expression, from the shipped late-game
   * saves (difficulty 2, threshold 60, band floor 40): 1505/1550 fixtures
   * read rebel_sentiment 0-1 with census 2-3 (v = 0); the 1680 save reads
   * 23/51/82 with census 51/85/128 (v = 11/43/100) and carries
   * rebellion_pct_last_notified = 43 on the nation that is inside the band.
   * The popup belongs to a mature AI empire, not to 1530.
   */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    uint64_t pop_sum = 0;
    uint64_t sol_sum = 0;
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != nation_id) {
        continue;
      }
      /* DOS adds colony +0x1f as-is (a 0-pop record weighs 0), no min-1. */
      const uint64_t pop = (uint64_t)c->population;
      int sol = 0;
      if (c->rebel_divisor > 0) {
        sol = (int)((c->rebel_dividend * 100u) / c->rebel_divisor);
      }
      if (sol < 0) {
        sol = 0;
      }
      sol += founding_fathers_bolivar_sol_bonus(ctx->col1, nation_id);
      if (sol > 100) {
        sol = 100;
      }
      sol_sum += (uint64_t)sol * pop;
      pop_sum += pop;
    }
    if (pop_sum > 0) {
      return (int)(sol_sum / pop_sum);
    }
  }
  return 0;
}

/*
 * FUN_43f7_1eca colony-SoL bias (catalog: promote when colony SoL>50%).
 * Prefer Col1 rebel_dividend/divisor at the unit tile; else nation SoL (0004).
 * King promote path only — not FF Washington mass-promote / combat upgrade.
 */
int ai_king_colony_sol_at(const ColonizeTurnContext* ctx, int nation_id, int x, int y) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != nation_id) {
        continue;
      }
      if ((int)c->x != x || (int)c->y != y) {
        continue;
      }
      /* FUN_15eb_0274: divisor 0 means nothing has accumulated = 0%. The old
       * `div = divisor ? divisor : 1` guard turned a fresh record into
       * `dividend * 100` instead. */
      int sol = 0;
      if (c->rebel_divisor > 0) {
        sol = (int)(((uint64_t)c->rebel_dividend * 100ull) / (uint64_t)c->rebel_divisor);
      }
      if (sol < 0) {
        sol = 0;
      }
      /* FUN_15eb_0274 Bolivar display boost (same as colony_prod_sol_percent). */
      sol += founding_fathers_bolivar_sol_bonus(ctx->col1, nation_id);
      if (sol > 100) {
        sol = 100;
      }
      return sol;
    }
  }
  return ai_king_sol_percent(ctx, nation_id);
}

/*
 * DOS 0x5382 bit0 (head.game_options.woi) is the real WoI latch — read that,
 * not the market_demand_pool_raw[0] stand-in: market_demand_pool_raw[0..5] alias DOS market_demand_pool
 * words 0–2 (col1_save.h), so on a real DOS-authored save market_demand_pool_raw[0] holds
 * live price data, not a war flag, and is nonzero almost every game. A save
 * never touched by ai_king_set_independence (i.e. every save that didn't
 * come out of this port's own turn_end) would misreport WoI as declared.
 * Set only on declare (ai_king_try_declare / ai_king_set_independence) when
 * SoL≥AI_KING_DECLARE_SOL_MIN — never by restless chrome.
 */
int ai_king_independence_declared(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  return col1->head.game_options.woi != 0;
}

void ai_king_set_independence(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  /* game_options.woi IS the latch — ai_king_latch_set(AI_KING_WOI_BYTE)
   * writes this same field and returns, so the "legacy Linux mirror" call
   * that used to precede this line was writing it twice (deleted 2026-09-14
   * with the market_demand_pool_raw[] mirror it referred to, which no longer exists). */
  col1->head.game_options.woi = on ? 1 : 0;
  if (on) {
    col1->head.event.colony_burning = 1; /* chrome hint */
  }
}

/*
 * Pack/unpack the KING_AUDIENCE popup payload: the tax delta that was
 * actually applied (1..8, always positive — only raises ever reach the
 * tea-party choice) and the tonnage-walk-picked cargo (0..15) that a tea party
 * would boycott/confiscate. Small ints, trivially reversible.
 */
int ai_king_teaparty_payload(int applied, int cargo) {
  return applied * 100 + cargo;
}
void ai_king_teaparty_payload_parts(int payload, int* out_applied, int* out_cargo) {
  if (out_applied) {
    *out_applied = payload / 100;
  }
  if (out_cargo) {
    *out_cargo = payload % 100;
  }
}
