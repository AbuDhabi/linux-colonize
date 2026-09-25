/*
 * King / REF AI — Mercenary hire offers, frigate offers & peacetime mercenary hire
 *
 * Split out of ai_king.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_king_internal.h. See ai_king.c for the module prologue and the
 * crown/boycott, tea-party and REF-bookkeeping helpers.
 *
 * Sections:
 *   Mercenary hire offers (ai_king_merc_payload .. ai_king_merc_offer)
 *   Frigate offers & peacetime mercenary hire (ai_king_frigate_threat_counts .. ai_king_peacetime_merc_offer)
 */

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

/* ===== Mercenary hire offers (ai_king_merc_payload .. ai_king_merc_offer) ===== */
static int ai_king_merc_payload(int hx, int hy, int qty_a, int extra_flag, int price) {
  return ((hx & 0x3f) << 26) | ((hy & 0x3f) << 20) | ((qty_a & 0xf) << 16) |
         ((extra_flag & 1) << 15) | (price & 0x7fff);
}

void ai_king_merc_payload_parts(int payload, int* out_hx, int* out_hy, int* out_qty_a,
                                       int* out_extra_flag, int* out_price) {
  if (out_hx) {
    *out_hx = (payload >> 26) & 0x3f;
  }
  if (out_hy) {
    *out_hy = (payload >> 20) & 0x3f;
  }
  if (out_qty_a) {
    *out_qty_a = (payload >> 16) & 0xf;
  }
  if (out_extra_flag) {
    *out_extra_flag = (payload >> 15) & 1;
  }
  if (out_price) {
    *out_price = payload & 0x7fff;
  }
}

/*
 * FUN_43f7_2022 rebel-branch accept (viceroy_unpacked.c:75060-75070): debit
 * the rolled price, then `thunk_FUN_2a1f_010a(uVar7)` = FUN_43f7_10f0(1) —
 * the paid hire and the free intervention are the SAME landing routine, and
 * 10f0's `param_1` is exactly the paid flag. Ported 2026-09-10 (audit D6):
 * this used to be a second implementation that spawned bare "Regular"/
 * "Dragoon"/"Artillery" at (hx, hy+1) with no terrain test at all — the
 * water-spawn class bugs.md #255 fixed for 06a6 — and skipped the colony
 * roulette, the Man-O-War transport and the FUN_43f7_0082 type map (which
 * for a rebel human at war yields Cont. Army / Cont. Cav., not colonial
 * Regulars). It now hands 10f0 the mercenary counts DOS keeps at DS:0x9e46
 * (−0x61ba): slot 0 = qty_a Regulars-class, and exactly one of slot 1
 * (Cavalry) or slot 3 (Artillery) per the offer-time coin flip.
 *
 * DOS re-derives the landing colony inside 10f0 at accept time; hx/hy from
 * the payload survive only as an offer-time sanity field (see
 * ai_king_merc_payload). Returns 1 when the hire went through, 0 when the
 * treasury cannot cover the price.
 */
int ai_king_do_merc_hire_at(ColonizeTurnContext* ctx, int human, int hx, int hy,
                                   int qty_a, int extra_flag, int price) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || human < 0 || human >= 4) {
    return 0;
  }
  if (qty_a < 1 || price < 0 || hx < 0 || hy < 0) {
    return 0;
  }
  if (europe_nation_gold(ctx->europe, ctx->col1, human) < (uint32_t)price) {
    return 0;
  }
  /* Audit G3: one debit through the accessor — the old pair read the stale
   * record and then assigned it over the live purse. DOS debits before the
   * 10f0 call (75052-75059). */
  europe_nation_gold_add(ctx->europe, ctx->col1, human, -(long)price);
  /*
   * DOS 2022 writes the count array as: 0x9e46 = qty_a; then
   * `if (rng(0,1) == 0) *0x9e4c = 1; else *0x9e48 = 1;` — 0x9e4c is slot 3
   * (Artillery), 0x9e48 slot 1 (Cavalry). extra_flag carries that raw roll.
   */
  int merc_counts[4] = {0, 0, 0, 0};
  merc_counts[0] = qty_a;
  merc_counts[extra_flag == 0 ? 3 : 1] = 1;
  /* Stand-in status first: the landing overwrites it with the @MERCS line
   * when it actually puts troops ashore. */
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size,
             "Mercenaries join the Continental cause (−%d gold).", price);
  }
  ai_king_10f0_land(ctx, human, 1, merc_counts);
  return 1;
}

/*
 * Linux-invented stand-in, NOT a faithful port of FUN_43f7_2244 or
 * FUN_43f7_2022 — corrected 2026-08-14, see king_ref.md "2244/2022 —
 * corrected". Neither DOS function has an SoL/300-gold gate or a
 * once-per-war human CHOICE; 2022 is a recurring per-turn self-funded
 * roll (any Euro nation's own treasury, wartime); 2244 is 2022's
 * peacetime twin for AI nations only, unrelated to a human hire offer.
 * This function now ports 2022's rebel branch faithfully: recurring
 * per-turn 1-in-3 roll while REF is absent or the Artillery backup pool
 * is empty; on a hit, roll quantity/price and offer a CHOICE (or
 * auto-accept without ai_popups). market_demand_pool_raw[3] is no longer a gate —
 * DOS has no once-per-war flag here — kept only as a "pending offer
 * already queued" guard so a re-roll can't stack a second CHOICE while
 * one is unanswered.
 */
/*
 * @UNIT row name for a mercenary count slot. DOS splices the live type-table
 * pointer (`type * 0xe + 0x5230`), so a modded NAMES.TXT @UNIT block wins;
 * the DOS spelling is the fallback for the synthetic test pools.
 */
const char* ai_king_merc_unit_name(
  const ColonizeUnitPool* units, ColonizeUnitKind kind
) {
  if (units) {
    const int ty = units_kind_type_index(units, kind);
    if (ty >= 0) {
      const ColonizeUnitType* t = units_type(units, ty);
      if (t && t->name[0] != '\0') {
        return t->name;
      }
    }
  }
  return "";
}

static int ai_king_merc_offer_pending(const AiPopupState* st) {
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_KING_MERC) {
      return 1;
    }
  }
  return st->open && st->current.tag == AI_POPUP_TAG_KING_MERC;
}

/* @MERCENARIES body + token-filled choice rows; returns the choice count. */
static int ai_king_merc_fill_dialog(
  ColonizeTurnContext* ctx,
  const PopupMsgTokens* tok,
  const char* fallback,
  char* body,
  char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN]
) {
  popup_msg_fill(ctx->messages, "MERCENARIES", tok, fallback, body, AI_POPUP_BODY_LEN);
  const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "MERCENARIES");
  const int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
  for (int i = 0; i < nch; ++i) {
    char filled[AI_POPUP_CHOICE_LEN];
    popup_msg_apply_tokens(filled, sizeof(filled), choice_buf[i], tok);
    str_copy_trunc(choice_buf[i], sizeof(choice_buf[i]), filled);
  }
  return nch;
}

void ai_king_merc_offer(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->rng) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  /* Don't stack a second offer while one is already pending a response. */
  if (ai_king_human_popups(ctx) && ai_king_merc_offer_pending(ctx->ai_popups)) {
    return;
  }
  /*
   * FUN_43f7_2022 line 75007: `(*(byte*)0x5382 & 2) == 0 || *(int*)0x53e6 == 0`
   * — gate reads the Man-O-War/colony-count pool (backup_force[2], see
   * ai_king_seed_backup_force_1a26), not the Artillery pool. Bit2 is the
   * intervention-announced latch (see ai_king_10f0_land), which
   * the port used to conflate with ref_present.
   */
  const int intervened =
    ai_king_latch_get(ctx->col1, AI_KING_INTERVENE_ANNOUNCED_BYTE) != 0;
  const int mow_pool = ctx->col1->head.backup_force[2];
  if (intervened && mow_pool != 0) {
    return; /* free backup-force drain path (ai_king_10f0_land) covers this beat */
  }
  if (dos_rng_range(ctx->rng, 0, AI_KING_MERC_ROLL_CHANCE - 1) != 0) {
    return; /* 1-in-3 chance to even attempt this turn */
  }
  const int difficulty = ctx->col1->head.difficulty;
  const int qty_a = dos_rng_range(ctx->rng, 2, ((4 - difficulty) >> 1) + 2);
  const int extra_flag = dos_rng_range(ctx->rng, 0, 1);
  const int roll2 = dos_rng_range(ctx->rng, 0, 6);
  const int price = (qty_a + 2) * ((difficulty + 3) * 2 + roll2) * 100;
  if (europe_nation_gold(ctx->europe, ctx->col1, human) < (uint32_t)price) {
    return; /* DOS silently skips the offer when unaffordable — no status/dialog */
  }
  int hx = 0;
  int hy = 0;
  /* bugs.md #878g: FUN_43f7_2022's merc body has NO colony pick — the
   * landing colony is chosen by 10f0's own roulette on accept. Keep the
   * weakest-port probe only to fill the port-side hx/hy payload; a failure
   * must not abort the offer after the draws have already been burnt. */
  (void)ai_king_weakest_port(ctx, human, &hx, &hy);
  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    /*
     * DOS 75050 `FUN_291f_0ac8(0, 0, *0x53d6)`: %STRING0 is the COUNTRY name
     * (mode 0 → FUN_15b3_0144, table DS:-0x72be) of rival slot 2 — the Euro
     * power selling the mercenaries, the same slot the @MERCS arrival line
     * names. The "Europe" stand-in that was here named nobody.
     */
    const int seller = ai_king_intervention_nation_slot(ctx, human, 1);
    const char* seller_name =
      (seller >= 0 && seller < 4) ? reports_nation_country_name(seller) : "";
    tok.string0 = seller_name;
    /*
     * %STRING1 is a COMPOSED LIST, not one word. DOS builds it in a local
     * buffer (raw 75028-75041): the count (`FUN_281f_0182`) then DS:0x50
     * " " and the @UNIT name of type 6, then — for each set count slot —
     * DS:0x52 ", " and that slot's @UNIT name:
     *   *0x5284 = type 6  Regulars   (slot 0, the rolled quantity)
     *   *0x52a0 = type 8  Cavalry    (slot 1, 0x9e48)
     *   *0x52ca = type 11 Artillery  (slot 3, 0x9e4c)
     * (`type * 0xe + 0x5230` is the @UNIT name pointer — raw 14128.)
     * 2022's rebel roll sets exactly one extra: roll 0 → 0x9e4c Artillery,
     * roll 1 → 0x9e48 Cavalry. The port named that single extra and dropped
     * the count and the Regulars head; it also called slot 1 "Dragoons".
     */
    char merc_list[96];
    int list_n = snprintf(
      merc_list, sizeof(merc_list), "%d %s", qty_a,
      ai_king_merc_unit_name(ctx->units, UNITS_KIND_REGULAR)
    );
    if (list_n < 0) {
      list_n = 0;
    }
    if ((size_t)list_n < sizeof(merc_list)) {
      snprintf(
        merc_list + list_n, sizeof(merc_list) - (size_t)list_n, ", %s",
        ai_king_merc_unit_name(ctx->units, extra_flag == 0 ? UNITS_KIND_ARTILLERY : UNITS_KIND_CAVALRY)
      );
    }
    tok.string1 = merc_list;
    tok.number0 = price;
    tok.has_number0 = true;
    char body[AI_POPUP_BODY_LEN];
    char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const int nch = ai_king_merc_fill_dialog(ctx, &tok, "", body, choice_buf);
    /* GAME.TXT: No thank you. / Pay $ — map to Decline / Hire. */
    const char* labels[2];
    const int ids[] = {AI_KING_CHOICE_DECLINE, AI_KING_CHOICE_HIRE};
    if (nch >= 2) {
      labels[0] = choice_buf[0];
      labels[1] = choice_buf[1];
    } else {
      labels[0] = "";
      labels[1] = "";
    }
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_MERC, human,
                                    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
                                    ai_king_merc_payload(hx, hy, qty_a, extra_flag, price), NULL,
                                    body, labels, ids, 2)) {
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size,
                 "Mercenaries offer to join the Continental cause (−%d gold).", price);
      }
      return;
    }
  }
  (void)ai_king_do_merc_hire_at(ctx, human, hx, hy, qty_a, extra_flag, price);
}

/*
 * FUN_3844_00f2 tail (viceroy_unpacked.c:58393-58424) — @KINGFRIGATE.
 * The nation-EOT census loop above it (58239-58300) scans an 11x11 box
 * around each of the nation's colonies for foreign armed ships (@UNIT
 * table byte 0x5236+type*0xe non-zero) within path distance < 6 and sets
 * colony +0x1b bit 2 (a Frigate) / bit 1 (any other warship); DS:0xa89b
 * counts Frigate-threatened colonies, DS:0xa89a the rest. Then, when
 * (a89b != 0 || a89a > 3), the nation owns no Frigate (per-nation
 * unit-type count table -0x6db4 + 0x11 == 0), WoI is not declared
 * (0x5382 bit0) and DS:0x538e (turn) & 7 == 0: a human nation gets
 * @KINGFRIGATE Yes/No behind the 0x3e audience tune, AI nations
 * auto-accept. Yes → a Frigate spawns sailing from Europe to the nation's
 * landfall (unit flag 0x40, FUN_291f_0aee voyage roll) and, human only,
 * FUN_38fd_3dc8(KINGTAX, 10) — the ordinary +10% hike with its tea-party
 * choice. 2026-09-08d: reads the real census blockade bits (colony +0x1b
 * & 3, incl. the FUN_2a1f_027e/6662_0906 flood-cost < 6 refinement) now
 * that the 4962_0018 probe runs for the human at TURN_PROC_FINISH — DOS
 * reads DS:0xa89b/0xa89a right after the same census in 00f2. The old
 * Chebyshev-only rescan here is retired.
 */
/* ===== Frigate offers & peacetime mercenary hire (ai_king_frigate_threat_counts .. ai_king_peacetime_merc_offer) ===== */
static int ai_king_frigate_threat_counts(
  const ColonizeTurnContext* ctx, int nation, int* out_frigate, int* out_other
) {
  int frig = 0;
  int other = 0;
  if (!ctx || !ctx->colonies) {
    return 0;
  }
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0) {
      frig++;
    }
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP) != 0) {
      other++;
    }
  }
  if (out_frigate) {
    *out_frigate = frig;
  }
  if (out_other) {
    *out_other = other;
  }
  return 1;
}

static int ai_king_frigate_spawn(ColonizeTurnContext* ctx, int nation) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || nation < 0 || nation >= 4) {
    return -1;
  }
  const int ti = units_kind_type_index(ctx->units, UNITS_KIND_FRIGATE);
  if (ti < 0) {
    return -1;
  }
  int x = (int)ctx->col1->nation[nation].return_from_europe_x;
  int y = (int)ctx->col1->nation[nation].return_from_europe_y;
  if (x == 0 && y == 0) {
    x = 236;
    y = 236;
  }
  const int id = units_spawn_allow_stack(ctx->units, ti, x, y);
  ColonizeUnit* u = units_get(ctx->units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation);
  u->orders = UNITS_ORDER_AI_SAIL;
  u->col1_flags15 = (uint8_t)(u->col1_flags15 | 0x40u);
  u->goto_x = x;
  u->goto_y = y;
  const bool magellan = founding_fathers_nation_has(ctx->col1, nation, FF_FERDINAND_MAGELLAN);
  /*
   * DOS 58419 `FUN_291f_0aee(0x281f, iVar5, x, y)` — the SAME voyage roll
   * the manual sail path uses, and it reads DS:0x9418[nation] (the
   * FUN_48d3_0002 hull tally), not a live-pool rescan. Audit second-wave
   * lead 3: this used to be a bare `units_count_sea_for_nation`, a third
   * spelling that omitted the human's EuropeScreen harbour/expected/bound
   * hulls — DOS keeps those on the Europe sentinel diagonal, inside the
   * tally. A human with his whole fleet in the harbour therefore got the
   * `< 3 hulls` fast crossing for the Crown's gift Frigate.
   */
  const int dur = europe_voyage_turns_roll(
    ctx->rng, magellan, turn_voyage_ship_count(ctx, nation)
  );
  u->col1_counter16 = (uint8_t)dur;
  return id;
}

void ai_king_frigate_accept(ColonizeTurnContext* ctx, int nation) {
  if (ai_king_frigate_spawn(ctx, nation) < 0) {
    return;
  }
  if (nation == ctx->human_nation) {
    /* Port-authored notice: no GAME.TXT section covers the frigate-departure
     * status beat (KINGFRIGATE only carries the audience offer itself). */
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "Royal frigate dispatched.");
    }
    ai_king_tax_hike_apply(ctx, nation, 10, NULL);
  }
}

void ai_king_frigate_offer(ColonizeTurnContext* ctx, int nation) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->colonies ||
      nation < 0 || nation >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  if ((ctx->col1->head.turn & 7u) != 0) {
    return;
  }
  const int ft = units_kind_type_index(ctx->units, UNITS_KIND_FRIGATE);
  /* Slot array is sparse — despawns clear a slot but decrement unit_count, so
   * bound with the array size, not the live population (as every other unit
   * sweep in this file does). Bounding by unit_count hid an existing Frigate
   * behind any hole and re-fired the offer (free ship + a real +10% tax). */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    const ColonizeUnit* u = &ctx->units->units[ui];
    if (u->active && u->nation_id == nation && u->type_index == ft) {
      return; /* per-nation Frigate count != 0 */
    }
  }
  int frig = 0;
  int other = 0;
  if (!ai_king_frigate_threat_counts(ctx, nation, &frig, &other)) {
    return;
  }
  if (frig == 0 && other <= 3) {
    return;
  }
  if (nation != ctx->human_nation || !ai_king_human_popups(ctx)) {
    ai_king_frigate_accept(ctx, nation); /* AI nations: FUN_281f_03fe skipped, local_4 = 1 */
    return;
  }
  for (int q = 0; q < ctx->ai_popups->queue_count; ++q) {
    if (ctx->ai_popups->queue[q].tag == AI_POPUP_TAG_KING_FRIGATE) {
      return;
    }
  }
  const int d = (int)ctx->col1->head.difficulty;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = reports_difficulty_title(d >= 0 && d < 5 ? d : 0);
  tok.string1 = ctx->col1->player[nation].name[0] ? ctx->col1->player[nation].name
                                                    : "";
  tok.string2 = ctx->col1->player[nation].country_name[0]
                  ? ctx->col1->player[nation].country_name
                  : "";
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    ctx->messages, "KINGFRIGATE", &tok,
    "",
    body, sizeof(body)
  );
  char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "KINGFRIGATE");
  const int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
  const char* labels[2];
  const int ids[] = {AI_KING_CHOICE_ACCEPT, AI_KING_CHOICE_REFUSE};
  if (nch >= 2) {
    labels[0] = choice_buf[0];
    labels[1] = choice_buf[1];
  } else {
    labels[0] = "";
    labels[1] = "";
  }
  sound_play(0x3e); /* FUN_3844_00f2 3844:0350: audience tune (281f_048e) before the CHOICE */
  if (!ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_FRIGATE, nation,
                                   ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, nation), 0, NULL, body, labels, ids, 2)) {
    ai_king_frigate_accept(ctx, nation); /* queue full: DOS has no "no answer" path */
  }
}

/*
 * FUN_43f7_2244 (viceroy_unpacked.c:75074-75152) — the PEACETIME mercenary
 * offer to the human. Reached via FUN_281f_0668 (:32150) from the
 * `*(char *)(n*0x34+0x543f) == '\0'` (human, control == 0) arm of the
 * FUN_130d_0290 year loop (:6409-6421): after FUN_281f_0644 (= 3844_00f2,
 * the nation's own EOT / census / king beats) and right before FUN_281f_062c
 * (Move Pieces). It never runs for an AI nation — the `== '\x01'` sibling
 * arm at :6397 is the AI turn (6d8e) and has no 0668 call. Until 2026-09-15
 * this was ported as an AI-nation "self-funded troop gift"; that premise is
 * refuted, see king_ref.md "2244 — human peacetime merc offer".
 *
 * Raw, in order:
 *   75087  (0x5382 & 1) == 0  &&  RNG(0,0x14) == 0        WoI not declared, 1-in-21
 *   75090  seller = RNG(0,3);  *0x53d6 = seller           rival slot 2 stamped BEFORE the test
 *   75092  seller == *0x5398 (human)  ||  FUN_281f_0a38(seller, human) & 0x40 (PEACE)
 *   75095  0x9e46..0x9e4c = 0                             {regular, cavalry, -, artillery}
 *   75100  regular = RNG(1,3)
 *   75102  RNG(0,1)==0 → artillery = 1, RNG(0,1)==0 → artillery = 2   else regular += 1
 *   75111  roll = RNG(0,6)
 *   75112  price = ((artillery + cavalry)*2 + regular) * ((difficulty+4)*2 + roll) * 100
 *   75115  %STRING1 = "<regular> " + @UNIT[4] (Dragoons)  [", " + @UNIT[8]]  [", " + ["<n> "] + @UNIT[11]]
 *          (0x5268/0x52a0/0x52ca = @UNIT name pointers of types 4 / 8 / 11; the
 *          artillery count is only spelled out when > 1; cavalry is never set here)
 *   75131  offer only when price <= human gold (32-bit at *0x84fc+0x2a/0x2c)
 *   75136  %STRING0 = country name of seller (FUN_291f_0ac8 mode 0), %NUMBER0 = price
 *   75139  FUN_281f_0652(0x134c = @MERCENARIES, 1) — "No thank you." / "Pay {%NUMBER0$}."
 *   75141  choice 2 (Pay) → debit human gold, thunk_FUN_2a1f_010a(1) = FUN_43f7_10f0(1)
 *          — the shared paid landing: colony roulette, seller's Man-O-War
 *          ferries the troops and is despawned, @MERCS arrival line.
 *
 * FUN_43f7_0082's type map for the human BEFORE independence gives type 4
 * (Dragoons) for both the regular and the cavalry slot, Artillery for slot
 * 3 — which is why the dialog names Dragoons (see ai_king_10f0_spawn_unit).
 *
 * Port shape: the roll and the eligibility/affordability gates run here, at
 * the same point of the turn (TURN_PROC_KING tail, after the 00f2 chrome
 * and before the player gets control). The CHOICE is queued with the rolled
 * counts + price in the payload; Pay is applied in ai_king_apply_popup_result
 * (AI_POPUP_TAG_KING_MERC_PEACE), which debits and calls ai_king_10f0_land's
 * paid arm exactly as 2022's Hire does. Without a human popup queue
 * (headless / harness) the offer is dropped after the draws: DOS blocks on
 * a dialog nobody can answer, and auto-buying with the player's gold would
 * be an invention. The RNG draws are burned either way, as in DOS.
 */
static int ai_king_merc_peace_payload(int regular, int artillery, int price) {
  return ((regular & 0xf) << 20) | ((artillery & 0x3) << 16) | (price & 0xffff);
}

void ai_king_merc_peace_payload_parts(
  int payload, int* out_regular, int* out_artillery, int* out_price
) {
  if (out_regular) {
    *out_regular = (payload >> 20) & 0xf;
  }
  if (out_artillery) {
    *out_artillery = (payload >> 16) & 0x3;
  }
  if (out_price) {
    *out_price = payload & 0xffff;
  }
}

static int ai_king_merc_peace_offer_pending(const AiPopupState* st) {
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_KING_MERC_PEACE) {
      return 1;
    }
  }
  return st->open && st->current.tag == AI_POPUP_TAG_KING_MERC_PEACE;
}

/* Pay arm of @MERCENARIES (raw 75141-75146): debit, then the 10f0 paid landing. */
int ai_king_do_merc_peace_hire(
  ColonizeTurnContext* ctx, int human, int regular, int artillery, int price
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || human < 0 || human >= 4) {
    return 0;
  }
  if (regular < 1 || price < 0) {
    return 0;
  }
  if (europe_nation_gold(ctx->europe, ctx->col1, human) < (uint32_t)price) {
    return 0;
  }
  europe_nation_gold_add(ctx->europe, ctx->col1, human, -(long)price);
  int merc_counts[4] = {0, 0, 0, 0};
  merc_counts[0] = regular;
  merc_counts[3] = artillery;
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "Mercenaries hired (−%d gold).", price);
  }
  /* DOS debits before 10f0 runs; a failed roulette / water scan keeps the
   * gold spent. Kept literal. */
  ai_king_10f0_land(ctx, human, 1, merc_counts);
  return 1;
}

void ai_king_peacetime_merc_offer(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->rng) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return; /* 75087: 0x5382 bit0 set → no peacetime offer */
  }
  if (dos_rng_range(ctx->rng, 0, 20) != 0) {
    return; /* 1-in-21 */
  }
  const int seller = dos_rng_range(ctx->rng, 0, 3);
  /* 75091: rival slot 2 = seller, stamped before the eligibility test; it is
   * the slot 10f0's paid arm names in the @MERCS arrival line. */
  ctx->col1->head.rival_nation_slot_2 = (uint16_t)seller;
  if (seller != human && (ai_diplo_read(ctx->col1, seller, human) & AI_DIPLO_PEACE) == 0) {
    return;
  }

  const int difficulty = ctx->col1->head.difficulty;
  int regular = dos_rng_range(ctx->rng, 1, 3);
  int artillery = 0;
  if (dos_rng_range(ctx->rng, 0, 1) == 0) {
    artillery = 1;
    if (dos_rng_range(ctx->rng, 0, 1) == 0) {
      artillery += 1;
    }
  } else {
    regular += 1;
  }
  const int roll = dos_rng_range(ctx->rng, 0, 6);
  const int price = (artillery * 2 + regular) * ((difficulty + 4) * 2 + roll) * 100;

  if (europe_nation_gold(ctx->europe, ctx->col1, human) < (uint32_t)price) {
    return; /* 75131: unaffordable → no dialog at all */
  }
  if (!ai_king_human_popups(ctx)) {
    return; /* nobody to answer the CHOICE — see header */
  }
  if (ai_king_merc_peace_offer_pending(ctx->ai_popups)) {
    return;
  }

  /* %STRING1 (75115-75130): "<n> Dragoons[, Artillery | , 2 Artillery]". */
  char merc_list[96];
  int list_n = snprintf(
    merc_list, sizeof(merc_list), "%d %s", regular,
    ai_king_merc_unit_name(ctx->units, UNITS_KIND_DRAGOON)
  );
  if (list_n < 0) {
    list_n = 0;
  }
  if (artillery > 0 && (size_t)list_n < sizeof(merc_list)) {
    if (artillery > 1) {
      snprintf(
        merc_list + list_n, sizeof(merc_list) - (size_t)list_n, ", %d %s", artillery,
        ai_king_merc_unit_name(ctx->units, UNITS_KIND_ARTILLERY)
      );
    } else {
      snprintf(
        merc_list + list_n, sizeof(merc_list) - (size_t)list_n, ", %s",
        ai_king_merc_unit_name(ctx->units, UNITS_KIND_ARTILLERY)
      );
    }
  }

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  const char* seller_name = reports_nation_country_name(seller);
  tok.string0 = seller_name;
  tok.string1 = merc_list;
  tok.number0 = price;
  tok.has_number0 = true;
  char body[AI_POPUP_BODY_LEN];
  char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const int nch = ai_king_merc_fill_dialog(ctx, &tok, "", body, choice_buf);
  /* GAME.TXT: No thank you. / Pay {%NUMBER0$}. — DOS choice 2 = Pay. */
  const char* labels[2];
  const int ids[] = {AI_KING_CHOICE_DECLINE, AI_KING_CHOICE_HIRE};
  if (nch >= 2) {
    labels[0] = choice_buf[0];
    labels[1] = choice_buf[1];
  } else {
    labels[0] = "";
    labels[1] = "";
  }
  if (ai_popup_enqueue_choice_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_MERC_PEACE, human, seller,
        ai_king_merc_peace_payload(regular, artillery, price), NULL, body, labels, ids, 2
      )) {
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "%s", body);
    }
  }
}

/*
 * FUN_38fd_5930 (viceroy_unpacked.c 68305-68415) — @KINGNEWWAR, the
 * PEACETIME crown declaration run from the Europe-EOT king slot: the King
 * cancels the human's peace with a random peer he is at peace with, then
 * pays compensation. Gates (raw 68335-68365): human slot, not Franklin
 * (FUN_281f_07b4(nation, 0x13)), `(difficulty + 2) * turn > 799`, at least
 * one peace peer, NO met-but-not-at-peace peer, peer land strength sum
 * (-0x6be4, 14 passes) ≤ own, and a `rng(0, (4 - peace_n) * 20) <=
 * difficulty` roll. Grant (raw 68372-68411): count/gold scaled by the
 * field-combat gap (-0x6bd4), capped at 6 − difficulty and (5 − difficulty)
 * * 500, Veteran Soldiers (profession 0x15) spawned in Europe, relation bits
 * 0x40 cleared / 0x10 set, war year latched at 0x53c8.
 *
 * (The FUN_43f7_2022 REF land hunt / MoW unload / capital rally / 1eca
 * promote description that used to sit here documented code deleted with D1
 * — see the closure note above ai_king_ref_pre_euro_beat.)
 */
