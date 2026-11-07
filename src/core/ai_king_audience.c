/*
 * King / REF AI — King audience rolls, tax/tea-party dialogs, succession & Declare Independence
 *
 * Split out of ai_king.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_king_internal.h. See ai_king.c for the module prologue and the
 * crown/boycott, tea-party and REF-bookkeeping helpers.
 *
 * Sections:
 *   Audience rolls & tax/tea-party dialogs (ai_king_msg_list_entry .. ai_king_tax_hike_apply)
 *   Succession & Declare Independence (ai_king_succession .. ai_king_menu_declare_independence)
 *   mods.kings_audience: King's audience chamber (ai_king_grant_amount .. ai_king_audience_specialist)
 */

#include "core/internal.h"
#include "core/ai_king.h"
#include "core/ai_king_internal.h"
#include "core/ai_diplo.h"
#include "core/sound.h"

#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/mods.h"
#include "core/combat_strength.h"
#include "core/col1_save.h"
#include "core/col1_stuff_census.h"
#include "core/dos_buggy.h"
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
 * One entry of a plain GAME.TXT list section (@COUNTRIES, @ORDINAL) — DOS
 * FUN_281f_0422(0x87c, tag, index) renders line `index` of the section into
 * the %STRING2 scratch buffer at DS:0x833c. A missing section or an
 * out-of-range index yields the empty string, which is DOS's "missing
 * sections show nothing".
 */
/* ===== Audience rolls & tax/tea-party dialogs (ai_king_msg_list_entry .. ai_king_tax_hike_apply) ===== */
static void ai_king_msg_list_entry(
  const ColonizeMsgCatalog* catalog,
  const char* section_name,
  int index,
  char* out,
  size_t out_size
) {
  if (out && out_size) {
    out[0] = '\0';
  }
  const ColonizeMsgSection* sec = assets_msg_find(catalog, section_name);
  if (!sec || index < 0 || !out || !out_size) {
    return;
  }
  int seen = 0;
  for (int i = 0; i < sec->line_count; ++i) {
    const char* line = sec->lines[i];
    if (!line[0] || popup_msg_is_directive(line)) {
      continue;
    }
    if (seen == index) {
      str_copy_trunc(out, out_size, line);
      return;
    }
    ++seen;
  }
}

/*
 * FUN_38fd_5be8: King-audience favor-score ladder → signed tax-rate delta.
 * Real DOS gating/formula (no invented Accept/Refuse-whether-it-happens
 * gate here — see divergence note above ai_king_tax_event for history).
 * Source: original_sources_decompiled/viceroy_unpacked.c:68420.
 *
 * Gate 0 (DOS's FIRST line, raw 68433-68435: `if (*(char *)(iVar1 + -0x6d68)
 * == '\0') return 0;`): the audience nation owns at least one colony.
 * −0x6d68 = DS:0x9298 = stuff.colony_counts[] (save_format_map.md row 241),
 * the same table ai_king_rank_nations_0218 reads. Ported 2026-09-10 (audit
 * D9) — a human with zero colonies used to keep drawing tax audiences.
 * Gate: turn counter (DS:0x538e, OpenCol ctx->turn_number) >= 30; interval
 * base 18/15/12/9 by year band (>1600/>1700/>1750), narrowed by
 * difficulty (DOS: only when the audience's own nation *is* the human —
 * this port only ever rolls the audience for the human's nation, so that
 * gate is always true here); modulo turn counter; skip if tax_rate > 85.
 *
 * Score = RNG(1,1000) + (rebel_sentiment_report*2 − tax_rate)*5
 *       + treasury/100 + this-nation SoL% + turn/30.
 * DOS reads a cached per-nation SoL% table at DS:(nation−0x6bf0) for the
 * last term; this port has no such cache, so it recomputes the same value
 * live via ai_king_sol_percent — a documented substitution, not a guess
 * (see docs pointer in the file header).
 *
 * Ladder: score<100 → cut = −min(RNG(2,5), tax_rate), but no event at all
 * if that cut would be 0 (tax already 0%); 100≤score<650 and streak<30 →
 * +1 (streak++); score>949 → +3/+4 (score<1100) or +5..+8; else → +2
 * (covers 650..949, and the streak≥30 fallback out of the +1 band).
 * Returns 1 and writes king_audience_tax_delta + *out_delta when an event
 * fires; 0 (no state touched) when the gate fails or the cut degenerates.
 * *out_flavor receives the rung's GAME.TXT section and %STRING2 — see
 * AiKingAudienceFlavor above; the rungs are not interchangeable text, each
 * names its own section in DOS.
 */
static int ai_king_audience_roll(
  ColonizeTurnContext* ctx,
  int human,
  int* out_delta,
  AiKingAudienceFlavor* out_flavor
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->rng || human < 0 || human >= 4) {
    return 0;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Nation* nat = &col1->nation[human];
  /*
   * DOS's first gate: colony_counts[audience nation] != 0. The census mirror
   * is refreshed per nation at its EOT (turn.c → col1_stuff_census_4962_w),
   * but a synthetic fixture can leave the whole window blank, so a zero row
   * falls back to the live colony count before the early-out bites.
   */
  {
    int owned = (int)col1->stuff.colony_counts[human];
    if (owned == 0) {
      owned = ai_king_human_colonies(ctx, human);
    }
    if (owned == 0) {
      return 0;
    }
  }
  const uint32_t turn = ctx->turn_number ? *ctx->turn_number : 0u;
  if (turn < 30) {
    return 0;
  }
  const int year = ctx->game_year ? (int)*ctx->game_year : 1492;
  int interval_base = 18;
  if (year > 1600) {
    interval_base = 15;
  }
  if (year > 1700) {
    interval_base -= 3;
  }
  if (year > 1750) {
    interval_base -= 3;
  }
  const int diff = col1->head.difficulty;
  /* 5be8 raw 68443-68448: `diff - 2` only for a human-controlled nation;
   * every AI nation gets 0 (bare 18/15/12/9 interval). port_bugs.md #1053. */
  const int crown_adjust = (col1->player[human].control == 0) ? diff - 2 : 0;
  const int interval = interval_base - 2 * crown_adjust;
  if (interval <= 0 || (int)(turn % (uint32_t)interval) != 0) {
    return 0;
  }
  if (nat->tax_rate > 85) {
    return 0;
  }

  /*
   * 38fd_5be8 score (viceroy_unpacked.c:68460-68467). The 4th term reads
   * DS:nation-0x6bf0 — that is 0x9410 census_pop_proxy (population proxy),
   * NOT a per-nation SoL cache as this port assumed until 2026-09-06 (the
   * older king_ref.md "same value, no stored cache" note was wrong; the
   * live FUN_4962_0018 census now backs the real field).
   * DOS bug, reproduced for now (docs/tolerated_dos_bugs.md TB-40).
   * TB-40: DOS reads the human's SoL report for every nation; fixed under
   * !dos_buggy_behavior() to the audience nation's own SoL (nation+0x19).
   */
  const int sol = dos_buggy_behavior() ? (int)col1->head.rebel_sentiment_report
                                       : (int)nat->rebel_sentiment;
  const int score =
    dos_rng_range(ctx->rng, 1, 1000) +
    (sol * 2 - (int)nat->tax_rate) * 5 +
    (int)(europe_nation_gold(ctx->europe, col1, human) / 100u) +
    (int)col1->stuff.census_pop_proxy[human] +
    (int)(turn / 30);

  AiKingAudienceFlavor flavor;
  memset(&flavor, 0, sizeof(flavor)); /* every rung below sets .section */
  const ColonizeMsgCatalog* msgs = ctx->messages;

  int delta;
  if (score < 100) {
    const int roll = dos_rng_range(ctx->rng, 2, 5);
    int cut = (roll < (int)nat->tax_rate) ? roll : (int)nat->tax_rate;
    if (cut < 1) {
      return 0; /* DOS: no audience event when tax is already 0% */
    }
    delta = -cut;
    /* 38fd:5d2d — @KINGVICTORY, "our recent victory over %STRING2": the
     * country the King is currently at war with (DS:0x53a8, 1-based into
     * @COUNTRIES), left exactly as the last @KINGWAR rung set it. */
    flavor.section = "KINGVICTORY";
    ai_king_msg_list_entry(
      msgs, "COUNTRIES", (int)col1->head.king_audience_last_pick - 1,
      flavor.string2, sizeof(flavor.string2)
    );
    flavor.has_string2 = true;
  } else if (score < 650 && col1->head.king_audience_streak < 30) {
    delta = 1;
    /* 38fd:5d64 — @KINGWIFE. DS:0x53a7 is the King's wife counter, not a
     * generic streak: it is bumped here (floored at 1, the branch itself
     * gated at < 30) and names the ordinal in "our %STRING2 wife". */
    if (col1->head.king_audience_streak < 255) {
      col1->head.king_audience_streak++;
    }
    if (col1->head.king_audience_streak < 1) {
      col1->head.king_audience_streak = 1;
    }
    flavor.section = "KINGWIFE";
    ai_king_msg_list_entry(
      msgs, "ORDINAL", (int)col1->head.king_audience_streak - 1,
      flavor.string2, sizeof(flavor.string2)
    );
    flavor.has_string2 = true;
  } else if (score > 949) {
    if (score < 1100) {
      delta = dos_rng_range(ctx->rng, 3, 4);
      /* 38fd:5de2 — @KINGNAVACT, a new Navigation Act; no %STRING2. */
      flavor.section = "KINGNAVACT";
    } else {
      delta = dos_rng_range(ctx->rng, 5, 8);
      /* 38fd:5dfe — @KINGSTAMPACT, "the colonists in {%STRING2}": the
       * player's New World name (nation*0x34 + 0x5426 = player.country_name). */
      flavor.section = "KINGSTAMPACT";
      str_copy_trunc(flavor.string2, sizeof(flavor.string2),
                     col1_player_country_name(col1, human)[0]
                       ? col1_player_country_name(col1, human)
                       : "");
      flavor.has_string2 = true;
    }
  } else {
    delta = 2;
    /* 38fd:5d9e — @KINGWAR. DS:0x53a8 is the country the crown is warring
     * with, rerolled 1..8 until it differs from the last one; it also feeds
     * the @KINGVICTORY rung above, so this is a state write, not just a
     * text anti-repeat. */
    int pick;
    do {
      pick = dos_rng_range(ctx->rng, 1, 8);
    } while (pick == col1->head.king_audience_last_pick);
    col1->head.king_audience_last_pick = (uint8_t)pick;
    flavor.section = "KINGWAR";
    ai_king_msg_list_entry(
      msgs, "COUNTRIES", pick - 1, flavor.string2, sizeof(flavor.string2)
    );
    flavor.has_string2 = true;
  }
  if (out_flavor) {
    *out_flavor = flavor;
  }

  nat->king_audience_tax_delta = (int16_t)delta;
  if (out_delta) {
    *out_delta = delta;
  }
  return 1;
}

/*
 * FUN_38fd_3dc8 core clamp: tax_rate += delta, floored so it can never go
 * below 0%, ceiled at 75% (excess trimmed back out of the applied delta).
 * *out_applied receives the delta actually applied post-clamp — the value
 * the village-goods/tea-party branch below reverts on a "hold a tea
 * party" choice.
 */
/*
 * Same clamp as ai_king_audience_apply_delta, without writing: the tax
 * audience needs to know what a delta WOULD come to before the player has
 * answered (port_bugs.md — the raise must not be in the save while the popup that
 * proposes it is still on screen).
 */
static int ai_king_audience_preview_delta(const ColonizeCol1Nation* nat, int delta) {
  int applied = delta;
  if (applied < 0) {
    const int mag = -applied;
    if (mag > (int)nat->tax_rate) {
      applied = -(int)nat->tax_rate;
    }
  }
  const int new_tax = (int)nat->tax_rate + applied;
  if (new_tax > 75) {
    applied -= (new_tax - 75);
  }
  return applied;
}

/* Same clamp as the preview, plus the store. */
static void ai_king_audience_apply_delta(ColonizeCol1Nation* nat, int delta, int* out_applied) {
  const int applied = ai_king_audience_preview_delta(nat, delta);
  int new_tax = (int)nat->tax_rate + applied;
  if (new_tax > 75) {
    new_tax = 75;
  }
  if (new_tax < 0) {
    new_tax = 0; /* safety net; the floor clamp above already prevents this */
  }
  nat->tax_rate = (uint8_t)new_tax;
  if (out_applied) {
    *out_applied = applied;
  }
}

/*
 * FUN_38fd_3dc8 tea-party candidate scan (raw 64146-64200).
 *
 * DOS-LITERAL FUN_38fd_3dc8 raw 64132-64175: aiStack_cc[c] = the largest stock
 * of cargo c across the human's COASTAL colonies, aiStack_a4[c] = that colony;
 * local_7a[c] = the signed walk weight. *out_weights, when set, points at a
 * COLONIZE_CARGO_COUNT-sized caller-owned buffer valid only as long as
 * weight_buf is.
 */
static uint16_t ai_king_teaparty_candidate_mask(
  const ColonizeTurnContext* ctx,
  int human,
  int weight_buf[COLONIZE_CARGO_COUNT],
  const int** out_weights
) {
  uint16_t candidate_mask = 0;
  *out_weights = NULL;
  if (!ctx) {
    return 0;
  }
  /*
   * DOS-LITERAL FUN_38fd_3dc8 raw 64160-64175:
   *   `if (*(char*)(ce*0xca + 0x5d60) == *(char*)0x9e12 &&
   *       (*(byte*)(ce*0xca + 0x5d62) & 0x40) != 0)`
   * — only the human's COASTAL colonies (+0x1c bit 0x40, the save-carried bit
   * stamped at founding, same decode as ai_king_ref.c's 0982/1528 scans)
   * contribute to aiStack_cc[] / aiStack_a4[]. A cargo only enters the
   * selection walk if one of those colonies actually holds some of it
   * (`aiStack_cc[local_ac] != 0` gates both the weight sum and the walk):
   * you cannot dump 0 tons in protest, and you cannot dump it inland
   * (port_bugs.md #904).
   */
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    weight_buf[c] = 0;
  }
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* col = &ctx->colonies->colonies[i];
      if (!col->active || col->nation_id != human) {
        continue;
      }
      if ((col->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) == 0) {
        continue;
      }
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        if (col->stock[c] > 0) {
          candidate_mask = (uint16_t)(candidate_mask | (uint16_t)(1u << c));
        }
      }
    }
  }
  /*
   * DOS-LITERAL FUN_38fd_3dc8 raw 64146-64159: the walk weight is the
   * nation's cumulative traded tonnage, not any Europe price —
   *   `local_7a[c] = FUN_1d1d_0ec6(FUN_1d1d_0ddc(tons_lo, tons_hi), 100)`
   * i.e. the LOW WORD of labs(*(int32*)(DS:0x84fc + 0xbc + c*4)) / 100
   * (0ec6 is the signed 32-bit divide; was `* 100` until port_bugs.md #1143).
   * DS:0x84fc is the acting (human) nation record and +0xbc ==
   * offsetof(ColonizeCol1Nation, trade.tons) == 188 (offset-checked), so this
   * is nation.trade.tons[c]. The store truncates to a 16-bit word, and the
   * four de-weight shifts that follow (raw 64156-64159) are arithmetic on
   * that signed word:
   *   local_7a[0] >>= 1   (bp-0x7a + 0  -> cargo 0,  Food)
   *   local_6a   >>= 2    (bp-0x6a      -> cargo 8,  Horses)
   *   local_5e   >>= 1    (bp-0x5e      -> cargo 14, Tools)
   *   local_5c   >>= 2    (bp-0x5c      -> cargo 15, Muskets)
   * (stack word index = (0x7a - off)/2; the four names are the same stack
   * array Ghidra declared as `uint local_7a[8]`.) Every other cargo, craft
   * goods 9..12 included, keeps the full weight. The port had weighted by the
   * live Europe bid instead (port_bugs.md #903).
   */
  if (ctx->col1_ok && ctx->col1 && human >= 0 && human < 4) {
    const int32_t* tons = ctx->col1->nation[human].trade.tons;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      int32_t t = tons[c];
      if (t < 0) {
        t = -t;
      }
      weight_buf[c] = (int)(int16_t)(uint16_t)((uint32_t)t / 100u);
    }
    weight_buf[0] = (int)(int16_t)(weight_buf[0] >> 1);
    weight_buf[8] = (int)(int16_t)(weight_buf[8] >> 2);
    weight_buf[14] = (int)(int16_t)(weight_buf[14] >> 1);
    weight_buf[15] = (int)(int16_t)(weight_buf[15] >> 2);
    *out_weights = weight_buf;
  }
  return candidate_mask;
}

/*
 * Tea-party choice apply: a human answered the KING_AUDIENCE CHOICE with
 * "hold a tea party" after a real tax raise. FUN_38fd_3dc8: revert the
 * just-applied hike, boycott the tonnage-walk-picked cargo, confiscate its
 * stock. DOS-LITERAL raw 64285-64299 (answer 2): local_7c = min(100,
 * stock[local_4]) of colony aiStack_a4[local_4] (the human COASTAL colony
 * holding the most, colony+0x9a stock word), subtracted from that stock and
 * added to the SAME colony's 32-bit +0xc2 rebel_dividend (SoL numerator).
 * There is no royal-stock pile: the goods vanish and become rebel sentiment.
 * The seize + @TEAPARTY (0x106a) live in ai_king_enqueue_teaparty_ok.
 * Only caller: the KING_AUDIENCE Refuse result (ai_king_war.c).
 */
void ai_king_tax_teaparty(ColonizeTurnContext* ctx, int human, int cargo) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  /*
   * No revert to do any more: ai_king_tax_hike_apply defers the raise until
   * Accept, so refusing simply never commits it (port_bugs.md). DOS's
   * apply-then-revert reaches the same rate.
   */
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | (uint16_t)(1u << cargo));
  ai_king_set_boycott(ctx->col1, 1);

  if (ai_king_human_popups(ctx)) {
    ai_king_enqueue_teaparty_ok(ctx, human, cargo);
    /* DOS 3dc8 shows @TEAPARTY inline, right after the refused audience — the
     * queue tail put it behind whatever else the turn had pending. */
    (void)ai_popup_move_tag_to_front(ctx->ai_popups, AI_POPUP_TAG_KING_TAX);
  }
}

/*
 * FUN_38fd_5be8 + FUN_38fd_3dc8: real King-audience tax-rate-change event.
 * Ported 2026-08-19, replacing the earlier invented "Accept/Refuse gates
 * whether the hike happens" design (see docs/archive/mysteries_catalog.md,
 * king_audience_tax_delta, for the divergence this replaces).
 *
 * Real DOS shape: the audience fires on a turn-counter interval (no
 * spring-only restriction — that was also invented; see
 * original_sources_decompiled/viceroy_unpacked.c:68539 FUN_38fd_5e52, the
 * caller, which has no season gate either). A delta is always rolled and
 * applied unconditionally (ai_king_audience_roll + ai_king_audience_apply_
 * delta) — cuts and clamped-away deltas are never asked about. Only a
 * genuine positive applied delta (a real raise) can lead to a village-
 * goods popup, and only when an eligible cargo/colony candidate exists;
 * that popup's real semantics are "keep it" vs. "hold a tea party", which
 * REVERTS the raise just applied and boycotts the picked cargo — it does
 * not gate whether the raise happens in the first place.
 *
 * The per-cargo boycott-holds-future-hikes behavior from the old design
 * (market_demand_pool_raw[2] gating this function) is not real DOS (5be8/3dc8 never
 * check it) and has been dropped; nation.boycott_bitmap / the tea-party
 * flag are still set/read for presentation and for the Fugger-clears-
 * boycotts sync, just no longer block the audience interval gate.
 */
bool ai_king_tax_event(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return false;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return false;
  }
  /* Fugger / external bitmap clear → drop the boycott presentation flag. */
  ai_king_sync_boycott_refuse(ctx->col1, human);

  int delta = 0;
  AiKingAudienceFlavor flavor;
  memset(&flavor, 0, sizeof(flavor));
  if (!ai_king_audience_roll(ctx, human, &delta, &flavor)) {
    return false; /* no audience this turn: interval gate, or degenerate 0% cut */
  }
  ai_king_tax_hike_apply(ctx, human, delta, &flavor);
  /* FUN_38fd_5be8 raw 68531-68533: once a rung is rolled 5be8 returns 1,
   * even when 3dc8 clamps the delta to 0 and shows nothing (raw 64179-64186). */
  return true;
}

/*
 * port_bugs.md #1053: FUN_38fd_5e52 ends every peacetime nation's EOT with the
 * 5be8 roll (raw 68614, thunk_FUN_291f_0b7a) — no control gate. For an AI
 * nation 3dc8 applies the clamped delta (raw 64170-64180) and skips only the
 * audience / tea-party dialog (`control == 0` gate, raw 64181).
 */
void ai_king_tax_event_ai(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4 ||
      ctx->col1->player[nation_id].control != 1) {
    return;
  }
  int delta = 0;
  if (ai_king_audience_roll(ctx, nation_id, &delta, NULL)) {
    ai_king_tax_delta_ai(ctx, nation_id, delta);
  }
}

void ai_king_tax_delta_ai(ColonizeTurnContext* ctx, int nation_id, int delta) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4 ||
      ctx->col1->player[nation_id].control != 1) return;
  ai_king_audience_apply_delta(&ctx->col1->nation[nation_id], delta, NULL);
}

/*
 * FUN_38fd_3dc8 body for an explicit delta — the audience roll above and
 * the @KINGFRIGATE acceptance (3844_00f2 → 3dc8(KINGTAX, 10)) both land
 * here: clamp + apply, then the Kiss-the-ring / Tea-party dialog.
 */
/*
 * Write a settled tax delta: rate, the Europe mirror, and the REF-growth
 * approximation. Split out of ai_king_tax_hike_apply so the CHOICE path can
 * defer all three until the player has answered (port_bugs.md).
 */
void ai_king_tax_commit(ColonizeTurnContext* ctx, int human, int delta) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4 || delta == 0) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  int applied = 0;
  ai_king_audience_apply_delta(nat, delta, &applied);
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  /* (2026-09-06) The invented "grow REF pools on every audience event"
   * stand-in was removed: FUN_43f7_1d42's real mechanic — royal_money
   * stipend + 1800-gold pool buys — now runs per peacetime turn from
   * ai_king_nation_turn (ai_king_1d42_royal_purse), independent of tax
   * audiences. */
}

void ai_king_tax_hike_apply(
  ColonizeTurnContext* ctx,
  int human,
  int delta,
  const AiKingAudienceFlavor* flavor
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  /* The audience passes the rung's own section (@KINGVICTORY / @KINGWIFE /
   * @KINGWAR / @KINGNAVACT / @KINGSTAMPACT); the @KINGFRIGATE acceptance
   * calls 3dc8 with @KINGTAX and no flavour noun. */
  const char* section = (flavor && flavor->section) ? flavor->section : "KINGTAX";
  const char* flavor_string2 =
    (flavor && flavor->has_string2 && flavor->string2[0]) ? flavor->string2 : NULL;
  const int difficulty = (int)ctx->col1->head.difficulty;
  const char* king_title =
    reports_difficulty_title(difficulty >= 0 && difficulty < 5 ? difficulty : 0);
  const char* king_addressee =
    ctx->col1->player[human].name[0] ? ctx->col1->player[human].name : "";
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  /*
   * port_bugs.md: DOS's 3dc8 applies the delta and then offers keep-vs-revert, so
   * the raise is briefly real while the dialog asks about it — the port
   * showed the new rate in Europe/status before the player had answered.
   * The arithmetic outcome of both orderings is identical (accept = raised,
   * tea party = not raised), so the hike is now previewed here and committed
   * in ai_king_apply_popup_result on any answer but the party (Esc too).
   * Everything DOS never asks about (cuts, fully clamped deltas, the no-cargo
   * fallback) still commits at once.
   */
  const int applied = ai_king_audience_preview_delta(nat, delta);

  if (applied < 0) {
    /* DOS 3dc8 draws its (unused) roulette rand for every nonzero human
     * delta, cuts included: raw 64176 precedes the `param_2 < 0` arm (#1143). */
    if (ctx->rng) {
      (void)dos_rng_next(ctx->rng);
    }
    ai_king_tax_commit(ctx, human, delta);
    if (ai_king_human_popups(ctx)) {
      /* @KINGVICTORY: "To celebrate our recent victory over %STRING2, we have
       * magnanimously decided to LOWER your tax rate by {%NUMBER0%%}. The tax
       * rate is now {%NUMBER1%%}." */
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = king_title;
      tok.string1 = king_addressee;
      tok.string2 = flavor_string2;
      tok.number0 = -applied;
      tok.has_number0 = true;
      tok.number1 = (int)nat->tax_rate;
      tok.has_number1 = true;
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, section, &tok, "", body, sizeof(body));
      if (ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human,
                                  ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human), (int)nat->tax_rate,
                                  NULL, body)) {
        /* DOS 3dc8's message-only arm (38fd:402a `MOV word [0x1f5c],0x8`,
         * then FUN_281f_03fe) wears the King flair too — the port had it on
         * the @KINGTAX choice only, so a tax CUT lost the portrait. */
        ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
      }
    }
    return;
  }

  if (applied == 0) {
    return; /* delta rolled but fully clamped away (already at 75%) */
  }

  /* applied > 0: a real hike is on the table. */
  const int proposed = (int)nat->tax_rate + applied;

  int weight_buf[COLONIZE_CARGO_COUNT];
  const int* weights = NULL;
  const uint16_t candidate_mask =
    ai_king_teaparty_candidate_mask(ctx, human, weight_buf, &weights);
  int picked = ctx->rng
    ? ai_king_pick_dump_goods_cargo(nat->boycott_bitmap, candidate_mask, ctx->rng, weights)
    : -1;
  /*
   * DOS-LITERAL FUN_38fd_3dc8 raw 64176-64200: there is NO retry. Boycotted
   * cargos never enter aiStack_cc[] at all (the colony scan, raw 64160-64175,
   * only fills cc[c] under `(local_a6 & 1<<c) == 0`), so the selection walk
   * `if (aiStack_cc[local_ac] != 0)` can only ever land on a non-boycotted
   * stocked cargo, and raw 64196's guard
   *   `if ((local_4 < 0) || ((int)param_2 < 0) ||
   *        ((local_a6 & 1 << local_4) != 0))`
   * ABORTS the whole party — the hike simply stands, no second draw with the
   * boycott mask cleared. The port used to re-roll with boycott = 0, which
   * both invented a party DOS never holds and burnt an extra RNG draw
   * (port_bugs.md #907).
   */

  if (picked < 0) {
    /* No eligible (stocked, un-boycotted, coastal) cargo, no RNG (tests), or
     * popups disabled for this nation — DOS's own choice UI has nothing to
     * drive here either, so there is nothing to defer: the hike stands. */
    ai_king_tax_commit(ctx, human, delta);
    if (ai_king_human_popups(ctx)) {
      /* Same rung section as the choice arm below, just without the
       * tea-party rows DOS has nothing to offer here. */
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = king_title;
      tok.string1 = king_addressee;
      tok.string2 = flavor_string2;
      tok.number0 = applied;
      tok.has_number0 = true;
      tok.number1 = (int)nat->tax_rate;
      tok.has_number1 = true;
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, section, &tok, "", body, sizeof(body));
      /* 38fd:3f8d dispatches 0x3e for this no-cargo audience arm.  The
       * separate 0x56 dispatch belongs only to the completed tea party. */
      sound_play(0x3e);
      if (ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human,
                                  ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human), (int)nat->tax_rate,
                                  NULL, body)) {
        /* Same 38fd:402a arm — DOS reaches it when no cargo is eligible for
         * the tea party, and still stands the King beside the message. */
        ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
      }
    }
    return;
  }

  if (ai_king_human_popups(ctx)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    /* DOS 3dc8: NUMBER0 = |applied delta| (FUN_281f_09ae(0, abs(param_2))
     * after both clamps), not a constant 1 — port_bugs.md: popup claimed "raise
     * by 1%" for a 7% hike. */
    tok.number0 = applied;
    tok.has_number0 = true;
    /* @KINGTAX "The tax rate is now {%NUMBER1%%}" — the total the raise would
     * come to. The rate itself is still the old one until the answer. */
    tok.number1 = proposed;
    tok.has_number1 = true;
    /* The rung sections open with "%STRING0 %STRING1." — the difficulty title
     * and the player's name — and three of them name a flavour noun in
     * %STRING2. @KINGTAX itself uses neither, so the same token block serves
     * every section 3dc8 can be handed. */
    tok.string0 = king_title;
    tok.string1 = king_addressee;
    tok.string2 = flavor_string2;
    /* @TAXOPTIONS "Hold '{%STRING3 Party}.'" — DOS names it after the colony
     * that will be raided plus the boycotted cargo, not "Tea". */
    char party[96];
    ai_king_teaparty_party_name(
      party, sizeof(party), ai_king_teaparty_colony(ctx, human, picked), picked);
    tok.string3 = party;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      section,
      &tok,
      "",
      body,
      sizeof(body)
    );
    char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const ColonizeMsgSection* taxopt = assets_msg_find(ctx->messages, "TAXOPTIONS");
    int nch = 0;
    if (taxopt) {
      char raw_choices[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
      nch = popup_msg_rows(taxopt, raw_choices, AI_POPUP_CHOICE_MAX);
      for (int i = 0; i < nch; ++i) {
        popup_msg_apply_tokens(choice_buf[i], sizeof(choice_buf[i]), raw_choices[i], &tok);
      }
    }
    const char* labels[2];
    const int ids[] = {AI_KING_CHOICE_ACCEPT, AI_KING_CHOICE_REFUSE};
    if (nch >= 2) {
      labels[0] = choice_buf[0];
      labels[1] = choice_buf[1];
    } else {
      labels[0] = "";
      choice_buf[1][0] = '\0';
      labels[1] = choice_buf[1];
    }
    sound_play(0x3e); /* FUN_38fd_3dc8 38fd:4022/4068: royal-audience tune */
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_AUDIENCE, human,
                                    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
                                    ai_king_teaparty_payload(applied, picked),
                                    NULL, body, labels, ids, 2)) {
      /* port_bugs.md: DOS's 3dc8 dialog sets DS:0x1f5c = 8 — the animated King
       * flair stands beside the tax audience. */
      ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
      return; /* effect deferred to ai_king_apply_popup_result */
    }
    /* Queue full — fall through to auto resolve. */
  }

  /* Headless (no popup queue): nobody answers 3dc8, and an idle player's
   * Space takes the first option, Accept (independence-seed-100 000002:
   * DOS 60 -> 62 %). The old SoL/bells tea-party heuristic was invented. */
  ai_king_tax_commit(ctx, human, delta);
}

/*
 * FUN_43f7_0218 — War of the Spanish Succession (viceroy_unpacked.c
 * 73601-73712). Fires PRE-WoI, the first time the human's SoL passes 49%
 * (caller gate `0x31 < SoL && *0x53d2 < 0`), and 1a26 falls back to it at
 * declare. It frees the slot the King will borrow:
 *   - rank the 4 powers ascending by ship_counts*3 + colony_counts*2 +
 *     census_pop_proxy (raw 73627-73629, ai_king_rank_nations_0218);
 *   - weakest AI (local_c) is merged INTO the next-weakest AI (local_a):
 *     its colonies change owner (rebel accumulators +0xc2/+0xc4 zeroed),
 *     its units transfer when standing in a colony and are DESPAWNED in
 *     the field (`0302(x,y)==0 → 0808`); seen bits, layer3 owner nibbles
 *     and village mission nibbles pass to the heir;
 *   - @SUCCESSION (0x128c) announces the Treaty of Utrecht;
 *   - merged slot control=2, DS:0x53d2 = the vacated slot.
 * THIS is why the King must never inherit a live nation's estate (port_bugs.md
 * follow-up: the port's fixed slot borrow handed the King Quebec + 5
 * Caravels that were simply France's).
 */
/* ===== Succession & Declare Independence (ai_king_succession .. ai_king_menu_declare_independence) ===== */
void ai_king_succession(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  if (col1->head.crown_nation_id >= 0) {
    return; /* slot already vacated */
  }
  /* DOS-LITERAL FUN_43f7_0218 raw 73622: the whole body sits under
   * `(*(byte *)0x5381 & 0x80) == 0` — no War of Succession in hot-seat
   * (docs/hotseat.md); @MULTIREV clears the bit before 1a26 calls it. */
  if (col1_save_hotseat(col1)) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  /* FUN_43f7_0218: rank ascending by ships*3 + colonies*2 + census — the
   * real DOS table (was an invented per-colony pop score; see
   * ai_king_rank_nations_0218). */
  int order[4];
  ai_king_rank_nations_0218(col1, order);
  int merged = -1;  /* DOS local_c: weakest AI — the slot the King takes */
  int heir = -1;    /* DOS local_a: next-weakest AI — receives the estate */
  for (int i = 0; i < 4; ++i) {
    const int n = order[i];
    if (n == human) {
      continue;
    }
    if (merged < 0) {
      merged = n;
    } else if (heir < 0) {
      heir = n;
    }
  }
  if (merged < 0 || heir < 0) {
    return;
  }
  /* DOS-LITERAL FUN_43f7_0218 raw 73658-73672: every tile the merged nation
   * had seen (bit 0x10 << n) is seen by the heir, and every layer3 owner
   * nibble of the merged nation passes to the heir. */
  if (ctx->map) {
    const size_t n_tiles = (size_t)ctx->map->width * (size_t)ctx->map->height;
    for (size_t t = 0; t < n_tiles; ++t) {
      if (ctx->map->seen && (ctx->map->seen[t] & (0x10u << merged))) {
        ctx->map->seen[t] |= (uint8_t)(0x10u << heir);
      }
      if (ctx->map->layer3 && (ctx->map->layer3[t] >> MAP_L3_OWNER_SHIFT) == (unsigned)merged) {
        ctx->map->layer3[t] = (uint8_t)((ctx->map->layer3[t] & MAP_L3_CONTINENT_MASK) |
                                        (unsigned)heir << MAP_L3_OWNER_SHIFT);
      }
    }
  }
  /* Colonies: owner swap + rebel accumulators zeroed (DOS +0xc2/+0xc4), and
   * the tile's owner nibble stamped for the heir (raw 73696 FUN_281f_0704 ->
   * FUN_137f_0228). 0228's other arm runs only when FUN_137f_0392 finds a
   * village on the tile, never true under a Euro colony or unit. */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == merged) {
        c->nation_id = heir;
        map_set_owner_nibble(ctx->map, c->x, c->y, heir);
      }
    }
  }
  if (col1->colony) {
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      ColonizeCol1Colony* c = &col1->colony[i];
      if ((int)c->nation_id == merged) {
        c->nation_id = (uint8_t)heir;
        c->rebel_dividend = 0;
        c->rebel_divisor = 0;
      }
    }
  }
  /* raw 73699-73705: each village's mission (tribe +5) low nibble moves from
   * the merged nation to the heir; the high nibble stays. */
  if (col1->tribe) {
    for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
      uint8_t* m = &col1->tribe[i].mission;
      if ((*m & COL1_TRIBE_MISSION_NATION_MASK) == (unsigned)merged) {
        *m = (uint8_t)((*m & 0xf0u) | (unsigned)heir);
      }
    }
  }
  /* Units: every unit inside the map interior transfers to the heir; the rest
   * (Europe dock, sailing lanes) are gone (DOS 0302 = FUN_137f_000a interior
   * test; 0302==0 → 0808 despawn; port_bugs.md #1144). */
  if (ctx->units) {
    for (int i = 0; i < units_slot_end(ctx->units); ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != merged) {
        continue;
      }
      if (map_coords_inset(ctx->map, u->x, u->y)) {
        units_set_nation(u, heir);
        map_set_owner_nibble(ctx->map, u->x, u->y, heir); /* raw 73684 FUN_281f_0704 */
      } else {
        (void)units_despawn(ctx->units, u->id);
      }
    }
  }
  /* raw 73707-73708: FUN_291f_0a74 -> FUN_4962_0018 reruns the census for
   * the merged slot, then the heir, before the 0x543f / 0x53d2 writes, so
   * the census readers see the transfer at once. */
  {
    const ColonizeWorld cw = world_from_turn_ctx(ctx);
    col1_stuff_census_4962_w(&cw, &col1->stuff, merged);
    col1_stuff_census_4962_w(&cw, &col1->stuff, heir);
  }
  col1->player[merged].control = 2; /* withdrawn — the King's future slot */
  col1->head.crown_nation_id = (int16_t)merged;
  /* @SUCCESSION Treaty of Utrecht announcement. */
  if (ai_king_human_popups(ctx)) {
    /* raw 73649 FUN_291f_0ac8(0, 1, merged): sentence-start phrase form. */
    const char* ceder = reports_nation_country_phrase(merged, 1);
    const char* domain = col1_player_country_name(col1, merged)[0]
                           ? col1_player_country_name(col1, merged)
                           : "";
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ceder;
    tok.string1 = domain;
    /* Per-index buffers in reports.c, so heir and merged can be held at once. */
    const char* heir_adj = reports_nation_adjective_display_name(heir);
    const char* merged_adj = reports_nation_adjective_display_name(merged);
    tok.string2 = heir_adj;
    tok.string3 = merged_adj;
    char body[AI_POPUP_BODY_LEN];
    sound_set_bgm(3); /* asm 0x3C712 FUN_281f_04ac(3), port_bugs.md #1402 */
    popup_msg_fill(ctx->messages, "SUCCESSION", &tok, "", body, sizeof(body));
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_INFO, human, merged, heir, NULL, body
    );
  }
  if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
    const char* ceder = reports_nation_country_phrase(merged, 1);
    const char* domain = col1_player_country_name(col1, merged)[0]
                           ? col1_player_country_name(col1, merged)
                           : "";
    const char* heir_adj = reports_nation_adjective_display_name(heir);
    const char* merged_adj = reports_nation_adjective_display_name(merged);
    PopupMsgTokens status_tok;
    memset(&status_tok, 0, sizeof(status_tok));
    status_tok.string0 = ceder;
    status_tok.string1 = domain;
    status_tok.string2 = heir_adj;
    status_tok.string3 = merged_adj;
    popup_msg_fill(ctx->messages, "SUCCESSION", &status_tok, "", ctx->status, ctx->status_size);
    popup_msg_strip_markup(ctx->status);
  }
}

/*
 * FUN_43f7_1a26 declare body (after 2564 confirm / auto).
 * Fallback-seeds REF by difficulty only when it is still all zero; seeds the
 * 10f0 foreign-intervention pools via ai_king_seed_backup_force_1a26;
 * withdraws other Euros; thin 160a rename.
 */
void ai_king_do_declare(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /* DOS 1a26: `if (*0x53d2 < 0) 0364` — run the succession merger now if the
   * SoL>49 trigger never fired, so the King borrows an EMPTY slot instead of
   * inheriting a live nation's colonies and ships (port_bugs.md follow-up). */
  ai_king_succession(ctx);
  ai_king_set_independence(ctx->col1, 1); /* WoI: market_demand_pool_raw[0] if not already */
  ai_king_write_rival_nation_slots(ctx->col1, human);
  /* FUN_43f7_2564 raw 75238-75240 writes nothing here: its @DECLARE (0x1397)
   * answer 2 calls thunk_FUN_291f_0356 = FUN_210d_0dab (RTLink overlay load)
   * + FUN_43f7_1a26, i.e. straight into this body. The port's write-only
   * "congress" latch (pad[1] 0x04, no reader) was a stand-in and is gone. */
  /*
   * DOS FUN_43f7_1a26 writes only the foreign-intervention pool
   * (0x53e2..0x53e8 backup_force) here — the Expeditionary Force itself is
   * whatever accumulated since the new-game seed (75c2:360b: regulars
   * 8*diff+15 etc.) plus the tax-event growth; re-seeding it at declare
   * (an earlier stand-in from when new games started at 0) shrank an
   * accumulated force. Fallback-seed only if it is still all zero (a save
   * from a build without the new-game seed).
   */
  if (ctx->col1->head.expeditionary_force[0] == 0 &&
      ctx->col1->head.expeditionary_force[1] == 0 &&
      ctx->col1->head.expeditionary_force[2] == 0 &&
      ctx->col1->head.expeditionary_force[3] == 0) {
    ai_king_ref_seed_floor(ctx->col1->head.difficulty, ctx->col1->head.expeditionary_force);
  }
  ai_king_seed_backup_force_1a26(ctx, human);
  /* FUN_43f7_1a26 right after the pool seed: latch the declaration year into
   * DS:0x53a7/0x53a8 (year/100, year%100 — the king-audience RNG bytes,
   * dead once the King is gone; FUN_41f2_0092's early-revolution bonus reads
   * them back) and zero the human's liberty_bells_pool so bells accrue
   * "since declaring" (score's REF-present bells line). */
  ctx->col1->head.king_audience_streak = (uint8_t)(ctx->col1->head.year / 100);
  ctx->col1->head.king_audience_last_pick = (uint8_t)(ctx->col1->head.year % 100);
  /* DOS-LITERAL FUN_43f7_1a26 raw 74738: `*(*(int*)0x84fc + 0xc) = 0`.
   * port_bugs.md #933: +0xc IS the live FF/intervention bell pool, so this also
   * clears the human's Europe mirror — the WoI intervention threshold must
   * be met with bells produced after the declaration, not before it. */
  founding_fathers_reset_bells_pool(ctx, human);
  ai_king_set_ref_present(ctx->col1, 1);
  /*
   * port_bugs.md: nothing ever set the WAR bit between the rebel and the
   * crown's borrowed slot, so a player Frigate (or Man-O-War/Privateer)
   * attacking a REF ship bounced with "At peace — cannot attack". Raw
   * ai_diplo_or_both, not declare_war_ctx: the WoI is its own regime — no
   * Franklin gate, no peer embargo/tax chrome.
   */
  /*
   * FUN_43f7_1a26 (viceroy_unpacked.c:74832-74833), decoded 2026-09-06d:
   *   caseD_10(human, crown, 0x22)      -> FUN_15b3_0066 = or_both
   *   FUN_281f_0a10(human, crown, 0x40) -> FUN_15b3_00d0 = clear_both
   * With the 2026-08-27 T1.19 bit map (ai_diplo.h) 0x22 is exactly
   * WAR(0x02)|MET(0x20) — the port's existing write was already DOS-exact;
   * the missing half was the PEACE(0x40) clear. king_ref's old note read
   * "0x40 MET" off the stale pre-T1.19 map, which is why this looked like a
   * conflict with the user-verified WAR|MET set. Clearing PEACE cannot
   * regress the rebel-attacks-REF fix (units.c's gate short-circuits on WAR
   * before ai_contact's @HAVETREATY prompt ever reads PEACE) and it stops the
   * crown pair from reading "treaty signed" in the F8 report mid-war.
   */
  {
    const int crown_slot = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
    ai_diplo_or_both(ctx->col1, human, crown_slot, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_MET));
    ai_diplo_clear_both(ctx->col1, human, crown_slot, (uint8_t)AI_DIPLO_PEACE);
  }
  /* port_bugs.md: no landing on the declaration turn itself. */
  ai_king_latch_set(ctx->col1, AI_KING_REF_WAVE_WAIT_BYTE, 1);
  /*
   * FUN_43f7_0108 (eliminate nation), called from FUN_43f7_1a26 for every
   * nation that is neither the declaring human nor the crown proxy
   * (DS:0x5398 / 0x53d2 gate) -- WoI narrows the world to rebel vs REF, so
   * the other Euro powers are fully removed: diplomatic status withdrawn
   * *and* every unit they own destroyed (colonies are untouched by 0108
   * itself -- DOS leaves them ownerless/inert once their nation's
   * status=2). OpenCol already set control=2 here ("withdrawn"); the
   * unit-scrub half was missing. Crown-nation units (the REF spawns below)
   * must survive.
   */
  const int crown_fold = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  /*
   * port_bugs.md #228: DOS 1a26 stores the crown slot in DS:0x53d2 and sets its
   * control byte to 1 (`*(0x53d2*0x34+0x543f)=1`). The port never wrote
   * head.crown_nation_id, so a save exported mid-WoI reached DOS with
   * 0x53d2 = -1; DOS then picked its own crown — sometimes the very nation
   * the port had cached as the intervention ally (0x53d4), which made the
   * intervention force render as "Tory".
   */
  ctx->col1->head.crown_nation_id = (int16_t)crown_fold;
  /*
   * port_bugs.md #229: DOS 1a26 zeroes the bell pool (`*(pool+0xc)=0`) — the FF
   * election in progress is cancelled and bells start accruing toward the
   * foreign intervention instead.
   */
  founding_fathers_consume_woi_bell_pool(ctx, human);
  /* next_founding_father (+0x12) stays: 1a26 clears +0xc and +0x16 only
   * (independence-seed-100 000003..000020 keep the Dutch nominee 1). */
  for (int n = 0; n < 4; ++n) {
    if (n == human) {
      continue;
    }
    /* Crown slot stays a live AI combatant (DOS control 1), the other two
     * Euro powers withdraw (control 2). */
    ctx->col1->player[n].control = (uint8_t)(n == crown_fold ? 1 : 2);
    if (n != crown_fold && ctx->col1_ok) {
      /*
       * FUN_43f7_0108 diplo-clear/set (viceroy_unpacked.c:73554-73557: clear
       * 0x0b vs DS:0x5398 then vs DS:0x53d2, OR 0x60 vs each -- DOS never
       * targets the crown itself with 0108, matching the n==crown_fold skip).
       *
       * Bit map corrected 2026-09-06d: the old comment here ("0xb =
       * WAR|PEACE|unmapped-bit3; 0x60 = unmapped-bit5|MET") predates the
       * 2026-08-27 T1.19 re-derivation in ai_diplo.h. Under the live map
       * 0x0b = WAR_INTENT(0x01)|WAR(0x02)|amicable-latch(0x08) and
       * 0x60 = MET(0x20)|PEACE(0x40) -- every bit is mapped, nothing is
       * "unmapped" any more. The port had it inverted for PEACE: it CLEARED
       * 0x40 where DOS SETS it, so a withdrawn Euro power came out of the
       * declare fold not-at-peace with both the rebel and the crown.
       * (0x08 is AI_DIPLO_AMICABLE, the DOS amicable-negotiation latch --
       * clearing it is the DOS write. Renamed 2026-09-09, smell #99: it was
       * called AI_DIPLO_TREASURE_STRONGER, which wrongly implied a
       * OpenCol-invented second owner of the bit; DOS writes 0x08 both here,
       * at 153e's tail, and in FUN_465b_0000's treasure arm.)
       */
      const uint8_t k_0108_clear =
        (uint8_t)(AI_DIPLO_WAR_INTENT | AI_DIPLO_WAR | AI_DIPLO_AMICABLE);
      const uint8_t k_0108_set = (uint8_t)(AI_DIPLO_MET | AI_DIPLO_PEACE);
      ai_diplo_clear_both(ctx->col1, n, human, k_0108_clear);
      ai_diplo_or_both(ctx->col1, n, human, k_0108_set);
      ai_diplo_clear_both(ctx->col1, n, crown_fold, k_0108_clear);
      ai_diplo_or_both(ctx->col1, n, crown_fold, k_0108_set);
    }
    if (n == crown_fold || !ctx->units) {
      continue;
    }
    /* 0108 walks DS:0x539c down to 0 (raw 73559-73563): each delete leaves
     * the stale compaction tail later creates inherit (units.c), so the
     * order matters (independence 7->8: the REF Artillery's facing byte). */
    for (int i = units_slot_end(ctx->units) - 1; i >= 0; --i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->nation_id == n) {
        units_despawn(ctx->units, u->id);
      }
    }
  }
  /*
   * FUN_43f7_0188(human) — ported 2026-09-06: right after the eliminate
   * loop, 1a26 deletes every HUMAN unit whose coordinates fail
   * FUN_281f_0302 (map_tile_in_bounds) — i.e. everything parked on the
   * Europe/high-seas lanes (DOS x/y 228+n / 232+n / 244+n) — and pops
   * @SEIZURE (0x1284, "%STRING0 seized on the high seas by the Royal
   * Navy!") for each ship type (0xd..0x12); passengers/cargo aboard die
   * silently with their ship. In this port those ships live in the Europe
   * lane lists, not the unit pool, so the seizure clears
   * europe->harbor/bound/expected.
   */
  if (ctx->europe) {
    EuropeHarborShip* lanes[3] = {ctx->europe->harbor, ctx->europe->bound,
                                  ctx->europe->expected};
    int* counts[3] = {&ctx->europe->harbor_ships, &ctx->europe->bound_ships,
                      &ctx->europe->expected_ships};
    for (int li = 0; li < 3; ++li) {
      for (int si = 0; si < *counts[li]; ++si) {
        const EuropeHarborShip* ship = &lanes[li][si];
        if (ai_king_human_popups(ctx)) {
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = ship->name[0] ? ship->name : "";
          char body[AI_POPUP_BODY_LEN];
          popup_msg_fill(ctx->messages, "SEIZURE", &tok, "", body, sizeof(body));
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown_fold, 0, NULL, body
          );
        }
      }
      *counts[li] = 0;
    }
    ctx->europe->selected_harbor = -1;
    ctx->europe->dock_count = 0; /* the dock colonists go with them (0188) */
  }
  /* The same 0188 sweep over the unit pool: a human unit whose coordinates
   * fail FUN_281f_0302 (dock and lane sentinels) is destroyed
   * (independence-seed-100: the colonist waiting on the Dutch dock is gone
   * in 000003). */
  if (ctx->units && ctx->map) {
    for (int i = units_slot_end(ctx->units) - 1; i >= 0; --i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->nation_id == human && !map_coords_inset(ctx->map, u->x, u->y)) {
        (void)units_despawn(ctx->units, u->id);
      }
    }
  }
  /*
   * DOS-LITERAL FUN_43f7_1a26 raw 74818-74827: right after the WoI bit, every
   * tile of the seen layer (FUN_281f_0736(0, 0)) is ANDed with ~crown (the
   * slot INDEX, not its bit; a no-op for crown 0) and gets the crown's seen
   * bit wherever the human's is set. The REF inherits the rebels' map
   * (independence-seed-100 000003: all 3487 Dutch-seen tiles carry England's
   * bit, 800 before the declaration), which its pathfinder then reads.
   * DOS bug, reproduced for now (docs/tolerated_dos_bugs.md TB-36).
   */
  if (ctx->map && ctx->map->seen) {
    /* TB-36: DOS masks with ~crown INDEX; fixed under !dos_buggy_behavior()
     * to clear the crown's own seen bit (~(0x10 << crown)). */
    const uint8_t keep = dos_buggy_behavior() ? (uint8_t)~crown_fold
                                              : (uint8_t)~(0x10u << crown_fold);
    const uint8_t human_bit = (uint8_t)(0x10u << human);
    const uint8_t crown_bit = (uint8_t)(0x10u << crown_fold);
    const size_t n_tiles = (size_t)ctx->map->width * (size_t)ctx->map->height;
    for (size_t t = 0; t < n_tiles; ++t) {
      ctx->map->seen[t] &= keep;
      if (ctx->map->seen[t] & human_bit) {
        ctx->map->seen[t] |= crown_bit;
      }
    }
  }
  /*
   * FUN_43f7_1a26 tail (43f7:1c07..1c1b): every human unit's MP is spent via
   * FUN_281f_0934 (unit_exhaust_mp) — "This will end our turn" (@DECLARE).
   */
  if (ctx->units) {
    for (int i = 0; i < units_slot_end(ctx->units); ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->nation_id == human) {
        u->moves = 0;
      }
    }
  }
  /*
   * FUN_43f7_1a26 last writes (OVL07 asm 2c44..2c5c): ff_count_end_prob
   * (nation+0x16, DS:-0x77e2) zeroed for the human AND the crown slot, and
   * the crown's nation_flags bit 0x04 ("achieved independence from its
   * King") cleared. col1_save.h already documented the field as "cleared on
   * independence"; the write itself landed here 2026-09-06.
   */
  ctx->col1->nation[human].ff_count_end_prob = 0;
  if (crown_fold >= 0 && crown_fold < 4) {
    ctx->col1->nation[crown_fold].ff_count_end_prob = 0;
    ctx->col1->nation[crown_fold].nation_flags &= (uint8_t)~0x04u;
  }
  /*
   * Thin 160a independence rename (the letter animation itself lives in
   * core/declaration.c and is armed by game_loop off the KING_LETTER popup).
   * Writable Col1 player.country_name (and europe.nation_name if present).
   * No status line (DOS shows only the popups below).
   * Human queue: thin rename OK + WoI-begins OK (FUN_43f7_160a / 1a26 chain).
   * Letter chrome: KING_LETTER body carries the @INDEPENDENCE wording; the
   * signing animation (DECOIND.PIK + DEC-UPP/LOW/SQIG.SS) is core/declaration.c.
   * DECLARAT.PIK is an unused leftover — no DOS executable references it.
   */
  /* port_bugs.md #239: no "United Colonies" rename — DOS 160a is only the signing
   * cinematic and never touches country_name. Under the WoI the player
   * faction reads "Rebel"/"Rebels" (@MISC 69/86) via reports_nation_*_woi. */
  if (ai_king_human_popups(ctx)) {
    /* FUN_43f7_160a rename OK; the cinematic rides on the KING_LETTER tag. */
    const char* leader =
      (human >= 0 && human < 4 && ctx->col1->player[human].name[0] != '\0')
        ? ctx->col1->player[human].name
        : "";
    PopupMsgTokens letter_tok;
    memset(&letter_tok, 0, sizeof(letter_tok));
    letter_tok.string0 = leader;
    char letter[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "INDEPENDENCE",
      &letter_tok,
      "",
      letter,
      sizeof(letter)
    );
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_KING_LETTER,
      human,
      ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human),
      0,
      NULL,
      letter
    );
    /* port_bugs.md #236: @HOWTOWIN does NOT fire at the declaration. DOS shows it
     * once at the first colony the rebel recaptures with the REF present
     * (5fef capture tail, DS:0x5386 bit0 latch) — see
     * units_try_capture_foreign_colony. */
  }
}

/*
 * FUN_43f7_2564 gate (SoL≥AI_KING_DECLARE_SOL_MIN) + 1a26 declare.
 * Human + ctx->ai_popups → CHOICE from GAME.TXT @DECLARE (Never / Yes;
 * effect in apply_popup_result). Else auto-declare when SoL past 2564/fandom
 * threshold and SoL ≥ min.
 *
 * 2026-08-24: resolved the "two-stage declare popup" question flagged by a
 * prior pass (docs/sons_of_liberty.md). Raw 2564 body
 * (viceroy_unpacked.c:75217-75253): when SoL≤49 it shows msg 0x1386 with
 * `*(int*)0x53d0` (the SoL value) as its NUMBER0 arg — that argument shape
 * matches GAME.TXT `@TOOTORY` ("Only {%NUMBER0%%} of the colonists support
 * the independence movement... until the {majority} is behind us") exactly,
 * so 0x1386 = @TOOTORY. Unreachable *here*: this function (the per-turn
 * auto-check) only runs once `sol >= AI_KING_DECLARE_SOL_MIN` already.
 * 2564 is *also* reachable from the MENU.TXT @GAME "DECLARE INDEPENDENCE"
 * command at any SoL in the original — the port models that path
 * separately as `ai_king_menu_declare_independence` (wired to
 * MAP_MENU_ACTION_DECLARE_INDEPENDENCE in game_loop.c), which is where
 * @TOOTORY actually fires below threshold.
 *
 * The `*(byte*)0x5381 & 0x80` branch (raw 75226-75235) is hot-seat
 * (docs/hotseat.md; the bit is set only by FUN_75c2_10ae when 2+ powers are
 * ticked on @MULTI): `FUN_281f_0652(0x138e, 1)` = @MULTIREV asks first, any
 * answer but 1 ("Declare independence") returns, and 1 clears the bit and
 * sets `*(u16*)0x5398 = *(u16*)0x5394` before the same @DECLARE confirm every
 * path shares. Port: ai_king_show_multirev_choice / ai_king_multirev_accept.
 * (The `*(byte*)0x5382&1` else-branch's 0x1374 is @ALREADYREVOLUTION, shown
 * by ai_king_menu_declare_independence.)
 */
/*
 * @DECLARE Never/Yes confirm body — called once `sol` is already known
 * ≥ AI_KING_DECLARE_SOL_MIN from the menu-invoked path
 * (ai_king_menu_declare_independence). DOS FUN_43f7_2564 has no per-turn
 * call site: nothing declares on its own at the SoL threshold, headless
 * or not (independence-seed-100 campaign, DOSBox load of START_SAVE).
 */
static void ai_king_show_declare_choice(ColonizeTurnContext* ctx, int human, int sol) {
  if (ai_king_human_popups(ctx)) {
    /* port_bugs.md #235: %STRING0 is the Crown nation ("England"), never the
     * player's new-world country_name ("New England"). FUN_43f7_2564 raw
     * 75237: FUN_291f_0ac8(0, 0, 0x5398), the mid-sentence phrase form
     * ("the Netherlands"). */
    const char* motherland =
      (human >= 0 && human <= 3) ? reports_nation_country_phrase(human, 0) : "";
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = motherland;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "DECLARE", &tok, "", body, sizeof(body));
    char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "DECLARE");
    int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
    for (int i = 0; i < nch; ++i) {
      char filled[AI_POPUP_CHOICE_LEN];
      popup_msg_apply_tokens(filled, sizeof(filled), choice_buf[i], &tok);
      str_copy_trunc(choice_buf[i], sizeof(choice_buf[i]), filled);
    }
    /* GAME.TXT: Never… / Yes… — map to Not yet / Confirm. */
    const char* labels[2];
    const int ids[] = {AI_KING_CHOICE_NOT_YET, AI_KING_CHOICE_CONFIRM};
    if (nch >= 2) {
      labels[0] = choice_buf[0];
      labels[1] = choice_buf[1];
    } else {
      labels[0] = "";
      labels[1] = "";
    }
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_CONGRESS, human,
                                    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human), sol, NULL,
                                    body, labels, ids, 2)) {
      return; /* #1405: the invented "Congress debates" status line is gone */
    }
    /* Queue full — fall through to auto declare. */
  }
  /* No popup (headless) or a full queue: the Confirm answer, exactly as the
   * AI_POPUP_TAG_KING_CONGRESS handler applies it (declare, then the wave
   * call that burns the one-turn latch). DOSBox independence-seed-100: the
   * REF lands at the crown slot right after the declaration turn. */
  ai_king_do_declare(ctx, human);
  ai_king_ref_wave(ctx);
}

/*
 * FUN_43f7_2564 raw 75234-75235, @MULTIREV answer 1 ("Declare independence"):
 * hot-seat ends (`0x5381 &= 0x7f`), the declaring power becomes the fixed
 * human (`0x5398 = 0x5394`), then the shared @DECLARE confirm. Port
 * divergence (docs/hotseat.md "After a hot-seat declaration"): every other
 * control-0 power goes to the AI (control 1), as @MULTIREV's own text says.
 */
void ai_king_multirev_accept(ColonizeTurnContext* ctx, int human, int sol) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  col1->head.hotseat_woi_redirect_pending &= (uint8_t)~0x80u;
  col1->head.human_player = (uint16_t)human;
  for (int n = 0; n < 4; ++n) {
    if (n != human && col1->player[n].control == 0) {
      col1->player[n].control = 1;
    }
  }
  ai_king_show_declare_choice(ctx, human, sol);
}

/* FUN_43f7_2564 raw 75228-75233: the @MULTIREV warning ahead of @DECLARE.
 * GAME.TXT: Declare independence / Never Mind; Esc or Never Mind = DOS's
 * `!= 1` return. No popup queue (or a full one): the Yes answer, as
 * ai_king_show_declare_choice falls through to its Confirm. */
static void ai_king_show_multirev_choice(ColonizeTurnContext* ctx, int human, int sol) {
  if (ai_king_human_popups(ctx)) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "MULTIREV", NULL, "", body, sizeof(body));
    char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    const int nch = popup_msg_choices(
      assets_msg_find(ctx->messages, "MULTIREV"), choice_buf, AI_POPUP_CHOICE_MAX
    );
    const char* labels[2] = {nch >= 2 ? choice_buf[0] : "", nch >= 2 ? choice_buf[1] : ""};
    const int ids[] = {AI_KING_CHOICE_MULTIREV, AI_KING_CHOICE_NOT_YET};
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_CONGRESS, human,
                                    ai_king_crown_nation_col1(ctx->col1, human), sol, NULL,
                                    body, labels, ids, 2)) {
      return;
    }
  }
  ai_king_multirev_accept(ctx, human, sol);
}

/*
 * Menu-invoked DECLARE INDEPENDENCE (MENU.TXT @GAME item, MAP_MENU_ACTION_
 * DECLARE_INDEPENDENCE), the only way into DOS FUN_43f7_2564. Below the
 * threshold, show @TOOTORY (Congress won't back a rebellion yet) as a
 * plain OK notice; at/above threshold, the Never/Yes @DECLARE confirm.
 */
void ai_king_menu_declare_independence(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    /* FUN_43f7_2564 else arm (0x5382 & 1): popup 0x1374 = @ALREADYREVOLUTION. */
    if (ai_king_human_popups(ctx)) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "ALREADYREVOLUTION", NULL, "", body, sizeof(body));
      (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_INFO, human,
                                    ai_king_crown_nation_col1(ctx->col1, human), 0, NULL, body);
    }
    return;
  }
  /* FUN_43f7_2564 raw 75226-75227 tests DS:0x53d0 itself (> 0x31): the SoL
   * cached by 2424 each peacetime turn for the fixed power (hot-seat: whoever
   * declares), plus Bolivar's +20 and cheat stage 1's 75 — not a live
   * recompute (#1471). @TOOTORY's %NUMBER0 is the same word (raw 75245). */
  const int sol = (int)ctx->col1->head.rebel_sentiment_report;
  if (sol < AI_KING_DECLARE_SOL_MIN) {
    if (ai_king_human_popups(ctx)) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.number0 = sol;
      tok.has_number0 = true;
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "TOOTORY", &tok, "", body, sizeof(body));
      (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_INFO, human,
                                    ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human), sol, NULL,
                                    body);
    }
    if (ctx->status && ctx->status_size) {
      PopupMsgTokens status_tok;
      memset(&status_tok, 0, sizeof(status_tok));
      status_tok.number0 = sol;
      status_tok.has_number0 = true;
      popup_msg_fill(ctx->messages, "TOOTORY", &status_tok, "", ctx->status, ctx->status_size);
      popup_msg_strip_markup(ctx->status);
    }
    return;
  }
  if (col1_save_hotseat(ctx->col1)) {
    ai_king_show_multirev_choice(ctx, human, sol);
    return;
  }
  ai_king_show_declare_choice(ctx, human, sol);
}

/* ===== mods.kings_audience: King's audience chamber (ai_king_grant_amount .. ai_king_audience_specialist) ===== */
/*
 * The cut 38fd:4668 chamber: DOS ships the whole menu and its three handlers
 * but no caller and no @AUDIENCE / @KINGGOAWAY text. The mod adds the caller
 * (VIEW menu row, K) and the text (data/mods/GAME.TXT). Human only: 44a4's AI
 * arm stays unreachable, as in DOS. Every entry returns at once with the mod
 * off.
 */

/* DOS-LITERAL FUN_38fd_44a4 raw 64455-64512, human arm:
 * max(1, 6 - (hikes+1)/2 - difficulty) * 100. */
int ai_king_grant_amount(int hike_count, int difficulty) {
  const int n = 6 - ((hike_count + 1) >> 1) - difficulty;
  return (n < 1 ? 1 : n) * 100;
}

/* FUN_38fd_4590 (asm UA 102871-102939; Ghidra swaps the tag and delta
 * arguments): T = ((difficulty & 0xfe) * 2 + 4) * (turn / 400 + 1), signed char. */
int ai_king_petition_threshold(int difficulty, int turn) {
  return (int)(int8_t)((((difficulty & 0xfe) * 2) + 4) * (turn / 400 + 1));
}

/* One of the chamber's plain King messages, "%STRING0 %STRING1" = difficulty
 * title + player name (38fd:462e), drawn on the throne-room scroll. False when
 * nothing was queued (no popups, a GAME.TXT without the section, queue full). */
static bool ai_king_audience_notice(ColonizeTurnContext* ctx, int human, const char* section, int number0) {
  if (!ai_king_human_popups(ctx)) {
    return false;
  }
  const int difficulty = (int)ctx->col1->head.difficulty;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = reports_difficulty_title(difficulty >= 0 && difficulty < 5 ? difficulty : 0);
  tok.string1 = ctx->col1->player[human].name;
  tok.number0 = number0;
  tok.has_number0 = true;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, section, &tok, "", body, sizeof(body));
  return body[0] && ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_CHAMBER_MSG, human,
                                            ai_king_crown_nation_col1(ctx->col1, human), 0, NULL, body);
}

/* A chamber CHOICE: body and rows from one GAME.TXT section, ids 1..n (DOS's
 * 1-based FUN_281f_0998 result; Esc = 0 = leave). */
static void ai_king_audience_choice_popup(
  ColonizeTurnContext* ctx, int human, AiPopupTag tag, const char* section, int number0, int payload
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.number0 = number0;
  tok.has_number0 = true;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, section, &tok, "", body, sizeof(body));
  char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const int n =
    popup_msg_choices(assets_msg_find(ctx->messages, section), choice_buf, AI_POPUP_CHOICE_MAX);
  const char* labels[AI_POPUP_CHOICE_MAX];
  int ids[AI_POPUP_CHOICE_MAX];
  for (int i = 0; i < n; ++i) {
    labels[i] = choice_buf[i];
    ids[i] = i + 1;
  }
  if (n > 0) {
    (void)ai_popup_enqueue_choice_ctx(ctx->ai_popups, tag, human,
                                      ai_king_crown_nation_col1(ctx->col1, human), payload,
                                      NULL, body, labels, ids, n);
  }
}

static bool ai_king_audience_ready(const ColonizeTurnContext* ctx, int human) {
  return mods_on(MOD_KINGS_AUDIENCE) && ctx && ctx->col1_ok && ctx->col1 && human >= 0 && human < 4;
}

/* 38fd:4668: the once-per-turn latch, the 462e greeting, then @AUDIENCE. */
void ai_king_menu_audience(ColonizeTurnContext* ctx) {
  if (!ctx || !ai_king_audience_ready(ctx, ctx->human_nation) || !ai_king_human_popups(ctx)) {
    return;
  }
  const int human = ctx->human_nation;
  if (ai_king_independence_declared(ctx->col1)) {
    /* Port decision: DOS never had to choose (the chamber is dead code). The
     * Crown does not receive rebels, like Europe during the war; no GAME.TXT
     * text exists for it, so nothing is shown. */
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  /* 38fd:4668-4682; FUN_38fd_5e52 clears bit 0x20 every turn (europe_harbor.c). */
  if (nat->nation_flags & 0x20u) {
    (void)ai_king_audience_notice(ctx, human, "KINGGOAWAY", 0);
    return;
  }
  /* DOS sets the latch before the greeting; the port sets it only once the
   * greeting is queued, so a full queue does not eat the turn's audience. */
  if (ai_king_audience_notice(ctx, human, "KINGWELCOME0", 0)) {
    nat->nation_flags = (uint8_t)(nat->nation_flags | 0x20u);
    ai_king_audience_choice_popup(ctx, human, AI_POPUP_TAG_KING_CHAMBER, "AUDIENCE", 0, 0);
  }
}

/* FUN_38fd_4590: ask for lower taxes. */
static void ai_king_audience_petition(ColonizeTurnContext* ctx, int human) {
  const int d = (int)ctx->col1->head.difficulty;
  const int tax = (int)ctx->col1->nation[human].tax_rate;
  const int t = ai_king_petition_threshold(d, (int)ctx->col1->head.turn);
  AiKingAudienceFlavor flavor;
  memset(&flavor, 0, sizeof(flavor));
  if (tax <= 1 || tax <= t + 5) {
    /* Asking while taxes are already low is punished: +2 * RNG(1, d). */
    flavor.section = "KINGRAISE";
    ai_king_tax_hike_apply(ctx, human, 2 * dos_rng_range(ctx->rng, 1, d), &flavor);
  } else if (dos_rng_range(ctx->rng, 1, d + 1) == 1) {
    flavor.section = "KINGLOWER";
    ai_king_tax_hike_apply(ctx, human, -dos_rng_range(ctx->rng, 1, 5 - d), &flavor);
  } else {
    ai_king_audience_notice(ctx, human, "KINGNOTHING", 0);
  }
}

static void ai_king_audience_specialist_offer(ColonizeTurnContext* ctx, int human);

/* @AUDIENCE answer. Row 2 is DOS's Train dialog (38fd:6372 -> FUN_38fd_41ce);
 * the mod offers the King's own specialists instead. */
void ai_king_audience_choice(ColonizeTurnContext* ctx, int human, int choice) {
  if (!ai_king_audience_ready(ctx, human)) {
    return;
  }
  const ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  switch (choice) {
    case 1:
      ai_king_audience_notice(ctx, human, "KINGBLESS", 0);
      break;
    case 2:
      ai_king_audience_specialist_offer(ctx, human);
      break;
    case 3:
      /* FUN_38fd_44a4: refused at 60% tax or more. */
      if (nat->tax_rate >= 60) {
        ai_king_audience_notice(ctx, human, "KINGNO", 0);
      } else {
        const int grant =
          ai_king_grant_amount(nat->tax_hike_count, (int)ctx->col1->head.difficulty);
        ai_king_audience_choice_popup(ctx, human, AI_POPUP_TAG_KING_GRANT, "KINGFUND", grant, grant);
      }
      break;
    case 4:
      ai_king_audience_petition(ctx, human);
      break;
    case 5:
      ai_king_audience_notice(ctx, human, "KINGLAUGH", 0);
      break;
    default:
      break;
  }
}

/* FUN_38fd_44a4 accept: gold first, then 3dc8(@KINGTAX, hike), then the hike
 * counter (nation +5, written only here). DOS's hike is hikes + d/2 + 5; the
 * mod uses + 1, the + 5 being too steep for the small grants. */
void ai_king_audience_grant_accept(ColonizeTurnContext* ctx, int human, int grant) {
  if (!ai_king_audience_ready(ctx, human) || grant <= 0) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  europe_nation_gold_add(ctx->europe, ctx->col1, human, grant);
  const int hike = (int)nat->tax_hike_count + (int)ctx->col1->head.difficulty / 2 + 1;
  ai_king_tax_hike_apply(ctx, human, hike, NULL);
  nat->tax_hike_count = (uint8_t)(nat->tax_hike_count + 1);
}

/* 1% for the first specialist, doubled for each one after. */
int ai_king_specialist_rise(int earlier) {
  return 1 << (earlier < 0 ? 0 : earlier > 7 ? 7 : earlier);
}

/* @AUDIENCE row 2: the Masters and the Firebrand Preacher (@JOB rows;
 * soldiers, pioneers, plain experts and statesmen are not the King's to give),
 * from Europe's Train list, paid by a tax rise instead of gold. Refused
 * (@KINGNO) when the rise would pass the 75% tax cap. */
static void ai_king_audience_specialist_offer(ColonizeTurnContext* ctx, int human) {
  static const int k_jobs[] = {
    COLONIZE_PROF_DISTILLER, COLONIZE_PROF_TOBACCONIST, COLONIZE_PROF_WEAVER,
    COLONIZE_PROF_FUR_TRADER, COLONIZE_PROF_CARPENTER, COLONIZE_PROF_BLACKSMITH,
    COLONIZE_PROF_GUNSMITH, COLONIZE_PROF_PREACHER,
  };
  const EuropeScreen* eu = ctx->europe;
  if (!eu || !ai_king_human_popups(ctx)) {
    return;
  }
  const int rise = ai_king_specialist_rise(ctx->col1->nation[human].unknown23_pad[2]);
  if ((int)ctx->col1->nation[human].tax_rate + rise > 75) {
    ai_king_audience_notice(ctx, human, "KINGNO", 0);
    return;
  }
  const char* labels[AI_POPUP_CHOICE_MAX];
  int ids[AI_POPUP_CHOICE_MAX];
  int n = 0;
  for (int i = 0; i < eu->train_count && n < AI_POPUP_CHOICE_MAX; ++i) {
    for (size_t k = 0; k < sizeof(k_jobs) / sizeof(k_jobs[0]); ++k) {
      if (eu->train[i].job_index == k_jobs[k]) {
        labels[n] = eu->train[i].expert_name;
        ids[n++] = k_jobs[k];
      }
    }
  }
  if (n == 0) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.number0 = rise;
  tok.has_number0 = true;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "KINGSPECIAL", &tok, "", body, sizeof(body));
  (void)ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_SPECIALIST, human,
                                    ai_king_crown_nation_col1(ctx->col1, human), rise, NULL,
                                    body, labels, ids, n);
}

/* A @KINGSPECIAL pick: the expert on the docks, the tax up by `rise` (no
 * refusal: the price was on the offer), the counter up. */
void ai_king_audience_specialist(ColonizeTurnContext* ctx, int human, int job, int rise) {
  if (!ai_king_audience_ready(ctx, human) || !ctx->europe) {
    return;
  }
  EuropeScreen* eu = ctx->europe;
  int row = -1;
  for (int i = 0; i < eu->train_count; ++i) {
    if (eu->train[i].job_index == job) {
      row = i;
    }
  }
  if (row < 0 || !europe_train_place(eu, row, ctx->rng)) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  ai_king_audience_apply_delta(nat, rise, NULL);
  eu->tax_percent = (int)nat->tax_rate;
  /* The audience's own nation (hot-seat: the human at the controls), the same
   * nation whose tax and 0x20 latch the chamber uses. */
  uint8_t* count = &nat->unknown23_pad[2];
  if (*count < 255) {
    ++*count;
  }
  /* The @UNREST immigrant notice's shape (turn_colony.c): %STRING0 = home
   * port, %STRING1 = the expert. */
  if (ctx->ai_popups && ctx->messages) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.country = eu->nation_name;
    tok.string0 = eu->port_city;
    tok.string1 = eu->train[row].expert_name;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "KINGSPECIALOK", &tok, "", body, sizeof(body));
    if (body[0]) {
      ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    }
  }
}
