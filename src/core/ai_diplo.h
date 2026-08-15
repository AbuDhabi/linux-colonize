#ifndef COLONIZE_AI_DIPLO_H
#define COLONIZE_AI_DIPLO_H

#include <stdint.h>

#include "core/col1_save.h"
#include "core/turn.h"

/*
 * Euro diplomacy — partial structural port of FUN_15b3_* + FUN_5bfb war/ally.
 * Thin map: original_sources_annotated/ai/euro_diplo.md
 */

#define AI_DIPLO_WAR 0x01
#define AI_DIPLO_PEACE 0x02
#define AI_DIPLO_ALLY 0x04
#define AI_DIPLO_MET 0x40
/*
 * FUN_4720_049e Treasure Train tension bump (euro_unit_act.md). DOS sets
 * the real bit 0x80 here (confirmed transient alert, set/cleared
 * elsewhere in FUN_15b3_153e) plus a weaker/stronger follow-up bit, DOS
 * literal "2"/"8".
 *
 * 2026-08-15, `153e` bit-semantics pass: bit 2 confirmed to genuinely BE
 * `AI_DIPLO_PEACE` after all — the earlier "grudge/pressure, don't reuse"
 * caution was built on `153e` call sites that turned out to read a
 * *different* table (`FUN_1000_8c28` is a raw-byte accessor confirmed via
 * its own decompile, `FUN_0000_5b34`: nation param <4 reads exactly
 * `euro_relation`, but >=4 reads a wholly separate Indian-side flags
 * table at absolute `23000`, stride `0x4e` — the misleading citations
 * were Indian-range calls, not Euro-Euro ones). The real Euro-Euro bit-2
 * sites in `153e` (direct `-0x77c4` reads, no accessor) are consistent
 * with plain `PEACE`: discounts negotiation "worthiness" when already
 * peaceful, and gets set alongside establishing contact (mirrors this
 * port's own `ai_diplo_read` "unmet defaults to PEACE|MET" convention).
 * Thematically fits the treasure mechanic too: a weaker rival responds to
 * a wealthy/strong nation by seeking peace. **Now uses the real DOS bit.**
 *
 * Bit 8: found one Euro-Euro site (`153e` line ~1361, `*pbVar3 |= 8`),
 * gated on a local (`iStack_b0`) that gets set when the pair is already
 * MET *and* (already peaceful *or* the other nation is weaker) — reads
 * as "negotiation concluded / other party already amenable", not a
 * clean parallel to bit 2's plain peace-seeking. Doesn't cleanly match
 * this mechanic's "stronger rival" branch (the opposite condition) —
 * kept as a Linux-only stand-in (`AI_DIPLO_TREASURE_STRONGER`) rather
 * than reuse a bit whose real DOS role points the other way; fully
 * mapping it would need `153e`'s own `iStack_a8`/negotiation-flow depth,
 * out of scope for this specific bit-semantics pass.
 */
#define AI_DIPLO_TREASURE_ALERT 0x80
#define AI_DIPLO_TREASURE_STRONGER 0x08

uint8_t ai_diplo_read(const ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_write(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t value);
void ai_diplo_or_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits);
void ai_diplo_clear_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits);

int ai_diplo_at_war(const ColonizeCol1Save* col1, int nation_a, int nation_b);
/* War-turn helper alias of ai_diplo_at_war (pair). */
int ai_diplo_at_war_with(const ColonizeCol1Save* col1, int nation_a, int nation_b);
/* True if Euro nation is at war with any other Euro (feeler / drift / lift gate). */
int ai_diplo_at_war_with_any(const ColonizeCol1Save* col1, int nation);
/* First declare: thin 153e sting + war-hit. Franklin pair → no-op (fandom NW peace). */
void ai_diplo_declare_war(ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_make_peace(ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_form_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b);
void ai_diplo_break_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b);

/* Thin 102a/1092 status chrome (Contact/King pattern): call existing
 * declare/make_peace/form_alliance/break_alliance then write ctx->status when
 * human is involved; also enqueue AI OK popup when ctx->ai_popups is set
 * (FUN_15b3 / 5bfb). form_alliance_ctx: first form → "Alliance formed with %s";
 * prefer "Alliance with %s costs gold." when 25g drains. AI callers keep using
 * declare_war / make_peace / form_alliance / break_alliance without status.
 * FA 3f41 full UI PARKED. */
void ai_diplo_declare_war_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b);

/* Player-facing nation name (Col1 country_name if set, else "rival"). */
const char* ai_diplo_rival_name(const ColonizeCol1Save* col1, int nation);
void ai_diplo_make_peace_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b);
void ai_diplo_form_alliance_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b);
void ai_diplo_break_alliance_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b);

/* FUN_5bfb_0000/00f8/312e-shaped military score (unpark #5 deepen). */
int ai_diplo_military_score(const ColonizeTurnContext* ctx, int nation_id);

/* 6d8e step 4: decrement per-rival treaty timer bytes (before planning);
 * also thin peaceful Indian relation drift when not at Euro war. */
void ai_diplo_treaty_timers(ColonizeTurnContext* ctx, int nation_id);

/* Opportunistic war/ally by military balance (5bfb_10ec/13b0; not timer slot).
 * Also thin FA ally-aid + FA gift while allied (full 3f41 PARKED);
 * FA gift/longevity human status ("Alliance with %s strengthened/holds") +
 * thin Foreign Affairs OK (DIPLO_FA tag + "Foreign Affairs" title);
 * at-war Privateer spawn once/war peer on hunt-ready water (unknown26[9]);
 * PARKED 8g treasury prize only when units null (no hold-plunder API);
 * war-fatigue (timer==0) + near-parity → make_peace_ctx;
 * AI→human war/peace/alliance/break offers enqueue CHOICE Accept/Refuse.
 * Franklin FF: NW pair with Benjamin Franklin → skip 10ec declare pressure;
 * at-war → always offer/conclude peace (fandom; FA 3f41 UI PARKED). */
void ai_diplo_euro_balance(ColonizeTurnContext* ctx, int nation_id);

/* Thin FA 3f41 goodwill gift: 15g from→to + both treaty timers +2 when
 * donor gold >= 100 and peer gold < donor*2. euro_balance calls when ALLY
 * and timer==1; if gift no-ops, longevity timer+1 (no second gold) + human
 * alliance longevity status. Full FA dialog UI PARKED. */
void ai_diplo_fa_gift(ColonizeCol1Save* col1, int from, int to);

/* Alias → ai_diplo_treaty_timers (6d8e timer pass). */
void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id);

/* FUN_4cc6_00f2 / 15dc_00e0-shaped Indian relation scalar (not full 15b3). */
void ai_diplo_indian_relation_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
);

/* Read-only pair of relation_delta: indian_nation 4..11 → Euro cell.
 * Contact/king consumers; does not invent combat %. */
uint8_t ai_diplo_indian_relation(
  const ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation
);

/* Thin Indian×Euro matrix cell: relation_by_indian[indian_idx] (0..7). */
uint8_t ai_diplo_indian_read(const ColonizeCol1Save* col1, int euro_nation, int indian_idx);

/* Thin stand-in: at war with Indian nation when relation < 50.
 * Fuller Indian×Euro 15b3 matrix Done structural (unpark #5); deep/VGA PARKED;
 * sticky unknown26[8]. */
int ai_diplo_indian_at_war(const ColonizeCol1Save* col1, int euro_nation, int indian_idx);

/* True if any of 8 Indian slots is at war (relation < 50). Contact/diplo helper. */
int ai_diplo_indian_any_at_war(const ColonizeCol1Save* col1, int euro_nation);

/* Read unknown26[8] Indian hostility sticky: 0 clear, 1 at-war, 2 very-low deepen.
 * sticky==2 → peace feeler self-gates off (matrix + make_peace) + refuses new
 * alliances this balance + skips FA gift to peers (no gold) + human
 * "Natives remain hostile." / alliance-refuse status. */
uint8_t ai_diplo_indian_hostility_sticky(const ColonizeCol1Save* col1, int euro_nation);

/* Sync sticky from relation matrix (set/clear/deepen). Call after relation hits. */
void ai_diplo_indian_hostility_sync(ColonizeCol1Save* col1, int euro_nation);

/*
 * Fandom capital-destroy surrender: reset alarm/friction toward euro, set
 * indian peace bit, floor relation. Cite: docs/fandom_col1994.md Capital destroy.
 */
void ai_diplo_indian_capital_surrender(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation
);

/* Apply human choice from map AI popup (alliance / peace / war / break
 * Accept/Refuse). Alliance Accept → form_alliance_ctx (follow-up OK
 * "Alliance formed…" + treaty timer ≥8 if was 0); peace Accept →
 * make_peace_ctx; peace Refuse → status + OK; war Accept →
 * declare_war_ctx; war Refuse → status + OK; break Accept →
 * break_alliance_ctx; break Refuse → status + OK. No-op if tag mismatch,
 * cancelled, or OK (choice_id 0). FUN_5bfb_13b0 / 15b3 / 10ec /
 * war-fatigue; FA 3f41 full UI PARKED. Thin FA gift/longevity OK uses
 * AI_POPUP_TAG_DIPLO_FA + title "Foreign Affairs". */
void ai_diplo_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup);

#endif
