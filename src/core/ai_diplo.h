#ifndef COLONIZE_AI_DIPLO_H
#define COLONIZE_AI_DIPLO_H

#include <stdint.h>

#include "core/col1_save.h"
#include "core/turn.h"

/*
 * Euro diplomacy — partial structural port of FUN_15b3_* + FUN_5bfb war/ally.
 * Thin map: original_sources_annotated/ai/euro_diplo.md
 */

/*
 * nation.euro_relation[peer] bit map — re-derived 2026-08-27 from the DOS
 * writers (T1.19): 0x20 MET (FUN_5bfb_022e/3180 first contact), 0x40 PEACE
 * (FUN_5bfb_0182 / 13b0 peace branch / 3844_0442; cleared at every attack
 * site), 0x02 WAR (FUN_465b_0000, 5fef_1b0e, 684c_08c0, 6cb2_24b8 attacks;
 * 13b0 paid @SMITE*), 0x01 planner war-intent (FUN_521d_6d8e, 1-in-4 roll
 * while bit 0x08 is up; cleared by 465b after the attack), 0x10 crown-arms
 * event (FUN_38fd_5930). Same encoding as indian.euro_diplo. Real saves show
 * 00/20/22/60/a0/e0/e2/e8 and the bits are directional (a→b ≠ b→a).
 * AI_DIPLO_ALLY (0x04) is never SET on Euro pairs (DOS uses 0x04 only on
 * Indian pairs as "attack-village confirmed"; the Linux-only alliance
 * machinery that set it was retired T2.4 2026-09-06). Readers left: the
 * self-pair virtual in ai_diplo_read, and ai_king 2244's byte-faithful
 * eligibility check (reduces to self-only, as in DOS). TREASURE_* Linux.
 */
#define AI_DIPLO_WAR 0x02
#define AI_DIPLO_PEACE 0x40
#define AI_DIPLO_ALLY 0x04
#define AI_DIPLO_MET 0x20
#define AI_DIPLO_WAR_INTENT 0x01
#define AI_DIPLO_CROWN_ARMED 0x10 /* FUN_38fd_5930 @KINGNEWWAR: Crown cancelled our peace with this peer */
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
 *
 * 2026-08-27 static close: DOS bit 0x08 = "amicable-negotiation latch".
 * Set only at 153e's common tail (viceroy_unpacked.c:98415) when the pair
 * is already MET and (peace kept or target weaker). Consumed by
 * FUN_521d_6d8e: when the pair's cooldown (unknown26 +0x40..) reads 0 and
 * the latch is set, 1-in-3 per turn → `rel = (rel & 0xb7) | WAR` (clears
 * 0x08|0x40, declares war) — a random relapse after a truce cools off.
 * Also listed by the foreign-affairs report (string 0x42). Linux still
 * uses 0x08 as the TREASURE_STRONGER stand-in; not reconciled here.
 */
#define AI_DIPLO_TREASURE_ALERT 0x80
#define AI_DIPLO_TREASURE_STRONGER 0x08


/*
 * FUN_5bfb_153e phase 1 — negotiation worthiness score for the human-self
 * diplomacy encounter (FUN_5bfb_3180 caller). Real terms as of 2026-08-27:
 * G-table continent tallies (colonies / land units / exposed combat /
 * skilled), the per-colony FUN_5bfb_0000 border probe, the DS:0x53c8
 * cooldown stamp (refreshed to `turn` as a side effect), DS:0xa153 top-
 * ranked nation, Franklin, the treasury clamp. Only the AI-self entry
 * branch (13b0) runs for AI nations — DOS never scores an AI self here.
 * The commit / demand / flavor phases (raw ~594+) are not ported.
 */
typedef struct Ai153eWorthinessScore {
  int handled;         /* raw uStack_8e - did the phase run to completion */
  int worthy;           /* raw bVar12/iStack_a8 at phase end */
  int dominance_bonus;  /* raw iStack_ce */
  int score;             /* raw uStack_68 - feeds the (unported) commit phase */
  int at_war;            /* raw uStack_ae: euro_relation[target][self] bit 0x02,
                             direct (non-accessor) read = WAR (T1.19 bit map;
                             the older "consistent with PEACE" reading is retired) */
  int old_stamp;         /* raw local_8c: DS:0x53c8[target] before the refresh */
  int own_border;        /* raw local_8: Σ border-probe value where the human's units matched */
  int border_value;      /* raw local_b2: Σ (doubled off-continent) target-matched probe value */
  int any_border;        /* raw local_62: any target-matched colony probe */
} Ai153eWorthinessScore;

Ai153eWorthinessScore ai_diplo_153e_worthiness_score(
  ColonizeTurnContext* ctx, int self, int target, int encounter_unit, int forced_gate
);

/* FUN_5bfb_00f8 rank table: DS:0xa153, the top-ranked Euro nation (-1 if no col1). */
int ai_diplo_00f8_top_ranked_nation(const ColonizeCol1Save* col1);

/*
 * FUN_5bfb_3180 Euro x Euro branch -> FUN_5bfb_153e phases 2-4: the human's
 * unit `unit_id` stands next to a unit of AI nation `target`. Runs phase 1
 * (unmet pair / 16-turn cooldown gate) and, when it opens, drives the
 * encounter dialog through ctx->ai_popups (AI_POPUP_TAG_DIPLO_TALK).
 * Returns 1 when a talk started.
 */
int ai_diplo_153e_encounter(ColonizeTurnContext* ctx, int human, int target, int unit_id);

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

/* Thin 102a/1092 status chrome (Contact/King pattern): call existing
 * declare/make_peace then write ctx->status when human is involved; also
 * enqueue AI OK popup when ctx->ai_popups is set (FUN_15b3 / 5bfb). AI
 * callers keep using declare_war / make_peace without status.
 * FA 3f41 full UI PARKED. Linux-only alliance machinery retired T2.4. */
void ai_diplo_set_sound_hook(void (*play_fn)(int id));
void ai_diplo_declare_war_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b);

/* Player-facing nation name (Col1 country_name if set, else "rival"). */
const char* ai_diplo_rival_name(const ColonizeCol1Save* col1, int nation);
void ai_diplo_make_peace_ctx(ColonizeTurnContext* ctx, int nation_a, int nation_b);

/* FUN_5bfb_0000/00f8/312e-shaped military score (unpark #5 deepen). */
int ai_diplo_military_score(const ColonizeTurnContext* ctx, int nation_id);

/* 6d8e step 4: decrement per-rival treaty timer bytes (before planning);
 * also thin peaceful Indian relation drift when not at Euro war. */
void ai_diplo_treaty_timers(ColonizeTurnContext* ctx, int nation_id);

/* Opportunistic war by military balance (5bfb_10ec) + 13b0 treaty
 * sign/cancel tick (Linux-only alliance arms retired T2.4 2026-09-06);
 * at-war Privateer spawn once/war peer on hunt-ready water (unknown26[9]);
 * PARKED 8g treasury prize only when units null (no hold-plunder API);
 * war-fatigue (timer==0) + near-parity → make_peace_ctx;
 * AI→human war/peace offers enqueue CHOICE Accept/Refuse.
 * Franklin FF: NW pair with Benjamin Franklin → skip 10ec declare pressure;
 * at-war → always offer/conclude peace (fandom; FA 3f41 UI PARKED). */
void ai_diplo_euro_balance(ColonizeTurnContext* ctx, int nation_id);

/* Alias → ai_diplo_treaty_timers (6d8e timer pass). */
void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id);

/*
 * DOS-native Indian nation alarm (FUN_15dc_00e0 read / FUN_4cc6_00f2 write):
 * indian[idx].alarm_by_player[euro], 0..100, HIGH = HOSTILE. Map-gen seeds
 * RNG(0,14); first contact clamps <= 20; no per-turn decay (TURN3-7 saves).
 * Use these at sites transcribed from DOS (152e, 1816, 2820, 417e, 0x4b gates).
 */
int ai_diplo_indian_alarm(const ColonizeCol1Save* col1, int indian_nation, int euro_nation);
void ai_diplo_indian_alarm_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
);

/*
 * Linux-side "relation" view of the same store: relation = 100 - alarm
 * (high = friendly), delta d == alarm_delta(-d). Kept for the fandom-derived
 * sites written in relation terms. nation.relation_by_indian is NOT this
 * scalar — in every DOS save it is the 0x60 (MET|PEACE) flag byte.
 */
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

/* Relation (100-alarm) for a met slot, 0 when unmet (euro_diplo MET bit clear). */
uint8_t ai_diplo_indian_read(const ColonizeCol1Save* col1, int euro_nation, int indian_idx);

/* Thin stand-in: at war with Indian nation when met and relation < 50 (alarm > 50). */
int ai_diplo_indian_at_war(const ColonizeCol1Save* col1, int euro_nation, int indian_idx);

/* True if any of 8 Indian slots is at war (relation < 50). Contact/diplo helper. */
int ai_diplo_indian_any_at_war(const ColonizeCol1Save* col1, int euro_nation);

/* Read unknown26[8] Indian hostility sticky: 0 clear, 1 at-war, 2 very-low deepen.
 * sticky==2 → peace feeler self-gates off (matrix + make_peace) + refuses new
 * treaties this balance + human "Natives remain hostile." status. */
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

/* Apply human choice from map AI popup (peace / war Accept/Refuse).
 * Peace Accept → make_peace_ctx; peace Refuse → status + OK; war Accept →
 * declare_war_ctx; war Refuse → status + OK. No-op if tag mismatch,
 * cancelled, or OK (choice_id 0). FUN_5bfb / 15b3 / 10ec / war-fatigue;
 * FA 3f41 full UI PARKED. (Alliance CHOICE arms retired T2.4.) */
void ai_diplo_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup);

#endif
