/*
 * Indian contact — friction/gift-gold, demand economics, visit mood, beg-food, incite & relation tick
 *
 * Split out of ai_contact.c (2026-09-23) verbatim; shared symbols are
 * declared in ai_contact_internal.h. See ai_contact.c for the module
 * prologue and the DOS provenance notes.
 *
 * Sections:
 *   Friction/gift-gold, choice popups & incite pricing/confirmation
 *   Tools/gold demand pipeline & gift-or-demand economics (raw 2154)
 *   Visit mood, demand gating & beg-food flow
 *   Human incite response, missionary convert/flee, WoI defect, prelude & relation tick
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

/* ===================== Friction/gift-gold, choice popups & incite pricing/confirmation (ai_contact_friction_decay .. ai_contact_apply_incite) ===================== */


/* Decay alarm_by_player + this nation's tribe frictions by `amount` (floor 0). */
static void ai_contact_friction_decay(
  ColonizeCol1Indian* ind,
  ColonizeCol1Save* col1,
  int nation_id,
  int e,
  int amount
) {
  if (!ind || amount <= 0 || e < 0 || e > 3) {
    return;
  }
  if ((int)ind->alarm_by_player[e] > amount) {
    ind->alarm_by_player[e] = (uint16_t)(ind->alarm_by_player[e] - (uint16_t)amount);
  } else {
    ind->alarm_by_player[e] = 0;
  }
  if (!col1 || !col1->tribe) {
    return;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    if ((int)t->alarm[e].friction > amount) {
      t->alarm[e].friction = (uint8_t)(t->alarm[e].friction - (uint8_t)amount);
    } else {
      t->alarm[e].friction = 0;
    }
  }
}

/* Max of alarm_by_player and tribe frictions for this Indian×Euro pair. */
int ai_contact_pair_friction(
  const ColonizeCol1Indian* ind,
  const ColonizeCol1Save* col1,
  int nation_id,
  int e
) {
  int friction = ind ? (int)ind->alarm_by_player[e] : 0;
  if (!col1 || !col1->tribe) {
    return friction;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->nation_id == nation_id && (int)t->alarm[e].friction > friction) {
      friction = (int)t->alarm[e].friction;
    }
  }
  return friction;
}

/*
 * Apply a gift-band gold drain (CONTACT_GIFT amount CHOICE or auto Large).
 * Small −5 / friction −1; Large −10 / friction −2. Cite: FUN_5bfb_102a;
 * indian_contact.md gift stand-in (no invented crosses).
 * Pocahontas: gift decays full friction (half-rate applies only to positive
 * alarm/friction bumps — prelude/encroachment/raid; wiki/fandom).
 */
void ai_contact_apply_gift_gold(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  unsigned gold_cost,
  int friction_decay
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || e < 0 || e > 3) {
    return;
  }
  if (!ind->euro_diplo[e]) {
    return;
  }
  /*
   * ai_contact_pair_friction seeds from alarm_by_player[e] and then only
   * grows, so `friction >= alarm_by_player[e]` always holds: the
   * `alarm_by_player[e] >= 55` disjunct could never decide anything, and
   * `>= 55 || >= 40` is just `>= 40`. Reduced 2026-09-09 (smell #54).
   */
  const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
  if (friction >= 40) {
    ai_contact_refuse_chrome(ctx, e, nation_id, AI_POPUP_TAG_CONTACT_GIFT, "Gift", "gifts");
    return;
  }
  /* audit G3: one treasury per nation — the human's live purse is
   * EuropeScreen.gold and the record lags it, so the gate and the debit both
   * go through the accessor pair. Gifts are human-reachable (the CONTACT_GIFT
   * amount CHOICE below), which is exactly the case the record-only read got
   * wrong: a player who had just sold in Europe could not afford a gift his
   * sidebar said he could. */
  if (europe_nation_gold(ctx->europe, ctx->col1, e) < gold_cost) {
    ai_contact_refuse_chrome(ctx, e, nation_id, AI_POPUP_TAG_CONTACT_GIFT, "Gift", "gifts");
    return;
  }
  europe_nation_gold_add(ctx->europe, ctx->col1, e, -(long)gold_cost);
  ai_contact_friction_decay(ind, ctx->col1, nation_id, e, friction_decay);
  {
    char gift_fb[AI_POPUP_BODY_LEN];
    snprintf(
      gift_fb,
      sizeof(gift_fb),
      "Gift of gold eases tensions with the %s.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_GIFT, nation_id, "Gift", gift_fb
    );
  }
}

/*
 * Human Gift amount CHOICE (Small −5 / Large −10 / Generous −20). Returns 1 if
 * enqueued. Cite: FUN_5bfb_102a amount stand-in; indian_contact.md.
 */
/*
 * Shared head/tail of the human amount CHOICE enqueues (audit AC-29): the
 * gift-amount and demand-amount builders differed only in which rows they
 * pushed, so the "is a human CHOICE reachable at all" guard and the
 * titleless enqueue live here.
 */
static int ai_contact_choice_ctx_ready(const ColonizeTurnContext* ctx, int e) {
  return ctx && ctx->ai_popups && ctx->col1_ok && ctx->col1 && e >= 0 && e <= 3 &&
         ai_contact_euro_is_human(ctx, e);
}

static int ai_contact_enqueue_choice(
  ColonizeTurnContext* ctx,
  AiPopupTag tag,
  int e,
  int nation_id,
  int payload,
  const char* body,
  const char* const* labels,
  const int* ids,
  int n
) {
  return ai_popup_enqueue_choice_ctx(
           ctx->ai_popups, tag, e, nation_id, payload, NULL, body, labels, ids, n
         )
           ? 1
           : 0;
}

int ai_contact_enqueue_gift_amount_choice(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id
) {
  if (!ai_contact_choice_ctx_ready(ctx, e)) {
    return 0;
  }
  /* audit G3: single treasury — the human's purse is EuropeScreen.gold. */
  const unsigned gold = (unsigned)europe_nation_gold(ctx->europe, ctx->col1, e);
  if (gold < 5u) {
    return 0; /* cannot pay Small — caller refuses */
  }
  const char* labels[3];
  int ids[3];
  int n = 0;
  labels[n] = "Small gift (5 gold)";
  ids[n] = AI_CONTACT_GIFT_SMALL;
  n++;
  if (gold >= 10u) {
    labels[n] = "Large gift (10 gold)";
    ids[n] = AI_CONTACT_GIFT_LARGE;
    n++;
  }
  if (gold >= 20u) {
    labels[n] = "Generous gift (20 gold)";
    ids[n] = AI_CONTACT_GIFT_GENEROUS;
    n++;
  }
  char body[AI_POPUP_BODY_LEN];
  snprintf(
    body,
    sizeof(body),
    "Offer gold to the %s?",
    ai_contact_tribe_name(nation_id)
  );
  return ai_contact_enqueue_choice(
    ctx, AI_POPUP_TAG_CONTACT_GIFT, e, nation_id, 0, body, labels, ids, n
  );
}

/*
 * FUN_4d56_417e price (Incite Indians / WARPATH — identified 2026-08-13,
 * see original_sources_annotated/ai/indian_incite_417e.md for the full
 * disassembly trail and live-capture confirmation). DOS formula:
 *   base = table[-0x69d6]*8 + (table[-0x6e7c]>>2&0xfe - 2*table[-0x69d6])
 *        + INDIAN_STATE.signed_byte[7]*2 + INDIAN_STATE.signed_byte[8]*2
 *   price = base / (relation_score + 0x4b); floor 500
 *
 * 2026-08-14: both previously-unnamed tables identified while tracing the
 * deep Euro G-table (euro_g_table_0a60.md / FUN_4962_06b6, same DS
 * neighborhood) — neither is a static lookup constant, both are live
 * per-turn recomputed sums over this tribe *type* (nation_id-4):
 *   table[-0x69d6][type] = count of villages of that tribe type
 *   table[-0x6e7c][type] = Σ combat_unit_base_x8(brave, mode=1) [attack-
 *                          mode value, matching the traced FUN_281f_09c8
 *                          call] over every Brave of that tribe type,
 *                          byte-clamped (DOS's saturating FUN_4962_0006)
 * INDIAN_STATE.signed_byte[7]/[8] were already-named fields all along —
 * `ind->muskets` / `ind->horse_herds` (col1_save.h) — just never wired
 * into this formula; the `ind->tech`/`alarm_by_player` stand-in below was
 * approximating the wrong quantities entirely, not just missing two
 * unconfirmed numbers.
 */
static uint32_t ai_contact_incite_price(
  ColonizeTurnContext* ctx,
  const ColonizeCol1Indian* ind,
  int nation_id,
  int inciter,
  int target,
  int is_missionary,
  int is_capital
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || nation_id < 4 || nation_id > 11) {
    return 500u;
  }
  const ColonizeCol1Save* col1 = ctx->col1;

  /* `tribe.nation_id` is the full 4-11 nation id, not a 0-7 "type" —
   * confirmed independently via colony.c/units.c's own `-4` indexing
   * into col1->indian[8] — so this compares directly against `nation_id`,
   * not `nation_id - 4`. (Was comparing against a `tribe_type` local that
   * made this loop count zero villages always; see the discount-loop
   * comment below for the matching fix + how this was found.) */
  int village_count = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      if ((int)col1->tribe[ti].nation_id == nation_id) {
        ++village_count;
      }
    }
  }
  if (village_count > 0xff) {
    village_count = 0xff;
  }

  int brave_value_sum = 0;
  if (ctx->units) {
    const ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
    /* Slot walk, `u->id` to the id-taking accessors — see
     * ai_contact_land_combat_sum's note (ids are 1-based and never recycled,
     * so `i` is not a unit id). Fixed 2026-09-10. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      if (units_is_sea(ctx->units, u->id)) {
        continue;
      }
      const int val = combat_unit_base_x8(&sctx, u->id, 1, NULL);
      brave_value_sum += val;
      if (brave_value_sum > 0xff) {
        brave_value_sum = 0xff;
        break;
      }
    }
  }

  const int base =
    village_count * 8 + (((brave_value_sum >> 2) & 0xfe) - 2 * village_count) +
    (int)ind->muskets * 2 + (int)ind->horse_herds * 2;

  /* FUN_281f_030c reads the DS:0x5b1c table raw — and that table is the
   * ALARM mirror (Linux alarm_by_player), not a friendliness score: the
   * 4528 ship gate treats >=0x4b as @MADATSHIPS, FUN_4cc6_00f2 writes it
   * with the French/Pocahontas alarm-growth halving, and
   * indian_actions_menu.md pins 84fc == indian.alarm_by_player[e]. The
   * multiplier is therefore (alarm + 75): the angrier the tribe already
   * is at the inciter, the MORE gold it demands to go to war for them.
   * (2026-09-06 polarity fix — this line previously used
   * ai_diplo_indian_relation, i.e. 100-alarm, inverting the price curve.) */
  const int relation = ai_diplo_indian_alarm(col1, nation_id, inciter); /* raw 0x5b1c value, 0..100 */
  (void)target; /* alarm_by_player no longer used — real formula has no target term here */
  /* Base-combine op resolved byte-exact, 2026-08-14: read the actual
   * decompiled bodies of FUN_1d1d_0f60/FUN_1d1d_0ec6 (viceroy_unpacked.c
   * lines 20742-20829, both "exact"-kind mapped) instead of trusting
   * FUNCTION_CATALOG.md's inferred labels secondhand — confirms the
   * catalog was right both times: 0f60 (called here) really is a plain
   * 32-bit multiply (`return (ulong)param_1*(ulong)param_3;` for the
   * common small-operand case), 0ec6 (called by the French branch below)
   * really is the signed-division helper. So this line is `base *
   * (relation+75)`, NOT a division — a previous pass's `* 100 /` here
   * was backwards in both operation and shape. See indian_incite_417e.md
   * "Base-combine op resolved" for the byte-exact walkthrough. */
  int price = base * (relation + 75);
  /* French get a real DOS price break here (nation_A==1, matches this
   * project's own English/French/Spanish/Dutch=0/1/2/3 ordering) — genuine
   * Colonization lore (French have the best native relations) and now
   * byte-exact: raw disassembly's FUN_1d1d_0ec6(price<<1, ..., 3, 0) is a
   * real division, i.e. price = price*2/3 (~33% off). */
  if (inciter == 1) {
    price = (int)(((long)price * 2) / 3);
  }
  /* Discount loop — now byte-exact (2026-08-14, replaces the
   * `ai_diplo_indian_relation>128`/flat-100 stand-in). Raw disassembly:
   * for every tribe record, if tribe.nation_id(+2) == this village's own
   * nation_id AND tribe.mission(+5)&0xf == inciter (i.e. that OTHER
   * village of the same tribe already has a mission from the inciting
   * Euro power — both fields already named/real in ColonizeCol1Tribe,
   * no invented globals needed): discount 250, or 1000 if it's a
   * Jesuit-grade mission (mission&0x10), doubled again if that other
   * village is the tribe capital (state.capital, DOS state+3&4).
   * Floor is applied ONCE at the very end in DOS (not before the loop
   * too) — the previous premature clamp before this loop is removed to
   * match.
   * Also fixes a real regression from earlier the same session: a prior
   * "bonus fix" here compared `t->nation_id` against `tribe_type`
   * (nation_id-4, 0-7), assuming the field was a raw 0-7 type — but
   * `colony.c`/`units.c` both independently confirm (via their own
   * `tribe.nation_id - 4` indexing into `col1->indian[8]`) the field is
   * really 4-11, same range as this function's own `nation_id` param, so
   * the ORIGINAL direct comparison was correct all along and the "fix"
   * broke it. Reverted to comparing against `nation_id` directly. */
  int discount = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      if ((t->mission & COL1_TRIBE_MISSION_NATION_MASK) != (uint8_t)inciter) {
        continue;
      }
      int amt = (t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) ? 1000 : 250;
      if (t->state.capital) {
        amt *= 2;
      }
      discount += amt;
    }
  }
  price -= discount;
  /*
   * Two more flat DOS discounts, wired 2026-08-14 (captured at popup-offer
   * time as booleans, not re-derived at apply time — same discipline as
   * ai_king_merc's offer-time landing capture, avoids needing a live unit
   * id or specific village index to survive the async CHOICE round-trip):
   * -1500 if the unit performing Incite is a Missionary
   * (UNITS_JOB_MISSIONARY, DOS unit-state byte 0x18 — a real, already-named
   * Linux constant); -500 if the specific village visited is the tribe
   * capital (DOS CUR_TRIBE_PTR state+3&4, ColonizeCol1Tribe.state.capital).
   */
  if (is_missionary) {
    price -= 1500;
  }
  if (is_capital) {
    price -= 500;
  }
  if (price < 500) {
    price = 500;
  }
  return (uint32_t)price;
}

/*
 * Human Incite Indians target CHOICE (FUN_4d56_417e Mode 1 — the menu step
 * of @INDIANWARPATH "Whom would you like us to attack?"). Lists the other
 * Euro nations the inciter can afford to incite this tribe against.
 * Returns 1 if enqueued.
 */
int ai_contact_enqueue_incite_target_choice(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  int is_missionary,
  int is_capital
) {
  if (!ctx || !ctx->ai_popups || !ctx->col1_ok || !ctx->col1 || e < 0 || e > 3) {
    return 0;
  }
  if (nation_id < 4 || nation_id > 11) {
    return 0;
  }
  const ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  /* audit G3: single treasury — incite pricing is human-reachable. */
  const uint32_t gold = europe_nation_gold(ctx->europe, ctx->col1, e);

  /*
   * 417e Mode-1 target set (viceroy_unpacked.c 83595-83615): before the
   * declaration the menu loop skips the inciter AND DS:0x53d2
   * (head.crown_nation_id, -1 until WoI); once DS:0x5382 bit0
   * (game_options.woi) is set there is no menu at all — the target is
   * fixed to the Crown nation (the only enemy left worth inciting
   * against). The old "AMERICA-scenario map" guess in
   * indian_incite_417e.md was wrong: 0x5382 bit0 is the WoI latch.
   */
  const int crown = (int)ctx->col1->head.crown_nation_id;
  const int woi_fixed =
    ctx->col1->head.game_options.woi && crown >= 0 && crown <= 3 && crown != e;

  (void)gold;
  (void)ind;

  /*
   * DOS row set (viceroy_unpacked.c 83600-83607): every Euro nation except
   * the inciter and the Crown, listed by plain nation name — there is no
   * affordability filter here. A target the treasury cannot pay for is
   * still offered and answered with @UNFORTUNATE after the confirm, which
   * is why the price never appears on these rows (it belongs to the
   * @INDIANWARPATH2 confirm below). The port used to filter by gold and
   * print "Incite against the X (N gold)"; both were Linux inventions.
   */
  const char* labels[3];
  char label_buf[3][48];
  int ids[3];
  int n = 0;
  for (int target = 0; target < 4 && n < 3; ++target) {
    if (target == e) {
      continue;
    }
    if (woi_fixed ? (target != crown) : (target == crown)) {
      continue;
    }
    snprintf(label_buf[n], sizeof(label_buf[n]), "%s", ai_contact_euro_name(target));
    labels[n] = label_buf[n];
    ids[n] = target;
    n++;
  }
  if (n == 0) {
    return 0;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_tribe_name(nation_id);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "INDIANWARPATH", &tok, "", body, sizeof(body));
  /* Carry the offer-time-captured discount flags through to apply time
   * (bit0=is_missionary, bit1=is_capital) — see ai_contact_incite_price. */
  const int payload = (is_missionary ? 1 : 0) | (is_capital ? 2 : 0);
  return ai_popup_enqueue_choice_ctx(
           ctx->ai_popups,
           AI_POPUP_TAG_CONTACT_INCITE,
           e,
           nation_id,
           payload,
           NULL,
           body,
           labels,
           ids,
           n
         )
           ? 1
           : 0;
}

/*
 * @INDIANWARPATH2 (0x16c1), the second step of FUN_4d56_417e Mode 1
 * (viceroy_unpacked.c 83626-83633): once the player has named a target the
 * tribe quotes its price and asks for the money. DOS runs the @NOCONTACT
 * gate (0a38 & 0x20) BEFORE the quote, so a tribe that never met the target
 * says so instead of naming a price; the treasury check comes after the
 * confirm, as @UNFORTUNATE. The confirm rides the same CONTACT_INCITE tag
 * as the target menu, with payload bit 2 marking the second stage and the
 * chosen target in bits 3-4, so no extra queue tag is needed.
 */
#define AI_CONTACT_INCITE_STAGE_CONFIRM 4
#define AI_CONTACT_INCITE_PAY 1
#define AI_CONTACT_INCITE_NEVERMIND 0

void ai_contact_enqueue_incite_confirm(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  int target,
  int is_missionary,
  int is_capital
) {
  if (!ctx || !ctx->ai_popups || !ind || e < 0 || e > 3 || target < 0 || target > 3 ||
      target == e) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_euro_name(target);
  if ((ind->euro_diplo[target] & COL1_INDIAN_MET_BIT) == 0) {
    /* @NOCONTACT (0x16b7): the tribe has never met the named nation. */
    char nb[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "NOCONTACT", &tok, "", nb, sizeof(nb));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", nb);
    return;
  }
  const uint32_t price =
    ai_contact_incite_price(ctx, ind, nation_id, e, target, is_missionary, is_capital);
  tok.number0 = (int)price;
  tok.has_number0 = true;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "INDIANWARPATH2", &tok, "", body, sizeof(body));
  char pay_fb[POPUP_MSG_CHOICE_LEN];
  snprintf(pay_fb, sizeof(pay_fb), "Pay %u.", (unsigned)price);
  char row_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    ctx->messages, "INDIANWARPATH2", &tok, pay_fb, "", row_buf, labels
  );
  int ids[2];
  ids[0] = AI_CONTACT_INCITE_PAY;
  ids[1] = AI_CONTACT_INCITE_NEVERMIND;
  const int payload = (is_missionary ? 1 : 0) | (is_capital ? 2 : 0) |
                      AI_CONTACT_INCITE_STAGE_CONFIRM | (target << 3);
  (void)ai_popup_enqueue_choice_ctx(
    ctx->ai_popups,
    AI_POPUP_TAG_CONTACT_INCITE,
    e,
    nation_id,
    payload,
    NULL,
    body,
    labels,
    ids,
    2
  );
}

/*
 * The @INDIANWARFARE announcement DOS shows at 417e's LAB_4d56_4499 tail
 * (string id 0x16e9 — resolved 2026-09-06 via the raw-EXE 121248+addr DS
 * string read; the annotation's old "0x16e9 = @INDIANWARPATH2" guess was
 * off by one tag — 0x16c1 is WARPATH2, the pay confirm). Fires for both
 * Mode 1 and Mode 2; shown to the human viewer.
 */
static void ai_contact_incite_warfare_chrome(
  ColonizeTurnContext* ctx,
  int viewer,
  int nation_id,
  int inciter,
  int target
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_tribe_name(nation_id);
  tok.string1 = ai_contact_euro_name(inciter);
  tok.string2 = ai_contact_tribe_name(nation_id);
  tok.string3 = ai_contact_euro_name(target);
  char fb[AI_POPUP_BODY_LEN];
  fb[0] = '\0';
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "INDIANWARFARE", &tok, fb, body, sizeof(body));
  ai_contact_human_chrome(
    ctx, viewer, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", body
  );
}

/*
 * Apply Incite Indians (FUN_4d56_417e Mode-1 tail, viceroy_unpacked.c
 * 83616-83650 + LAB_4d56_4499). DOS gate order after the target pick:
 *   1. 0a38(tribe, target) & 0x20 clear → @NOCONTACT (0x16b7), no charge;
 *   2. @INDIANWARPATH2 pay confirm (0x16c1 — merged into the Linux
 *      target menu, which already shows the price per row);
 *   3. treasury < price → @UNFORTUNATE (0x16d0), no charge;
 *   4. 030c(tribe, target) >= 0x4b (tribe already in the war band with
 *      the target) → @ALREADYSMITE (0x16dc), no charge;
 *   5. @INDIANWARFARE (0x16e9) announce, then
 *      FUN_281f_0d6c(tribe, target, 100, 0) — resolved 2026-09-06 via
 *      address_mapping.csv to FUN_4cc6_00f2, the alarm-delta writer:
 *      alarm(tribe→target) += 100 (halved for a French target and again
 *      for Pocahontas inside 00f2, clamped at 100) — i.e. the incite
 *      slams the tribe into the war band against the target; the old
 *      flat +10 here was a placeholder — and finally gold -= price.
 */
void ai_contact_apply_incite(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  int target,
  int is_missionary,
  int is_capital
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || e < 0 || e > 3 ||
      target < 0 || target > 3 || target == e) {
    return;
  }
  if ((ind->euro_diplo[target] & COL1_INDIAN_MET_BIT) == 0) {
    /* @NOCONTACT: the tribe has never met the target nation. */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_euro_name(target);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "NOCONTACT", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", body);
    return;
  }
  const uint32_t price =
    ai_contact_incite_price(ctx, ind, nation_id, e, target, is_missionary, is_capital);
  if (europe_nation_gold(ctx->europe, ctx->col1, e) < price) {
    /* @UNFORTUNATE (0x16d0). */
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages, "UNFORTUNATE", NULL,
      "",
      body, sizeof(body)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", body
    );
    return;
  }
  if (ai_diplo_indian_alarm(ctx->col1, nation_id, target) >= 0x4b) {
    /* @ALREADYSMITE: tribe already in the war band with the target. */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_euro_name(target);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "ALREADYSMITE", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", body);
    return;
  }
  ai_contact_incite_warfare_chrome(ctx, e, nation_id, e, target);
  /* Raw +100: the French/Pocahontas halving now lives inside
   * ai_diplo_indian_alarm_delta, as in DOS 00f2 (2026-09-07d). */
  ai_contact_alarm_delta_00f2(ctx, nation_id, target, 100);
  europe_nation_gold_add(ctx->europe, ctx->col1, e, -(long)price); /* audit G3 */
}
/* ===================== Tools/gold demand pipeline & gift-or-demand economics (raw 2154) (ai_contact_nearest_colony .. ai_contact_gift_or_demand) ===================== */


/* Nearest Euro colony with warehouse tools ≥20 (mid demand tools arm). */
/*
 * The one "nearest active colony of `nation`" scan behind the three this file
 * used to hand-roll (audit AC-7). `min_tools` >= 0 also demands that much
 * TOOLS in stock; `continent` >= 0 restricts to that continent; `max_dist` is
 * the exclusive ceiling each caller's old `best_d` seed supplied. Returns the
 * colony-pool index or -1, and writes the winning distance to `*out_dist`
 * when a colony was found.
 */
int ai_contact_nearest_colony(
  const ColonizeTurnContext* ctx,
  int nation,
  int x,
  int y,
  int continent,
  int min_tools,
  int max_dist,
  int* out_dist
) {
  if (!ctx || !ctx->colonies) {
    return -1;
  }
  int best = -1;
  int best_d = max_dist;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    if (min_tools >= 0 && c->stock[COLONIZE_CARGO_TOOLS] < min_tools) {
      continue;
    }
    if (continent >= 0 && ctx->map && map_continent_id_at(ctx->map, c->x, c->y) != continent) {
      continue;
    }
    const int d = map_chebyshev(c->x, c->y, x, y);
    if (d < best_d) {
      best_d = d;
      best = ci;
    }
  }
  if (best >= 0 && out_dist) {
    *out_dist = best_d;
  }
  return best;
}

static ColonizeColony* ai_contact_nearest_tools_colony(
  ColonizeTurnContext* ctx,
  int e,
  int near_x,
  int near_y
) {
  if (!ctx || e < 0 || e > 3) {
    return NULL;
  }
  const int ci = ai_contact_nearest_colony(ctx, e, near_x, near_y, -1, 20, 99, NULL);
  return ci >= 0 ? &ctx->colonies->colonies[ci] : NULL;
}

/*
 * Nearest Euro Wagon Train with TOOLS hold ≥20 (GAME.TXT @INDIANWAGONS
 * reparations stand-in). Reach matches auto-trade wagon band (French 5 / else 4).
 */
static ColonizeUnit* ai_contact_nearest_tools_wagon(
  ColonizeTurnContext* ctx,
  int e,
  int near_x,
  int near_y,
  int* out_hold
) {
  if (out_hold) {
    *out_hold = -1;
  }
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return NULL;
  }
  const int max_dist = (e == 1) ? 5 : 4;
  int best_id = -1;
  int best_hold = -1;
  int best_d = 99;
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* u = &ctx->units->units[ui];
    if (!u->active || u->nation_id != e) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
    if (!units_type_is_wagon(ty) || ty->cargo <= 0) {
      continue;
    }
    const int dist = map_chebyshev(u->x, u->y, near_x, near_y);
    if (dist > max_dist || dist >= best_d) {
      continue;
    }
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (u->hold_goods_type[h] == COLONIZE_CARGO_TOOLS &&
          u->hold_goods_amount[h] >= 20) {
        best_d = dist;
        best_id = u->id;
        best_hold = h;
        break;
      }
    }
  }
  if (best_id < 0) {
    return NULL;
  }
  if (out_hold) {
    *out_hold = best_hold;
  }
  return units_get(ctx->units, best_id);
}

static int ai_contact_demand_can_pay_tools(
  ColonizeTurnContext* ctx,
  int e,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (ai_contact_nearest_tools_colony(ctx, e, near_x, near_y)) {
    return 1;
  }
  if (ai_contact_nearest_tools_wagon(ctx, e, near_x, near_y, NULL)) {
    return 1;
  }
  return other && other->tools >= 20;
}

static int ai_contact_demand_can_pay_gold(const ColonizeTurnContext* ctx, int e) {
  /* audit G3: single treasury — tribute demands hit the human. */
  return ctx && ctx->col1_ok && ctx->col1 && e >= 0 && e <= 3 &&
         europe_nation_gold(ctx->europe, ctx->col1, e) >= 50u;
}

/*
 * Shared 40..54 demand band gate (audit AC-4): must be met, and the pair
 * friction must sit in the mid band — below 40 there is nothing to appease,
 * at 55 and above the tribe refuses to talk at all. Same reduction as the
 * gift gate (smell #54): pair_friction dominates alarm_by_player[e], so that
 * disjunct is dead; the band itself is kept verbatim.
 */
static int ai_contact_demand_band_ok(
  ColonizeTurnContext* ctx,
  const ColonizeCol1Indian* ind,
  int nation_id,
  int e
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || e < 0 || e > 3) {
    return 0;
  }
  if (!ind->euro_diplo[e]) {
    return 0;
  }
  const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
  if (friction >= 55 || friction < 40) {
    ai_contact_refuse_chrome(ctx, e, nation_id, AI_POPUP_TAG_CONTACT_DEMAND, "", "demands");
    return 0;
  }
  return 1;
}

/*
 * Mid-band demand tools drain (−10 stock / wagon hold / unit tools) + friction −3.
 * Cite: FUN_5bfb_102a / 1092; GAME.TXT @INDIANWAGONS; indian_contact.md mid demand.
 */
int ai_contact_apply_demand_tools(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (!ai_contact_demand_band_ok(ctx, ind, nation_id, e)) {
    return 0;
  }
  ColonizeColony* c = ai_contact_nearest_tools_colony(ctx, e, near_x, near_y);
  int wag_hold = -1;
  ColonizeUnit* wag =
    c ? NULL : ai_contact_nearest_tools_wagon(ctx, e, near_x, near_y, &wag_hold);
  if (c) {
    c->stock[COLONIZE_CARGO_TOOLS] -= 10;
  } else if (wag && wag_hold >= 0) {
    wag->hold_goods_amount[wag_hold] -= 10;
    if (wag->hold_goods_amount[wag_hold] <= 0) {
      wag->hold_goods_amount[wag_hold] = 0;
      wag->hold_goods_type[wag_hold] = 0;
    }
  } else if (other && other->tools >= 20) {
    other->tools -= 10;
  } else {
    ai_contact_refuse_chrome(ctx, e, nation_id, AI_POPUP_TAG_CONTACT_DEMAND, "", "demands");
    return 0;
  }
  ai_contact_friction_decay(ind, ctx->col1, nation_id, e, 3);
  {
    char trib_fb[AI_POPUP_BODY_LEN];
    snprintf(
      trib_fb,
      sizeof(trib_fb),
      "Tribute paid; tensions ease with the %s.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "", trib_fb
    );
  }
  return 1;
}

/*
 * Mid-band demand gold drain (−15 when treasury ≥50) + friction −3.
 * Cite: indian_contact.md mid demand gold stand-in (tools short / player pick).
 */
int ai_contact_apply_demand_gold(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e
) {
  if (!ai_contact_demand_band_ok(ctx, ind, nation_id, e)) {
    return 0;
  }
  if (europe_nation_gold(ctx->europe, ctx->col1, e) < 50u) {
    ai_contact_refuse_chrome(ctx, e, nation_id, AI_POPUP_TAG_CONTACT_DEMAND, "", "demands");
    return 0;
  }
  europe_nation_gold_add(ctx->europe, ctx->col1, e, -15L); /* audit G3 */
  ai_contact_friction_decay(ind, ctx->col1, nation_id, e, 3);
  {
    char trib_fb[AI_POPUP_BODY_LEN];
    snprintf(
      trib_fb,
      sizeof(trib_fb),
      "Tribute paid; tensions ease with the %s.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "", trib_fb
    );
  }
  return 1;
}

/*
 * Human Demand amount CHOICE (tools vs gold) when mid-band and purse allows.
 * Returns 1 if enqueued. Cite: FUN_5bfb_102a / 1092; gift amount CHOICE mirror.
 */
int ai_contact_enqueue_demand_amount_choice(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (!ai_contact_choice_ctx_ready(ctx, e)) {
    return 0;
  }
  const int can_tools = ai_contact_demand_can_pay_tools(ctx, e, other, near_x, near_y);
  const int can_gold = ai_contact_demand_can_pay_gold(ctx, e);
  if (!can_tools && !can_gold) {
    return 0;
  }
  const char* labels[2];
  int ids[2];
  int n = 0;
  if (can_tools) {
    labels[n] = "Pay tools (10)";
    ids[n] = AI_CONTACT_DEMAND_TOOLS;
    n++;
  }
  if (can_gold) {
    labels[n] = "Pay gold (15)";
    ids[n] = AI_CONTACT_DEMAND_GOLD;
    n++;
  }
  char body[AI_POPUP_BODY_LEN];
  snprintf(
    body,
    sizeof(body),
    "The %s demand tribute. How do you pay?",
    ai_contact_tribe_name(nation_id)
  );
  return ai_contact_enqueue_choice(
    ctx, AI_POPUP_TAG_CONTACT_DEMAND, e, nation_id, 0, body, labels, ids, n
  );
}

/*
 * FUN_4d56_2154 meet economics: dual 16-word tables ask[] (DS:0x9e58) /
 * bid[] (DS:0x9e78). Phases 1–5 from viceroy_unpacked.c 81743–82057.
 * Cover: tribe-local 25-cell mask (full relative ring; 281f_0ce0 OPEN).
 * Terrain: map_dos_terr_class_at (281f_078c). Divisor *(0x8d52/−0x69d6) →
 * head.difficulty (0..4). Cite: indian_meet_scoring_2154.md.
 */

static int ai_contact_2154_signed_quarter_double(int x) {
  /* Decomp: abs via xor, >>2, restore sign, *2. */
  const unsigned u = (unsigned)x;
  const unsigned sign = (unsigned)(x >> 15);
  const unsigned absv = (u ^ sign) - sign;
  const unsigned q = absv >> 2;
  return (int)(((q ^ sign) - sign) * 2u);
}

static int ai_contact_2154_clamp0_50(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 0x32) {
    return 0x32;
  }
  return v;
}

int ai_contact_meet_economics_2154(
  ColonizeTurnContext* ctx,
  int indian_nation,
  const ColonizeCol1Tribe* tribe,
  AiContactMeetEcon2154* out
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->map || !tribe || !out ||
      indian_nation < 4 || indian_nation > 11) {
    return 0;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[indian_nation - 4];
  memset(out, 0, sizeof(*out));

  /* Phase 1: tribe-local 25-cell cover from colony relative overlap. */
  uint8_t cover[25];
  memset(cover, 0, sizeof(cover));
  const int tx = (int)tribe->x;
  const int ty = (int)tribe->y;
  if (ctx->colonies) {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      const ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      for (int ly = 0; ly < 5; ++ly) {
        for (int lx = 0; lx < 5; ++lx) {
          const int rel_x = tx - c->x + lx;
          const int rel_y = ty - c->y + ly;
          const int abs_x = rel_x - 2;
          const int abs_y = rel_y - 2;
          if (abs_x < 0 || abs_y < 0 || abs_x >= ctx->map->width ||
              abs_y >= ctx->map->height) {
            continue;
          }
          if (rel_x < 0 || rel_x > 4 || rel_y < 0 || rel_y > 4) {
            continue;
          }
          /*
           * FUN_281f_0ce0 -> FUN_15eb_06a6 -> FUN_15eb_05e2: ring-slot lookup
           * then colony+0x70 worker byte read; DOS compares the returned byte
           * as *signed* (0xff == "no worker" == not-covered). Decomp special-
           * cases the colony's own tile (lx==2 && ly==2, i.e. dx=dy=0) as
           * always covered; every other covered cell must be one of the 8
           * immediate work-ring tiles (N,E,S,W,NW,NE,SE,SW — colony.h order)
           * *and* currently worked. Col1 colony tiles[8..19] (the raw-save
           * 20-slot ring beyond the 8 immediate directions) never carry a
           * worker in either DOS or col1_bridge.c, so those cells are simply
           * never covered — an unworked adjacent tile stays "free" land for
           * the tribe scorer, matching the DOS byte-for-byte. Cite:
           * indian_meet_scoring_2154.md Phase 1 (281f_0ce0 work-slot OPEN).
           */
          const int dx = lx - 2;
          const int dy = ly - 2;
          if (dx == 0 && dy == 0) {
            cover[ly * 5 + lx] = 1;
            continue;
          }
          for (int dir = 0; dir < 8; ++dir) {
            if (MAP_DIR8_DX[dir] == dx && MAP_DIR8_DY[dir] == dy && c->tiles[dir] >= 0) {
              cover[ly * 5 + lx] = 1;
              break;
            }
          }
        }
      }
    }
  }

  /* Phase 2–3: tribe ±2 terr_class buckets (skip covered local cells). */
  int local_a0 = 0;
  int local_9e = 0;
  int local_9c = 0;
  int local_5e = 0;
  int local_74 = 0;
  int local_5a = 0;
  int local_6a = 0;
  int local_6e = 0;
  int local_58 = 0;
  int local_7a = 0;
  int local_68 = 0;
  int local_7e = 0;
  int local_66 = 0;

  for (int y = ty - 2; y <= ty + 2; ++y) {
    for (int x = tx - 2; x <= tx + 2; ++x) {
      if (x < 0 || y < 0 || x >= ctx->map->width || y >= ctx->map->height) {
        continue;
      }
      const int lx = x - tx + 2;
      const int ly = y - ty + 2;
      if (lx < 0 || lx > 4 || ly < 0 || ly > 4) {
        continue;
      }
      if (cover[ly * 5 + lx]) {
        continue;
      }
      const int terr = map_dos_terr_class_at(ctx->map, x, y) & 31;
      if (terr == 0x1b) {
        local_a0++;
      }
      if (terr == 0x1c) {
        local_9c++;
      }
      if (terr == 0x18) {
        local_5a += 4;
      }
      if (!(((terr < 8) || (terr > 15)) && ((terr < 16) || (terr > 23)))) {
        /* Forest bands 8..15 / 16..23. */
        local_6e++;
        if (terr >= 8 && terr <= 15) {
          local_9e = terr - 8;
        }
        if (terr >= 16 && terr <= 23) {
          local_9e = terr - 16;
        }
        if (local_9e < 3) {
          local_5e++;
          local_5a += 2;
        } else {
          local_74++;
          local_6a++;
          if (local_9e == 5) {
            local_58 += 2;
          }
          if (local_9e == 4) {
            local_7e += 2;
          }
          if (local_9e == 3) {
            local_68 += 2;
          }
        }
      } else if (terr == 0x19 || terr == 0x1a) {
        local_7a += (int)ind->tech + 1;
        while (local_7a > 2) {
          local_7a -= 3;
          local_6e += 2;
        }
      } else if (terr < 8) {
        if (terr == 5) {
          local_58 += 4;
        }
        if (terr == 7) {
          local_58 += 2;
        }
        if (terr == 4) {
          local_7e += 4;
        }
        if (terr == 6) {
          local_7e += 2;
        }
        if (terr == 3) {
          local_68 += 4;
        }
        if (terr == 0) {
          local_66 += 2;
        }
        if (terr == 2) {
          local_68 += 1;
          local_6e += 2;
        }
        if (terr < 2) {
          if (terr == 1) {
            local_6a += 4;
          } else if (terr == 0) {
            local_5a += 3;
          }
        } else if (terr < 6) {
          local_6e += 3;
          if ((terr & 4) != 0) {
            local_6a += 2;
          } else {
            local_5a += 2;
          }
        } else {
          local_66 += 1;
          local_6e += 2;
        }
      }
    }
  }

  /* Phase 4: write ask/bid words from tech, pop, buckets, indian fields. */
  const int pop = (int)tribe->population;
  const int pop1 = pop + 1;
  int tech = (int)ind->tech;
  if (tech > 6) {
    tech = 6; /* avoid (7-tech) zero / negative */
  }
  const int diff = (int)ctx->col1->head.difficulty; /* DS 0x8d52/−0x69d6 stand-in */
  int diff_div = diff;
  if (diff_div < 1) {
    diff_div = 1;
  }

  out->bid[0] = (int16_t)(((tech + pop1) * local_6e) / (7 - tech));
  out->ask[0] = (int16_t)((pop1 * pop1 * 4) >> (1 < tech ? 1 : 0));
  if (tech != 0) {
    if (tech > 1) {
      out->bid[7] = (int16_t)((int)ind->hill_silver_bid_bonus / diff_div);
      int a0_shift = local_a0 << 2;
      if (tech > 2) {
        a0_shift = local_a0 << 3;
      }
      out->bid[7] = (int16_t)((int)out->bid[7] + a0_shift);
    }
    if (ind->tech != 0) {
      out->bid[6] =
        (int16_t)((int)out->bid[6] + local_9c * 2 + local_a0 + local_66);
    }
  }
  out->bid[4] =
    (int16_t)((int)out->bid[4] + (local_5e * 2 + (local_74 >> 1)) / (tech + 1));
  out->bid[12] =
    (int16_t)ai_contact_2154_signed_quarter_double(((int)out->bid[4] + tech) * 2);
  out->bid[2] = (int16_t)((int)out->bid[2] + local_7e);
  out->bid[1] = (int16_t)((int)out->bid[1] + local_58);
  out->ask[11] =
    (int16_t)((tech + pop1) * pop1 + (local_6a >> 1) + local_5a);
  out->bid[3] = (int16_t)((int)out->bid[3] + local_68);
  {
    const int b11 = ai_contact_2154_signed_quarter_double((tech + (int)out->bid[3]) * 2);
    out->bid[11] = (int16_t)b11;
    out->ask[3] = (int16_t)(b11 + local_6a);
  }
  out->ask[2] = (int16_t)((6 - tech) * pop1 + local_5a * 2 + 5);
  out->ask[10] = (int16_t)(((pop1 * 2 - tech) + 7) * 2);
  out->ask[12] = (int16_t)(local_5a * 8 + (int)out->bid[4]);
  out->ask[9] = (int16_t)(((tech * 2 + pop1) * 2 + local_6a) * 2);
  out->ask[13] = (int16_t)((tech + 2) * (pop + 4) + 8);
  {
    const int sh = ((local_5a >> 1) + 1) & 31;
    out->ask[14] = (int16_t)((tech * pop1) << sh);
  }
  out->ask[15] = (int16_t)((-tech - ((int)ind->muskets - 7)) * 4);
  out->bid[8] =
    (int16_t)((int)ind->horse_breeding / ((diff >> 1) + 1));
  out->ask[8] = (int16_t)((-tech - ((int)ind->horse_herds - 9)) * 4);
  out->bid[15] = 0;

  /* Phase 5: clamp ask 0..0x32; capital mix; tons mix; half-cross. */
  for (int i = 0; i < 16; ++i) {
    out->ask[i] = (int16_t)ai_contact_2154_clamp0_50((int)out->ask[i]);
  }
  if (tribe->state.capital) {
    for (int i = 0; i < 8; ++i) {
      out->ask[i] = (int16_t)((int)out->ask[i] << 1);
    }
    for (int i = 13; i < 16; ++i) {
      out->ask[i] = (int16_t)((int)out->ask[i] + ((int)out->ask[i] >> 1));
    }
    for (int i = 7; i < 16; ++i) {
      out->bid[i] = (int16_t)((int)out->bid[i] << 1);
    }
  }
  for (int i = 0; i < 16; ++i) {
    const int bid0 = (int)out->bid[i];
    const int ask0 = (int)out->ask[i];
    const int tons = (int)ind->tons[i];
    if (tons < 1) {
      if (tons < 0) {
        out->bid[i] = (int16_t)(bid0 + ((tons + 0x32) / 100) * 2);
      }
    } else {
      out->ask[i] = (int16_t)(((-0x32 - tons) / 100) * 2 + ask0);
    }
    {
      const int bid1 = (int)out->bid[i];
      out->bid[i] = (int16_t)(bid1 - ((int)out->ask[i] >> 1));
      {
        const int floor_b = (bid0 > 0) ? 1 : 0;
        if ((int)out->bid[i] <= floor_b) {
          out->bid[i] = (int16_t)floor_b;
        }
      }
      out->ask[i] = (int16_t)((int)out->ask[i] - (bid1 >> 1));
      {
        const int floor_a = (ask0 > 0) ? 1 : 0;
        if ((int)out->ask[i] < floor_a) {
          out->ask[i] = (int16_t)floor_a;
        }
      }
    }
  }
  return 1;
}

/*
 * Gift / demand (5bfb_102a / 1092 via ai_popup; VGA PARKED).
 * 2154 tables: gift Generous when ask[0]-bid[0]≥1, gold≥0x4b, thin RNG;
 * else Large when gold≥20. Demand: ask[0]<bid[0] → gold-first else tools-first.
 * Cite: indian_meet_scoring_2154.md; FUN_5bfb after 2a1f_0434.
 */
void ai_contact_gift_or_demand(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !other || e < 0 || e > 3) {
    return;
  }
  if (!ind->euro_diplo[e]) {
    return;
  }
  const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
  const int human = ai_contact_euro_is_human(ctx, e);
  /*
   * Same ≥55 gate as refuse-talk/teach: alarmed → no gift and no demand
   * payoff (no invented gold penalties). Cite: fandom Alarm — refuse trade.
   * The `|| alarm_by_player[e] >= 55` disjunct this gate used to carry was
   * dead: ai_contact_pair_friction seeds friction FROM alarm_by_player[e] and
   * only raises it, so friction >= alarm always (smell #54, reduced here by
   * audit D10 2026-09-10).
   * The message band below is a different quantity on purpose: it maxes the
   * TRIBE friction rows only, without the alarm_by_player seed, so the
   * gift-band (<40) "refuse gifts" wording stays reachable even when the
   * pair value that opened this arm came from alarm alone.
   */
  if (friction >= 55) {
    if (human) {
      int tribe_fr = 0;
      if (ctx->col1->tribe) {
        for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
          const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
          if ((int)t->nation_id == nation_id && (int)t->alarm[e].friction > tribe_fr) {
            tribe_fr = (int)t->alarm[e].friction;
          }
        }
      }
      const int demand_band = tribe_fr >= 40;
      ai_contact_refuse_chrome(
        ctx,
        e,
        nation_id,
        demand_band ? AI_POPUP_TAG_CONTACT_DEMAND : AI_POPUP_TAG_CONTACT_GIFT,
        "", /* title param is unused by ai_contact_human_chrome */
        demand_band ? "demands" : "gifts"
      );
    }
    return; /* alarmed / very high — raids handle hostility; no invented gold penalty */
  }

  /* audit G3: single treasury — this whole band is human-reachable, so the
   * three purse gates below read through the accessor rather than the record
   * the human's live purse only reaches at save time. */
  const uint32_t purse = europe_nation_gold(ctx->europe, ctx->col1, e);

  AiContactMeetEcon2154 econ;
  memset(&econ, 0, sizeof(econ));
  int have_econ = 0;
  const ColonizeCol1Tribe* sample = NULL;
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id == nation_id) {
        sample = t;
        break;
      }
    }
  }
  if (sample) {
    have_econ = ai_contact_meet_economics_2154(ctx, nation_id, sample, &econ);
  }
  const int ask0 = have_econ ? (int)econ.ask[0] : 0;
  const int bid0 = have_econ ? (int)econ.bid[0] : 0;
  const int delta = ask0 - bid0;

  /* Low friction gift / tribute (2154 ask−bid + gold≥0x4b → Generous). */
  if (friction < 40) {
    /* Cannot pay −10 gift drain → refuse with status (widgets unparked). */
    if (purse < 10u) {
      ai_contact_refuse_chrome(ctx, e, nation_id, AI_POPUP_TAG_CONTACT_GIFT, "Gift", "gifts");
      return;
    }
    if (purse < 20u) {
      return; /* mid purse: skip silent (needs ≥20 band to auto-gift Large) */
    }
    /*
     * DOS 5bfb after 2154: Generous when delta≥1, gold≥0x4b, and delta≥RNG
     * (281f_04d4 stand-in). Else Large. Cite: indian_meet_scoring_2154.md.
     */
    int generous = 0;
    if (delta >= 1 && purse >= 0x4bu) {
      ColonizeDosRng local;
      ai_contact_local_rng(ctx, nation_id, &local);
      ColonizeDosRng* rng = ctx->rng ? ctx->rng : &local;
      const int roll = dos_rng_range(rng, 1, 100);
      if (delta >= roll) {
        generous = 1;
      }
    }
    if (generous) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 20u, 3);
      return;
    }
    ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 10u, 2);
    return;
  }

  /*
   * Mid friction (40–54) demand / payoff; ≥55 refused above.
   * ask[0] < bid[0] → gold-first; else tools-first (DOS LAB_5bfb_096c shape).
   */
  {
    const int gold_first = have_econ && ask0 < bid0;
    if (gold_first) {
      if (ai_contact_demand_can_pay_gold(ctx, e)) {
        ai_contact_apply_demand_gold(ctx, ind, nation_id, e);
        return;
      }
      if (ai_contact_demand_can_pay_tools(ctx, e, other, near_x, near_y)) {
        ai_contact_apply_demand_tools(ctx, ind, nation_id, e, other, near_x, near_y);
        return;
      }
    } else {
      if (ai_contact_demand_can_pay_tools(ctx, e, other, near_x, near_y)) {
        ai_contact_apply_demand_tools(ctx, ind, nation_id, e, other, near_x, near_y);
        return;
      }
      if (ai_contact_demand_can_pay_gold(ctx, e)) {
        ai_contact_apply_demand_gold(ctx, ind, nation_id, e);
        return;
      }
    }
  }
  {
    ai_contact_refuse_chrome(ctx, e, nation_id, AI_POPUP_TAG_CONTACT_DEMAND, "", "demands");
  }
}
/* ===================== Visit mood, demand gating & beg-food flow (ai_contact_beg_food_gift .. ai_contact_try_village_beg_food) ===================== */


/*
 * FUN_5bfb_022e already-met Brave/Euro adjacency accept/refuse
 * (viceroy_unpacked.c:96751-96827) — the mechanic settlement_record_8d4a.md
 * flagged as blocked on an accept/decline sign ambiguity. Resolved
 * 2026-08-14 via live user gameplay testimony captured during this
 * session's DOSBox-X live-capture pass (see that doc's "SIGN CONVENTION
 * RESOLVED" section): giving food improves tribe relations, refusing
 * worsens them. GAME.TXT `@INDIANBEGFOOD` ("...will our brothers of
 * {colony} share the bounty...") is the matching text — confirmed zero
 * references anywhere in this project before this pass, a genuine unwired
 * gap, not an approximation being replaced.
 *
 * Real DOS trigger: tribe's `2154` scorer `ask[0]>bid[0]`, a candidate
 * Euro colony's relevant stock `> 0x4a` (74) — matches the user's own
 * independently-reported trigger condition ("only ever seen with
 * substantial food stores") — then `dos_rng_range(1,100) <= (ask-bid)`.
 * Which branch is which (re-read off the decompile 2026-08-31, bugs.md):
 * the choice returns `local_c`, and it is `local_c == 2` — the second
 * GAME.TXT row, "We offer you {%NUMBER0} of our {%NUMBER1 food}" — that
 * subtracts `colony->food >> 1`. So the *half is the voluntary gift*, and
 * refusing costs no food at all; the earlier reading had the two branches'
 * effects swapped, so accepting handed over a quarter and angered them
 * while refusing seized half and calmed them. Corrected here.
 *
 * On accept (`local_c == 2`): colony food −= half, the begging
 * settlement's alarm word toward this European is zeroed, and the nation
 * alarm is walked down 5 at a time from −5 (−10 off a capital) until it
 * sits below 0x47 — the floor-loop that was previously "not fully
 * reconciled" is exactly that `while (alarm + d >= 0x47) d -= 5;`.
 * On refuse (`local_c == 1`): no food moves, the settlement's alarm word
 * goes ×1.5 (`w += w >> 1`), and the nation alarm rises by
 * `((difficulty + 1) >> 1) + 1`, doubled off a capital.
 *
 * Still approximated: DOS halves that refuse bump when its upstream
 * "calm enough" roll passed (`bVar5`), part of the demand/threat selector
 * this port drives differently; and the DOS AI-vs-human branch-selection
 * nuance (a `param_1==2` hardcode in the human-controlled arm wasn't
 * reconciled with the user's own experience of a real Give/Refuse choice
 * regardless of nation played — implemented as a real CHOICE for any
 * human nation instead).
 */
/*
 * @INDIANBEGFOOD gift: exactly half the colony's store, DOS's own
 * `iVar15 = colony->food >> 1` in FUN_5bfb_022e (colony pointer DS:0x8542,
 * food at +0x9a). The same half is what NUMBER0 names in the accept row
 * ("We offer you {%NUMBER0} of our {%NUMBER1 food}") and what accepting
 * actually hands over, so label and effect read it from here rather than
 * computing it twice. Was a quarter, which under-asked (bugs.md).
 */
static int ai_contact_beg_food_gift(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  const int have = c->stock[COLONIZE_CARGO_FOOD];
  int gift = have >> 1;
  if (gift < 0) {
    gift = 0;
  }
  if (gift > have) {
    gift = have;
  }
  return gift;
}

/*
 * FUN_5bfb_022e entry mood — `local_10` / `bVar5` / `bVar6`, viceroy 96707-96731.
 *
 *   iVar16 = village attitude word toward e   (*(0x8d4a + e*2 + 10))
 *   local_10 = (0x7f < iVar16) || contact_state[e] == 1
 *   if (local_10) { if (rng(1,0x80) < iVar16 - 0x80) abort the whole visit; }
 *   bVar5 = max(0, alarm - 0x19) * 4 + iVar16 <= rng(1, 0x148)
 *   bVar6 = (local_10 == 0) && bVar5
 *
 * and then the demand half is `if (!bVar6) { LAB_5bfb_0def ... }`.
 *
 * bugs.md ("Incas ... I have no colonies over there ... demanding stuff"):
 * the port split 022e's two halves into two functions, and only the gift half
 * evaluated bVar6. The @INDIANWAGONS flavor of the demand half needs no colony
 * at all, so for a Brave that wandered up to a lone Wagon Train the gift half
 * bailed early (no colony of `e` anywhere near) and the demand half then ran
 * with NO mood gate — a tribe at alarm 0 whose village word is 0 demanded
 * reparations, where DOS's bVar5 (`0 <= rng(1,0x148)`, always true) makes
 * bVar6 true and skips LAB_5bfb_0def outright.
 *
 * DOS draws once per encounter, so the gift arm publishes its verdict here and
 * the demand arm reuses it instead of re-rolling; when the gift arm never got
 * as far as the draw (no colony, or the hostile `local_10` latch, which DOS
 * reaches with its own extra roll), the demand arm runs the full DOS sequence
 * itself.
 */

AiContactVisitMood ai_contact_s_visit_mood[8][4];

/* The verdict belongs to ONE encounter: same turn, same visiting Brave. */
void ai_contact_visit_mood_publish(
  const ColonizeTurnContext* ctx, int nation_id, int e, int brave_id, int bvar6
) {
  if (nation_id < 4 || nation_id > 11 || e < 0 || e > 3) {
    return;
  }
  AiContactVisitMood* m = &ai_contact_s_visit_mood[nation_id - 4][e];
  m->valid = 1;
  m->bvar6 = bvar6;
  m->turn = (ctx && ctx->turn_number) ? (int)*ctx->turn_number : -1;
  m->brave_id = brave_id;
}

void ai_contact_visit_mood_clear(int nation_id, int e) {
  if (nation_id < 4 || nation_id > 11 || e < 0 || e > 3) {
    return;
  }
  ai_contact_s_visit_mood[nation_id - 4][e].valid = 0;
}

/*
 * Answers DOS's `if (!bVar6)` for the demand half. Returns 1 when DOS would
 * fall into LAB_5bfb_0def, 0 when the encounter is generous (bVar6) or the
 * hostile roll aborted the visit.
 */
int ai_contact_visit_demand_allowed(
  ColonizeTurnContext* ctx,
  const ColonizeCol1Indian* ind,
  const ColonizeCol1Tribe* t,
  int nation_id,
  int e,
  int brave_id,
  int alarm
) {
  if (nation_id >= 4 && nation_id <= 11 && e >= 0 && e <= 3) {
    const AiContactVisitMood* m = &ai_contact_s_visit_mood[nation_id - 4][e];
    const int turn = (ctx && ctx->turn_number) ? (int)*ctx->turn_number : -1;
    if (m->valid && m->turn == turn && m->brave_id == brave_id) {
      return m->bvar6 ? 0 : 1;
    }
  }
  if (!ctx || !ctx->rng || !ind || !t) {
    return 0;
  }
  const int word = col1_tribe_attitude(t, e);
  const int local_10 = (word > 0x7f) || (ind->contact_state[e] == 1);
  if (local_10) {
    if (dos_rng_range(ctx->rng, 1, 0x80) < word - 0x80) {
      return 0; /* goto LAB_5bfb_1005 — no visit at all */
    }
  }
  const int roll = dos_rng_range(ctx->rng, 1, 0x148);
  int over = alarm - 0x19;
  if (over < 0) {
    over = 0;
  }
  const int bvar5 = (over * 4 + word) <= roll;
  const int bvar6 = (local_10 == 0) && bvar5;
  ai_contact_visit_mood_publish(ctx, nation_id, e, brave_id, bvar6);
  return bvar6 ? 0 : 1;
}

/*
 * FUN_5bfb_022e already-met arm run from FUN_5bfb_3180 on the BRAVE's own
 * step (465b commit tail): raw 96745-96760. The mood rolls happen right
 * there, on the shared stream, before the tribe's next Brave acts — the
 * post-pulse gift/demand passes then consume the published verdict instead
 * of rolling again. Returns 1 when a roll sequence ran.
 */
int ai_contact_visit_step_roll(ColonizeTurnContext* ctx, int nation_id, int e, int brave_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || !ctx->rng || nation_id < 4 ||
      nation_id > 11 || e < 0 || e > 3) {
    return 0;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  const ColonizeUnit* brave = units_get_const(ctx->units, brave_id);
  if (!brave || brave->home_tribe_id < 0 ||
      brave->home_tribe_id >= (int)ctx->col1->head.tribe_count) {
    return 0;
  }
  const ColonizeCol1Tribe* t = &ctx->col1->tribe[brave->home_tribe_id];
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
  if (alarm > 0x4a) {
    return 0; /* raw 96752: no peaceful visit at all */
  }
  const int word = col1_tribe_attitude(t, e);
  const int local_10 = (word > 0x7f) || (ind->contact_state[e] == 1);
  if (local_10) {
    if (dos_rng_range(ctx->rng, 1, 0x80) < word - 0x80) {
      ai_contact_visit_mood_publish(ctx, nation_id, e, brave_id, 0);
      return 1; /* LAB_5bfb_1005 */
    }
  }
  const int roll = dos_rng_range(ctx->rng, 1, 0x148);
  int over = alarm - 0x19;
  if (over < 0) {
    over = 0;
  }
  const int bvar5 = (over * 4 + word) <= roll;
  int bvar6 = (local_10 == 0) && bvar5;
  if (bvar6 && alarm > 0x31) {
    bvar6 = 0;
    ind->contact_state[e] = 2;
  }
  ai_contact_visit_mood_publish(ctx, nation_id, e, brave_id, bvar6);
  return 1;
}

/*
 * `home_tribe` is the VISITING Brave's own settlement — DOS binds it with
 * FUN_281f_0a4c(unit+0x314a) before the encounter body runs (viceroy 96706),
 * and every settlement-scoped effect below (the attitude-word zero / ×1.5,
 * the capital doubling) reads that record. smell #74: this used to re-scan
 * for the FIRST tribe of the nation, so a visit by a satellite village
 * zeroed the capital's word and charged the capital's doubled refuse cost.
 * -1 keeps the old first-tribe fallback for a caller with no visit record.
 */
void ai_contact_apply_beg_food(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  int colony_id,
  int home_tribe,
  int accept
) {
  (void)ind;
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->colonies || e < 0 || e > 3) {
    return;
  }
  if (colony_id < 0 || colony_id >= COLONIZE_COLONIES_MAX) {
    return;
  }
  ColonizeColony* c = &ctx->colonies->colonies[colony_id];
  if (!c->active || c->nation_id != e) {
    return;
  }
  int capital = 0;
  ColonizeCol1Tribe* target_tribe = NULL;
  if (ctx->col1->tribe) {
    if (home_tribe >= 0 && home_tribe < (int)ctx->col1->head.tribe_count &&
        (int)ctx->col1->tribe[home_tribe].nation_id == nation_id) {
      target_tribe = &ctx->col1->tribe[home_tribe];
      capital = target_tribe->state.capital != 0;
    } else {
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id == nation_id) {
          target_tribe = t;
          capital = t->state.capital != 0;
          break;
        }
      }
    }
  }
  ai_contact_bind_names(ctx);
  if (accept) {
    /*
     * DOS FUN_5bfb_022e, `local_c == 2` (the "We offer you ..." row): the
     * colony loses the half it just offered, the begging settlement's own
     * alarm word toward this European is zeroed outright, and the nation
     * alarm is walked down in steps of 5 (base -5, -10 from a capital)
     * until it lands under 0x47 --- `while (alarm + d >= 0x47) d -= 5;`.
     * Giving cannot make them angrier; the earlier port had the accept and
     * refuse effects the wrong way round (bugs.md).
     */
    const int gift = ai_contact_beg_food_gift(c);
    c->stock[COLONIZE_CARGO_FOOD] -= gift;
    if (c->stock[COLONIZE_CARGO_FOOD] < 0) {
      c->stock[COLONIZE_CARGO_FOOD] = 0;
    }
    if (target_tribe) {
      col1_tribe_attitude_set(target_tribe, e, 0);
    }
    int d = capital ? -10 : -5;
    const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
    /* Bounded: d only ever falls, and alarm is a 0..100 byte. */
    for (int guard = 0; guard < 32 && alarm + d >= 0x47; ++guard) {
      d -= 5;
    }
    ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, -d);
    if (ctx->status && ctx->status_size) {
      snprintf(
        ctx->status, ctx->status_size, "We share %d food with the %s.", gift,
        ai_contact_tribe_name(nation_id)
      );
    }
  } else {
    /*
     * DOS `local_c == 1` (the "we gave at the office" row): refusing costs
     * no food at all --- the tribe begs, it does not raid. What it does is
     * multiply the settlement's alarm word by 1.5 (`w += w >> 1`) and add
     * `((difficulty + 1) >> 1) + 1` to the nation alarm, doubled off a
     * capital. (DOS also halves that add when the upstream "calm enough"
     * roll passed; that roll is part of the demand/threat selector this
     * port drives differently, so it is not modelled here.)
     */
    if (target_tribe) {
      const int w = col1_tribe_attitude(target_tribe, e);
      col1_tribe_attitude_set(target_tribe, e, w + (w >> 1));
    }
    int d = (((int)ctx->col1->head.difficulty + 1) >> 1) + 1;
    if (capital) {
      d *= 2;
    }
    ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, -d);
    if (ctx->status && ctx->status_size) {
      snprintf(
        ctx->status, ctx->status_size, "We turn the %s away empty-handed.",
        ai_contact_tribe_name(nation_id)
      );
    }
  }
}

/*
 * "A Brave walked up to this colony this turn."
 *
 * DOS never polls standing adjacency: FUN_5bfb_022e is reached from
 * FUN_465b's move tail (FUN_281f_0984 → FUN_5bfb_3180) for the tile a unit
 * just stepped beside, so a Brave that has been parked next to a colony for
 * twenty turns raises nothing, while one that walks over raises a visit.
 * The Linux native pulse commits its steps inline and runs the contact arms
 * once per nation afterwards (ai.c §9), so this reconstructs the trigger
 * from the pulse's recorded pre-move tile: the Brave must have moved this
 * turn AND not already have been adjacent to this colony before it moved.
 *
 * This is the port's one adaptation of the trigger and it matters: without
 * it every Brave loitering beside a colony gifted it goods every few turns,
 * which broke the DOS colony-production goldens (New Amsterdam +13 ore).
 */
int ai_contact_brave_walked_up_to(const ColonizeUnit* brave, int cx, int cy) {
  if (!brave) {
    return 0;
  }
  int ox = 0;
  int oy = 0;
  if (!ai_native_brave_turn_origin(brave->id, &ox, &oy)) {
    return 0; /* the pulse never touched this unit this turn */
  }
  if (ox == brave->x && oy == brave->y) {
    return 0; /* stood still — DOS runs no move tail, so no encounter */
  }
  if (abs(ox - cx) <= 1 && abs(oy - cy) <= 1) {
    return 0; /* already beside this colony before the step */
  }
  return 1;
}

static int ai_contact_beg_food_pending(const AiPopupState* st) {
  return ai_popup_pending(st, AI_POPUP_TAG_CONTACT_BEGFOOD, -1, AI_POPUP_KEY_ANY, 0, 0) ? 1 : 0;
}

/*
 * LAB_5bfb_0def, the demand half's OTHER two sites — defined after the
 * −0x7b44 price row it needs (see ai_contact_try_village_reparations).
 * DOS runs @INDIANBEGFOOD (0x181c) first and only falls into LAB_5bfb_0def
 * when that block did not resolve the visit, which is why the port hangs
 * it off this function's tail rather than off a new ai.c call site.
 */
void ai_contact_try_village_reparations(ColonizeTurnContext* ctx, int nation_id);

/*
 * Trigger side of the above — once per Indian nation's §9 (post-pulse,
 * not inside the seed-100-sensitive quiet 14fe pulse), pick the first
 * already-met Euro colony with food stock > 74 and enough tribe/Euro
 * economic pressure (2154 ask>bid, RNG-gated) to beg. Human gets a real
 * Give/Refuse CHOICE (`@INDIANBEGFOOD`); AI Euro nations auto-accept
 * when it can spare the food (matches this file's established
 * AI-defaults-generous convention for gift-shaped decisions elsewhere).
 */
void ai_contact_try_village_beg_food(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->colonies || !ctx->col1->tribe || !ctx->rng) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  if (ctx->ai_popups && ai_contact_beg_food_pending(ctx->ai_popups)) {
    return;
  }
  /*
   * bugs.md: DOS begs at the colony a Brave actually walked next to — the
   * tribe cannot ask from across the map. The walked-up trigger below plus
   * the per-turn contact_state latch are DOS's whole pacing (see the visit
   * pacing note at the top of this file); there is no turn cooldown.
   */
  for (int e = 0; e < 4; ++e) {
    if (!ind->euro_diplo[e]) {
      continue;
    }
    /*
     * smell #73: ONE gate for the whole 022e encounter, not two. DOS reads
     * the alarm once at entry (`iVar9 = FUN_281f_030c(...)`, viceroy 96615)
     * and bails the entire visit — gift half and demand/beg half alike — at
     * `if (0x4a < iVar9) goto LAB_5bfb_1005;` (viceroy 96716). The port had
     * `>= 55` here against the gift arm's `> 0x4a`, so alarm 55..74 produced
     * no visit at all where DOS still runs the demand half.
     */
    if (ai_diplo_indian_alarm(ctx->col1, nation_id, e) > 0x4a) {
      continue;
    }
    /*
     * DOS's demand half is latched off once this pair has resolved a generous
     * visit *this turn*: `if (contact_state != 2 && (colony || wagon))` guards
     * LAB_5bfb_0def, and state 1 (a past demand) symmetrically disables the
     * gift arm (`local_10`).
     *
     * The latch is per-turn, NOT permanent: the Indian mid-pass zeroes all 32
     * contact_state entries at the top of every turn (DOS FUN_4d56_1b3a,
     * viceroy 81704-81707, `word[(i*0x27 + j)*2 + 0x5b04] = 0`, ported as
     * ai_indian_midpass_clear_tables() in ai.c). So each tribe/Euro pair
     * resolves at most one gift-or-beg visit per turn, then is eligible again
     * next turn — which is why DOS villages keep visiting instead of going
     * permanently quiet.
     */
    if (ind->contact_state[e] == 2) {
      continue;
    }
    int best_ci = -1;
    int visit_brave_id = -1;
    int visit_home_tribe = -1;
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id != e || c->stock[COLONIZE_CARGO_FOOD] <= 0x4a) {
        continue;
      }
      bool brave_adjacent = false;
      /* Slot walk — `ui` is not a unit id; see ai_contact_land_combat_sum. */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX && !brave_adjacent; ++ui) {
        const ColonizeUnit* bu = &ctx->units->units[ui];
        if (!bu->active || bu->nation_id != nation_id || !units_is_on_map(bu)) {
          continue;
        }
        if (abs(bu->x - c->x) > 1 || abs(bu->y - c->y) > 1) {
          continue;
        }
        /* Same DOS move-tail trigger as the gift half — see the helper. */
        if (!ai_contact_brave_walked_up_to(bu, c->x, c->y)) {
          continue;
        }
        brave_adjacent = true;
        visit_brave_id = bu->id;
        visit_home_tribe = bu->home_tribe_id;
      }
      if (!brave_adjacent) {
        continue;
      }
      best_ci = ci;
      break;
    }
    if (best_ci < 0) {
      continue;
    }
    /*
     * bugs.md 2026-09-04 (bug 6, "do Aztecs really beg?"): DOS scores the
     * economics of the VISITING Brave's own village (FUN_281f_0a4c binds
     * `unit+0x314a`, its home settlement, before FUN_2a1f_0434 runs), so
     * whether a tribe begs is a per-village terrain/population question, not
     * a per-nation one. The port used to score one arbitrary "sample"
     * village for the whole nation, which made every visit by that nation
     * inherit tribe[0]'s food shortfall. Ask minus bid on cargo 0 is the
     * DOS gate (`0 < *0x9e58 - *0x9e78`) and the mirror of the gift arm's
     * `bid[0] > ask[0]` food surplus.
     */
    const ColonizeCol1Tribe* home = NULL;
    int home_index = -1;
    if (visit_home_tribe >= 0 && visit_home_tribe < (int)ctx->col1->head.tribe_count &&
        (int)ctx->col1->tribe[visit_home_tribe].nation_id == nation_id) {
      home = &ctx->col1->tribe[visit_home_tribe];
      home_index = visit_home_tribe;
    } else {
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        if ((int)ctx->col1->tribe[ti].nation_id == nation_id) {
          home = &ctx->col1->tribe[ti];
          home_index = (int)ti;
          break;
        }
      }
    }
    if (!home) {
      continue;
    }
    AiContactMeetEcon2154 econ;
    memset(&econ, 0, sizeof(econ));
    if (!ai_contact_meet_economics_2154(ctx, nation_id, home, &econ)) {
      continue;
    }
    const int delta = (int)econ.ask[0] - (int)econ.bid[0];
    if (delta <= 0) {
      continue; /* this village has no food shortfall — nothing to beg for */
    }
    const int roll = dos_rng_range(ctx->rng, 1, 100);
    if (roll > delta) {
      continue;
    }
    ai_contact_bind_names(ctx);
    if (ai_contact_euro_is_human(ctx, e)) {
      const ColonizeColony* beg_colony = &ctx->colonies->colonies[best_ci];
      /*
       * bugs.md 2026-09-04: DOS FUN_5bfb_022e visibly walks the visitor into
       * the colony (FUN_281f_0e08/02d0/09ba slide) before the popup — reuse
       * the combat-bump watch so the visit reads on screen first.
       */
      ai_contact_mark_visit_brave(ctx, nation_id, visit_brave_id);
      units_combat_watch_notify(ctx->units, visit_brave_id, beg_colony->x, beg_colony->y);
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = ai_contact_tribe_name(nation_id);
      tok.string1 = beg_colony->name;
      /*
       * bugs.md: the accept row is "We offer you {%NUMBER0} of our
       * {%NUMBER1 food}" — without these it offered 0. NUMBER0 is exactly what
       * accepting hands over, NUMBER1 the colony's whole store.
       */
      tok.number0 = ai_contact_beg_food_gift(beg_colony);
      tok.has_number0 = true;
      tok.number1 = beg_colony->stock[COLONIZE_CARGO_FOOD];
      tok.has_number1 = true;
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        ctx->messages, "INDIANBEGFOOD", &tok,
        "",
        body, sizeof(body)
      );
      /* The rows carry %NUMBER tokens of their own — the helper fills them,
       * in the GAME.TXT row and in the fallback alike. */
      const char* labels[2];
      char label_buf[2][POPUP_MSG_CHOICE_LEN];
      popup_msg_section_labels(
        ctx->messages,
        "INDIANBEGFOOD",
        &tok,
        "",
        "",
        label_buf,
        labels
      );
      const int ids[2] = {1, 2}; /* 1=decline (label[0]), 2=accept (label[1]) */
      /* Payload = colony id | (home settlement index + 1) << 16 — the outcome
       * binds to the VISITING Brave's own village (smell #74), so the visit
       * record has to survive the popup round-trip. */
      const int payload = (best_ci & 0xffff) | ((home_index + 1) << 16);
      if (ai_popup_enqueue_choice_ctx(
            ctx->ai_popups, AI_POPUP_TAG_CONTACT_BEGFOOD, e, nation_id, payload, NULL, body,
            labels, ids, 2
          )) {
        if (ctx->status && ctx->status_size) {
          snprintf(
            ctx->status, ctx->status_size, "The %s beg for food at %s.",
            ai_contact_tribe_name(nation_id), ctx->colonies->colonies[best_ci].name
          );
        }
      }
    } else {
      /* AI Euro: auto-accept, handing over the same half a human would. */
      ai_contact_apply_beg_food(ctx, ind, nation_id, e, best_ci, home_index, 1);
    }
    return; /* one beg-for-food event per Indian nation per turn */
  }
  /*
   * Nothing begged this visit → LAB_5bfb_0def, the demand half's remaining
   * two sites (@INDIANCITY / @INDIANWAGONS reparations).
   */
  ai_contact_try_village_reparations(ctx, nation_id);
}
/* ===================== Human incite response, missionary convert/flee, WoI defect, prelude & relation tick (ai_contact_ai_incite_human .. ai_contact_indian_relation_tick) ===================== */


/*
 * Missionary adjacent to tribe → convert / heresy pulse (5bfb / fandom
 * Missionaries / wiki heresy denounce):
 *  - mission unset (0xff) → set mission owner, alarm/friction decay (−1
 *    peaceful / −2 Jesuit mid-range 40..54), +1 nation crosses; human status
 *    "The %s accept conversion." / "… at %s."
 *  - own mission already set → skip (one-shot; no re-crosses).
 *  - foreign mission → denounce heresy 50/50 (wiki/HandWiki equal chance):
 *      success → replace with denouncer nation (regular cross — GameFAQs:
 *      heresy install is not Jesuit-bright), +1 crosses;
 *      fail → despawn denouncer (burned at the stake).
 *    Cite: docs/manual_gap.md; fandom Missionaries denounce; WARPATH gold
 *    Done thin (mechanic FUN_4d56_417e, structure mapped in
 *    original_sources_annotated/ai/indian_incite_417e.md; wired as a 6th
 *    village-meet CHOICE — ai_contact_apply_incite / AI_POPUP_TAG_CONTACT_INCITE
 *    below — price formula byte-faithful since 2026-08-14: both formerly
 *    unnamed tables identified as live per-tribe-type sums (village count,
 *    Σ combat_unit_base_x8 over Braves) and wired for real in
 *    ai_contact_incite_price above. Mode-2 (AI Missionary auto-incites
 *    the village against the human, FUN_4d56_4528 tail-switch case 7) is
 *    ported too — ai_contact_ai_incite_human below, hooked ahead of the
 *    mission/heresy arms per the case-7 priority in the 4528 switch).
 *  - alarmed (≥55 refuse-talk gate) → refuse convert/heresy; no crosses
 *  - mid (40..54) convert: Jesuit-grade only (PEDIA @JOB24 / Brebeuf).
 * Teach/convert widgets Done structural; deep 2820 PARKED.
 */
/*
 * FUN_4d56_4528 non-human branch, unit type 3 (Missionary) → case 7 →
 * FUN_4d56_417e Mode 2 (static port 2026-08-27, T4.5). Gate, all of:
 * alarm(tribe → human) < 0x4b; human has MET the tribe; wealth rank of the
 * AI nation < the human's on the FUN_5bfb_00f8 table (compared literally on
 * ctx->euro_power_rank); AI gold >= 1500; RNG(0,4) != 0 or the village has
 * no mission. Then 417e Mode 2: target = human, no menu/confirm, the
 * tribe↔human MET/alarm gates (both redundant with the 4528 gate above),
 * affordability, then the shared LAB_4d56_4499 tail: @INDIANWARFARE
 * announce, alarm(tribe → human) += 100 via FUN_4cc6_00f2 (French/
 * Pocahontas-halved, clamped at 100 — the war-band slam), pay.
 * Returns 1 when the incite fired (the village keeps its mission state).
 */
int ai_contact_ai_incite_human(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  ColonizeCol1Tribe* t,
  int nation_id,
  int e,
  int is_missionary
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->rng || !ind || !t || e < 0 || e > 3 ||
      nation_id < 4 || nation_id > 11) {
    return 0;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human > 3 || human == e || ai_contact_euro_is_human(ctx, e)) {
    return 0;
  }
  if (ai_diplo_indian_alarm(ctx->col1, nation_id, human) >= 0x4b) {
    return 0;
  }
  if ((ind->euro_diplo[human] & COL1_INDIAN_MET_BIT) == 0) {
    return 0;
  }
  if (!ctx->euro_power_rank_ok || ctx->euro_power_rank[e] >= ctx->euro_power_rank[human]) {
    return 0; /* wealth_rank[ai] < wealth_rank[human] (0x917c table) */
  }
  /* audit G3: single treasury (the inciter is an AI here, so the accessor
   * answers from the record unless that AI is the one borrowing the purse —
   * which is exactly the case a raw record read got wrong). */
  if (europe_nation_gold(ctx->europe, ctx->col1, e) < 1500u) {
    return 0;
  }
  if (dos_rng_range(ctx->rng, 0, 4) == 0 && t->mission != COL1_TRIBE_MISSION_NONE) {
    return 0;
  }
  /*
   * 417e Mode-2 body (viceroy_unpacked.c 83652-83668): the "diplo gate"
   * is 0a38(tribe, human) & 0x20 — the tribe↔human MET bit, the very
   * check already made above (NOT the Euro↔Euro relation byte an earlier
   * pass gated on here — DOS never consults AI↔human diplomacy for this);
   * then 030c(tribe, human) >= 0x4b → reject (same alarm value gated at
   * the top, unchanged in between); then affordability against the real
   * price; then the LAB_4d56_4499 tail shared with Mode 1.
   */
  const uint32_t price = ai_contact_incite_price(
    ctx, ind, nation_id, e, human, is_missionary, t->state.capital ? 1 : 0
  );
  if (europe_nation_gold(ctx->europe, ctx->col1, e) < price) {
    return 0;
  }
  /*
   * Shared 4499 tail: @INDIANWARFARE announce (0x16e9 — the human sees
   * the War Council popup naming the inciting nation), then
   * FUN_281f_0d6c(tribe, human, 100, 0) = FUN_4cc6_00f2 alarm slam
   * (+100, French/Pocahontas-halved, clamped — the old flat +10 was a
   * placeholder), then gold -= price.
   */
  ai_contact_incite_warfare_chrome(ctx, human, nation_id, e, human);
  /* Raw +100: the French/Pocahontas halving now lives inside
   * ai_diplo_indian_alarm_delta, as in DOS 00f2 (2026-09-07d). */
  ai_contact_alarm_delta_00f2(ctx, nation_id, human, 100);
  europe_nation_gold_add(ctx->europe, ctx->col1, e, -(long)price); /* audit G3 */
  return 1;
}

/*
 * (Retired 2026-09-08, smell #75.) ai_contact_mission_convert_visit lived
 * here: a second, standing-adjacency @INDIANSCONVERT pulse with its own
 * rng(0,0xf) draw. DOS has no such pulse --- the convert roll is one draw
 * inside the 022e visit itself (viceroy 96996-97010), between the attitude
 * word zero and LAB_5bfb_096c's gift arms, reached only off FUN_465b's move
 * tail. It now lives at that site, in ai_contact_try_village_gifts.
 */

/*
 * (Retired 2026-09-22, bugs.md #557.) ai_contact_missionary_convert and
 * ai_contact_missionary_flee lived here, plus the ai_contact_step_tile_ok /
 * _step_commit / _flee_one_tile helpers they alone used: a standing-adjacency
 * pulse that established missions, rolled a 50/50 heresy, bumped crosses,
 * decayed alarm and nudged missionaries away from angry tribes. None of it
 * has a DOS site — thunk_FUN_1000_a5dc (establish, overlays.c 77201) and
 * thunk_FUN_1000_a594 (heresy, 75753) are reached only from the FUN_4d56_4528
 * switch tail (OVL13 0x4bdb), i.e. from a real village entry, and there is no
 * flee arm anywhere. The DOS AI missionary path is now
 * ai_contact_ai_missionary_village (4528 non-human switch case 3).
 */
/*
 * ai_contact_mission_pacify_meet lived here: a meet-pulse "mission pacify
 * deepen" that took −2 off tribe friction and alarm_by_player in the 40..80
 * band, cited "Source: fandom Alarm". Retired 2026-09-09 (smell #48) as the
 * last straggler of the fandom alarm-drip class whose siblings went in the
 * 2026-09-03 encroachment sweep (see the note in ai_contact_indian_prelude).
 *
 * Evidence of absence: DOS's mission goodwill is already modelled, once, in
 * FUN_4d56_152e (viceroy 81387+) — the mission nation's euro_relation_accum
 * takes the (de las Casas ×2 / de Sepulveda ÷2) mission term and every −8
 * crossing spends one alarm −1 through FUN_4cc6_00f2; the same term also
 * moves the DS:0x54f6 attitude word by local_8 * −3, and `friction` IS that
 * word's low byte (col1_save.h:753). So this helper double-counted the DOS
 * mission term, and it did so with raw byte/word writes that skipped
 * ai_diplo_indian_alarm_delta's war-clear and attitude-tier update.
 */

/*
 * FUN_4d56_1816 item 2 (War of Independence tribe defection) — thin port.
 * See indian_woi_defect_1816.md for the full raw-decomp derivation. Once
 * per Indian nation per turn while WoI is declared, a not-yet-resolved
 * tribe may side with the CROWN (Tory natives): ALARM toward the rebel
 * (human) nation jumps by +100 and alarm toward the crown drops by -100
 * (DOS's own literal deltas, not a hard "set to max/min"), plus a one-time
 * musket/horse windfall, then the tribe is latched (woi_defect_resolved).
 * The latch is set only on a defection that actually fires — a tribe that
 * fails the alarm/RNG eligibility gate or the difficulty roll stays
 * unlatched and is re-rolled next turn, as in DOS.
 *
 * Approximated: DOS derives the musket/horse windfall's tech cap from a
 * `DS:0x8d52`-selected "tribe" tech-lookup table whose value at this call
 * site isn't independently confirmed; substituted the same nation's own
 * already-mapped `tech` field (same conceptual quantity, most likely the
 * same table). Exact DOS status-message wording is not reproduced (see
 * doc). `woi_defect_forced` (DOS bit 0x40) has no known setter this pass —
 * the eligibility gate below never gets forced on for now. DOS's
 * `FUN_2a1f_0398` "mission clear" side-effect IS wired (below, after the
 * windfall) — byte-exact once its target (FUN_4cc6_0000) was fully read.
 */
void ai_contact_indian_woi_defect(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->rng || nation_id < 4 || nation_id > 11) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human > 3) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  if (ind->woi_defect_resolved) {
    return;
  }

  int eligible = ind->woi_defect_forced != 0;
  if (!eligible) {
    /* FUN_281f_030c = alarm toward the rebel nation: >= 25 and RNG(1,400) >= alarm. */
    const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, human);
    if (alarm >= 25) {
      const int roll = dos_rng_range(ctx->rng, 1, 400);
      eligible = roll >= alarm;
    }
  }
  if (!eligible) {
    return;
  }

  const int difficulty = ctx->col1->head.difficulty;
  const int span = (5 - difficulty) * 2; /* (difficulty-5)*-2: 10/8/6/4/2 */
  if (dos_rng_range(ctx->rng, 0, span) != 0) {
    return;
  }

  ind->woi_defect_resolved = 1;
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  /*
   * FUN_4cc6_00f2(tribe, declaring, +100) / (tribe, crown, -100) are ALARM
   * deltas: the tribe turns fully hostile to the rebels and content with the
   * Crown (Tory natives). The earlier port had this inverted via the
   * relation_by_indian mis-mapping (2026-08-27).
   */
  ai_contact_alarm_delta_00f2(ctx, nation_id, human, 100);
  ai_contact_alarm_delta_00f2(ctx, nation_id, crown, -100);

  /*
   * 2026-09-06d correction: the clamp operand is the tribe's **village
   * count**, not its tech level. DOS reads `*(char *)(*(int *)0x8d52 +
   * -0x69d6)`, i.e. `DS:0x962a[indian slot]` = `tribe_village_counts` —
   * the same array `FUN_4962_06b6` refreshes each Indian turn and the
   * Incite price quotes (save_format_map.md file offset 581). The old
   * `ind->tech` read was a different field entirely, so a big tribe with
   * low tech was disarmed on defection and a small high-tech one over-armed.
   */
  const int slot = nation_id - 4;
  const int villages = (int)ctx->col1->stuff.tribe_village_counts[slot];
  int muskets = (int)(int8_t)ind->muskets;
  if (muskets > villages) {
    muskets = villages;
  }
  ind->muskets = (uint8_t)(muskets * 4); /* DOS <<2; byte truncation matches */
  int horses = (int)(int8_t)ind->horse_herds;
  if (horses > villages) {
    horses = villages;
  }
  ind->horse_herds = (uint8_t)horses;
  ind->horse_breeding = (uint16_t)(horses * 25);

  /*
   * FUN_2a1f_0398 "mission clear" side-effect, resolved 2026-08-14 (thunk
   * to FUN_4cc6_0000, viceroy_unpacked.c:80774-80802 — a clean, uncorrupted
   * canonical copy, no Ghidra needed): scans col1->tribe[] (same 18-byte
   * stride / +2 nation_id / +5 mission fields the Incite discount loop
   * already uses) for records with nation_id == this tribe's own
   * nation_id and mission's low nibble == the declaring (human) nation,
   * clearing them to "none". I.e. every village of this SAME Indian
   * nation that currently hosts a mission from the rebel side loses it
   * when one of its villages defects — DOS's own literal condition
   * (type == param_1+4 where param_1 is the tribe *type*, 0-7) collapses
   * to exactly `nation_id` here since this call always passes the
   * defecting tribe's own type. DOS then shows the human player an
   * informational popup (string id 0x14c8, exact text unrecoverable
   * without a live capture) only when anything was actually cleared —
   * folded into the existing status line instead.
   */
  int missions_cleared = 0;
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      if ((t->mission & COL1_TRIBE_MISSION_NATION_MASK) != (uint8_t)human) {
        continue;
      }
      t->mission = COL1_TRIBE_MISSION_NONE;
      missions_cleared = 1;
    }
  }

  /*
   * @INDIANGRUDGE (0x14f6) — the popup DOS actually shows here, found
   * 2026-09-16: FUN_4d56_1816 item 2 loads subst slots 0 and 1 with the
   * tribe's two name forms (281f_09a4 / 281f_0a1a) and flushes the dialog
   * with BX = 0x14f6 right before the ±100 alarm pair
   * (viceroy_unpacked.asm 136388). The port had only a status line here
   * because the tag id was unresolved. The mission-clear note keeps riding
   * the status line, since DOS's own mission-clear popup is the separate
   * @INDIANBURN inside FUN_4cc6_0000.
   */
  {
    ai_contact_bind_names(ctx);
    PopupMsgTokens gtok;
    memset(&gtok, 0, sizeof(gtok));
    gtok.string0 = ai_contact_tribe_name(nation_id);
    gtok.string1 = gtok.string0;
    char gfb[AI_POPUP_BODY_LEN];
    gfb[0] = '\0';
    char gbody[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "INDIANGRUDGE", &gtok, gfb, gbody, sizeof(gbody));
    ai_contact_human_chrome(
      ctx, human, AI_POPUP_TAG_CONTACT_RAID, nation_id, "War Council", gbody
    );
  }

  if (ctx->status && ctx->status_size > 0) {
    ai_contact_bind_names(ctx);
    /* Smell #68: the mechanic is a TORY flip (+100 alarm vs. rebels, -100
     * vs. Crown) — the old line said "declares for the rebel cause". */
    if (missions_cleared) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "The %s tribe declares for the Crown! Our missions among them are cleared.",
        ai_contact_tribe_name(nation_id)
      );
    } else {
      snprintf(
        ctx->status,
        ctx->status_size,
        "The %s tribe declares for the Crown!",
        ai_contact_tribe_name(nation_id)
      );
    }
  }
}

void ai_contact_indian_prelude(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ai_contact_bind_names(ctx);
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  ai_contact_clamp_alarms(ind);

  /*
   * Smell #72 — "flag-body alarm escalate" RETIRED 2026-09-09. It had no DOS
   * counterpart at all. The 1816 §2 block it claimed to be (viceroy 81558-81599)
   * is the WoI tribe defection, already ported separately as
   * ai_contact_indian_woi_defect: `0x5382 & 1` (war declared) plus
   * `(indian_rec + 3) & 0x20` (woi_defect_resolved) gate a relation/RNG
   * eligibility test and a difficulty roll whose only effects are the ±100
   * relation flip (viceroy 81576-81578), the mission clear (81580), and the
   * musket/horse windfall (81581-81592) — it never touches alarm_by_player or
   * tribe friction, and the `0x20` it latches is at indian record +3, not +6.
   *
   * Linux's escalate ran on `unknown31_flags` = indian record **+6**, a byte
   * no DOS export reads or writes (offset tally over all three decompiled
   * exports: only +0/+2/+3/+5/+7/+8/+10 are ever touched) — so both the
   * once-per-nation latch and the `alarm < 30` band were invented, and the
   * latch additionally aliased nothing real. Precedent: the encroachment
   * drips retired 2026-09-03 below; DOS's sole alarm grower is FUN_4d56_152e's
   * threat accumulator, and the Pocahontas/French halving already lives
   * inside ai_diplo_indian_alarm_delta (FUN_4cc6_00f2, viceroy 80844-80850).
   * Isolated-RNG-only block, so no shared dos_rng draws change.
   */

  if (!ctx->col1->tribe) {
    return;
  }

  /*
   * Retired 2026-09-03 (bugs.md "alarm rises incredibly fast"): the former
   * unit-encroacher and colony-encroachment +2/turn direct bumps on tribe
   * friction + alarm_by_player were fandom-invented, not DOS. DOS grows
   * alarm only through FUN_4d56_152e's threat-score accumulator (colonies
   * within distance 7 + the 20-tile military ring feed euro_relation_accum;
   * every −8 crossing = alarm +1), which a 1-colonist colony scores ~0 on —
   * hence decades of quiet in DOS vs ~50 turns to alarm 100 here. The
   * @INDIANCOMMENT land-use chrome fired only off those invented bumps and
   * retires with them (DOS's own trigger unlocated).
   */

  /*
   * Retired 2026-09-09 (smell #48), same class and same sweep as the
   * encroachment bumps above: a per-village "mission pacifies" −1 on tribe
   * friction and alarm_by_player, every Indian turn, whenever the value sat
   * in 1..39. It had no DOS counterpart — FUN_4d56_152e already carries the
   * one mission goodwill term DOS has (euro_relation_accum + the ×2/÷2
   * de las Casas / de Sepulveda scaling, spent as alarm −1 per −8 crossing
   * through FUN_4cc6_00f2, plus attitude += local_8 * −3 on the DS:0x54f6
   * word whose low byte is `friction`), so this was a second, raw-write copy
   * of it that also skipped ai_diplo_indian_alarm_delta's war-clear and
   * attitude-tier update. Its meet-pulse twin (the −2 "pacify deepen") went
   * with it.
   */

  /*
   * Mission burn: the old Linux "alarm/friction ≥80 each tick" stand-in was
   * retired 2026-09-07d. DOS burns missions only from FUN_4cc6_00f2's
   * escalation tail — alarm delta lands the pair at 100 while at PEACE,
   * difficulty-gated RNG roll — now ported as ai_contact_alarm_delta_00f2.
   *
   * 2026-09-09 (smell #46): that "every ctx-bearing call site routes through
   * it" claim was false when written — the 152e accumulator (ai.c, DOS's
   * sole alarm-growth channel) and three ai_contact sites (tribute bump,
   * heresy pair, mission founding) still called the bare first half, so the
   * burn could never fire from them. Fixed. The remaining bare
   * ai_diplo_indian_alarm_delta callers (units.c, game_loop.c) genuinely
   * have no turn context in hand; DOS has no bare writer at all — 0x5b1c is
   * written only inside 00f2 — so those are still port debt.
   */
}

/*
 * FUN_2a1f_0270 -> FUN_4962_06b6 — the per-tribe-type census recount
 * (ported 2026-09-06d).
 *
 * Callee identity recovered with the standard chain, one extra hop because
 * Ghidra's `viceroy_overlays.c` decompile of this particular thunk is
 * garbage (`out(...)` / stray ADD — a reloc-`0000` misparse):
 *   FUN_2a1f_0270 = FUN_1000_a460 (address_mapping.csv)
 *   raw bytes at ram:1000:a460 = `CALLF FUN_1000_1e7b; JMPF <reloc>:06b6`
 *   trailer overlay id `08 00` -> OVL09, and `FUN_4962_06b6` is the only
 *   `:06b6` in OVL09_L0040 -> FUN_4962_06b6 (viceroy_unpacked.c:78378).
 * Cross-check: save_format_map.md already credits FUN_4962_06b6 with
 * writing exactly the five arrays below, and 1816's very next statement
 * reads `tribe_population_totals[slot]` — which only makes sense if this
 * call is what just refreshed it.
 *
 * DOS body, verbatim shape:
 *   tribe_data_9184[slot] = 0; tribe_population_totals[slot] = 0;
 *   tribe_village_counts[slot] = 0;
 *   for i in 0..15: tribe_dwellings_91cc[slot*16+i] = 0;
 *                   village_counts_by_continent[i] = 0;   // NOT slot-scoped
 *   for each settlement record with nation == slot+4:
 *       village_counts[slot]++; population_totals[slot] += population;
 *       village_counts_by_continent[continent(x,y)]++;
 *   for each unit with (nation & 0xf) == slot+4:
 *       v = FUN_281f_09c8(unit, 1)          // = combat_unit_base_x8 mode 1
 *       tribe_data_9184[slot]        = min(255, +v)   // FUN_4962_0006
 *       c = FUN_281f_081c(unit)             // = FUN_1427_0f0e = continent
 *       if (c >= 0) tribe_dwellings_91cc[slot*16+c] = min(255, +v)
 *
 * **DOS quirk kept literal:** `village_counts_by_continent[]` is zeroed on
 * every call but only refilled from the *current* tribe type, so after the
 * eight 1816 calls it holds the last surviving tribe type's villages only.
 * FUN_4962_0018 (the Euro census) is the array's other writer. Not
 * "fixed" here — no invented behavior.
 */
static void ai_contact_indian_census_4962_06b6(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Stuff* st = &col1->stuff;
  const int slot = nation_id - 4;
  if (slot < 0 || slot >= 8) {
    return;
  }

  st->tribe_data_9184[slot] = 0;
  st->tribe_population_totals[slot] = 0;
  st->tribe_village_counts[slot] = 0;
  for (int i = 0; i < 16; ++i) {
    st->tribe_dwellings_91cc[slot * 16 + i] = 0;
    st->village_counts_by_continent[i] = 0;
  }

  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      if (st->tribe_village_counts[slot] < 255u) {
        st->tribe_village_counts[slot]++;
      }
      int pop = (int)st->tribe_population_totals[slot] + (int)t->population;
      st->tribe_population_totals[slot] = (uint8_t)(pop > 255 ? 255 : pop);
      const int cont = map_continent_id_at(ctx->map, (int)t->x, (int)t->y);
      if (cont >= 0 && cont < 16 && st->village_counts_by_continent[cont] < 255u) {
        st->village_counts_by_continent[cont]++;
      }
    }
  }

  if (!ctx->units) {
    return;
  }
  const ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  /* Slot walk, `u->id` to the id-taking accessors — see
   * ai_contact_land_combat_sum's note. Fixed 2026-09-10. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    const int v = combat_unit_base_x8(&sctx, u->id, 1, NULL);
    if (v <= 0) {
      continue;
    }
    int sum = (int)st->tribe_data_9184[slot] + v;
    st->tribe_data_9184[slot] = (uint8_t)(sum > 255 ? 255 : sum);
    if (!units_is_on_map(u)) {
      continue; /* FUN_1427_0f0e on an off-map unit returns no continent. */
    }
    const int cont = map_continent_id_at(ctx->map, u->x, u->y);
    if (cont < 0 || cont >= 16) {
      continue;
    }
    int cs = (int)st->tribe_dwellings_91cc[slot * 16 + cont] + v;
    st->tribe_dwellings_91cc[slot * 16 + cont] = (uint8_t)(cs > 255 ? 255 : cs);
  }
}

void ai_contact_indian_relation_tick(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  /*
   * The former "relation ±1 by alarm band" arm is gone (2026-08-27): it moved
   * a Linux-only scalar; DOS alarm has no per-turn drift (seed-100 TURN3-7
   * saves) — the real ±1 is the 152e accumulator in ai.c. Friction part kept.
   */
  /*
   * Retired 2026-09-03 (bugs.md "alarm rises incredibly fast"): the fandom
   * friction ±1-per-turn band drift is gone with the encroachment bumps —
   * DOS friction moves only through 152e (mission −3·local_8, threat-word
   * bump + alarm/5) and discrete events (trade, raids, land-work), never a
   * per-turn drift.
   */

  /*
   * FUN_4d56_1816 §6, ported 2026-09-06d (was an empty ordering anchor).
   * The raw function's `do { ... } while(true)` with the `if (0xf < local_8)`
   * guard at the top is a decompiler rendering of two sequential blocks:
   * a 16-iteration loop, then the tail. Raw: viceroy_unpacked.c:81640-81683.
   *
   * §6a — goods ledger decay. `*(int *)(0x8d4e + 0xe + 2*i)`, i = 0..15, is
   * `ColonizeCol1Indian.tons[16]` (+0x0e, the 16 Col1 cargo types). Every
   * entry walks toward zero by `tech + 1` per turn and clamps at 0 — DOS
   * never lets it cross:
   *     v > 0 -> v = max(0, v - tech - 1)
   *     v < 0 -> v = min(0, v + tech + 1)
   *     v == 0 -> untouched (no store at all)
   * This is the tribe's trade memory bleeding off, which is why a village
   * you have flooded with one cargo eventually pays well for it again.
   */
  {
    const int step = (int)ind->tech + 1;
    for (int i = 0; i < (int)COLONIZE_COL1_CARGO_TYPES; ++i) {
      int v = (int)ind->tons[i];
      if (v > 0) {
        v -= step;
        if (v < 0) {
          v = 0;
        }
        ind->tons[i] = (int16_t)v;
      } else if (v < 0) {
        v += step;
        if (v > 0) {
          v = 0;
        }
        ind->tons[i] = (int16_t)v;
      }
    }
  }

  /* §6b — census recount (FUN_2a1f_0270). Must precede §6c: that reads
   * `tribe_population_totals[slot]`, which this call is what refreshes. */
  ai_contact_indian_census_4962_06b6(ctx, nation_id);

  /*
   * §6c — horse breeding. `if (horse_herds != 0) horse_breeding +=
   * horse_herds`, capped at `(tribe_population_totals[slot] + 0x19) * 2`.
   * DOS reads +8 (`horse_herds`) as a signed char and +10 as an int.
   * The cap is why a tribe with few/small villages never accumulates enough
   * breeding stock to field Mounted Braves.
   */
  {
    const int herds = (int)(int8_t)ind->horse_herds;
    if (herds != 0) {
      const int slot = nation_id - 4;
      int hb = (int)ind->horse_breeding + herds;
      const int cap = ((int)ctx->col1->stuff.tribe_population_totals[slot] + 0x19) * 2;
      if (hb > cap) {
        hb = cap;
      }
      if (hb < 0) {
        hb = 0;
      }
      ind->horse_breeding = (uint16_t)hb;
    }
  }
}

/*
 * FUN_4d56_2af6 abort-trade close (catalog): clear tribe last_bought /
 * last_sold bookkeeping before refuse chrome. Deep demand-table wipe at
 * DOS −25000 stays PARKED (no Linux table). Cite: FUNCTION_CATALOG 2af6.
 */
/*
 * ===========================================================================
 * FUN_4d56_2820 — village trade (structural port; rewritten 2026-08-29
 * against the clean 595-line recovery in indian_trade_2820.md).
 *
 * One function in DOS, three phases here, tied together by a per-Euro
 * session (`ai_contact_s_2820[e]`) that survives the popup round trips:
 *
 *   shell   — tables (2154 ask/bid + the shell's own zeroing), hold pick
 *             (iStack_7e/iStack_c8; human: menu of holds, AI: RNG), then
 *   sell    — LAB_002bbc: the unit sells the picked hold to the tribe
 *             (@TRADE0/@TRADE1 loop, gift arm, @BADHAGGLE0), gold CREDITED,
 *             the whole hold slot removed (FUN_1000_8cdc), qty = hold amount
 *             (DS:0x8dc4 is that amount, stashed by the slot remover); then
 *   buy     — LAB_002e92: @BRING, then the tribe sells its own goods
 *             (@BUYWHICH → @BUY0/@BUY1 loop, @NOTENOUGH, @BADHAGGLE2).
 *             Only reached after a completed sale (iStack_c6 != 0); an
 *             empty-handed unit gets @BRING and nothing else.
 *
 * Human-with-cargo gates before the sell loop: last_bought/last_sold ==
 * cargo or ask[cargo] == 0 → @BADCARGO; sticky_trade_good == cargo →
 * @BADHAGGLE1. AI (iStack_8 == 0): iStack_5e = alarm > 0x31 ? 3 (gift) : 1.
 *
 * Not ported: BGM cue (FUN_1000_8688 5/6/7), the VGA chief portrait frame
 * (ai_popup portrait side-channel covers the picture).
 * ===========================================================================
 */
AiContact2820 ai_contact_s_2820[4];
