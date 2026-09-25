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
 * Gate: turn counter (DS:0x538e, Linux ctx->turn_number) >= 30; interval
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
   * is refreshed every turn (turn.c → col1_stuff_census_refresh_colony_counts),
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
  const int crown_adjust = diff - 2; /* "is human" branch — always true here */
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
   */
  const int score =
    dos_rng_range(ctx->rng, 1, 1000) +
    (col1->head.rebel_sentiment_report * 2 - (int)nat->tax_rate) * 5 +
    (int)(europe_nation_gold(ctx->europe, col1, human) / 100u) +
    (int)col1->stuff.census_pop_proxy[human] +
    (int)(turn / 30);

  AiKingAudienceFlavor flavor;
  memset(&flavor, 0, sizeof(flavor));
  flavor.section = "KINGTAX";
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
                     col1->player[human].country_name[0]
                       ? col1->player[human].country_name
                       : "the colonies");
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
 * answered (bugs.md — the raise must not be in the save while the popup that
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
 * local_7a[c] = the roulette weight. *out_weights, when set, points at a
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
   * roulette if one of those colonies actually holds some of it
   * (`aiStack_cc[local_ac] != 0` gates both the weight sum and the walk):
   * you cannot dump 0 tons in protest, and you cannot dump it inland
   * (bugs.md #904).
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
   * DOS-LITERAL FUN_38fd_3dc8 raw 64146-64159: the roulette weight is the
   * nation's cumulative traded tonnage, not any Europe price —
   *   `local_7a[c] = FUN_1d1d_0ec6(FUN_1d1d_0ddc(tons_lo, tons_hi), 100)`
   * i.e. the LOW WORD of labs(*(int32*)(DS:0x84fc + 0xbc + c*4)) * 100.
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
   * live Europe bid instead (bugs.md #903).
   */
  if (ctx->col1_ok && ctx->col1 && human >= 0 && human < 4) {
    const int32_t* tons = ctx->col1->nation[human].trade.tons;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      int32_t t = tons[c];
      if (t < 0) {
        t = -t;
      }
      weight_buf[c] = (int)(int16_t)(uint16_t)((uint32_t)t * 100u);
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
 * just-applied hike, boycott the roulette-picked cargo, confiscate its
 * stock (ai_king_enqueue_teaparty_ok dumps up to 100 tons from the
 * richest human colony — thin stand-in for the colony-array seize into
 * DOS's own royal-stock pile, real field unresolved, see file header).
 * Used by both the human CHOICE-apply path and the no-popups auto path.
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
   * Accept, so refusing simply never commits it (bugs.md). DOS's
   * apply-then-revert reaches the same rate.
   */
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | (uint16_t)(1u << cargo));
  ai_king_set_boycott(ctx->col1, 1);

  if (ctx->status && ctx->status_size) {
    snprintf(
      ctx->status,
      ctx->status_size,
      "Audience: tea party! Tax stays at %u%%. %s boycotted in Europe.",
      nat->tax_rate,
      reports_cargo_display_name(cargo)
    );
  }
  if (ai_king_human_popups(ctx)) {
    ai_king_enqueue_teaparty_ok(ctx, human, cargo);
  }
}

void ai_king_apply_dump_goods_choice(ColonizeTurnContext* ctx, int human, int cargo) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | (uint16_t)(1u << cargo));
  if (ctx->status && ctx->status_size) {
    char cargos[96];
    if (ai_king_format_boycott_cargos(cargos, sizeof(cargos), nat->boycott_bitmap)) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Dump goods: boycotted in Europe: %s.",
        cargos
      );
    }
  }
  /* GAME.TXT @TEAPARTY for the chosen dump cargo (thin 3dc8 stock dump). */
  if (ai_king_human_popups(ctx)) {
    ai_king_enqueue_teaparty_ok(ctx, human, cargo);
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
void ai_king_tax_hike_apply(
  ColonizeTurnContext* ctx,
  int human,
  int delta,
  const AiKingAudienceFlavor* flavor
);

void ai_king_tax_event(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  /* Fugger / external bitmap clear → drop the boycott presentation flag. */
  ai_king_sync_boycott_refuse(ctx->col1, human);

  int delta = 0;
  AiKingAudienceFlavor flavor;
  memset(&flavor, 0, sizeof(flavor));
  if (!ai_king_audience_roll(ctx, human, &delta, &flavor)) {
    return; /* no audience this turn: interval gate, or degenerate 0% cut */
  }
  ai_king_tax_hike_apply(ctx, human, delta, &flavor);
}

/*
 * FUN_38fd_3dc8 body for an explicit delta — the audience roll above and
 * the @KINGFRIGATE acceptance (3844_00f2 → 3dc8(KINGTAX, 10)) both land
 * here: clamp + apply, then the Kiss-the-ring / Tea-party dialog.
 */
/*
 * Write a settled tax delta: rate, the Europe mirror, and the REF-growth
 * approximation. Split out of ai_king_tax_hike_apply so the CHOICE path can
 * defer all three until the player has answered (bugs.md).
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
   * bugs.md: DOS's 3dc8 applies the delta and then offers keep-vs-revert, so
   * the raise is briefly real while the dialog asks about it — the port
   * showed the new rate in Europe/status before the player had answered.
   * The arithmetic outcome of both orderings is identical (accept = raised,
   * tea party = not raised), so the hike is now previewed here and committed
   * in ai_king_apply_popup_result on Accept. Everything DOS never asks about
   * (cuts, fully clamped deltas, the no-cargo fallback) still commits at
   * once.
   */
  const int applied = ai_king_audience_preview_delta(nat, delta);

  if (applied < 0) {
    ai_king_tax_commit(ctx, human, delta);
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "Audience: the King lowers taxes to %u%%.", nat->tax_rate);
    }
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
      char fallback[AI_POPUP_BODY_LEN];
      snprintf(fallback, sizeof(fallback),
               "The King, moved by your poverty, lowers taxes to %u%%.", nat->tax_rate);
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, section, &tok, fallback, body, sizeof(body));
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
  const int* bids = NULL;
  const uint16_t candidate_mask = ai_king_teaparty_candidate_mask(ctx, human, weight_buf, &bids);
  int picked = ctx->rng
    ? ai_king_pick_dump_goods_cargo(nat->boycott_bitmap, candidate_mask, ctx->rng, bids)
    : -1;
  /*
   * DOS-LITERAL FUN_38fd_3dc8 raw 64176-64200: there is NO retry. Boycotted
   * cargos never enter aiStack_cc[] at all (the colony scan, raw 64160-64175,
   * only fills cc[c] under `(local_a6 & 1<<c) == 0`), so the roulette walk
   * `if (aiStack_cc[local_ac] != 0)` can only ever land on a non-boycotted
   * stocked cargo, and raw 64196's guard
   *   `if ((local_4 < 0) || ((int)param_2 < 0) ||
   *        ((local_a6 & 1 << local_4) != 0))`
   * ABORTS the whole party — the hike simply stands, no second draw with the
   * boycott mask cleared. The port used to re-roll with boycott = 0, which
   * both invented a party DOS never holds and burnt an extra RNG draw
   * (bugs.md #907).
   */

  if (picked < 0) {
    /* No eligible (stocked, un-boycotted, coastal) cargo, no RNG (tests), or
     * popups disabled for this nation — DOS's own choice UI has nothing to
     * drive here either, so there is nothing to defer: the hike stands. */
    ai_king_tax_commit(ctx, human, delta);
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "Audience: the King raises taxes to %u%%.", nat->tax_rate);
    }
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
      char fallback[AI_POPUP_BODY_LEN];
      snprintf(fallback, sizeof(fallback), "The King raises taxes to %u%%.", nat->tax_rate);
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, section, &tok, fallback, body, sizeof(body));
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
     * after both clamps), not a constant 1 — bugs.md: popup claimed "raise
     * by 1%" for a 7% hike. */
    tok.number0 = applied;
    tok.has_number0 = true;
    /* @KINGTAX "The tax rate is now {%NUMBER1%%}" — the total the raise would
     * come to. The rate itself is still the old one until Accept. */
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
      /* bugs.md: DOS's 3dc8 dialog sets DS:0x1f5c = 8 — the animated King
       * flair stands beside the tax audience. */
      ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
      return; /* effect deferred to ai_king_apply_popup_result */
    }
    /* Queue full — fall through to auto resolve. */
  }

  /*
   * Auto path (no popups attached): DOS's tea-party choice is inherently
   * player-interactive — there is no documented AI/auto answer. Stand-in
   * heuristic (invented, not a decode): tea-party when the hike pushes tax
   * to a high band and SoL/liberty bells suggest the colonies would balk.
   */
  const int sol = ai_king_sol_percent(ctx, human);
  const int auto_teaparty =
      (proposed >= AI_KING_BOYCOTT_TAX_MIN) &&
      (sol >= AI_KING_BOYCOTT_SOL_MIN || nat->liberty_bells_pool >= AI_KING_BOYCOTT_BELLS_MIN);
  if (auto_teaparty) {
    ai_king_tax_teaparty(ctx, human, picked);
    return;
  }
  ai_king_tax_commit(ctx, human, delta);
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size,
             "Audience: the King raises taxes to %u%%.", nat->tax_rate);
  }
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
 *     the field (`0302(x,y)==0 → 0808`), map owner/vis and tribe-alarm
 *     nibbles are remapped (thin: skipped here);
 *   - @SUCCESSION (0x128c) announces the Treaty of Utrecht;
 *   - merged slot control=2, DS:0x53d2 = the vacated slot.
 * THIS is why the King must never inherit a live nation's estate (bugs.md
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
  /* Colonies: owner swap + rebel accumulators zeroed (DOS +0xc2/+0xc4). */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == merged) {
        c->nation_id = heir;
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
  /* Units: in a colony → transfer to the heir; in the field/at sea → gone
   * (DOS 0302==0 → 0808 despawn). */
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != merged) {
        continue;
      }
      const int cid =
        ctx->colonies ? colonies_id_at(ctx->colonies, u->x, u->y) : -1;
      if (cid >= 0) {
        units_set_nation(u, heir);
      } else {
        (void)units_despawn(ctx->units, u->id);
      }
    }
  }
  col1->player[merged].control = 2; /* withdrawn — the King's future slot */
  col1->head.crown_nation_id = (int16_t)merged;
  /* @SUCCESSION Treaty of Utrecht announcement. */
  if (ai_king_human_popups(ctx)) {
    const char* ceder = reports_nation_country_name(merged);
    const char* domain = col1->player[merged].country_name[0]
                           ? col1->player[merged].country_name
                           : "its colonies";
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
    popup_msg_fill(ctx->messages, "SUCCESSION", &tok, "", body, sizeof(body));
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_INFO, human, merged, heir, NULL, body
    );
  }
  if (ctx->status && ctx->status_size && ctx->status[0] == '\0') {
    const char* ceder = reports_nation_country_name(merged);
    const char* domain = col1->player[merged].country_name[0]
                           ? col1->player[merged].country_name
                           : "its colonies";
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
 * withdraws other Euros; thin 160a rename; market_demand_pool_raw[5] congress.
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
   * inheriting a live nation's colonies and ships (bugs.md follow-up). */
  ai_king_succession(ctx);
  ai_king_set_independence(ctx->col1, 1); /* WoI: market_demand_pool_raw[0] if not already */
  ai_king_write_rival_nation_slots(ctx->col1, human);
  /* FUN_43f7_2564 congress-confirm stand-in. */
  ai_king_latch_set(ctx->col1, AI_KING_CONGRESS_BYTE, 1);
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
    const int diff = ctx->col1->head.difficulty;
    ctx->col1->head.expeditionary_force[0] = (uint16_t)(8 * diff + 15);
    ctx->col1->head.expeditionary_force[1] = (uint16_t)(5 * (diff + 1));
    ctx->col1->head.expeditionary_force[2] = (uint16_t)(3 * diff + 2);
    ctx->col1->head.expeditionary_force[3] = (uint16_t)(6 * diff + 2);
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
   * bugs.md #933: +0xc IS the live FF/intervention bell pool, so this also
   * clears the human's Europe mirror — the WoI intervention threshold must
   * be met with bells produced after the declaration, not before it. */
  founding_fathers_reset_bells_pool(ctx, human);
  ai_king_set_ref_present(ctx->col1, 1);
  /*
   * bugs.md: nothing ever set the WAR bit between the rebel and the
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
  /* bugs.md: no landing on the declaration turn itself. */
  ai_king_latch_set(ctx->col1, AI_KING_REF_WAVE_WAIT_BYTE, 1);
  /*
   * FUN_43f7_0108 (eliminate nation), called from FUN_43f7_1a26 for every
   * nation that is neither the declaring human nor the crown proxy
   * (DS:0x5398 / 0x53d2 gate) -- WoI narrows the world to rebel vs REF, so
   * the other Euro powers are fully removed: diplomatic status withdrawn
   * *and* every unit they own destroyed (colonies are untouched by 0108
   * itself -- DOS leaves them ownerless/inert once their nation's
   * status=2). Linux already set control=2 here ("withdrawn"); the
   * unit-scrub half was missing. Crown-nation units (the REF spawns below)
   * must survive.
   */
  const int crown_fold = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  /*
   * bugs.md #228: DOS 1a26 stores the crown slot in DS:0x53d2 and sets its
   * control byte to 1 (`*(0x53d2*0x34+0x543f)=1`). The port never wrote
   * head.crown_nation_id, so a save exported mid-WoI reached DOS with
   * 0x53d2 = -1; DOS then picked its own crown — sometimes the very nation
   * the port had cached as the intervention ally (0x53d4), which made the
   * intervention force render as "Tory".
   */
  ctx->col1->head.crown_nation_id = (int16_t)crown_fold;
  /*
   * bugs.md #229: DOS 1a26 zeroes the bell pool (`*(pool+0xc)=0`) — the FF
   * election in progress is cancelled and bells start accruing toward the
   * foreign intervention instead.
   */
  founding_fathers_consume_woi_bell_pool(ctx, human);
  ctx->col1->nation[human].next_founding_father = -1;
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
       * Linux-invented second owner of the bit; DOS writes 0x08 both here,
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
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
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
  }
  /*
   * FUN_43f7_1a26 tail (43f7:1c07..1c1b): every human unit's MP is spent via
   * FUN_281f_0934 (unit_exhaust_mp) — "This will end our turn" (@DECLARE).
   */
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
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
   * Congress status below; same-turn 0982/1528 wave may overwrite if it spawns
   * (wave only writes status when non-empty arrival — leave congress if empty).
   * Human queue: thin rename OK + WoI-begins OK (FUN_43f7_160a / 1a26 chain).
   * Letter chrome: KING_LETTER body carries the @INDEPENDENCE wording; the
   * signing animation (DECOIND.PIK + DEC-UPP/LOW/SQIG.SS) is core/declaration.c.
   * DECLARAT.PIK is an unused leftover — no DOS executable references it.
   */
  /* bugs.md #239: no "United Colonies" rename — DOS 160a is only the signing
   * cinematic and never touches country_name. Under the WoI the player
   * faction reads "Rebels" (LABELS 84/101) via units_combat_nation_label. */
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "Congress declares independence!");
  }
  if (ai_king_human_popups(ctx)) {
    /* FUN_43f7_160a rename OK; the cinematic rides on the KING_LETTER tag. */
    const char* leader =
      (human >= 0 && human < 4 && ctx->col1->player[human].name[0] != '\0')
        ? ctx->col1->player[human].name
        : "Washington";
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
    /* bugs.md #236: @HOWTOWIN does NOT fire at the declaration. DOS shows it
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
 * The `*(byte*)0x5381 & 0x80` branch gating a *first* popup (msg 0x138e)
 * ahead of the real confirm is NOT a "recommend declare" one-shot notice —
 * traced where that bit is ever *set*: only in `FUN_75c2_10ae`
 * (viceroy_unpacked.c:120544-120694), the new-game nation-select/setup
 * screen, when more than one nation slot is flagged human (`iVar5 > 1`,
 * counting bits in the per-nation "is human" mask at 0x1f54). That's a
 * hotseat/multi-human-player marker, not a per-turn SoL event — the extra
 * dialog picks *which* human player is declaring (`*(u16*)0x5398 =
 * *(u16*)0x5394` after it returns) before falling into the same @DECLARE
 * confirm every path shares. `ColonizeTurnContext` models exactly one
 * `human_nation`, so this branch has no reachable port equivalent — the
 * single Never/Yes @DECLARE popup below is the complete behavior for the
 * port's single-human model, not a missing feature. (0x138e's exact
 * wording was never recovered — not needed since the branch is
 * unreachable; the `*(byte*)0x5382&1` else-branch's msg 0x1374, shown when
 * already at war, is likewise unrecovered and likewise not required since
 * `ai_king_independence_declared` already short-circuits that case above.)
 */
/*
 * Shared @DECLARE Never/Yes confirm body — called once `sol` is already
 * known ≥ AI_KING_DECLARE_SOL_MIN, from both the automatic per-turn check
 * (ai_king_try_declare) and the menu-invoked path
 * (ai_king_menu_declare_independence).
 */
static void ai_king_show_declare_choice(ColonizeTurnContext* ctx, int human, int sol) {
  if (ai_king_human_popups(ctx)) {
    /* bugs.md #235: %STRING0 is the Crown nation ("England"), never the
     * player's new-world country_name ("New England"). NAMES.TXT @COUNTRY. */
    const char* motherland =
      (human >= 0 && human <= 3) ? reports_nation_country_name(human) : "the Crown";
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
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size,
                 "Congress debates independence (SoL %d%%).", sol);
      }
      return;
    }
    /* Queue full — fall through to auto declare. */
  }
  ai_king_do_declare(ctx, human);
}

void ai_king_try_declare(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int sol = ai_king_sol_percent(ctx, human);
  if (sol < AI_KING_DECLARE_SOL_MIN) {
    return;
  }
  /*
   * bugs.md: DOS never spawns the Never/Yes @DECLARE confirm on its own at a
   * SoL threshold. FUN_43f7_2564 has no call site in the decompile at all —
   * it is reached only from the MENU.TXT @GAME "DECLARE INDEPENDENCE" command
   * (ai_king_menu_declare_independence). Firing it per turn produced a
   * persistent popup at SoL 50 that the original does not have. Keep the
   * SoL≥min auto-declare only for the headless/AI path (no popup queue), where
   * there is nobody to answer a CHOICE.
   */
  if (ai_king_human_popups(ctx)) {
    return;
  }
  ai_king_show_declare_choice(ctx, human, sol);
}

/*
 * Menu-invoked DECLARE INDEPENDENCE (MENU.TXT @GAME item, MAP_MENU_ACTION_
 * DECLARE_INDEPENDENCE). DOS FUN_43f7_2564 reached from this same menu
 * command at any SoL, not just once auto-eligible — the per-turn
 * ai_king_try_declare check above only ever calls in once already ≥
 * AI_KING_DECLARE_SOL_MIN, so the sol<min branch (GAME.TXT @TOOTORY) was
 * unreachable there. This entry point makes it reachable: below the
 * threshold, show @TOOTORY (Congress won't back a rebellion yet) as a
 * plain OK notice; at/above threshold, show the same Never/Yes @DECLARE
 * confirm ai_king_try_declare would auto-fire on its next turn — so
 * declining "Not yet" from the auto-popup can be revisited here on demand
 * instead of only ever re-asked wholesale next turn.
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
    /* Port-authored notice: no GAME.TXT section covers this re-entry case
     * (menu DECLARE INDEPENDENCE after already at war with the Crown). */
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "Independence already declared.");
    }
    return;
  }
  const int sol = ai_king_sol_percent(ctx, human);
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
  ai_king_show_declare_choice(ctx, human, sol);
}
