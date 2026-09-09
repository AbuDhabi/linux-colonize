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
 * eligibility check (reduces to self-only, as in DOS). 0x08 AMICABLE and
 * 0x80 TREASURE_ALERT are DOS bits too — see the block below.
 */
#define AI_DIPLO_WAR 0x02
#define AI_DIPLO_PEACE 0x40
#define AI_DIPLO_ALLY 0x04
#define AI_DIPLO_MET 0x20
#define AI_DIPLO_WAR_INTENT 0x01
#define AI_DIPLO_CROWN_ARMED 0x10 /* FUN_38fd_5930 @KINGNEWWAR: Crown cancelled our peace with this peer */
/*
 * Treasure Train tension bump (euro_unit_act.md). Writer corrected
 * 2026-09-09 to FUN_465b_0000 (viceroy_unpacked.c:75527-75545); the old
 * FUN_4720_049e citation was wrong. DOS sets the real bit 0x80 there
 * (confirmed transient alert, set/cleared elsewhere in FUN_15b3_153e)
 * plus a weaker/stronger follow-up bit, DOS literal "2"/"8".
 *
 * 2026-08-15, `153e` bit-semantics pass: bit 2 confirmed to genuinely BE
 * `AI_DIPLO_PEACE` after all — the earlier "grudge/pressure, don't reuse"
 * caution was built on `153e` call sites that turned out to read a
 * *different* table (`FUN_1000_8c28` is a raw-byte accessor confirmed via
 * its own decompile, `FUN_0000_5b34`: nation param <4 reads exactly
 * `euro_relation`, but >=4 reads the Indian-side flags row at absolute
 * `23000`, stride `0x4e` — the misleading citations were Indian-range
 * calls, not Euro-Euro ones; "wholly separate table" corrected 2026-09-08,
 * `23000 + (t+4)*0x4e` is exactly `indian[t].euro_diplo`, and the Euro
 * branch's own `peer >= 4` half is `nation[].relation_by_indian[peer-4]`
 * — see ai_diplo.c's quadrant map). The real Euro-Euro bit-2
 * sites in `153e` (direct `-0x77c4` reads, no accessor) are consistent
 * with plain `PEACE`: discounts negotiation "worthiness" when already
 * peaceful, and gets set alongside establishing contact (mirrors this
 * port's own `ai_diplo_read` "unmet defaults to PEACE|MET" convention).
 * Thematically fits the treasure mechanic too: a weaker rival responds to
 * a wealthy/strong nation by seeking peace. **Now uses the real DOS bit.**
 *
 * 2026-08-27 static close: DOS bit 0x08 = "amicable-negotiation latch".
 * Set at 153e's common tail (viceroy_unpacked.c:98415) when the pair is
 * already MET and (peace kept or target weaker). Consumed by
 * FUN_521d_6d8e: when the pair's cooldown (unknown26 +0x40..) reads 0 and
 * the latch is set, 1-in-3 per turn → `rel = (rel & 0xb7) | WAR` (clears
 * 0x08|0x40, declares war) — a random relapse after a truce cools off.
 * Also read by the 0a60 continent-stance gate (ai_euro.c) and cleared by
 * FUN_43f7_0108's 0x0b mask. Also listed by the foreign-affairs report
 * (string 0x42).
 *
 * 2026-09-09 collision reconciled (smell #99). The Treasure-Train
 * "stronger rival" follow-up was carried as a Linux-only stand-in named
 * `AI_DIPLO_TREASURE_STRONGER`, and the name implied a second, invented
 * owner of the same bit. **REFUTED — the write is DOS's own.** The real
 * writer is not `FUN_4720_049e` (the old citation, chased down and found
 * wrong) but `FUN_465b_0000` (viceroy_unpacked.c:75527-75545): with the
 * acting unit a Treasure (`+0x3146 == 0x10`) it does
 * `nation[target].euro_relation[actor] |= 0x80`, then on
 * `rng(0,100) < difficulty+1` compares `land_combat_strength` (`-0x6be4`)
 * and ORs **`2` when the target is weaker, `8` when it is not** — the same
 * literal 0x08 the 153e tail sets. So DOS itself writes this bit from two
 * sites into one latch with one consumer; there is no double-booking to
 * split. Renamed `AI_DIPLO_AMICABLE` to stop the name asserting otherwise.
 */
#define AI_DIPLO_TREASURE_ALERT 0x80
/* DOS bit 0x08: amicable-negotiation latch (153e tail + 465b treasure arm). */
#define AI_DIPLO_AMICABLE 0x08


/*
 * FUN_5bfb_153e phase 1 — negotiation worthiness score for the human-self
 * diplomacy encounter (FUN_5bfb_3180 caller). Real terms as of 2026-08-27:
 * G-table continent tallies (colonies / land units / exposed combat /
 * skilled), the per-colony FUN_5bfb_0000 border probe, the DS:0x53c8
 * cooldown stamp (refreshed to `turn` as a side effect), DS:0xa153 top-
 * ranked nation, Franklin, the treasury clamp. Only the AI-self entry
 * branch (13b0) runs for AI nations — DOS never scores an AI self here.
 * The commit / demand / flavor phases (raw ~594+) live in the
 * ai_diplo_153e_encounter talk machine (@WANTSTUFF included, 2026-09-06).
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
  int own_border;        /* raw local_8: Σ border-probe value where the human's units matched.
                             2026-09-06 asm note (OVL16 0x1BED-0x1C7B): the probe call site
                             pushes param_3 (target) fixed, and FUN_5bfb_0000 only ever writes
                             param_4 or -1 into its matched-out — so the matched==param_2
                             branch is DEAD in DOS and own_border is always 0. Linux
                             faithfully reproduces that (probe never returns self). */
  int border_value;      /* raw local_b2: Σ (doubled off-continent) target-matched probe value */
  int any_border;        /* raw local_62: any target-matched colony probe */
  int forced;            /* raw iStack_c: forced-conflict flag AFTER the Franklin /
                             difficulty-threshold overrides — phase 2's rival-tally
                             demotion is skipped while it is set (raw :97650). */
} Ai153eWorthinessScore;

Ai153eWorthinessScore ai_diplo_153e_worthiness_score(
  ColonizeTurnContext* ctx, int self, int target, int encounter_unit, int forced_gate
);

/* FUN_5bfb_00f8 rank table: DS:0xa153, the top-ranked Euro nation (-1 if no col1). */
int ai_diplo_00f8_top_ranked_nation(const ColonizeCol1Save* col1);

/*
 * DS:0x9566 (`−0x6a9a`), stride 3, indexed by Euro nation — the per-leader
 * trait triple. **Resolved 2026-09-06d**: the writer is the NAMES.TXT
 * loader (`viceroy_unpacked.c:120960-120977`), whose `@LEADERNAME` pass
 * reads one name string into `DS:0x540e + nation*0x34` and then three
 * numbers into `−0x6a9a[nation*3 + 0..2]`. In the shipped NAMES.TXT:
 *
 *   Walter Raleigh        1, -1,  0     (England)
 *   Jacques Cartier       0,  1,  0     (France)
 *   Christopher Columbus  1,  0, -1     (Spain)
 *   Michiel De Ruyter    -1,  0,  1     (Netherlands)
 *
 * Signed bytes in {−1, 0, +1}. Column 0 is the belligerence term: it is
 * subtracted from the "how many rivals am I already at odds with" tally in
 * `FUN_5bfb_10ec` (war-worthiness) and in `FUN_521d_153e`'s peace-pressure
 * count, so a higher value means "declares war on thinner grounds".
 * Column 1 feeds `FUN_521d_03d0` (`4 − t1` divisor, `t1*3 − 7`), column 2
 * `FUN_5952_035e`'s `(t2 + 2) * 50`; those two are read but not yet wired
 * in this port. Live copy confirmed in every DOS save (Stuff file-off 0,
 * Linux `unknown34_pad`) and in the `original_memory_dumps` DOSBox-X
 * `Memory` blobs at DS:237D + 0x9566.
 *
 * `column` 0..2, `nation` 0..3; 0 when unavailable.
 */
int ai_diplo_leader_trait(const ColonizeTurnContext* ctx, int nation, int column);

/*
 * FUN_5bfb_3180 Euro x Euro branch -> FUN_5bfb_153e phases 2-4: the human's
 * unit `unit_id` stands next to a unit of AI nation `target`. Runs phase 1
 * (unmet pair / 16-turn cooldown gate) and, when it opens, drives the
 * encounter dialog through ctx->ai_popups (AI_POPUP_TAG_DIPLO_TALK).
 * Returns 1 when a talk started.
 */
int ai_diplo_153e_encounter(ColonizeTurnContext* ctx, int human, int target, int unit_id);

/*
 * FUN_15b3_0004 / 0032 / 0066 / 00d0 (decomp 9056-9117). Byte-audited
 * 2026-09-08: both sides take the FULL 0..11 nation space (4 Euro + 8
 * Indian) and the pair resolves to one of four Linux fields —
 * nation[].euro_relation[] / nation[].relation_by_indian[] /
 * indian[].euro_diplo[] / indian[].unknown33_pad[] — see the quadrant map
 * on ai_diplo_flag_byte in ai_diplo.c. Out-of-range reads 0 / writes nothing.
 * Self-pair is a Linux virtual (read → PEACE|ALLY, write → no-op); DOS has
 * no self case, so byte-faithful callers must go to the field directly.
 */
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

/* Military strength = the DS:0x941c census mirror stuff.land_combat_strength[]
 * (Σ FUN_281f_09c8(u,1) over land units; combat byte ×8). Same quantity every
 * other strength comparison in the port reads. The invented attack+defense /
 * pop / gold / rank blend it used to compute was retired 2026-09-09 (#51). */
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

/* At war with an Indian nation when met and either the euro_diplo WAR bit is
 * set or relation < AI_DIPLO_INDIAN_AT_WAR_REL (26), i.e. DOS alarm > 0x4a —
 * the FUN_5bfb_153e hostile tier. (Comment said 50 until 2026-09-09, smell
 * #57; the live constant has been 26 since the 153e band was ported.) */
int ai_diplo_indian_at_war(const ColonizeCol1Save* col1, int euro_nation, int indian_idx);

/* True if any of 8 Indian slots is at war (see ai_diplo_indian_at_war: WAR bit
 * or relation < 26). Contact/diplo helper. */
int ai_diplo_indian_any_at_war(const ColonizeCol1Save* col1, int euro_nation);

/* Read the Linux Indian-hostility sticky (nation record +0x4b, unknown26[11],
 * the one byte of that block DOS never touches): 0 clear, 1 at-war, 2 very-low
 * deepen. Moved off +0x48 2026-09-09 (smell #52) — that byte is the DOS
 * FUN_4d56_4528 grace/waiver counter.
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
