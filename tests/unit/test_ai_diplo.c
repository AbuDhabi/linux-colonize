/* Smoke: bilateral 15b3 diplo bytes, war gold/tax sting,
 * Euro war does not boycott Europe cargos, war-fatigue
 * peace + status, make_peace + full wartime mask lift + peace feeler restore
 * (sticky==1; sticky==2 refuses treaties), upkeep + human upkeep status,
 * privateer prize + human status + prize stops after peace, treaty timer
 * decrement, Indian drift/feeler status/sticky pressure,
 * Sugar/Tobacco/Tools + first newly boycotted cargo status +
 * Indian war-hit status chrome + war −5 relation floor + R13 war-fatigue peer
 * chrome / Peace concluded + R14 full wartime mask declare/peace smoke +
 * Marathon3 R1 Benjamin Franklin NW peace gate (declare no-op / euro_balance
 * skip war pressure / at-war always offer peace) + R2 spawn-only Privateer
 * (PARK 8g prize when units set) + Franklin Peace concluded human chrome +
 * R4 Franklin at-war skips upkeep/PARK prize (gold unchanged) +
 * Marathon4 R1 Privateer commission is status-only (no INFO OK popup).
 * (Linux-only Euro×Euro alliance machinery + its tests retired T2.4
 * 2026-09-06 — DOS has no Euro×Euro alliances.) */
#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* War embargo: Food=0, Sugar=1, Tobacco=2, Cotton=3, Furs=4, Lumber=5,
 * Ore=6, Silver=7, Horses=8, Rum=9, Cigars=10, Cloth=11, Coats=12,
 * Trade Goods=13, Tools=14, Muskets=15. */
#define AI_DIPLO_SMOKE_FOOD_BIT (1u << COLONIZE_CARGO_FOOD)
#define AI_DIPLO_SMOKE_EMBARGO_BIT (1u << COLONIZE_CARGO_FURS)
#define AI_DIPLO_SMOKE_TOBACCO_BIT (1u << COLONIZE_CARGO_TOBACCO)
#define AI_DIPLO_SMOKE_SUGAR_BIT (1u << COLONIZE_CARGO_SUGAR)
#define AI_DIPLO_SMOKE_COTTON_BIT (1u << COLONIZE_CARGO_COTTON)
#define AI_DIPLO_SMOKE_LUMBER_BIT (1u << COLONIZE_CARGO_LUMBER)
#define AI_DIPLO_SMOKE_HORSES_BIT (1u << COLONIZE_CARGO_HORSES)
#define AI_DIPLO_SMOKE_RUM_BIT (1u << COLONIZE_CARGO_RUM)
#define AI_DIPLO_SMOKE_CIGARS_BIT (1u << COLONIZE_CARGO_CIGARS)
#define AI_DIPLO_SMOKE_CLOTH_BIT (1u << COLONIZE_CARGO_CLOTH)
#define AI_DIPLO_SMOKE_COATS_BIT (1u << COLONIZE_CARGO_COATS)
#define AI_DIPLO_SMOKE_ORE_BIT (1u << COLONIZE_CARGO_ORE)
#define AI_DIPLO_SMOKE_SILVER_BIT (1u << COLONIZE_CARGO_SILVER)
#define AI_DIPLO_SMOKE_TRADE_GOODS_BIT (1u << COLONIZE_CARGO_TRADE_GOODS)
#define AI_DIPLO_SMOKE_TOOLS_BIT (1u << COLONIZE_CARGO_TOOLS)
#define AI_DIPLO_SMOKE_MUSKETS_BIT (1u << COLONIZE_CARGO_MUSKETS)
/* Full wartime boycott mask (all 16 @CARGO bits) for make_peace clear assert. */
#define AI_DIPLO_SMOKE_WARTIME_MASK                                              \
  (AI_DIPLO_SMOKE_FOOD_BIT | AI_DIPLO_SMOKE_SUGAR_BIT | AI_DIPLO_SMOKE_TOBACCO_BIT | \
   AI_DIPLO_SMOKE_COTTON_BIT | AI_DIPLO_SMOKE_EMBARGO_BIT | AI_DIPLO_SMOKE_LUMBER_BIT | \
   AI_DIPLO_SMOKE_ORE_BIT | AI_DIPLO_SMOKE_SILVER_BIT | AI_DIPLO_SMOKE_HORSES_BIT | \
   AI_DIPLO_SMOKE_RUM_BIT | AI_DIPLO_SMOKE_CIGARS_BIT | AI_DIPLO_SMOKE_CLOTH_BIT | \
   AI_DIPLO_SMOKE_COATS_BIT | AI_DIPLO_SMOKE_TRADE_GOODS_BIT | AI_DIPLO_SMOKE_TOOLS_BIT | \
   AI_DIPLO_SMOKE_MUSKETS_BIT)

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_diplo: FAIL %s\n", msg);
  return 1;
}

int main(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  /* Defend against memset quirks: FF slots stay unclaimed (−1). */
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    col1.head.founding_father[i] = -1;
  }
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }

  /* Pair independence: war(0,1) must not force war(0,2).
   * Thin 153e: first declare drains 100 gold + bumps tax_rate both sides.
   * Indians dislike war: −5 on relation_by_indian[0..7] both sides.
   * Euro war does not boycott Europe cargos (DOS king tea-party only).
   * War fatigue: seed peer treaty timer to 8 when was 0. */
  col1.nation[0].gold = 250;
  col1.nation[1].gold = 80;
  col1.nation[2].gold = 500;
  col1.nation[0].tax_rate = 10;
  col1.nation[1].tax_rate = 74;
  col1.nation[2].tax_rate = 20;
  col1.nation[3].tax_rate = 75; /* cap probe via separate declare later */
  col1.nation[0].unknown26[1] = 0;
  col1.nation[1].unknown26[0] = 0;
  for (int i = 0; i < 8; ++i) {
    col1.indian[i].alarm_by_player[0] = 50; /* relation 50 */
    col1.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.indian[i].alarm_by_player[1] = 80; /* DOS bands: relation 40 */
    col1.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    col1.indian[i].alarm_by_player[2] = 0; /* relation 100 */ /* untouched by war(0,1) */
    col1.indian[i].euro_diplo[2] |= COL1_INDIAN_MET_BIT;
  }
  ai_diplo_declare_war(&col1, 0, 1);
  if (!ai_diplo_at_war(&col1, 0, 1) || !ai_diplo_at_war(&col1, 1, 0)) {
    return fail("declare_war should be symmetric for pair 0-1");
  }
  if (ai_diplo_at_war(&col1, 0, 2) || ai_diplo_at_war(&col1, 2, 0)) {
    return fail("war(0,1) must not set war(0,2)");
  }
  if (ai_diplo_read(&col1, 0, 1) & AI_DIPLO_PEACE) {
    return fail("at-war pair should not keep PEACE");
  }
  if (col1.nation[0].gold != 150) {
    return fail("declare_war should drain 100 gold from nation 0");
  }
  if (col1.nation[1].gold != 0) {
    return fail("declare_war gold sting should floor at 0");
  }
  if (col1.nation[2].gold != 500) {
    return fail("war(0,1) must not drain gold of nation 2");
  }
  if (col1.nation[0].tax_rate != 11) {
    return fail("declare_war should bump tax_rate +1 on nation 0");
  }
  if (col1.nation[1].tax_rate != 75) {
    return fail("declare_war tax bump should cap at 75");
  }
  if (col1.nation[2].tax_rate != 20) {
    return fail("war(0,1) must not bump tax of nation 2");
  }
  /* DOS does not boycott Europe cargos on Euro×Euro war (king tea-party only). */
  if ((col1.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
      (col1.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
    return fail("declare_war must not set Europe boycott bits");
  }
  if ((col1.nation[2].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
    return fail("war(0,1) must not set wartime embargo bits on nation 2");
  }
  if (!ai_diplo_at_war_with(&col1, 0, 1) || ai_diplo_at_war_with(&col1, 0, 2)) {
    return fail("ai_diplo_at_war_with should mirror at_war for pair checks");
  }
  if (!ai_diplo_at_war_with_any(&col1, 0) || ai_diplo_at_war_with_any(&col1, 2)) {
    return fail("ai_diplo_at_war_with_any should detect any Euro×Euro war");
  }
  if (col1.nation[0].unknown26[1] != 8 || col1.nation[1].unknown26[0] != 8) {
    return fail("declare_war should seed war-fatigue treaty timer to 8 when was 0");
  }
  /* 2026-09-03: DOS declare-war sites never touch Indian relations (alarm
   * grows only via the 152e accumulator) — the −5/−10 war hit was retired
   * (bugs: "natives grow hostile" popped on a Euro-vs-Euro attack). */
  for (int i = 0; i < 8; ++i) {
    if (ai_diplo_indian_relation(&col1, 4 + (i), 0) != 50) {
      return fail("declare_war must not change Indian relations for nation 0");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (i), 1) != 20) {
      return fail("declare_war must not change Indian relations for nation 1");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (i), 2) != 100) {
      return fail("war(0,1) must not change Indian relations of nation 2");
    }
  }
  if (ai_diplo_indian_read(&col1, 0, 0) != 50 || ai_diplo_indian_at_war(&col1, 0, 0)) {
    return fail("indian_read/at_war: nation 0 slot0 rel 50 is not at war (DOS band: rel<26)");
  }
  if (!ai_diplo_indian_at_war(&col1, 1, 0)) {
    return fail("indian_at_war: nation 1 slot0 should be at war (rel 20 < 26)");
  }
  if (ai_diplo_indian_at_war(&col1, 2, 0)) {
    return fail("indian_at_war: nation 2 slot0 should be peaceful (rel=100)");
  }
  /* Re-declare: no second sting / tax bump / Indian hit; still no Europe boycott. */
  ai_diplo_declare_war(&col1, 0, 1);
  if (col1.nation[0].gold != 150) {
    return fail("re-declare_war should not re-sting gold");
  }
  if (col1.nation[0].tax_rate != 11) {
    return fail("re-declare_war should not re-bump tax");
  }
  if (ai_diplo_indian_relation(&col1, 4 + (0), 0) != 50) {
    return fail("re-declare_war should not re-hit Indian relations");
  }
  if ((col1.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
    return fail("re-declare_war must not boycott Europe cargos");
  }

  /* make_peace: clear WAR both ways, set PEACE, lift all wartime boycotts
   * (AI_DIPLO_SMOKE_WARTIME_MASK) when no Euro wars remain. No gold cost.
   * Nation 0 still only at war with 1 here. */
  {
    const uint16_t gold0 = col1.nation[0].gold;
    const uint16_t gold1 = col1.nation[1].gold;
    ai_diplo_make_peace(&col1, 0, 1);
    if (ai_diplo_at_war(&col1, 0, 1) || ai_diplo_at_war(&col1, 1, 0)) {
      return fail("make_peace should clear WAR both ways");
    }
    if ((ai_diplo_read(&col1, 0, 1) & AI_DIPLO_PEACE) == 0 ||
        (ai_diplo_read(&col1, 1, 0) & AI_DIPLO_PEACE) == 0) {
      return fail("make_peace should set PEACE both ways");
    }
    /* R11: bitmap clear of full wartime mask (all 16 @CARGO bits incl. Cotton). */
    if ((col1.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
        (col1.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
      return fail("make_peace should clear all wartime boycott bits when no Euro wars remain");
    }
    if (col1.nation[0].gold != gold0 || col1.nation[1].gold != gold1) {
      return fail("make_peace should not change gold (no cost)");
    }
  }

  /* Re-war for upkeep tests below. */
  ai_diplo_declare_war(&col1, 0, 1);
  if ((col1.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
      (col1.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
    return fail("re-war after make_peace must not boycott Europe cargos");
  }

  /* euro_balance at-war upkeep (before timer pass can PEACE-tweak zero timers).
   * Neutralize privateer by keeping peer gold equal after upkeep (or both 0).
   * Raise Indian relations so harassment −2 does not mix into upkeep asserts. */
  {
    ColonizeDosRng rng_up;
    dos_rng_seed(&rng_up, 1);
    uint32_t turn_up = 1;
    char status_up[128];
    status_up[0] = '\0';
    ColonizeTurnContext ctx_up;
    memset(&ctx_up, 0, sizeof(ctx_up));
    ctx_up.col1 = &col1;
    ctx_up.col1_ok = true;
    ctx_up.rng = &rng_up;
    ctx_up.turn_number = &turn_up;
    ctx_up.human_nation = 0;
    ctx_up.status = status_up;
    ctx_up.status_size = sizeof(status_up);
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      col1.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    col1.nation[0].gold = 40;
    col1.nation[1].gold = 35; /* after −5 upkeep: equal → no privateer */
    ai_diplo_euro_balance(&ctx_up, 0);
    if (col1.nation[0].gold != 35) {
      return fail("euro_balance at-war should drain 5 gold upkeep");
    }
    if (col1.nation[1].gold != 35) {
      return fail("euro_balance upkeep must not move gold when treasuries equal");
    }
    if (strcmp(status_up, "War upkeep costs gold.") != 0) {
      fprintf(stderr, "unit_ai_diplo: upkeep status '%s'\n", status_up);
      return fail("euro_balance should status War upkeep costs gold for human");
    }
    /* AI-only: no upkeep status overwrite. */
    snprintf(status_up, sizeof(status_up), "keep");
    ctx_up.human_nation = 2;
    col1.nation[0].gold = 40;
    col1.nation[1].gold = 35;
    ai_diplo_euro_balance(&ctx_up, 0);
    if (strcmp(status_up, "keep") != 0) {
      return fail("war upkeep status must not write for AI-only nation");
    }
    ctx_up.human_nation = 0;
    col1.nation[0].gold = 3;
    col1.nation[1].gold = 0; /* after floor: equal 0 → no privateer */
    ai_diplo_euro_balance(&ctx_up, 0);
    if (col1.nation[0].gold != 0) {
      return fail("euro_balance upkeep should floor gold at 0");
    }
    status_up[0] = '\0';
    ai_diplo_euro_balance(&ctx_up, 0);
    if (col1.nation[0].gold != 0) {
      return fail("euro_balance upkeep should no-op when gold already 0");
    }
    if (status_up[0] != '\0') {
      return fail("war upkeep status must not write when gold already 0");
    }
    if (col1.nation[2].gold != 500) {
      return fail("euro_balance upkeep must not drain peaceful peer treasury");
    }
  }

  /* Thin Indian harassment: relation<50 → −2 gold once per euro_balance tick.
   * Sticky sync: set 1 at-war, deepen 2 when very-low (<40), clear when none. */
  {
    ColonizeDosRng rng_h;
    dos_rng_seed(&rng_h, 2);
    uint32_t turn_h = 2;
    ColonizeTurnContext ctx_h;
    memset(&ctx_h, 0, sizeof(ctx_h));
    ctx_h.col1 = &col1;
    ctx_h.col1_ok = true;
    ctx_h.rng = &rng_h;
    ctx_h.turn_number = &turn_h;
    /* Peace with all Euros so upkeep/privateer do not fire; only harassment. */
    ai_diplo_make_peace(&col1, 0, 1);
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[0] = 80; /* DOS bands: relation 40 */ /* at war vs Indians, not very-low */
      col1.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    col1.nation[0].unknown26[8] = 0;
    col1.nation[0].gold = 20;
    ai_diplo_euro_balance(&ctx_h, 0);
    if (col1.nation[0].gold != 18) {
      return fail("euro_balance Indian harassment should drain 2 gold");
    }
    /* Cap: gold already 0 → no invent-below-zero; gold 1 → one floor to 0. */
    col1.nation[0].gold = 0;
    ai_diplo_euro_balance(&ctx_h, 0);
    if (col1.nation[0].gold != 0) {
      return fail("Indian harassment must not invent gold below 0 when already 0");
    }
    col1.nation[0].gold = 1;
    ai_diplo_euro_balance(&ctx_h, 0);
    if (col1.nation[0].gold != 0) {
      return fail("Indian harassment should floor once to 0 when gold < 2");
    }
    col1.nation[0].gold = 18; /* restore for sticky asserts below */
    if (ai_diplo_indian_hostility_sticky(&col1, 0) != 1) {
      return fail("indian_at_war should set unknown26[8] hostility sticky once");
    }
    if (!ai_diplo_indian_any_at_war(&col1, 0) || !ai_diplo_indian_at_war(&col1, 0, 0)) {
      return fail("indian_at_war/any_at_war should remain true at rel 40");
    }
    /* Second tick: sticky stays 1 (idempotent at rel 40). */
    ai_diplo_euro_balance(&ctx_h, 0);
    if (ai_diplo_indian_hostility_sticky(&col1, 0) != 1) {
      return fail("Indian hostility sticky must stay set");
    }
    /* Very-low deepen: relation < 40 → sticky 2. */
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[0] = 90; /* DOS bands: relation 30 */
      col1.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    ai_diplo_indian_hostility_sync(&col1, 0);
    if (ai_diplo_indian_hostility_sticky(&col1, 0) != 2) {
      return fail("sticky should deepen to 2 when any relation < 40");
    }
    /* Clear when all slots recover above at-war floor. */
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      col1.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    ai_diplo_indian_hostility_sync(&col1, 0);
    if (ai_diplo_indian_hostility_sticky(&col1, 0) != 0) {
      return fail("sticky should clear when no indian_at_war slots remain");
    }
    if (ai_diplo_indian_any_at_war(&col1, 0)) {
      return fail("any_at_war should be false after recover");
    }
    /* Restore Euro war for privateer/upkeep follow-ons; clear Indian hostility. */
    ai_diplo_declare_war(&col1, 0, 1);
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      col1.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      col1.indian[i].alarm_by_player[1] = 0; /* relation 100 */
      col1.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    ai_diplo_indian_hostility_sync(&col1, 0);
    ai_diplo_indian_hostility_sync(&col1, 1);
  }

  /* Thin privateer prize: richer→poorer 8g (separate from 5g upkeep); no units → treasury-only. */
  {
    ColonizeDosRng rng_pr;
    dos_rng_seed(&rng_pr, 4);
    uint32_t turn_pr = 4;
    ColonizeTurnContext ctx_pr;
    memset(&ctx_pr, 0, sizeof(ctx_pr));
    ctx_pr.col1 = &col1;
    ctx_pr.col1_ok = true;
    ctx_pr.rng = &rng_pr;
    ctx_pr.turn_number = &turn_pr;
    /* Precondition: still at war(0,1); nation 1 poorer. */
    if (!ai_diplo_at_war(&col1, 0, 1)) {
      return fail("privateer setup: nation 0-1 should be at war");
    }
    col1.nation[0].gold = 200;
    col1.nation[1].gold = 50;
    ai_diplo_euro_balance(&ctx_pr, 1); /* poorer nation tick */
    /* poorer: 50 − 5 upkeep + 8 prize = 53; richer: 200 − 8 = 192 */
    if (col1.nation[1].gold != 53) {
      return fail("euro_balance privateer should add 8 gold to poorer after upkeep");
    }
    if (col1.nation[0].gold != 192) {
      return fail("euro_balance privateer should take 8 gold from richer peer");
    }
    /* Donor below 8: no prize (upkeep still applies). Peer is richer but broke. */
    col1.nation[0].gold = 7;
    col1.nation[1].gold = 6; /* after −5: 1; peer 7 still richer, donor < 8 */
    ai_diplo_euro_balance(&ctx_pr, 1);
    if (col1.nation[1].gold != 1 || col1.nation[0].gold != 7) {
      return fail("privateer prize should require donor gold >= 8");
    }
    /* R7: human privateer status chrome when prize transfers. */
    {
      char status_pr[128];
      status_pr[0] = '\0';
      snprintf(col1.player[0].country_name, sizeof(col1.player[0].country_name), "England");
      ctx_pr.human_nation = 1;
      ctx_pr.status = status_pr;
      ctx_pr.status_size = sizeof(status_pr);
      col1.nation[0].gold = 200;
      col1.nation[1].gold = 50;
      for (int i = 0; i < 8; ++i) {
        col1.indian[i].alarm_by_player[1] = 0; /* relation 100 */
        col1.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
      }
      col1.nation[1].unknown26[8] = 0;
      ai_diplo_euro_balance(&ctx_pr, 1);
      if (strcmp(status_pr, "Privateer prize from England") != 0) {
        fprintf(stderr, "unit_ai_diplo: privateer status '%s'\n", status_pr);
        return fail("euro_balance privateer should status when human is a party");
      }
      /* AI-only tick: no status overwrite. */
      snprintf(status_pr, sizeof(status_pr), "keep");
      ctx_pr.human_nation = 2;
      col1.nation[0].gold = 200;
      col1.nation[1].gold = 50;
      ai_diplo_euro_balance(&ctx_pr, 1);
      if (strcmp(status_pr, "keep") != 0) {
        return fail("privateer status must not write for AI-only pairs");
      }
      ctx_pr.human_nation = -1;
      ctx_pr.status = NULL;
      ctx_pr.status_size = 0;
      col1.player[0].country_name[0] = '\0';
    }
  }

  /* 6d8e step 4: treaty timer decrement (the Linux-only ally expiry-break arm
   * was retired with the alliance machinery, T2.4 2026-09-06). */
  {
    col1.nation[0].unknown26[2] = 1; /* timer toward peer 2 → expires */
    /* Keep other peer timers non-zero so expiry does not PEACE-tweak war(0,1). */
    col1.nation[0].unknown26[1] = 5;
    col1.nation[0].unknown26[3] = 5;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 1);
    uint32_t turn = 1;
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.col1 = &col1;
    ctx.col1_ok = true;
    ctx.rng = &rng;
    ctx.turn_number = &turn;
    const uint8_t rel0_at_war = ai_diplo_indian_relation(&col1, 4 + (0), 0);
    if (!ai_diplo_at_war(&col1, 0, 1)) {
      return fail("precondition: nation 0 should still be at war with 1");
    }
    ai_diplo_treaty_timers(&ctx, 0);
    if (col1.nation[0].unknown26[2] != 0) {
      return fail("timer should decrement to 0");
    }
    if (!ai_diplo_at_war(&col1, 0, 1)) {
      return fail("timer pass must not clear war(0,1) when peer-1 timer live");
    }
    /* No Indian drift from the timer pass (DOS has no per-turn alarm decay). */
    col1.nation[0].unknown26[1] = 5;
    col1.nation[0].unknown26[3] = 5;
    ai_diplo_treaty_timers(&ctx, 0);
    if (ai_diplo_indian_relation(&col1, 4 + (0), 0) != rel0_at_war) {
      return fail("treaty_timers must not drift Indian relations while at war");
    }
  }

  /* Peaceful Indian drift: +1 per slot under 160, cap 160; skip if at war. */
  {
    ColonizeDosRng rng_d;
    dos_rng_seed(&rng_d, 2);
    uint32_t turn_d = 2;
    ColonizeTurnContext ctx_d;
    memset(&ctx_d, 0, sizeof(ctx_d));
    ctx_d.col1 = &col1;
    ctx_d.col1_ok = true;
    ctx_d.rng = &rng_d;
    ctx_d.turn_number = &turn_d;
    /* Nation 3 is peaceful (no war with anyone after break of 2-3 ally).
     * Drift only climbs slots below peaceful meet floor 96 (seed-100). */
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[3] = (uint16_t)(100 - (uint8_t)(50 + i));
      col1.indian[i].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    }
    col1.indian[7].alarm_by_player[3] = 4; /* relation 96 */ /* at meet floor — no drift */
    col1.indian[7].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    /* 2026-08-27: DOS alarm has no per-turn drift (seed-100 TURN3..7 saves) — must stay put. */
    ai_diplo_treaty_timers(&ctx_d, 3);
    ai_diplo_treaty_timers(&ctx_d, 3);
    for (int i = 0; i < 7; ++i) {
      if (ai_diplo_indian_relation(&col1, 4 + (i), 3) != (uint8_t)(50 + i)) {
        return fail("treaty_timers must not drift Indian alarm (no DOS decay)");
      }
    }
    if (ai_diplo_indian_relation(&col1, 4 + (7), 3) != 96) {
      return fail("treaty_timers must not drift Indian alarm at meet floor either");
    }
  }

  /*
   * Unpark #5: peace feeler — once per euro_balance, Euro at peace with peers,
   * mid/high Indian slots (≥50, <96) heal +2 toward content floor 96
   * (seed-100 TURN3 meet baseline). Hostile slots (<50) untouched; Euro×Euro
   * war skips feeler.
   */
  {
    ColonizeDosRng rng_f;
    dos_rng_seed(&rng_f, 6);
    uint32_t turn_f = 6;
    ColonizeTurnContext ctx_f;
    memset(&ctx_f, 0, sizeof(ctx_f));
    ctx_f.col1 = &col1;
    ctx_f.col1_ok = true;
    ctx_f.rng = &rng_f;
    ctx_f.turn_number = &turn_f;
    /* Ensure nation 3 has no Euro wars (declare 2-3 may still be live). */
    ai_diplo_make_peace(&col1, 2, 3);
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[3] = 10; /* relation 90 */
      col1.indian[i].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    }
    col1.indian[0].alarm_by_player[3] = 80; /* DOS bands: relation 40 */ /* at-war slot: no feeler */
    col1.indian[0].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    col1.indian[1].alarm_by_player[3] = 5; /* relation 95 */ /* near floor: clamp to 96 */
    col1.indian[1].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    col1.indian[2].alarm_by_player[3] = 4; /* relation 96 */ /* already at floor */
    col1.indian[2].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    col1.nation[3].unknown26[8] = 1;
    col1.nation[3].gold = 50; /* harassment will −2 (slot0 at war) */
    ai_diplo_euro_balance(&ctx_f, 3);
    if (ai_diplo_indian_relation(&col1, 4 + (0), 3) != 20) {
      return fail("peace feeler must not heal indian_at_war slots");
    }
    /* 2026-08-27: feeler heal retired (no DOS counterpart) — slots unchanged. */
    if (ai_diplo_indian_relation(&col1, 4 + (1), 3) != 95) {
      return fail("retired feeler must not touch near-floor slot");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (2), 3) != 96) {
      return fail("retired feeler must leave floor slot");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (3), 3) != 90) {
      return fail("retired feeler must not heal mid slots");
    }
    if (ai_diplo_indian_hostility_sticky(&col1, 3) != 1) {
      return fail("feeler tick should keep sticky while slot0 still at war");
    }
    if (col1.nation[3].gold != 48) {
      return fail("feeler tick with at-war Indian should still harass −2g");
    }
    /* Clear last hostile slot → sticky clears; no further harassment. */
    col1.indian[0].alarm_by_player[3] = 20; /* relation 80 */
    col1.indian[0].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    col1.nation[3].gold = 50;
    ai_diplo_euro_balance(&ctx_f, 3);
    if (ai_diplo_indian_relation(&col1, 4 + (0), 3) != 80) {
      return fail("retired feeler must not heal recovered slot");
    }
    if (ai_diplo_indian_hostility_sticky(&col1, 3) != 0) {
      return fail("sticky should clear after feeler when no at-war slots");
    }
    if (col1.nation[3].gold != 50) {
      return fail("no Indian harassment when sticky cleared / no at-war");
    }
    /* Euro×Euro war: feeler skipped (relations unchanged). */
    col1.nation[2].gold = 200;
    col1.nation[3].gold = 200;
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[3] = 10; /* relation 90 */
      col1.indian[i].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    }
    ai_diplo_declare_war(&col1, 2, 3);
    /* declare −5 → 85; sticky sync from hit */
    const uint8_t after_war = ai_diplo_indian_relation(&col1, 4 + (3), 3);
    ai_diplo_euro_balance(&ctx_f, 3);
    if (ai_diplo_indian_relation(&col1, 4 + (3), 3) != after_war) {
      return fail("peace feeler must skip while Euro at war with peers");
    }
    ai_diplo_make_peace(&col1, 2, 3);

    /*
     * R7/R9: make_peace restores Indian feeler when sticky was at-war (==1) —
     * nudge once via existing feeler path after WAR clear. sticky==2 refuses.
     */
    col1.nation[2].gold = 200;
    col1.nation[3].gold = 200;
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[3] = 10; /* relation 90 */
      col1.indian[i].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    }
    /* Slot0 rel 45 (< 50) → sticky at-war (==1) once the matrix tick syncs.
     * 2026-09-03: the declare-war Indian hit is retired — relations are
     * untouched by the declaration itself. */
    col1.indian[0].alarm_by_player[3] = 78; /* DOS bands: relation 45 */
    col1.indian[0].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    ai_diplo_declare_war(&col1, 2, 3);
    if (ai_diplo_indian_relation(&col1, 4 + (3), 3) != 90) {
      return fail("declare_war must not move Indian relations (war hit retired)");
    }
    ai_diplo_euro_balance(&ctx_f, 3); /* matrix tick syncs sticky from slots */
    if (ai_diplo_indian_hostility_sticky(&col1, 3) != 1) {
      return fail("peace-restore setup: sticky should be at-war (==1) after sync");
    }
    ai_diplo_make_peace(&col1, 2, 3);
    /* Feeler retired: mid slot stays 90; sub-50 slot untouched; sticky stays. */
    if (ai_diplo_indian_relation(&col1, 4 + (3), 3) != 90) {
      return fail("make_peace must not move Indian alarm (feeler retired)");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (0), 3) != 22) {
      return fail("make_peace feeler must not heal sub-50 slots");
    }
    /* Idempotent: already peaceful + sticky elevated must not re-nudge. */
    ai_diplo_make_peace(&col1, 2, 3);
    if (ai_diplo_indian_relation(&col1, 4 + (3), 3) != 90) {
      return fail("make_peace feeler nudge must not re-fire when already peaceful");
    }
    /* Sticky clear: no feeler nudge on peace. */
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[3] = 10; /* relation 90 */
      col1.indian[i].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    }
    col1.nation[3].unknown26[8] = 0;
    ai_diplo_declare_war(&col1, 2, 3);
    /* All mid after −5 → sticky clear; force sticky clear + mid 80. */
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[3] = 20; /* relation 80 */
      col1.indian[i].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    }
    col1.nation[3].unknown26[8] = 0;
    ai_diplo_make_peace(&col1, 2, 3);
    if (ai_diplo_indian_relation(&col1, 4 + (3), 3) != 80) {
      return fail("make_peace must not feeler-nudge when sticky was clear");
    }
    /* R9: sticky==2 refuses make_peace feeler restore (self-gated). */
    for (int i = 0; i < 8; ++i) {
      col1.indian[i].alarm_by_player[3] = 10; /* relation 90 */
      col1.indian[i].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    }
    col1.indian[0].alarm_by_player[3] = 100; /* relation 0 — very-low → sticky deep */
    col1.indian[0].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
    ai_diplo_declare_war(&col1, 2, 3);
    ai_diplo_euro_balance(&ctx_f, 3); /* sync sticky from matrix (war hit retired) */
    if (ai_diplo_indian_hostility_sticky(&col1, 3) != 2) {
      return fail("sticky2 make_peace setup should deepen sticky to 2");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (3), 3) != 90) {
      return fail("sticky2 make_peace setup: mid should stay 90 (war hit retired)");
    }
    ai_diplo_make_peace(&col1, 2, 3);
    if (ai_diplo_indian_relation(&col1, 4 + (3), 3) != 90) {
      return fail("make_peace must not feeler-nudge when sticky==2");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (0), 3) != 0) {
      return fail("sticky2 make_peace must leave very-low slot untouched");
    }
  }

  /*
   * Unpark #5: human status chrome when Indian sticky rises/clears.
   */
  {
    ColonizeCol1Save st;
    col1_save_init(&st);
    memset(st.nation, 0, sizeof(st.nation));
    for (int i = 0; i < 4; ++i) {
      st.player[i].control = 0;
    }
    char status[128];
    status[0] = '\0';
    ColonizeDosRng rng_st;
    dos_rng_seed(&rng_st, 7);
    uint32_t turn_st = 7;
    ColonizeTurnContext ctx_st;
    memset(&ctx_st, 0, sizeof(ctx_st));
    ctx_st.col1 = &st;
    ctx_st.col1_ok = true;
    ctx_st.rng = &rng_st;
    ctx_st.turn_number = &turn_st;
    ctx_st.human_nation = 0;
    ctx_st.status = status;
    ctx_st.status_size = sizeof(status);
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 80; /* DOS bands: relation 40 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    st.nation[0].unknown26[8] = 0;
    st.nation[0].gold = 30;
    ai_diplo_euro_balance(&ctx_st, 0);
    if (strcmp(status, "Natives grow hostile.") != 0) {
      fprintf(stderr, "unit_ai_diplo: indian sticky status '%s'\n", status);
      return fail("euro_balance should status when sticky rises for human");
    }
    /* Clear hostility → improve status. */
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 20; /* relation 80 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    status[0] = '\0';
    ai_diplo_euro_balance(&ctx_st, 0);
    if (strcmp(status, "Native tensions ease.") != 0) {
      fprintf(stderr, "unit_ai_diplo: indian clear status '%s'\n", status);
      return fail("euro_balance should status when sticky clears for human");
    }
    /* AI nation tick must not overwrite human status. */
    snprintf(status, sizeof(status), "keep");
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[1] = 90; /* DOS bands: relation 30 */
      st.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    st.nation[1].unknown26[8] = 0;
    ai_diplo_euro_balance(&ctx_st, 1);
    if (strcmp(status, "keep") != 0) {
      return fail("indian sticky status must only write for human nation");
    }
  }

  /* tax_rate already at cap stays put on first declare; embargo set again. */
  col1.nation[2].gold = 200;
  col1.nation[3].gold = 200;
  col1.nation[2].tax_rate = 75;
  col1.nation[3].tax_rate = 75;
  col1.nation[2].boycott_bitmap = 0;
  col1.nation[3].boycott_bitmap = 0;
  ai_diplo_declare_war(&col1, 2, 3);
  if (col1.nation[2].tax_rate != 75 || col1.nation[3].tax_rate != 75) {
    return fail("declare_war must not raise tax_rate above 75");
  }
  if ((col1.nation[2].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
      (col1.nation[3].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
    return fail("declare_war(2,3) must not boycott Europe cargos");
  }

  /* Indian relation delta clamps. */
  col1.indian[0].alarm_by_player[0] = 2; /* relation 98 */
  col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
  ai_diplo_indian_relation_delta(&col1, 4, 0, 20);
  if (ai_diplo_indian_relation(&col1, 4 + (0), 0) != 100) {
    return fail("indian delta should clamp at 100 (alarm floor 0)");
  }
  col1.indian[0].alarm_by_player[0] = 95; /* relation 5 */
  col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
  ai_diplo_indian_relation_delta(&col1, 4, 0, -20);
  if (ai_diplo_indian_relation(&col1, 4 + (0), 0) != 0) {
    return fail("indian delta should clamp at 0");
  }

  /*
   * R11 (rewritten 2026-09-03): the war Indian hit is retired — declare_war
   * leaves every Indian slot untouched, met or unmet.
   */
  {
    ColonizeCol1Save wf;
    col1_save_init(&wf);
    memset(wf.nation, 0, sizeof(wf.nation));
    for (int i = 0; i < 4; ++i) {
      wf.player[i].control = 0;
    }
    wf.nation[0].gold = 200;
    wf.nation[1].gold = 200;
    for (int i = 0; i < 8; ++i) {
      wf.indian[i].alarm_by_player[0] = 97; /* relation 3 */
      wf.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      wf.indian[i].euro_diplo[1] = (uint8_t)(wf.indian[i].euro_diplo[1] & ~COL1_INDIAN_MET_BIT); /* unmet */
    }
    ai_diplo_declare_war(&wf, 0, 1);
    for (int i = 0; i < 8; ++i) {
      if (ai_diplo_indian_relation(&wf, 4 + (i), 0) != 3) {
        return fail("declare_war must leave Indian relations untouched (hit retired)");
      }
      if (ai_diplo_indian_read(&wf, 1, i) != 0) {
        return fail("unmet slot must still read 0 after declare_war");
      }
    }
  }

  /*
   * Unpark #5: 153e trade deepen — colony gap ≥2 → extra 25g from richer
   * (Tools already OR'd on every first declare).
   * Military score: units + pop*2 + gold/50 (+ sea/fort weights).
   */
  {
    ColonizeCol1Save tw;
    col1_save_init(&tw);
    memset(tw.nation, 0, sizeof(tw.nation));
    for (int i = 0; i < 4; ++i) {
      tw.player[i].control = 0;
    }
    tw.head.colony_count = 3;
    tw.colony = calloc(3, sizeof(ColonizeCol1Colony));
    if (!tw.colony) {
      return fail("trade-war alloc colonies");
    }
    tw.colony[0].nation_id = 0;
    tw.colony[1].nation_id = 0;
    tw.colony[2].nation_id = 1; /* gap 2−1=1 — need gap≥2 */
    /* Rebuild: nation0 has 3, nation1 has 0 → gap 3. */
    tw.colony[2].nation_id = 0;
    tw.nation[0].gold = 400;
    tw.nation[1].gold = 50;
    ai_diplo_declare_war(&tw, 0, 1);
    if ((tw.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
        (tw.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
      free(tw.colony);
      return fail("colony-gap declare must not boycott Europe cargos");
    }
    /* 400 − 100 sting − 25 trade = 275 */
    if (tw.nation[0].gold != 275) {
      fprintf(stderr, "unit_ai_diplo: rich gold after trade war %u (want 275)\n",
              (unsigned)tw.nation[0].gold);
      free(tw.colony);
      return fail("colony-gap declare should drain extra 25 from richer");
    }
    if (tw.nation[1].gold != 0) {
      free(tw.colony);
      return fail("poorer still floored by 100 sting");
    }
    ai_diplo_make_peace(&tw, 0, 1);
    if ((tw.nation[0].boycott_bitmap & (1u << COLONIZE_CARGO_TOOLS)) != 0 ||
        (tw.nation[0].boycott_bitmap & (1u << COLONIZE_CARGO_FURS)) != 0 ||
        (tw.nation[0].boycott_bitmap & (1u << COLONIZE_CARGO_TOBACCO)) != 0) {
      free(tw.colony);
      return fail("make_peace should lift Furs+Tobacco+Tools embargo");
    }
    free(tw.colony);

    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 2;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
    units.types[0].attack = 2;
    units.types[0].defense = 2;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(units.types[1].name, sizeof(units.types[1].name), "Caravel");
    units.types[1].attack = 1;
    units.types[1].defense = 1;
    units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
    const int sid = units_spawn(&units, 0, 1, 1);
    const int ship = units_spawn(&units, 1, 2, 2);
    if (sid < 0 || ship < 0) {
      return fail("military score spawn");
    }
    units.units[sid].nation_id = 0;
    units.units[ship].nation_id = 0;
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    colonies.colonies[0].active = true;
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].population = 4;
    colonies.colony_count = 1;
    ColonizeCol1Save sc;
    col1_save_init(&sc);
    sc.nation[0].gold = 100; /* +2 from gold/50 */
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.units = &units;
    ctx.colonies = &colonies;
    ctx.col1 = &sc;
    ctx.col1_ok = true;
    /* soldier 2+2=4, ship 1+1+3=5, pop 4*2=8, gold 100/50=2 → 19 */
    const int score = ai_diplo_military_score(&ctx, 0);
    if (score != 19) {
      fprintf(stderr, "unit_ai_diplo: military_score %d (want 19)\n", score);
      return fail("unpark #5 military_score weights");
    }
  }

  /*
   * Unpark #5: thin war/peace status chrome (102a/1092 stand-in).
   * declare_war_ctx / make_peace_ctx write when human is a party.
   */
  {
    ColonizeCol1Save st;
    col1_save_init(&st);
    memset(st.nation, 0, sizeof(st.nation));
    for (int i = 0; i < 4; ++i) {
      st.player[i].control = 0;
      st.player[i].country_name[0] = '\0';
    }
    snprintf(st.player[1].country_name, sizeof(st.player[1].country_name), "France");
    st.nation[0].gold = 200;
    st.nation[1].gold = 200;
    char status[128];
    status[0] = '\0';
    ColonizeTurnContext ctx_st;
    memset(&ctx_st, 0, sizeof(ctx_st));
    ctx_st.col1 = &st;
    ctx_st.col1_ok = true;
    ctx_st.human_nation = 0;
    ctx_st.status = status;
    ctx_st.status_size = sizeof(status);

    ai_diplo_declare_war_ctx(&ctx_st, 1, 0);
    if (!ai_diplo_at_war(&st, 0, 1)) {
      return fail("declare_war_ctx should set WAR");
    }
    if (strcmp(status, "The France and rival are now at war.") != 0) {
      fprintf(stderr, "unit_ai_diplo: war status '%s'\n", status);
      return fail("declare_war_ctx should use @DECLAREWAR line (no wartime boycott)");
    }
    /* Re-declare: no status rewrite. */
    snprintf(status, sizeof(status), "keep");
    ai_diplo_declare_war_ctx(&ctx_st, 1, 0);
    if (strcmp(status, "keep") != 0) {
      return fail("re-declare_war_ctx should not rewrite status");
    }

    ai_diplo_make_peace_ctx(&ctx_st, 0, 1);
    if (ai_diplo_at_war(&st, 0, 1)) {
      return fail("make_peace_ctx should clear WAR");
    }
    if (strcmp(status, "The rival and France have signed a peace treaty.") != 0) {
      fprintf(stderr, "unit_ai_diplo: peace status '%s'\n", status);
      return fail("make_peace_ctx should use @SIGNTREATY when no Tools embargo");
    }

    /* R12: Sugar/Tobacco/Tools already set → name first newly boycotted cargo
     * (Food idx 0) instead of the war line. Keep Indians peaceful so sticky
     * does not steal status. Source: @CARGO / NAMES.TXT; FA UI PARKED. */
    st.player[2].country_name[0] = '\0';
    st.nation[0].gold = 200;
    st.nation[2].gold = 200;
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    st.nation[0].unknown26[8] = 0;
    st.nation[0].boycott_bitmap =
      (uint16_t)(AI_DIPLO_SMOKE_SUGAR_BIT | AI_DIPLO_SMOKE_TOBACCO_BIT |
                 AI_DIPLO_SMOKE_TOOLS_BIT);
    status[0] = '\0';
    ai_diplo_declare_war_ctx(&ctx_st, 0, 2);
    if ((st.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_FOOD_BIT) != 0) {
      return fail("declare_war_ctx must not OR Food onto existing king boycotts");
    }
    if (strcmp(status, "The rival and rival are now at war.") != 0) {
      fprintf(stderr, "unit_ai_diplo: first-cargo status '%s'\n", status);
      return fail("declare_war_ctx should use war line when no new boycott bits");
    }

    /* Full wartime mask already set → boycott chrome quiet → war line. */
    ai_diplo_make_peace(&st, 0, 2);
    st.nation[0].gold = 200;
    st.nation[2].gold = 200;
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    st.nation[0].unknown26[8] = 0;
    st.nation[0].boycott_bitmap = (uint16_t)AI_DIPLO_SMOKE_WARTIME_MASK;
    status[0] = '\0';
    ai_diplo_declare_war_ctx(&ctx_st, 0, 2);
    if (strcmp(status, "The rival and rival are now at war.") != 0) {
      fprintf(stderr, "unit_ai_diplo: rival status '%s'\n", status);
      return fail("declare_war_ctx should fall back to @DECLAREWAR war line when full wartime mask set");
    }

    /* Same scenario with both country names set and the real GAME.TXT catalog
     * loaded: authentic @DECLAREWAR %STRING0/1 substitution, not invented
     * "War declared with" text. */
    ai_diplo_make_peace(&st, 0, 2);
    st.nation[0].gold = 200;
    st.nation[2].gold = 200;
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    st.nation[0].unknown26[8] = 0;
    st.nation[0].boycott_bitmap = (uint16_t)AI_DIPLO_SMOKE_WARTIME_MASK;
    snprintf(st.player[0].country_name, sizeof(st.player[0].country_name), "Spain");
    snprintf(st.player[2].country_name, sizeof(st.player[2].country_name), "Holland");
    status[0] = '\0';
    {
      ColonizeMsgCatalog game_txt;
      memset(&game_txt, 0, sizeof(game_txt));
      if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
        return fail("declare_war_ctx: GAME.TXT load failed");
      }
      ctx_st.messages = &game_txt;
      ai_diplo_declare_war_ctx(&ctx_st, 0, 2);
      ctx_st.messages = NULL;
    }
    if (strcmp(status, "The Spain and Holland are now at war.") != 0) {
      fprintf(stderr, "unit_ai_diplo: named war status '%s'\n", status);
      return fail("declare_war_ctx should render authentic @DECLAREWAR with both country names");
    }
    st.player[0].country_name[0] = '\0';
    st.player[2].country_name[0] = '\0';

    /*
     * @SNEAK ("Sneak attack by the treacherous {attacker}!") — real text
     * confirmed 2026-08-14 (COLONIZE/GAME.TXT), user-confirmed live mechanic
     * (see src/core/ai_euro.c ai_euro_try_attack). Direct primitive check
     * since ai_euro_try_attack itself is static and its dispatcher-turn
     * context is entangled with ai_diplo_euro_balance's own opportunistic
     * declare_war_ctx (which can fire earlier in the same turn and win the
     * @DECLAREWAR-vs-@SNEAK race) — too flaky for an end-to-end assertion,
     * so this pins the exact fallback text and the real GAME.TXT rendering
     * that ai_euro.c's popup_msg_fill call produces. */
    {
      char sneak_buf[256];
      PopupMsgTokens tok = {0};
      tok.string0 = "Spain";
      popup_msg_fill(
        NULL, "SNEAK", &tok, "Sneak attack by the treacherous %STRING0!", sneak_buf,
        sizeof(sneak_buf)
      );
      if (strcmp(sneak_buf, "Sneak attack by the treacherous Spain!") != 0) {
        fprintf(stderr, "unit_ai_diplo: sneak fallback status '%s'\n", sneak_buf);
        return fail("@SNEAK fallback text should match ai_euro.c's popup_msg_fill call");
      }
      ColonizeMsgCatalog game_txt_sneak;
      memset(&game_txt_sneak, 0, sizeof(game_txt_sneak));
      if (!assets_msg_load_file(&game_txt_sneak, "COLONIZE/GAME.TXT")) {
        return fail("@SNEAK: GAME.TXT load failed");
      }
      sneak_buf[0] = '\0';
      popup_msg_fill(
        &game_txt_sneak, "SNEAK", &tok, "Sneak attack by the treacherous %STRING0!", sneak_buf,
        sizeof(sneak_buf)
      );
      /* popup_msg_fill keeps the {} emphasis markup now; renderers color it. */
      if (strcmp(sneak_buf, "Sneak attack by the treacherous {Spain}!") != 0) {
        fprintf(stderr, "unit_ai_diplo: sneak real-catalog status '%s'\n", sneak_buf);
        return fail("@SNEAK should render authentic GAME.TXT text with attacker name");
      }
    }

    /* 2026-09-03: the −5 war hit is retired (no DOS declare site touches
     * Indian relations) — declare_war_ctx must neither move the matrix nor
     * pop "Natives grow hostile" (bugs: it fired on a Euro-vs-Euro attack). */
    ai_diplo_make_peace(&st, 0, 2);
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 70; /* relation 30 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    st.nation[0].unknown26[8] = 0;
    st.nation[0].boycott_bitmap = (uint16_t)AI_DIPLO_SMOKE_WARTIME_MASK;
    st.nation[0].gold = 200;
    st.nation[2].gold = 200;
    status[0] = '\0';
    ai_diplo_declare_war_ctx(&ctx_st, 0, 2);
    if (ai_diplo_indian_relation(&st, 4 + (0), 0) != 30) {
      return fail("declare_war_ctx must not touch Indian relations (hit retired)");
    }
    if (strcmp(status, "Natives grow hostile.") == 0) {
      return fail("declare_war_ctx must not status Natives grow hostile (hit retired)");
    }

    /* AI-only pair: no status write. */
    snprintf(status, sizeof(status), "untouched");
    st.nation[1].gold = 200;
    st.nation[2].gold = 200;
    ai_diplo_declare_war_ctx(&ctx_st, 1, 2);
    if (strcmp(status, "untouched") != 0) {
      return fail("declare_war_ctx must not write status for AI-only pairs");
    }

    /* Bare declare_war / make_peace remain status-free. */
    snprintf(status, sizeof(status), "bare");
    ai_diplo_make_peace(&st, 0, 2);
    ai_diplo_declare_war(&st, 0, 2);
    if (strcmp(status, "bare") != 0) {
      return fail("bare declare_war must not touch ctx status");
    }
  }

  /*
   * Sticky→pressure: sticky==2 skips peace feeler + "Natives remain hostile."
   * ai_diplo_indian_relation read-only getter (pair of relation_delta).
   */
  {
    ColonizeCol1Save sp;
    col1_save_init(&sp);
    memset(sp.nation, 0, sizeof(sp.nation));
    for (int i = 0; i < 4; ++i) {
      sp.player[i].control = 0;
    }
    char status[128];
    status[0] = '\0';
    ColonizeDosRng rng_sp;
    dos_rng_seed(&rng_sp, 11);
    uint32_t turn_sp = 11;
    ColonizeTurnContext ctx_sp;
    memset(&ctx_sp, 0, sizeof(ctx_sp));
    ctx_sp.col1 = &sp;
    ctx_sp.col1_ok = true;
    ctx_sp.rng = &rng_sp;
    ctx_sp.turn_number = &turn_sp;
    ctx_sp.human_nation = 0;
    ctx_sp.status = status;
    ctx_sp.status_size = sizeof(status);

    /* Deep sticky: slot0 very-low; mid slots would be feeler-eligible if not blocked. */
    sp.indian[0].alarm_by_player[0] = 90; /* DOS bands: relation 30 */
    sp.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    for (int i = 1; i < 8; ++i) {
      sp.indian[i].alarm_by_player[0] = 20; /* relation 80 */
      sp.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    sp.nation[0].unknown26[8] = 2;
    sp.nation[0].gold = 40;
    ai_diplo_euro_balance(&ctx_sp, 0);
    if (ai_diplo_indian_relation(&sp, 4 + (1), 0) != 80) {
      return fail("sticky==2 should block peace feeler heal on mid slots");
    }
    if (ai_diplo_indian_hostility_sticky(&sp, 0) != 2) {
      return fail("sticky==2 should remain deep after matrix tick");
    }
    if (strcmp(status, "Natives remain hostile.") != 0) {
      fprintf(stderr, "unit_ai_diplo: remain status '%s'\n", status);
      return fail("sticky==2 should status Natives remain hostile");
    }
    if (sp.nation[0].gold != 38) {
      return fail("sticky==2 tick should still apply Indian harassment −2g");
    }

    /* indian_relation getter mirrors relation_by_indian / delta indexing. */
    if (ai_diplo_indian_relation(&sp, 4, 0) != 10 ||
        ai_diplo_indian_relation(&sp, 5, 0) != 80) {
      return fail("ai_diplo_indian_relation should read indian_nation 4..11 cells");
    }
    if (ai_diplo_indian_relation(&sp, 3, 0) != 0 ||
        ai_diplo_indian_relation(&sp, 12, 0) != 0) {
      return fail("ai_diplo_indian_relation should return 0 for out-of-range indian");
    }

    /* make_peace lifts wartime boycott mask when no Euro wars remain. */
    ColonizeCol1Save em;
    col1_save_init(&em);
    memset(em.nation, 0, sizeof(em.nation));
    for (int i = 0; i < 4; ++i) {
      em.player[i].control = 0;
    }
    em.head.colony_count = 3;
    em.colony = calloc(3, sizeof(ColonizeCol1Colony));
    if (!em.colony) {
      return fail("embargo make_peace alloc");
    }
    em.colony[0].nation_id = 0;
    em.colony[1].nation_id = 0;
    em.colony[2].nation_id = 0;
    em.nation[0].gold = 200;
    em.nation[1].gold = 200;
    ai_diplo_declare_war(&em, 0, 1);
    if ((em.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
      free(em.colony);
      return fail("colony-gap war must not boycott Europe cargos");
    }
    ai_diplo_make_peace(&em, 0, 1);
    if ((em.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
        (em.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
      free(em.colony);
      return fail("make_peace should lift full wartime boycott mask when no Euro wars remain");
    }
    free(em.colony);
  }

  /*
   * R2: war-fatigue peace (timer==0 gate) + human Tools-lift / Peace status,
   * Tools embargo human status, Tobacco already covered above with Furs
   * set/lift.
   */
  {
    ColonizeCol1Save wf;
    col1_save_init(&wf);
    memset(wf.nation, 0, sizeof(wf.nation));
    for (int i = 0; i < 4; ++i) {
      wf.player[i].control = 0;
      wf.player[i].country_name[0] = '\0';
    }
    snprintf(wf.player[1].country_name, sizeof(wf.player[1].country_name), "England");
    /* Near-parity military via gold/50 only: score 12 each (|diff|<15, both >10). */
    wf.nation[0].gold = 700;
    wf.nation[1].gold = 700;
    for (int i = 0; i < 8; ++i) {
      wf.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      wf.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      wf.indian[i].alarm_by_player[1] = 0; /* relation 100 */
      wf.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    ai_diplo_declare_war(&wf, 0, 1);
    /* After sting: 600/600 → score 12; fatigue timer seeded to 8. */
    if (wf.nation[0].unknown26[1] != 8) {
      return fail("war-fatigue setup: timer should be 8 after declare");
    }
    ColonizeDosRng rng_wf;
    dos_rng_seed(&rng_wf, 42);
    uint32_t turn_wf = 1;
    char status_wf[128];
    status_wf[0] = '\0';
    ColonizeTurnContext ctx_wf;
    memset(&ctx_wf, 0, sizeof(ctx_wf));
    ctx_wf.col1 = &wf;
    ctx_wf.col1_ok = true;
    ctx_wf.rng = &rng_wf;
    ctx_wf.turn_number = &turn_wf;
    ctx_wf.human_nation = 0;
    ctx_wf.status = status_wf;
    ctx_wf.status_size = sizeof(status_wf);
    /* While timer live, near-parity must not make_peace (fatigue gate). */
    for (int n = 0; n < 80; ++n) {
      wf.nation[0].gold = 600;
      wf.nation[1].gold = 600;
      wf.nation[0].unknown26[1] = 5; /* keep live */
      wf.nation[1].unknown26[0] = 5;
      ai_diplo_euro_balance(&ctx_wf, 0);
      if (!ai_diplo_at_war(&wf, 0, 1)) {
        return fail("war-fatigue: near-parity peace must wait for timer==0");
      }
    }
    /* timer==0 + many rolls → at least one peace (1/30) + human status. */
    int peaced = 0;
    for (int seed = 1; seed < 400 && !peaced; ++seed) {
      ai_diplo_declare_war(&wf, 0, 1);
      wf.nation[0].unknown26[1] = 0;
      wf.nation[1].unknown26[0] = 0;
      wf.nation[0].gold = 600;
      wf.nation[1].gold = 600;
      for (int i = 0; i < 8; ++i) {
        wf.indian[i].alarm_by_player[0] = 0; /* relation 100 */
        wf.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
        wf.indian[i].alarm_by_player[1] = 0; /* relation 100 */
        wf.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
      }
      status_wf[0] = '\0';
      dos_rng_seed(&rng_wf, (uint32_t)seed);
      for (int n = 0; n < 60 && ai_diplo_at_war(&wf, 0, 1); ++n) {
        wf.nation[0].gold = 600;
        wf.nation[1].gold = 600;
        wf.nation[0].unknown26[1] = 0;
        wf.nation[1].unknown26[0] = 0;
        ai_diplo_euro_balance(&ctx_wf, 0);
      }
      if (!ai_diplo_at_war(&wf, 0, 1)) {
        peaced = 1;
        if (strcmp(status_wf, "The rival and England have signed a peace treaty.") != 0) {
          fprintf(stderr, "unit_ai_diplo: war-fatigue status '%s'\n", status_wf);
          return fail("war-fatigue make_peace_ctx should status @SIGNTREATY for human");
        }
      }
    }
    if (!peaced) {
      return fail("war-fatigue: timer==0 near-parity should eventually make_peace");
    }

    /*
     * R13: war-fatigue chrome when human is the peer (AI actor peaces).
     * make_peace_ctx status_human_pair fires for either party.
     */
    {
      ColonizeCol1Save wf2;
      col1_save_init(&wf2);
      memset(wf2.nation, 0, sizeof(wf2.nation));
      for (int i = 0; i < 4; ++i) {
        wf2.player[i].control = 0;
        wf2.player[i].country_name[0] = '\0';
      }
      snprintf(wf2.player[0].country_name, sizeof(wf2.player[0].country_name), "England");
      snprintf(wf2.player[1].country_name, sizeof(wf2.player[1].country_name), "France");
      wf2.nation[0].gold = 700;
      wf2.nation[1].gold = 700;
      for (int i = 0; i < 8; ++i) {
        wf2.indian[i].alarm_by_player[0] = 0; /* relation 100 */
        wf2.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
        wf2.indian[i].alarm_by_player[1] = 0; /* relation 100 */
        wf2.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
      }
      ai_diplo_declare_war(&wf2, 0, 1);
      char status_peer[128];
      status_peer[0] = '\0';
      ColonizeDosRng rng_peer;
      uint32_t turn_peer = 2;
      ColonizeTurnContext ctx_peer;
      memset(&ctx_peer, 0, sizeof(ctx_peer));
      ctx_peer.col1 = &wf2;
      ctx_peer.col1_ok = true;
      ctx_peer.rng = &rng_peer;
      ctx_peer.turn_number = &turn_peer;
      ctx_peer.human_nation = 0; /* human is peer of AI actor 1 */
      ctx_peer.status = status_peer;
      ctx_peer.status_size = sizeof(status_peer);
      int peaced_peer = 0;
      for (int seed = 1; seed < 400 && !peaced_peer; ++seed) {
        ai_diplo_declare_war(&wf2, 0, 1);
        wf2.nation[0].unknown26[1] = 0;
        wf2.nation[1].unknown26[0] = 0;
        wf2.nation[0].gold = 600;
        wf2.nation[1].gold = 600;
        for (int i = 0; i < 8; ++i) {
          wf2.indian[i].alarm_by_player[0] = 0; /* relation 100 */
          wf2.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
          wf2.indian[i].alarm_by_player[1] = 0; /* relation 100 */
          wf2.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
        }
        status_peer[0] = '\0';
        dos_rng_seed(&rng_peer, (uint32_t)seed);
        for (int n = 0; n < 60 && ai_diplo_at_war(&wf2, 0, 1); ++n) {
          wf2.nation[0].gold = 600;
          wf2.nation[1].gold = 600;
          wf2.nation[0].unknown26[1] = 0;
          wf2.nation[1].unknown26[0] = 0;
          ai_diplo_euro_balance(&ctx_peer, 1); /* AI actor; human peer */
        }
        if (!ai_diplo_at_war(&wf2, 0, 1)) {
          peaced_peer = 1;
          if (strcmp(status_peer, "The France and England have signed a peace treaty.") != 0) {
            fprintf(stderr, "unit_ai_diplo: war-fatigue peer status '%s'\n",
                    status_peer);
            return fail("war-fatigue make_peace_ctx should status @SIGNTREATY when human is peer");
          }
        }
      }
      if (!peaced_peer) {
        return fail("war-fatigue: AI actor should eventually peace with human peer");
      }
      /* Tools already clear → Peace concluded chrome (either party). */
      status_peer[0] = '\0';
      ai_diplo_declare_war(&wf2, 0, 1);
      wf2.nation[0].boycott_bitmap = 0;
      wf2.nation[1].boycott_bitmap = 0;
      ai_diplo_make_peace_ctx(&ctx_peer, 1, 0);
      if (strcmp(status_peer, "The France and England have signed a peace treaty.") != 0) {
        fprintf(stderr, "unit_ai_diplo: Peace concluded status '%s'\n", status_peer);
        return fail("make_peace_ctx should status @SIGNTREATY when Tools already clear");
      }
    }

    /* Tools embargo human status set/lift (colony gap ≥2). */
    ColonizeCol1Save ts;
    col1_save_init(&ts);
    memset(ts.nation, 0, sizeof(ts.nation));
    for (int i = 0; i < 4; ++i) {
      ts.player[i].control = 0;
      ts.player[i].country_name[0] = '\0';
    }
    snprintf(ts.player[1].country_name, sizeof(ts.player[1].country_name), "Spain");
    ts.head.colony_count = 3;
    ts.colony = calloc(3, sizeof(ColonizeCol1Colony));
    if (!ts.colony) {
      return fail("Tools status alloc colonies");
    }
    ts.colony[0].nation_id = 0;
    ts.colony[1].nation_id = 0;
    ts.colony[2].nation_id = 0;
    ts.nation[0].gold = 400;
    ts.nation[1].gold = 200;
    char status[128];
    status[0] = '\0';
    ColonizeTurnContext ctx_ts;
    memset(&ctx_ts, 0, sizeof(ctx_ts));
    ctx_ts.col1 = &ts;
    ctx_ts.col1_ok = true;
    ctx_ts.human_nation = 0;
    ctx_ts.status = status;
    ctx_ts.status_size = sizeof(status);
    ai_diplo_declare_war_ctx(&ctx_ts, 0, 1);
    if ((ts.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_TOOLS_BIT) != 0) {
      free(ts.colony);
      return fail("Tools status setup: colony-gap must not set Tools bit");
    }
    if (strcmp(status, "The rival and Spain are now at war.") != 0) {
      fprintf(stderr, "unit_ai_diplo: Tools set status '%s'\n", status);
      free(ts.colony);
      return fail("declare_war_ctx should use war line (no wartime boycott)");
    }
    ai_diplo_make_peace_ctx(&ctx_ts, 0, 1);
    if ((ts.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_TOOLS_BIT) != 0) {
      free(ts.colony);
      return fail("make_peace_ctx should leave Tools bit clear");
    }
    if (strcmp(status, "The rival and Spain have signed a peace treaty.") != 0) {
      fprintf(stderr, "unit_ai_diplo: Tools lift status '%s'\n", status);
      free(ts.colony);
      return fail("make_peace_ctx should status @SIGNTREATY when Tools never set");
    }
    /* AI-only Tools war: no status overwrite. */
    snprintf(status, sizeof(status), "keep");
    ts.nation[1].gold = 400;
    ts.nation[2].gold = 200;
    ts.colony[0].nation_id = 1;
    ts.colony[1].nation_id = 1;
    ts.colony[2].nation_id = 1;
    ai_diplo_declare_war_ctx(&ctx_ts, 1, 2);
    if (strcmp(status, "keep") != 0) {
      free(ts.colony);
      return fail("Tools embargo status must not write for AI-only pairs");
    }
    free(ts.colony);
  }

  /*
   * R3: sticky==2 refuses new treaties this balance; Sugar wartime boycott
   * set/lift; at_war_with / at_war_with_any helpers (feeler already gated).
   * R4: Rum+Cigars boycott set/lift; Sugar/Tobacco boycott status (no Tools)
   * for human declare.
   * R11: Cotton leftover boycott set/lift (full wartime mask already smoked).
   */
  {
    ColonizeCol1Save r3;
    col1_save_init(&r3);
    memset(r3.nation, 0, sizeof(r3.nation));
    for (int i = 0; i < 4; ++i) {
      r3.player[i].control = 0;
    }
    /* Near-parity via gold/50: score 12 each — ally-eligible band. */
    r3.nation[0].gold = 600;
    r3.nation[1].gold = 600;
    r3.nation[0].euro_relation[1] = AI_DIPLO_MET; /* met, no treaty yet (raw; unmet is 0 now) */
    r3.nation[1].euro_relation[0] = AI_DIPLO_MET;
    for (int i = 0; i < 8; ++i) {
      r3.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      r3.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      r3.indian[i].alarm_by_player[1] = 0; /* relation 100 */
      r3.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    /* Deep sticky: very-low slot keeps sticky==2 across matrix tick. */
    r3.indian[0].alarm_by_player[0] = 90; /* DOS bands: relation 30 */
    r3.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    r3.nation[0].unknown26[8] = 2;
    char status[128];
    status[0] = '\0';
    ColonizeDosRng rng_r3;
    dos_rng_seed(&rng_r3, 7);
    uint32_t turn_r3 = 7;
    ColonizeTurnContext ctx_r3;
    memset(&ctx_r3, 0, sizeof(ctx_r3));
    ctx_r3.col1 = &r3;
    ctx_r3.col1_ok = true;
    ctx_r3.rng = &rng_r3;
    ctx_r3.turn_number = &turn_r3;
    /* 13b0 (2026-08-27) only runs on a 3180 encounter: adjacent units. */
    static ColonizeUnitPool units_r3;
    units_reset(&units_r3);
    units_r3.type_count = 1;
    snprintf(units_r3.types[0].name, sizeof(units_r3.types[0].name), "Soldier");
    units_r3.types[0].movement = 1;
    units_r3.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    {
      const int ua = units_spawn(&units_r3, 0, 5, 5);
      const int ub = units_spawn(&units_r3, 0, 6, 5);
      ColonizeUnit* pa = units_get(&units_r3, ua);
      ColonizeUnit* pb = units_get(&units_r3, ub);
      if (pa) pa->nation_id = 0;
      if (pb) pb->nation_id = 1;
    }
    ctx_r3.units = &units_r3;
    ctx_r3.human_nation = 0;
    ctx_r3.status = status;
    ctx_r3.status_size = sizeof(status);
    /* sticky==2 blocks the 13b0 treaty tick: no PEACE signed. */
    for (int n = 0; n < 30; ++n) {
      r3.nation[0].gold = 600;
      r3.nation[1].gold = 600;
      /* Keep deep sticky (harassment −2g; relation stays <40). */
      r3.indian[0].alarm_by_player[0] = 90; /* DOS bands: relation 30 */
      r3.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      ai_diplo_euro_balance(&ctx_r3, 0);
      if (r3.nation[0].euro_relation[1] & AI_DIPLO_PEACE) {
        return fail("sticky==2 must refuse signing new treaties this balance");
      }
      turn_r3++;
    }
    if (ai_diplo_indian_hostility_sticky(&r3, 0) != 2) {
      return fail("R3 sticky refuse setup should keep sticky==2");
    }
    /* Clear sticky → treaty may sign under same near-parity RNG. */
    for (int i = 0; i < 8; ++i) {
      r3.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      r3.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    r3.nation[0].unknown26[8] = 0;
    /* FUN_5bfb_13b0 (2026-08-27): not war-worthy (turn <= 39) and no PEACE →
     * @SIGNTREATY sets PEACE both ways within the (a+turn+b)%3 cadence. */
    int treaty = 0;
    for (int n = 0; n < 6; ++n) {
      r3.nation[0].gold = 600;
      r3.nation[1].gold = 600;
      ai_diplo_euro_balance(&ctx_r3, 0);
      if ((r3.nation[0].euro_relation[1] & AI_DIPLO_PEACE) &&
          (r3.nation[1].euro_relation[0] & AI_DIPLO_PEACE)) {
        treaty = 1;
        break;
      }
      turn_r3++;
    }
    if (!treaty) {
      return fail("without sticky==2, 13b0 should sign a peace treaty");
    }

    /* Sugar embargo: set on declare, lift on peace (same bit1 as king refuse). */
    ColonizeCol1Save sg;
    col1_save_init(&sg);
    memset(sg.nation, 0, sizeof(sg.nation));
    for (int i = 0; i < 4; ++i) {
      sg.player[i].control = 0;
    }
    sg.nation[0].gold = 200;
    sg.nation[1].gold = 200;
    /* Pre-seed king-style Sugar bit; war must not spread it. */
    sg.nation[0].boycott_bitmap = AI_DIPLO_SMOKE_SUGAR_BIT;
    ai_diplo_declare_war(&sg, 0, 1);
    if ((sg.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_SUGAR_BIT) == 0) {
      return fail("declare_war must not clear a king Sugar boycott");
    }
    if ((sg.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_SUGAR_BIT) != 0) {
      return fail("declare_war must not copy Sugar boycott onto the peer");
    }
    if ((sg.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_EMBARGO_BIT) != 0 ||
        (sg.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_TOBACCO_BIT) != 0) {
      return fail("declare_war must not OR extra wartime cargos");
    }
    ai_diplo_make_peace(&sg, 0, 1);
    if ((sg.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_SUGAR_BIT) != 0 ||
        (sg.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_SUGAR_BIT) != 0) {
      return fail("make_peace should lift Sugar boycott when no Euro wars remain");
    }
    if (ai_diplo_at_war_with_any(&sg, 0) || ai_diplo_at_war_with(&sg, 0, 1)) {
      return fail("after peace, at_war_with / at_war_with_any should be clear");
    }

    /* R4/R11: Rum+Cigars+Cotton wartime boycott set on declare, lift on peace. */
    ColonizeCol1Save rc;
    col1_save_init(&rc);
    memset(rc.nation, 0, sizeof(rc.nation));
    for (int i = 0; i < 4; ++i) {
      rc.player[i].control = 0;
    }
    rc.nation[0].gold = 200;
    rc.nation[1].gold = 200;
    ai_diplo_declare_war(&rc, 0, 1);
    if ((rc.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
        (rc.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
      return fail("declare_war must not OR Rum/Cigars/Cotton boycott bits");
    }
    ai_diplo_make_peace(&rc, 0, 1);
    if ((rc.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
        (rc.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
      return fail("make_peace should lift full wartime mask (Rum+Cigars+Cotton+…)");
    }

    /* Human declare uses @DECLAREWAR; no wartime Tools boycott. */
    ColonizeCol1Save st;
    col1_save_init(&st);
    memset(st.nation, 0, sizeof(st.nation));
    for (int i = 0; i < 4; ++i) {
      st.player[i].control = 0;
      st.player[i].country_name[0] = '\0';
    }
    snprintf(st.player[1].country_name, sizeof(st.player[1].country_name), "France");
    st.nation[0].gold = 300;
    st.nation[1].gold = 300;
    char status_st[128];
    status_st[0] = '\0';
    ColonizeTurnContext ctx_st;
    memset(&ctx_st, 0, sizeof(ctx_st));
    ctx_st.col1 = &st;
    ctx_st.col1_ok = true;
    ctx_st.human_nation = 0;
    ctx_st.status = status_st;
    ctx_st.status_size = sizeof(status_st);
    ai_diplo_declare_war_ctx(&ctx_st, 0, 1);
    if ((st.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_TOOLS_BIT) != 0) {
      return fail("declare_war must not OR Tools boycott");
    }
    if (strcmp(status_st, "The rival and France are now at war.") != 0) {
      fprintf(stderr, "unit_ai_diplo: Sugar/Tobacco/Tools status '%s'\n", status_st);
      return fail("declare_war_ctx should use war line (no wartime boycott)");
    }
  }

  /*
   * R6: Ore+Silver wartime boycott set/lift (COLONIZE_CARGO_ORE/SILVER bits);
   * war-fatigue Peace status covered above;
   * Indian −5 war-hit verified + sticky-rise status when boycott chrome quiet.
   */
  {
    ColonizeCol1Save os;
    col1_save_init(&os);
    memset(os.nation, 0, sizeof(os.nation));
    for (int i = 0; i < 4; ++i) {
      os.player[i].control = 0;
    }
    os.nation[0].gold = 200;
    os.nation[1].gold = 200;
    ai_diplo_declare_war(&os, 0, 1);
    if ((os.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
        (os.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
      return fail("declare_war must not OR Ore/Silver boycott bits");
    }
    ai_diplo_make_peace(&os, 0, 1);
    if ((os.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_ORE_BIT) != 0 ||
        (os.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_ORE_BIT) != 0 ||
        (os.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_SILVER_BIT) != 0 ||
        (os.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_SILVER_BIT) != 0) {
      return fail("make_peace should lift Ore+Silver boycott when no Euro wars remain");
    }
  }

  /*
   * R8: Lumber wartime boycott set/lift (COLONIZE_CARGO_LUMBER); make_peace
   * stops privateer prize (WAR-gated); Indian feeler human status when mid-band
   * nudge fires and sticky stays clear.
   */
  {
    ColonizeCol1Save r8;
    col1_save_init(&r8);
    memset(r8.nation, 0, sizeof(r8.nation));
    for (int i = 0; i < 4; ++i) {
      r8.player[i].control = 0;
    }
    r8.nation[0].gold = 200;
    r8.nation[1].gold = 200;
    ai_diplo_declare_war(&r8, 0, 1);
    if ((r8.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_LUMBER_BIT) != 0 ||
        (r8.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_LUMBER_BIT) != 0) {
      return fail("declare_war must not OR Lumber boycott bit");
    }
    ai_diplo_make_peace(&r8, 0, 1);
    if ((r8.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_LUMBER_BIT) != 0 ||
        (r8.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_LUMBER_BIT) != 0) {
      return fail("make_peace should lift Lumber boycott when no Euro wars remain");
    }

    /* Privateer prize stops after peace (no WAR branch → no 8g transfer). */
    {
      ColonizeCol1Save pr;
      col1_save_init(&pr);
      memset(pr.nation, 0, sizeof(pr.nation));
      for (int i = 0; i < 4; ++i) {
        pr.player[i].control = 0;
      }
      for (int i = 0; i < 8; ++i) {
        pr.indian[i].alarm_by_player[0] = 0; /* relation 100 */
        pr.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
        pr.indian[i].alarm_by_player[1] = 0; /* relation 100 */
        pr.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
      }
      pr.nation[0].gold = 250;
      pr.nation[1].gold = 80;
      ai_diplo_declare_war(&pr, 0, 1);
      /* After sting: 150 / 0; restore imbalance for prize. */
      pr.nation[0].gold = 200;
      pr.nation[1].gold = 50;
      ColonizeDosRng rng_pr;
      dos_rng_seed(&rng_pr, 9);
      uint32_t turn_pr = 9;
      ColonizeTurnContext ctx_pr;
      memset(&ctx_pr, 0, sizeof(ctx_pr));
      ctx_pr.col1 = &pr;
      ctx_pr.col1_ok = true;
      ctx_pr.rng = &rng_pr;
      ctx_pr.turn_number = &turn_pr;
      ai_diplo_euro_balance(&ctx_pr, 1); /* poorer: 50−5+8=53; richer 192 */
      if (pr.nation[1].gold != 53 || pr.nation[0].gold != 192) {
        return fail("R8 privateer setup: prize should fire while at war");
      }
      ai_diplo_make_peace(&pr, 0, 1);
      if (ai_diplo_at_war(&pr, 0, 1)) {
        return fail("R8 privateer: make_peace should clear WAR");
      }
      pr.nation[0].gold = 200;
      pr.nation[1].gold = 50;
      ai_diplo_euro_balance(&ctx_pr, 1);
      if (pr.nation[0].gold != 200 || pr.nation[1].gold != 50) {
        return fail("after peace, euro_balance must not apply privateer prize or war upkeep");
      }
    }

    /* Feeler mid-band heal → human "Native relations improve." (sticky clear). */
    {
      ColonizeCol1Save fl;
      col1_save_init(&fl);
      memset(fl.nation, 0, sizeof(fl.nation));
      for (int i = 0; i < 4; ++i) {
        fl.player[i].control = 0;
      }
      for (int i = 0; i < 8; ++i) {
        fl.indian[i].alarm_by_player[0] = 0; /* relation 100 */
        fl.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      }
      fl.indian[1].alarm_by_player[0] = 20; /* relation 80 */ /* mid-band feeler-eligible */
      fl.indian[1].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      fl.nation[0].unknown26[8] = 0;
      fl.nation[0].gold = 40;
      char status_fl[128];
      status_fl[0] = '\0';
      ColonizeDosRng rng_fl;
      dos_rng_seed(&rng_fl, 12);
      uint32_t turn_fl = 12;
      ColonizeTurnContext ctx_fl;
      memset(&ctx_fl, 0, sizeof(ctx_fl));
      ctx_fl.col1 = &fl;
      ctx_fl.col1_ok = true;
      ctx_fl.rng = &rng_fl;
      ctx_fl.turn_number = &turn_fl;
      ctx_fl.human_nation = 0;
      ctx_fl.status = status_fl;
      ctx_fl.status_size = sizeof(status_fl);
      ai_diplo_euro_balance(&ctx_fl, 0);
      if (ai_diplo_indian_relation(&fl, 4 + (1), 0) != 80) {
        return fail("R8: retired feeler must not move mid-band relation");
      }
      if (ai_diplo_indian_hostility_sticky(&fl, 0) != 0) {
        return fail("R8 feeler status setup should keep sticky clear");
      }
      if (status_fl[0] != '\0') {
        fprintf(stderr, "unit_ai_diplo: feeler status '%s'\n", status_fl);
        return fail("retired feeler must not write a status line");
      }
      /* AI-only: no status. */
      snprintf(status_fl, sizeof(status_fl), "keep");
      fl.indian[1].alarm_by_player[0] = 20; /* relation 80 */
      fl.indian[1].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      ctx_fl.human_nation = 2;
      ai_diplo_euro_balance(&ctx_fl, 0);
      if (strcmp(status_fl, "keep") != 0) {
        return fail("feeler status must not write for AI-only nation");
      }
    }
  }

  /*
   * R9: Horses+Muskets wartime boycott set/lift.
   */
  {
    ColonizeCol1Save r9;
    col1_save_init(&r9);
    memset(r9.nation, 0, sizeof(r9.nation));
    for (int i = 0; i < 4; ++i) {
      r9.player[i].control = 0;
    }
    for (int i = 0; i < 8; ++i) {
      r9.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      r9.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      r9.indian[i].alarm_by_player[1] = 0; /* relation 100 */
      r9.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    r9.nation[0].gold = 200;
    r9.nation[1].gold = 200;
    ai_diplo_declare_war(&r9, 0, 1);
    if ((r9.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_HORSES_BIT) != 0 ||
        (r9.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_HORSES_BIT) != 0) {
      return fail("declare_war must not OR Horses boycott bit");
    }
    if ((r9.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_MUSKETS_BIT) != 0 ||
        (r9.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_MUSKETS_BIT) != 0) {
      return fail("declare_war must not OR Muskets boycott bit");
    }
    ai_diplo_make_peace(&r9, 0, 1);
    if ((r9.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_HORSES_BIT) != 0 ||
        (r9.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_HORSES_BIT) != 0) {
      return fail("make_peace should lift Horses boycott when no Euro wars remain");
    }
    if ((r9.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_MUSKETS_BIT) != 0 ||
        (r9.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_MUSKETS_BIT) != 0) {
      return fail("make_peace should lift Muskets boycott when no Euro wars remain");
    }

  }

  /*
   * AI popup unpark: declare_war_ctx involving human enqueues OK (status kept).
   * FUN_15b3 / 5bfb 102a/1092 stand-in; boycott tag when embargo chrome wins.
   */
  {
    ColonizeCol1Save pop;
    col1_save_init(&pop);
    memset(pop.nation, 0, sizeof(pop.nation));
    for (int i = 0; i < 4; ++i) {
      pop.player[i].control = 0;
      pop.player[i].country_name[0] = '\0';
    }
    snprintf(pop.player[1].country_name, sizeof(pop.player[1].country_name), "France");
    pop.nation[0].gold = 200;
    pop.nation[1].gold = 200;
    char status_pop[128];
    status_pop[0] = '\0';
    AiPopupState popups;
    ai_popup_init(&popups);
    ColonizeTurnContext ctx_pop;
    memset(&ctx_pop, 0, sizeof(ctx_pop));
    ctx_pop.col1 = &pop;
    ctx_pop.col1_ok = true;
    ctx_pop.human_nation = 0;
    ctx_pop.status = status_pop;
    ctx_pop.status_size = sizeof(status_pop);
    ctx_pop.ai_popups = &popups;

    ai_diplo_declare_war_ctx(&ctx_pop, 1, 0);
    if (!ai_diplo_at_war(&pop, 0, 1)) {
      return fail("popup smoke: declare_war_ctx should set WAR");
    }
    if (strcmp(status_pop, "The France and rival are now at war.") != 0) {
      fprintf(stderr, "unit_ai_diplo: popup war status '%s'\n", status_pop);
      return fail("popup smoke: status line is @DECLAREWAR (no wartime boycott)");
    }
    if (popups.queue_count != 1) {
      return fail("popup smoke: declare_war_ctx should enqueue one OK");
    }
    if (popups.queue[0].tag != AI_POPUP_TAG_DIPLO_WAR) {
      return fail("popup smoke: war status should tag DIPLO_WAR");
    }
    if (strcmp(popups.queue[0].body, status_pop) != 0) {
      return fail("popup smoke: OK body should match status");
    }

    /* Re-declare: no second enqueue. */
    ai_diplo_declare_war_ctx(&ctx_pop, 1, 0);
    if (popups.queue_count != 1) {
      return fail("popup smoke: re-declare must not enqueue again");
    }

    /* Peace → @SIGNTREATY OK (PEACE tag; Tools embargo never set). */
    ai_diplo_make_peace_ctx(&ctx_pop, 0, 1);
    if (popups.queue_count != 2) {
      return fail("popup smoke: make_peace_ctx should enqueue OK");
    }
    if (popups.queue[1].tag != AI_POPUP_TAG_DIPLO_PEACE) {
      return fail("popup smoke: peace treaty should tag DIPLO_PEACE");
    }

    /*
     * R2: war OK both directions (human-as-a / human-as-b) once each;
     * re-declare must not spam. Peace offer CHOICE AI→human + Accept apply.
     * Cite: FUN_15b3 / FUN_5bfb; FA 3f41 full UI PARKED.
     */
    {
      ColonizeCol1Save w2;
      col1_save_init(&w2);
      memset(w2.nation, 0, sizeof(w2.nation));
      for (int i = 0; i < 4; ++i) {
        w2.player[i].control = 0;
        w2.player[i].country_name[0] = '\0';
      }
      snprintf(w2.player[1].country_name, sizeof(w2.player[1].country_name), "France");
      w2.nation[0].gold = 200;
      w2.nation[1].gold = 200;
      for (int i = 0; i < 8; ++i) {
        w2.indian[i].alarm_by_player[0] = 0; /* relation 100 */
        w2.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
        w2.indian[i].alarm_by_player[1] = 0; /* relation 100 */
        w2.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
      }
      char status_w2[128];
      status_w2[0] = '\0';
      AiPopupState pop_w2;
      ai_popup_init(&pop_w2);
      ColonizeTurnContext ctx_w2;
      memset(&ctx_w2, 0, sizeof(ctx_w2));
      ctx_w2.col1 = &w2;
      ctx_w2.col1_ok = true;
      ctx_w2.human_nation = 0;
      ctx_w2.status = status_w2;
      ctx_w2.status_size = sizeof(status_w2);
      ctx_w2.ai_popups = &pop_w2;

      /* human-as-a */
      ai_diplo_declare_war_ctx(&ctx_w2, 0, 1);
      if (pop_w2.queue_count != 1) {
        return fail("R2 war: human-as-a should enqueue one OK");
      }
      ai_diplo_declare_war_ctx(&ctx_w2, 0, 1);
      if (pop_w2.queue_count != 1) {
        return fail("R2 war: re-declare human-as-a must not spam");
      }
      ai_diplo_make_peace(&w2, 0, 1);
      ai_popup_clear(&pop_w2);
      status_w2[0] = '\0';
      w2.nation[0].gold = 200;
      w2.nation[1].gold = 200;
      for (int i = 0; i < 8; ++i) {
        w2.indian[i].alarm_by_player[0] = 0; /* relation 100 */
        w2.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
        w2.indian[i].alarm_by_player[1] = 0; /* relation 100 */
        w2.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
      }
      w2.nation[0].unknown26[8] = 0;
      /* human-as-b */
      ai_diplo_declare_war_ctx(&ctx_w2, 1, 0);
      if (pop_w2.queue_count != 1) {
        return fail("R2 war: human-as-b should enqueue one OK");
      }
      ai_diplo_declare_war_ctx(&ctx_w2, 1, 0);
      if (pop_w2.queue_count != 1) {
        return fail("R2 war: re-declare human-as-b must not spam");
      }

      /* Peace CHOICE: AI actor offers to human peer; Accept → make_peace. */
      ai_popup_clear(&pop_w2);
      status_w2[0] = '\0';
      {
        const char* labels[] = {"Accept", "Refuse"};
        const int ids[] = {1, 2};
        if (!ai_popup_enqueue_choice_ctx(
              &pop_w2,
              AI_POPUP_TAG_DIPLO_PEACE,
              1,
              0,
              0,
              "Peace",
              "France offers peace.",
              labels,
              ids,
              2
            )) {
          return fail("R2 peace: enqueue CHOICE");
        }
      }
      if (ai_diplo_at_war(&w2, 0, 1) == 0) {
        return fail("R2 peace: still at war before Accept");
      }
      ai_popup_try_present_next(&pop_w2);
      {
        ColonizeInputState in;
        memset(&in, 0, sizeof(in));
        in.last_key = COLONIZE_KEY_ENTER; /* Accept */
        if (!ai_popup_handle_input(&pop_w2, &in) || !pop_w2.has_result) {
          return fail("R2 peace: Accept should produce result");
        }
      }
      ai_diplo_apply_popup_result(&ctx_w2, &pop_w2);
      if (ai_diplo_at_war(&w2, 0, 1)) {
        return fail("R2 peace: Accept should make_peace");
      }
      ai_popup_consume_result(&pop_w2);

      /* Refuse leaves WAR. */
      ai_diplo_declare_war(&w2, 0, 1);
      ai_popup_clear(&pop_w2);
      {
        const char* labels[] = {"Accept", "Refuse"};
        const int ids[] = {1, 2};
        (void)ai_popup_enqueue_choice_ctx(
          &pop_w2,
          AI_POPUP_TAG_DIPLO_PEACE,
          1,
          0,
          0,
          "Peace",
          "France offers peace.",
          labels,
          ids,
          2
        );
      }
      ai_popup_try_present_next(&pop_w2);
      {
        ColonizeInputState in;
        memset(&in, 0, sizeof(in));
        in.last_key = COLONIZE_KEY_DOWN; /* Refuse selection */
        (void)ai_popup_handle_input(&pop_w2, &in);
        in.last_key = COLONIZE_KEY_ENTER;
        if (!ai_popup_handle_input(&pop_w2, &in) || !pop_w2.has_result) {
          return fail("R2 peace: Refuse should produce result");
        }
      }
      if (pop_w2.result_choice_id != 2) {
        return fail("R2 peace: Refuse choice_id should be 2");
      }
      ai_diplo_apply_popup_result(&ctx_w2, &pop_w2);
      if (!ai_diplo_at_war(&w2, 0, 1)) {
        return fail("R2 peace: Refuse must leave WAR");
      }
      if (strcmp(status_w2, "Peace refused with France") != 0) {
        fprintf(stderr, "unit_ai_diplo: peace refuse status '%s'\n", status_w2);
        return fail("R2 peace: Refuse should status Peace refused");
      }
      /* Refuse follow-up INFO OK demoted (invented; FA UI PARKED). */
      if (pop_w2.queue_count != 0) {
        return fail("M2R5 peace Refuse: must not enqueue invented follow-up OK");
      }
      ai_popup_consume_result(&pop_w2);

      /*
       * R3: privateer prize human status also enqueues OK (INFO).
       * Unit spawn covered in Marathon2 R1 below; FA 3f41 full UI PARKED.
       */
      {
        ColonizeCol1Save pr;
        col1_save_init(&pr);
        memset(pr.nation, 0, sizeof(pr.nation));
        for (int i = 0; i < 4; ++i) {
          pr.player[i].control = 0;
          pr.player[i].country_name[0] = '\0';
        }
        snprintf(pr.player[0].country_name, sizeof(pr.player[0].country_name), "England");
        for (int i = 0; i < 8; ++i) {
          pr.indian[i].alarm_by_player[0] = 0; /* relation 100 */
          pr.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
          pr.indian[i].alarm_by_player[1] = 0; /* relation 100 */
          pr.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
        }
        ai_diplo_declare_war(&pr, 0, 1);
        pr.nation[0].gold = 200;
        pr.nation[1].gold = 50;
        ColonizeDosRng rng_pr3;
        dos_rng_seed(&rng_pr3, 4);
        uint32_t turn_pr3 = 4;
        char status_pr3[128];
        status_pr3[0] = '\0';
        AiPopupState pop_pr3;
        ai_popup_init(&pop_pr3);
        ColonizeTurnContext ctx_pr3;
        memset(&ctx_pr3, 0, sizeof(ctx_pr3));
        ctx_pr3.col1 = &pr;
        ctx_pr3.col1_ok = true;
        ctx_pr3.rng = &rng_pr3;
        ctx_pr3.turn_number = &turn_pr3;
        ctx_pr3.human_nation = 1;
        ctx_pr3.status = status_pr3;
        ctx_pr3.status_size = sizeof(status_pr3);
        ctx_pr3.ai_popups = &pop_pr3;
        ai_diplo_euro_balance(&ctx_pr3, 1);
        if (strcmp(status_pr3, "Privateer prize from England") != 0) {
          fprintf(stderr, "unit_ai_diplo: R3 privateer status '%s'\n", status_pr3);
          return fail("R3 privateer: status Privateer prize from England");
        }
        /* Prize INFO OK demoted — status only. */
        for (int qi = 0; qi < pop_pr3.queue_count; ++qi) {
          if (pop_pr3.queue[qi].kind == AI_POPUP_KIND_OK &&
              strstr(pop_pr3.queue[qi].body, "Privateer prize") != NULL) {
            return fail("R3 privateer: must not enqueue Privateer prize INFO OK");
          }
        }
      }
    }
  }

  /*
   * Marathon2 R1/R3: wartime Privateer unit spawn once/war peer (unknown26[9]),
   * coastal water by colony (R3: assert water / hunt-ready !Europe); second
   * balance must not spam; Marathon3 R2: spawn-only — PARKED 8g prize skipped
   * when units present; peace clears spawn bit; thin FA report OK title
   * "Foreign Affairs" + DIPLO_FA tag.
   * Cite: Europe Privateer; fandom Drake; euro_unit_act §2b.
   */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    map.width = 16;
    map.height = 16;
    map.tile_count = 256;
    map.terrain = calloc(256, 1);
    map.layer2 = calloc(256, 1);
    map.layer3 = calloc(256, 1);
    if (!map.terrain || !map.layer2 || !map.layer3) {
      return fail("M2R1 privateer: map alloc");
    }
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1; /* land */
    }
    map.terrain[4 * 16 + 3] = 25; /* ocean west of colony (4,4) */

    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Privateer");
    units.types[0].movement = 4;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
    units.types[0].attack = 2;
    units.types[0].defense = 1;

    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    colonies.colonies[0].active = true;
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].x = 4;
    colonies.colonies[0].y = 4;
    colonies.colony_count = 1;
    if (!map_tile_is_coastal(&map, 4, 4)) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R1 privateer: colony (4,4) should be coastal");
    }

    ColonizeCol1Save pr;
    col1_save_init(&pr);
    memset(pr.nation, 0, sizeof(pr.nation));
    for (int i = 0; i < 4; ++i) {
      pr.player[i].control = 0;
      pr.player[i].country_name[0] = '\0';
    }
    snprintf(pr.player[1].country_name, sizeof(pr.player[1].country_name), "France");
    for (int i = 0; i < 8; ++i) {
      pr.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      pr.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      pr.indian[i].alarm_by_player[1] = 0; /* relation 100 */
      pr.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    ai_diplo_declare_war(&pr, 0, 1);
    /* Equal gold after upkeep → no prize; spawn chrome stays. */
    pr.nation[0].gold = 50;
    pr.nation[1].gold = 45;

    ColonizeDosRng rng;
    dos_rng_seed(&rng, 7);
    uint32_t turn = 7;
    char status[128];
    status[0] = '\0';
    AiPopupState pop;
    ai_popup_init(&pop);
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.col1 = &pr;
    ctx.col1_ok = true;
    ctx.units = &units;
    ctx.colonies = &colonies;
    ctx.map = &map;
    ctx.rng = &rng;
    ctx.turn_number = &turn;
    ctx.human_nation = 0;
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.ai_popups = &pop;

    ai_diplo_euro_balance(&ctx, 0);
    int priv_count = 0;
    int priv_x = -1;
    int priv_y = -1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 0) {
        continue;
      }
      const ColonizeUnitType* t = units_type(&units, u->type_index);
      if (t && strcmp(t->name, "Privateer") == 0) {
        priv_count++;
        priv_x = u->x;
        priv_y = u->y;
      }
    }
    if (priv_count != 1) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R1: euro_balance at war should spawn one Privateer");
    }
    if (priv_x != 3 || priv_y != 4) {
      fprintf(stderr, "unit_ai_diplo: Privateer at (%d,%d) want (3,4)\n", priv_x, priv_y);
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R1: Privateer should spawn on coastal water by colony");
    }
    /* Marathon2 R3: spawn tile water / hunt-ready (!Europe; ai_euro hunt gate). */
    if (!map_tile_is_water(&map, priv_x, priv_y)) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R3: Privateer spawn tile must be water");
    }
    if (priv_x >= 200 || priv_y >= 200) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R3: Privateer spawn must be hunt-ready (!Europe)");
    }
    if ((pr.nation[0].unknown26[9] & (1u << 1)) == 0) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R1: unknown26[9] peer bit should arm after spawn");
    }
    if (strcmp(status, "Privateer commissioned against France") != 0) {
      fprintf(stderr, "unit_ai_diplo: M2R1 spawn status '%s'\n", status);
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R1: human status Privateer commissioned against France");
    }
    /* Commission is status-only — no invented GAME.TXT Privateer INFO OK. */
    {
      for (int qi = 0; qi < pop.queue_count; ++qi) {
        if (pop.queue[qi].kind == AI_POPUP_KIND_OK &&
            pop.queue[qi].tag == AI_POPUP_TAG_INFO &&
            (strcmp(pop.queue[qi].title, "Privateer") == 0 ||
             strstr(pop.queue[qi].body, "Privateer commissioned") != NULL)) {
          free(map.terrain);
          free(map.layer2);
          free(map.layer3);
          return fail("M4R1: Privateer commission must not enqueue INFO OK");
        }
      }
    }

    /* Second tick: no spam spawn. */
    status[0] = '\0';
    ai_popup_clear(&pop);
    pr.nation[0].gold = 50;
    pr.nation[1].gold = 45;
    ai_diplo_euro_balance(&ctx, 0);
    priv_count = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 0) {
        continue;
      }
      const ColonizeUnitType* t = units_type(&units, u->type_index);
      if (t && strcmp(t->name, "Privateer") == 0) {
        priv_count++;
      }
    }
    if (priv_count != 1) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R1: second balance must not spawn another Privateer");
    }
    /* Marathon2 R5: unknown26[9] peer bit stays armed and blocks second spawn. */
    if ((pr.nation[0].unknown26[9] & (1u << 1)) == 0) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R5: unknown26[9] must stay armed after second balance");
    }
    if (strstr(status, "Privateer commissioned") != NULL) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R5: unknown26[9] gate must not re-status Privateer commissioned");
    }

    /* Prize must NOT fire when units pool is present (spawn-only path;
     * PARKED 8g treasury stand-in is null-units only — no invented rate). */
    pr.nation[0].gold = 200;
    pr.nation[1].gold = 50;
    status[0] = '\0';
    ai_popup_clear(&pop);
    ai_diplo_euro_balance(&ctx, 0);
    /* upkeep −5 only; no 8g prize while spawn path owns wartime Privateer. */
    if (pr.nation[0].gold != 195 || pr.nation[1].gold != 50) {
      fprintf(stderr, "unit_ai_diplo: M3R2 spawn-only gold %u/%u (want 195/50)\n",
              (unsigned)pr.nation[0].gold, (unsigned)pr.nation[1].gold);
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M3R2: with units pool, euro_balance must skip PARKED 8g prize");
    }
    if (strstr(status, "Privateer prize") != NULL) {
      fprintf(stderr, "unit_ai_diplo: M3R2 prize status '%s'\n", status);
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M3R2: spawn-only path must not status Privateer prize");
    }

    ai_diplo_make_peace(&pr, 0, 1);
    if ((pr.nation[0].unknown26[9] & (1u << 1)) != 0) {
      free(map.terrain);
      free(map.layer2);
      free(map.layer3);
      return fail("M2R1: make_peace should clear Privateer spawn peer bit");
    }

    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
  }

  /*
   * Marathon2 R3/R6: AI→human war declare CHOICE Accept/Refuse (10ec / 15b3).
   * Accept → declare_war_ctx; Refuse → status + follow-up OK, no WAR.
   * FA 3f41 PARKED.
   */
  {
    ColonizeCol1Save w3;
    col1_save_init(&w3);
    memset(w3.nation, 0, sizeof(w3.nation));
    for (int i = 0; i < 4; ++i) {
      w3.player[i].control = 0;
      w3.player[i].country_name[0] = '\0';
    }
    snprintf(w3.player[1].country_name, sizeof(w3.player[1].country_name), "France");
    w3.nation[0].gold = 200;
    w3.nation[1].gold = 200;
    for (int i = 0; i < 8; ++i) {
      w3.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      w3.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      w3.indian[i].alarm_by_player[1] = 0; /* relation 100 */
      w3.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    char status_w3[128];
    status_w3[0] = '\0';
    AiPopupState pop_w3;
    ai_popup_init(&pop_w3);
    ColonizeTurnContext ctx_w3;
    memset(&ctx_w3, 0, sizeof(ctx_w3));
    ctx_w3.col1 = &w3;
    ctx_w3.col1_ok = true;
    ctx_w3.human_nation = 0;
    ctx_w3.status = status_w3;
    ctx_w3.status_size = sizeof(status_w3);
    ctx_w3.ai_popups = &pop_w3;

    {
      const char* labels[] = {"Accept", "Refuse"};
      const int ids[] = {1, 2};
      if (!ai_popup_enqueue_choice_ctx(
            &pop_w3,
            AI_POPUP_TAG_DIPLO_WAR,
            1,
            0,
            0,
            "War",
            "France declares war!",
            labels,
            ids,
            2
          )) {
        return fail("M2R3 war: enqueue CHOICE");
      }
    }
    if (ai_diplo_at_war(&w3, 0, 1)) {
      return fail("M2R3 war: must not be at war before Accept");
    }
    ai_popup_try_present_next(&pop_w3);
    {
      ColonizeInputState in;
      memset(&in, 0, sizeof(in));
      in.last_key = COLONIZE_KEY_ENTER; /* Accept */
      if (!ai_popup_handle_input(&pop_w3, &in) || !pop_w3.has_result) {
        return fail("M2R3 war: Accept should produce result");
      }
    }
    ai_diplo_apply_popup_result(&ctx_w3, &pop_w3);
    if (!ai_diplo_at_war(&w3, 0, 1)) {
      return fail("M2R3 war: Accept should declare_war");
    }
    ai_popup_consume_result(&pop_w3);

    /* Refuse leaves peace. */
    ai_diplo_make_peace(&w3, 0, 1);
    ai_popup_clear(&pop_w3);
    status_w3[0] = '\0';
    w3.nation[0].gold = 200;
    w3.nation[1].gold = 200;
    for (int i = 0; i < 8; ++i) {
      w3.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      w3.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      w3.indian[i].alarm_by_player[1] = 0; /* relation 100 */
      w3.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    w3.nation[0].unknown26[8] = 0;
    w3.nation[1].unknown26[8] = 0;
    {
      const char* labels[] = {"Accept", "Refuse"};
      const int ids[] = {1, 2};
      (void)ai_popup_enqueue_choice_ctx(
        &pop_w3,
        AI_POPUP_TAG_DIPLO_WAR,
        1,
        0,
        0,
        "War",
        "France declares war!",
        labels,
        ids,
        2
      );
    }
    ai_popup_try_present_next(&pop_w3);
    {
      ColonizeInputState in;
      memset(&in, 0, sizeof(in));
      in.last_key = COLONIZE_KEY_DOWN; /* Refuse selection */
      (void)ai_popup_handle_input(&pop_w3, &in);
      in.last_key = COLONIZE_KEY_ENTER;
      if (!ai_popup_handle_input(&pop_w3, &in) || !pop_w3.has_result) {
        return fail("M2R3 war: Refuse should produce result");
      }
    }
    if (pop_w3.result_choice_id != 2) {
      return fail("M2R3 war: Refuse choice_id should be 2");
    }
    ai_diplo_apply_popup_result(&ctx_w3, &pop_w3);
    if (ai_diplo_at_war(&w3, 0, 1)) {
      return fail("M2R3 war: Refuse must leave peace");
    }
    if (strcmp(status_w3, "War refused with France") != 0) {
      fprintf(stderr, "unit_ai_diplo: war refuse status '%s'\n", status_w3);
      return fail("M2R3 war: Refuse should status War refused");
    }
    /* Refuse follow-up INFO OK demoted (invented; FA UI PARKED). */
    if (pop_w3.queue_count != 0) {
      return fail("M2R6 war Refuse: must not enqueue invented follow-up OK");
    }
  }

  /*
   * @CANCELPEACE: real 10ec AI→human war-declare CHOICE prompt body (not the
   * hand-built fixture above) — drive ai_diplo_euro_balance itself until the
   * 1-in-20 roll fires and assert the queued body is the authentic GAME.TXT
   * line, not invented "%s declares war!" text.
   */
  {
    ColonizeCol1Save cp;
    col1_save_init(&cp);
    memset(cp.nation, 0, sizeof(cp.nation));
    for (int i = 0; i < 4; ++i) {
      cp.player[i].control = 0;
      cp.player[i].country_name[0] = '\0';
    }
    snprintf(cp.player[0].country_name, sizeof(cp.player[0].country_name), "England");
    snprintf(cp.player[1].country_name, sizeof(cp.player[1].country_name), "France");
    cp.nation[1].gold = 2000; /* self score ≫ human's 0 → 10ec eligible */
    for (int i = 0; i < 8; ++i) {
      cp.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      cp.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      cp.indian[i].alarm_by_player[1] = 0; /* relation 100 */
      cp.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    char status_cp[128];
    status_cp[0] = '\0';
    AiPopupState pop_cp;
    ai_popup_init(&pop_cp);
    ColonizeDosRng rng_cp;
    dos_rng_seed(&rng_cp, 99);
    ColonizeTurnContext ctx_cp;
    memset(&ctx_cp, 0, sizeof(ctx_cp));
    ctx_cp.col1 = &cp;
    ctx_cp.col1_ok = true;
    ctx_cp.rng = &rng_cp;
    ctx_cp.human_nation = 0;
    ctx_cp.status = status_cp;
    ctx_cp.status_size = sizeof(status_cp);
    ctx_cp.ai_popups = &pop_cp;
    ColonizeMsgCatalog game_txt_cp;
    memset(&game_txt_cp, 0, sizeof(game_txt_cp));
    if (!assets_msg_load_file(&game_txt_cp, "COLONIZE/GAME.TXT")) {
      return fail("@CANCELPEACE: GAME.TXT load failed");
    }
    ctx_cp.messages = &game_txt_cp;
    int fired = 0;
    for (int n = 0; n < 200 && !fired; ++n) {
      ai_diplo_euro_balance(&ctx_cp, 1);
      if (pop_cp.queue_count > 0) {
        fired = 1;
      }
    }
    assets_msg_free(&game_txt_cp);
    if (!fired) {
      return fail("@CANCELPEACE: 10ec war-declare CHOICE never fired");
    }
    if (pop_cp.queue[0].tag != AI_POPUP_TAG_DIPLO_WAR ||
        pop_cp.queue[0].kind != AI_POPUP_KIND_CHOICE) {
      return fail("@CANCELPEACE: expected DIPLO_WAR CHOICE");
    }
    if (strcmp(pop_cp.queue[0].body, "{France} cancel peace treaty with {England}.") != 0) {
      fprintf(stderr, "unit_ai_diplo: CANCELPEACE body '%s'\n", pop_cp.queue[0].body);
      return fail("@CANCELPEACE: CHOICE body should be authentic GAME.TXT line");
    }
  }

  /*
   * Marathon2 R6: native sticky deepen status also enqueues INFO OK
   * ("Natives remain hostile." when sticky stays/deepens to 2).
   * Matrix tick already wrote status; ensure ai_popups path is wired.
   * FA 3f41 full UI PARKED.
   */
  {
    ColonizeCol1Save ns;
    col1_save_init(&ns);
    memset(ns.nation, 0, sizeof(ns.nation));
    for (int i = 0; i < 4; ++i) {
      ns.player[i].control = 0;
    }
    ns.indian[0].alarm_by_player[0] = 90; /* DOS bands: relation 30 */ /* very-low → deepen */
    ns.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    for (int i = 1; i < 8; ++i) {
      ns.indian[i].alarm_by_player[0] = 20; /* relation 80 */
      ns.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    ns.nation[0].unknown26[8] = 1; /* at-war sticky → deepen to 2 on sync */
    ns.nation[0].gold = 40;
    char status_ns[128];
    status_ns[0] = '\0';
    AiPopupState pop_ns;
    ai_popup_init(&pop_ns);
    ColonizeDosRng rng_ns;
    dos_rng_seed(&rng_ns, 13);
    uint32_t turn_ns = 13;
    ColonizeTurnContext ctx_ns;
    memset(&ctx_ns, 0, sizeof(ctx_ns));
    ctx_ns.col1 = &ns;
    ctx_ns.col1_ok = true;
    ctx_ns.rng = &rng_ns;
    ctx_ns.turn_number = &turn_ns;
    ctx_ns.human_nation = 0;
    ctx_ns.status = status_ns;
    ctx_ns.status_size = sizeof(status_ns);
    ctx_ns.ai_popups = &pop_ns;
    ai_diplo_euro_balance(&ctx_ns, 0);
    if (ai_diplo_indian_hostility_sticky(&ns, 0) != 2) {
      return fail("M2R6 sticky deepen: sync should deepen sticky to 2");
    }
    if (strcmp(status_ns, "Natives remain hostile.") != 0) {
      fprintf(stderr, "unit_ai_diplo: sticky deepen status '%s'\n", status_ns);
      return fail("M2R6 sticky deepen: should status Natives remain hostile");
    }
    if (pop_ns.queue_count < 1) {
      return fail("M2R6 sticky deepen: should enqueue native status OK");
    }
    {
      int found = 0;
      for (int i = 0; i < pop_ns.queue_count; ++i) {
        if (pop_ns.queue[i].tag == AI_POPUP_TAG_INFO &&
            pop_ns.queue[i].kind == AI_POPUP_KIND_OK &&
            strcmp(pop_ns.queue[i].body, "Natives remain hostile.") == 0) {
          found = 1;
          break;
        }
      }
      if (!found) {
        return fail("M2R6 sticky deepen: INFO OK body must be Natives remain hostile");
      }
    }
  }

  /*
   * Marathon3 R1: Benjamin Franklin NW peace (docs/fandom_col1994.md).
   * Ownership gate: declare_war no-op (no sting / war-hit); euro_balance skips
   * 10ec declare pressure; at-war → make_peace. No gold fiction.
   */
  {
    ColonizeCol1Save fr;
    col1_save_init(&fr);
    memset(fr.nation, 0, sizeof(fr.nation));
    for (int i = 0; i < 4; ++i) {
      fr.player[i].control = 0;
      fr.player[i].country_name[0] = '\0';
    }
    snprintf(fr.player[0].country_name, sizeof(fr.player[0].country_name), "England");
    snprintf(fr.player[1].country_name, sizeof(fr.player[1].country_name), "France");
    fr.nation[0].gold = 400;
    fr.nation[1].gold = 50;
    fr.nation[0].tax_rate = 10;
    fr.nation[1].tax_rate = 10;
    for (int i = 0; i < 8; ++i) {
      fr.indian[i].alarm_by_player[0] = 0; /* content (was 120 on the old 255 scale) */
      fr.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      fr.indian[i].alarm_by_player[1] = 0; /* content (was 120 on the old 255 scale) */
      fr.indian[i].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    }
    /* Nation 0 owns Franklin (head owner + nation bitmask). */
    fr.head.founding_father[FF_BENJAMIN_FRANKLIN] = 0;
    fr.nation[0].founding_fathers[FF_BENJAMIN_FRANKLIN / 8] |=
      (uint8_t)(1u << (FF_BENJAMIN_FRANKLIN % 8));
    if (!founding_fathers_franklin_keeps_nw_peace(&fr, 0)) {
      return fail("M3R1 Franklin: ownership gate should be true for nation 0");
    }
    if (founding_fathers_franklin_keeps_nw_peace(&fr, 1)) {
      return fail("M3R1 Franklin: peer without FF must not keep NW peace alone");
    }

    const uint16_t gold0 = fr.nation[0].gold;
    const uint16_t gold1 = fr.nation[1].gold;
    const uint8_t rel0 = ai_diplo_indian_relation(&fr, 4 + (0), 0);
    ai_diplo_declare_war(&fr, 0, 1);
    if (ai_diplo_at_war(&fr, 0, 1)) {
      return fail("M3R1 Franklin: declare_war must no-op when pair has Franklin");
    }
    if (fr.nation[0].gold != gold0 || fr.nation[1].gold != gold1) {
      return fail("M3R1 Franklin: declare no-op must not drain war gold sting");
    }
    if (ai_diplo_indian_relation(&fr, 4 + (0), 0) != rel0) {
      return fail("M3R1 Franklin: declare no-op must skip Indian war-hit");
    }
    if (fr.nation[0].boycott_bitmap != 0) {
      return fail("M3R1 Franklin: declare no-op must skip wartime embargo");
    }

    /* Force WAR bytes then euro_balance should conclude peace (AI↔AI). */
    ai_diplo_or_both(&fr, 0, 1, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_MET));
    ai_diplo_clear_both(&fr, 0, 1, AI_DIPLO_PEACE);
    if (!ai_diplo_at_war(&fr, 0, 1)) {
      return fail("M3R1 Franklin setup: forced WAR bytes");
    }
    ColonizeDosRng rng_fr;
    dos_rng_seed(&rng_fr, 7);
    uint32_t turn_fr = 1;
    char status_fr[128];
    status_fr[0] = '\0';
    ColonizeTurnContext ctx_fr;
    memset(&ctx_fr, 0, sizeof(ctx_fr));
    ctx_fr.col1 = &fr;
    ctx_fr.col1_ok = true;
    ctx_fr.rng = &rng_fr;
    ctx_fr.turn_number = &turn_fr;
    ctx_fr.human_nation = 0;
    ctx_fr.status = status_fr;
    ctx_fr.status_size = sizeof(status_fr);
    ai_diplo_euro_balance(&ctx_fr, 0);
    if (ai_diplo_at_war(&fr, 0, 1)) {
      return fail("M3R1 Franklin: euro_balance at-war must make_peace");
    }
    if ((ai_diplo_read(&fr, 0, 1) & AI_DIPLO_PEACE) == 0) {
      return fail("M3R1 Franklin: after peace, PEACE bit should be set");
    }
    /*
     * Marathon3 R4 defensive: Franklin peace path skips upkeep (−5) and PARK
     * 8g prize (null-units would otherwise transfer). Gold stays 400/50.
     */
    if (fr.nation[0].gold != 400 || fr.nation[1].gold != 50) {
      fprintf(stderr, "unit_ai_diplo: M3R4 Franklin gold %u/%u (want 400/50)\n",
              (unsigned)fr.nation[0].gold, (unsigned)fr.nation[1].gold);
      return fail("M3R4 Franklin: at-war peace must skip upkeep and PARK prize");
    }
    /* Marathon3 R2: human chrome when Franklin concludes peace (make_peace_ctx). */
    if (strcmp(status_fr, "The England and France have signed a peace treaty.") != 0) {
      fprintf(stderr, "unit_ai_diplo: M3R2 Franklin peace status '%s'\n", status_fr);
      return fail("M3R2 Franklin: human party should status @SIGNTREATY with France");
    }

    /*
     * 10ec declare pressure: military self ≫ peer must not declare when
     * Franklin protects the pair (many RNG rolls).
     */
    fr.nation[0].gold = 5000; /* score boost */
    fr.nation[1].gold = 0;
    for (int seed = 1; seed < 80; ++seed) {
      dos_rng_seed(&rng_fr, (uint32_t)seed);
      ai_diplo_euro_balance(&ctx_fr, 0);
      if (ai_diplo_at_war(&fr, 0, 1)) {
        return fail("M3R1 Franklin: euro_balance must skip 10ec declare pressure");
      }
    }

    /* Peer owns Franklin: nation 1 protected — nation 0 declare still no-ops. */
    ColonizeCol1Save fr2;
    col1_save_init(&fr2);
    memset(fr2.nation, 0, sizeof(fr2.nation));
    for (int i = 0; i < 4; ++i) {
      fr2.player[i].control = 0;
    }
    fr2.nation[0].gold = 300;
    fr2.nation[1].gold = 300;
    fr2.head.founding_father[FF_BENJAMIN_FRANKLIN] = 1;
    fr2.nation[1].founding_fathers[FF_BENJAMIN_FRANKLIN / 8] |=
      (uint8_t)(1u << (FF_BENJAMIN_FRANKLIN % 8));
    ai_diplo_declare_war(&fr2, 0, 1);
    if (ai_diplo_at_war(&fr2, 0, 1)) {
      return fail("M3R1 Franklin: peer ownership must also block declare_war");
    }

    /* Elect effect: forced WAR then founding_fathers_tick elect clears it. */
    ColonizeCol1Save fr3;
    col1_save_init(&fr3);
    memset(fr3.nation, 0, sizeof(fr3.nation));
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      fr3.head.founding_father[i] = -1;
    }
    for (int i = 0; i < 4; ++i) {
      fr3.player[i].control = 0;
    }
    fr3.player[0].control = 0; /* human */
    fr3.nation[0].liberty_bells_total = 40;
    fr3.nation[0].founding_father_count = 0;
    fr3.nation[0].next_founding_father = FF_BENJAMIN_FRANKLIN;
    fr3.nation[0].gold = 200;
    fr3.nation[1].gold = 200;
    /* Force WAR before elect (raw flags — declare would be blocked after elect). */
    ai_diplo_or_both(&fr3, 0, 1, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_MET));
    ai_diplo_clear_both(&fr3, 0, 1, AI_DIPLO_PEACE);
    if (!ai_diplo_at_war(&fr3, 0, 1)) {
      return fail("M3R1 Franklin elect setup: need WAR before tick");
    }
    /* founding_fathers_tick reads the bells-since-elect side pool, not
     * liberty_bells_total directly — seed it from this save first. */
    founding_fathers_reset();
    founding_fathers_sync_from_col1(&fr3);
    uint32_t turn_e = 1;
    char status_e[128];
    status_e[0] = '\0';
    ColonizeTurnContext ctx_e;
    memset(&ctx_e, 0, sizeof(ctx_e));
    ctx_e.col1 = &fr3;
    ctx_e.col1_ok = true;
    ctx_e.turn_number = &turn_e;
    ctx_e.human_nation = 0;
    ctx_e.status = status_e;
    ctx_e.status_size = sizeof(status_e);
    founding_fathers_tick(&ctx_e);
    if (!founding_fathers_nation_has(&fr3, 0, FF_BENJAMIN_FRANKLIN)) {
      return fail("M3R1 Franklin elect: should own Benjamin Franklin");
    }
    if (ai_diplo_at_war(&fr3, 0, 1)) {
      return fail("M3R1 Franklin elect: should make_peace with Euro peers");
    }
  }

  /*
   * DS:0x54f6 Indian grudge/tension tier-crossing clamp (FUN_4cc6_00f2's
   * second half, viceroy_unpacked.c:80864-80900) — wired into
   * ai_diplo_indian_relation_delta 2026-08-24. Only the reachable "clamp
   * down" arm exists; the DOS else-branch is dead code (see ai_diplo.c
   * comment). Own local save/tribe fixture — does not touch the shared
   * `col1` used by the rest of this file.
   */
  {
    ColonizeCol1Save gt;
    col1_save_init(&gt);
    gt.head.tribe_count = 2;
    ColonizeCol1Tribe tribes[2];
    memset(tribes, 0, sizeof(tribes));
    tribes[0].nation_id = 4; /* Indian nation 0 */
    tribes[1].nation_id = 5; /* Indian nation 1 — must stay untouched below */
    gt.tribe = tribes;
    int16_t tension[2 * 4];
    memset(tension, 0, sizeof(tension));
    tension[0 * 4 + 0] = 0x70; /* tribe0/euro0: above both caps */
    tension[0 * 4 + 1] = 0x70; /* tribe0/euro1: untouched control */
    tension[1 * 4 + 0] = 0x70; /* tribe1/euro0: untouched control */
    tension[1 * 4 + 1] = 0x70; /* tribe1/euro1: above both caps */
    gt.indian_tension = tension;
    gt.owned = false; /* stack-owned fixture, nothing to free */

    /* FUN_4cc6_00f2 gates on a NEGATIVE ALARM delta (tribe cooling). High-alarm
     * tier (new alarm >=50 -> cap 0x60). */
    gt.indian[0].alarm_by_player[0] = 62; /* indian_nation 4, euro 0 */
    gt.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ai_diplo_indian_alarm_delta(&gt, 4, 0, -10); /* 62 -> 52, crosses tier */
    if (ai_diplo_indian_alarm(&gt, 4, 0) != 52) {
      return fail("54f6: alarm delta itself must still apply");
    }
    if (tension[0 * 4 + 0] != 0x60) {
      return fail("54f6: tribe0/euro0 should clamp to 0x60 (new relation >=50)");
    }
    if (tension[0 * 4 + 1] != 0x70) {
      return fail("54f6: tribe0/euro1 must be untouched (different euro nation)");
    }
    if (tension[1 * 4 + 0] != 0x70) {
      return fail("54f6: tribe1/euro0 must be untouched (different Indian nation)");
    }

    /* Low-alarm tier crossing (new alarm <50 -> cap 0x20). */
    gt.indian[1].alarm_by_player[1] = 30; /* indian_nation 5, euro 1 */
    gt.indian[1].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    ai_diplo_indian_alarm_delta(&gt, 5, 1, -20); /* 30 -> 10, crosses tier */
    if (tension[1 * 4 + 1] != 0x20) {
      return fail("54f6: tribe1/euro1 should clamp to 0x20 (new relation <50)");
    }

    /* Positive alarm delta never triggers the clamp (DOS gates on delta<0). */
    tension[1 * 4 + 1] = 0x70;
    gt.indian[1].alarm_by_player[1] = 10;
    gt.indian[1].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    ai_diplo_indian_alarm_delta(&gt, 5, 1, 40); /* 10 -> 50, crosses tier */
    if (tension[1 * 4 + 1] != 0x70) {
      return fail("54f6: positive delta must not touch the tension table");
    }
  }

  fprintf(stderr, "unit_ai_diplo: ok\n");
  /*
   * FUN_5bfb_153e phase 1 (2026-08-27, real terms): human self 0 vs target 1,
   * target colony with an adjacent target unit and no garrison -> the border
   * probe asserts worthy=1 with a nonzero score; the DS:0x53c8 stamp refreshes.
   */
  {
    ColonizeWorldMap wmap;
    memset(&wmap, 0, sizeof(wmap));
    wmap.width = 16;
    wmap.height = 16;
    wmap.tile_count = 256;
    wmap.terrain = calloc(256, 1);
    wmap.layer2 = calloc(256, 1);
    wmap.layer3 = calloc(256, 1);
    if (!wmap.terrain || !wmap.layer2 || !wmap.layer3) {
      return fail("153e alloc map");
    }
    for (int i = 0; i < 256; ++i) {
      wmap.terrain[i] = 1;
      wmap.layer3[i] = 1; /* continent 1 */
    }
    static ColonizeUnitPool wunits;
    units_reset(&wunits);
    wunits.type_count = 1;
    snprintf(wunits.types[0].name, sizeof(wunits.types[0].name), "Soldiers");
    wunits.types[0].movement = 1;
    wunits.types[0].attack = 2;
    wunits.types[0].defense = 2;
    wunits.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    const int wsid = units_spawn(&wunits, 0, 9, 8);
    ColonizeUnit* wsol = units_get(&wunits, wsid);
    if (!wsol) {
      return fail("153e spawn soldier");
    }
    wsol->nation_id = 1;
    static ColonizeColonyPool wcol;
    colonies_init(&wcol);
    ColonizeColony* tc = &wcol.colonies[0];
    tc->id = 0;
    tc->active = true;
    tc->nation_id = 1;
    tc->x = 8;
    tc->y = 8;
    tc->population = 2;
    tc->colonist_count = 2;
    tc->building_in_production = -1;
    ColonizeColony* sc = &wcol.colonies[1];
    sc->id = 1;
    sc->active = true;
    sc->nation_id = 0;
    sc->x = 2;
    sc->y = 2;
    sc->population = 2;
    sc->colonist_count = 2;
    sc->building_in_production = -1;
    wcol.colony_count = 2;
    wcol.next_id = 2;
    ColonizeCol1Save w;
    col1_save_init(&w);
    memset(w.nation, 0, sizeof(w.nation));
    memset(w.head.nation_relation, 0, sizeof(w.head.nation_relation));
    w.head.turn = 100; /* past the (difficulty-10)*-10 no-war threshold */
    w.head.difficulty = 1;
    w.head.human_player = 0;
    w.nation[0].gold = 5000;
    w.stuff.colony_counts[0] = 1;
    w.stuff.colony_counts[1] = 1;
    w.stuff.field_combat_totals[1] = 10;
    w.nation[0].euro_relation[1] = AI_DIPLO_MET;
    w.nation[1].euro_relation[0] = AI_DIPLO_MET;
    ColonizeTurnContext wctx;
    memset(&wctx, 0, sizeof(wctx));
    wctx.col1 = &w;
    wctx.col1_ok = true;
    wctx.map = &wmap;
    wctx.units = &wunits;
    wctx.colonies = &wcol;
    wctx.human_nation = 0;
    const Ai153eWorthinessScore ws = ai_diplo_153e_worthiness_score(&wctx, 0, 1, -1, 1);
    free(wmap.terrain);
    free(wmap.layer2);
    free(wmap.layer3);
    if (!ws.handled) {
      return fail("153e: forced gate should run phase 1");
    }
    if (!ws.worthy || ws.score <= 0) {
      fprintf(stderr, "unit_ai_diplo: 153e worthy=%d score=%d\n", ws.worthy, ws.score);
      return fail("153e: unguarded target colony with adjacent target unit should be worthy");
    }
    if (w.head.nation_relation[1] != 100) {
      return fail("153e: DS:0x53c8[target] must be stamped with the turn");
    }
    if (ai_diplo_00f8_top_ranked_nation(&w) != 0) {
      return fail("153e: nation 0 (5000 gold) should top the 00f8 rank table");
    }
    /* AI self never scores (13b0 branch). */
    const Ai153eWorthinessScore wa = ai_diplo_153e_worthiness_score(&wctx, 1, 0, -1, 1);
    if (!wa.handled || wa.worthy || wa.score != 0) {
      return fail("153e: AI self must take the 13b0 branch (handled, no score)");
    }
  }

  /*
   * FUN_5bfb_153e phases 2-4 (2026-08-27): an unmet AI Euro unit next to the
   * human's unit opens the encounter dialog — greeting OK first, then the
   * partition-treaty CHOICE (WORTHY); accepting it signs PEACE both ways and
   * stamps the DS:0x53c8 cooldown.
   */
  {
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("153e talk alloc map");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 1;
      emap.layer3[i] = 1;
    }
    static ColonizeUnitPool eunits;
    units_reset(&eunits);
    eunits.type_count = 1;
    snprintf(eunits.types[0].name, sizeof(eunits.types[0].name), "Scouts");
    eunits.types[0].movement = 4;
    eunits.types[0].attack = 1;
    eunits.types[0].defense = 1;
    eunits.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    const int ha = units_spawn(&eunits, 0, 5, 5);
    const int hb = units_spawn(&eunits, 0, 6, 5);
    ColonizeUnit* pa = units_get(&eunits, ha);
    ColonizeUnit* pb = units_get(&eunits, hb);
    if (!pa || !pb) {
      return fail("153e talk spawn");
    }
    pa->nation_id = 0;
    pb->nation_id = 1;
    static ColonizeColonyPool ecol;
    colonies_init(&ecol);
    ColonizeCol1Save e;
    col1_save_init(&e);
    memset(e.nation, 0, sizeof(e.nation));
    memset(e.head.nation_relation, 0, sizeof(e.head.nation_relation));
    e.head.turn = 30;
    e.head.difficulty = 2;
    e.head.human_player = 0;
    e.nation[0].gold = 1000;
    e.nation[1].gold = 1000;
    snprintf(e.player[0].country_name, sizeof(e.player[0].country_name), "England");
    snprintf(e.player[1].country_name, sizeof(e.player[1].country_name), "France");
    AiPopupState epop;
    ai_popup_init(&epop);
    ColonizeDosRng erng;
    dos_rng_seed(&erng, 5);
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.col1 = &e;
    ectx.col1_ok = true;
    ectx.map = &emap;
    ectx.units = &eunits;
    ectx.colonies = &ecol;
    ectx.human_nation = 0;
    ectx.ai_popups = &epop;
    ectx.rng = &erng;
    const int started = ai_diplo_153e_encounter(&ectx, 0, 1, pa->id);
    if (!started || epop.queue_count < 2) {
      fprintf(stderr, "unit_ai_diplo: 153e talk started=%d queued=%d\n", started, epop.queue_count);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("153e talk: unmet adjacent AI Euro must open the greeting + partition CHOICE");
    }
    if (epop.queue[0].tag != AI_POPUP_TAG_DIPLO_TALK || epop.queue[0].kind != AI_POPUP_KIND_OK ||
        epop.queue[1].kind != AI_POPUP_KIND_CHOICE) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("153e talk: expected greeting OK then a CHOICE");
    }
    /* Drive the talk: answer every CHOICE with option 1 (SIEGES: stay,
     * TRIBUTE: refuse, WORTHY: Yes) until the queue drains. */
    for (int guard = 0; guard < 12 && epop.queue_count > 0; ++guard) {
      AiPopupRequest front = epop.queue[0];
      memmove(&epop.queue[0], &epop.queue[1], sizeof(epop.queue[0]) * (size_t)(epop.queue_count - 1));
      epop.queue_count--;
      if (front.kind != AI_POPUP_KIND_CHOICE) {
        continue;
      }
      epop.has_result = true;
      epop.result_cancelled = false;
      epop.result_tag = AI_POPUP_TAG_DIPLO_TALK;
      epop.result_choice_id = 1;
      epop.result_nation_a = 0;
      epop.result_nation_b = 1;
      epop.result_payload = front.payload;
      ai_diplo_apply_popup_result(&ectx, &epop);
      epop.has_result = false;
    }
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    if (!(e.nation[0].euro_relation[1] & AI_DIPLO_PEACE) || !(e.nation[1].euro_relation[0] & AI_DIPLO_PEACE)) {
      return fail("153e talk: accepting the partition treaty must sign PEACE both ways");
    }
    if (e.head.nation_relation[1] != 30 + 0x10) {
      return fail("153e talk: partition must stamp DS:0x53c8[target] = turn + 16");
    }
    if (ai_diplo_153e_encounter(&ectx, 0, 1, pa->id)) {
      return fail("153e talk: a second encounter the same turn must not reopen the talk");
    }
  }

  /*
   * FUN_5bfb_153e @WANTSTUFF demand phase (2026-09-06): when the encounter's
   * moving unit belongs to the TARGET (an AI unit walked up to a human
   * colony), the AI demands goods — dialog names the picked cargo, but the
   * DOS transfer indexes the stock rows with the stale rival-loop counter
   * (== 4), moving FURS (OVL16 asm 0x2995-0x29B2, byte-verified). Accepting
   * must move Furs by the demanded amount and leave the named cargo alone.
   */
  {
    ColonizeWorldMap qmap;
    memset(&qmap, 0, sizeof(qmap));
    qmap.width = 16;
    qmap.height = 16;
    qmap.tile_count = 256;
    qmap.terrain = calloc(256, 1);
    qmap.layer2 = calloc(256, 1);
    qmap.layer3 = calloc(256, 1);
    if (!qmap.terrain || !qmap.layer2 || !qmap.layer3) {
      return fail("153e wantstuff alloc map");
    }
    for (int i = 0; i < 256; ++i) {
      qmap.terrain[i] = 1;
      qmap.layer3[i] = 1;
    }
    static ColonizeUnitPool qunits;
    units_reset(&qunits);
    qunits.type_count = 1;
    snprintf(qunits.types[0].name, sizeof(qunits.types[0].name), "Dragoons");
    qunits.types[0].movement = 4;
    qunits.types[0].attack = 3;
    qunits.types[0].defense = 3;
    qunits.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    const int aiu = units_spawn(&qunits, 0, 5, 6); /* AI unit next to human colony */
    ColonizeUnit* pai = units_get(&qunits, aiu);
    if (!pai) {
      return fail("153e wantstuff spawn");
    }
    pai->nation_id = 1;
    static ColonizeColonyPool qcol;
    colonies_init(&qcol);
    ColonizeColony* hc = &qcol.colonies[0]; /* human colony = demand source */
    hc->id = 0;
    hc->active = true;
    hc->nation_id = 0;
    hc->x = 5;
    hc->y = 5;
    hc->population = 2;
    hc->colonist_count = 2;
    hc->building_in_production = -1;
    hc->stock[COLONIZE_CARGO_MUSKETS] = 60;
    hc->stock[COLONIZE_CARGO_FURS] = 10;
    ColonizeColony* ac = &qcol.colonies[1]; /* target's nearest colony = receiver */
    ac->id = 1;
    ac->active = true;
    ac->nation_id = 1;
    ac->x = 8;
    ac->y = 8;
    ac->population = 2;
    ac->colonist_count = 2;
    ac->building_in_production = -1;
    qcol.colony_count = 2;
    qcol.next_id = 2;
    ColonizeCol1Save q;
    col1_save_init(&q);
    memset(q.nation, 0, sizeof(q.nation));
    memset(q.head.nation_relation, 0, sizeof(q.head.nation_relation));
    q.head.turn = 100; /* past the (difficulty-10)*-10 threshold */
    q.head.difficulty = 1;
    q.head.human_player = 0;
    q.nation[0].gold = 0; /* gold < score → TRIBUTE skipped, score stays != 999 */
    q.stuff.colony_counts[0] = 1;
    q.stuff.colony_counts[1] = 1;
    q.stuff.field_combat_totals[1] = 10; /* target×3 >= self keeps worthy */
    q.stuff.colony_pop_totals[1] = 8;    /* quarter=2 > own_border(0) → SIEGES skipped */
    q.stuff.census_pop_proxy[0] = 8;     /* skip the small-nation score halving */
    q.nation[1].trade.euro_price[COLONIZE_CARGO_MUSKETS] = 200; /* -0x7b44 weight */
    snprintf(q.player[0].country_name, sizeof(q.player[0].country_name), "England");
    snprintf(q.player[1].country_name, sizeof(q.player[1].country_name), "France");
    AiPopupState qpop;
    ai_popup_init(&qpop);
    ColonizeDosRng qrng;
    dos_rng_seed(&qrng, 7);
    ColonizeTurnContext qctx;
    memset(&qctx, 0, sizeof(qctx));
    qctx.col1 = &q;
    qctx.col1_ok = true;
    qctx.map = &qmap;
    qctx.units = &qunits;
    qctx.colonies = &qcol;
    qctx.human_nation = 0;
    qctx.ai_popups = &qpop;
    qctx.rng = &qrng;
    const int started = ai_diplo_153e_encounter(&qctx, 0, 1, pai->id);
    free(qmap.terrain);
    free(qmap.layer2);
    free(qmap.layer3);
    if (!started) {
      return fail("153e wantstuff: AI unit next to human colony must open the talk");
    }
    if ((q.nation[0].euro_relation[1] & AI_DIPLO_MET) == 0 ||
        (q.nation[1].euro_relation[0] & AI_DIPLO_MET) == 0) {
      return fail("153e wantstuff: a started talk must stamp MET both ways (3180 :98530)");
    }
    int saw_wantstuff = 0;
    for (int guard = 0; guard < 12 && qpop.queue_count > 0; ++guard) {
      AiPopupRequest front = qpop.queue[0];
      memmove(&qpop.queue[0], &qpop.queue[1], sizeof(qpop.queue[0]) * (size_t)(qpop.queue_count - 1));
      qpop.queue_count--;
      if (front.kind != AI_POPUP_KIND_CHOICE) {
        continue;
      }
      /* WANTSTUFF: accept (choice 2). Everything else: option 1. */
      const int is_want = strstr(front.body, "reparations") != NULL;
      if (is_want && !saw_wantstuff) {
        saw_wantstuff = 1;
      }
      qpop.has_result = true;
      qpop.result_cancelled = false;
      qpop.result_tag = AI_POPUP_TAG_DIPLO_TALK;
      qpop.result_choice_id = (is_want && saw_wantstuff == 1) ? 2 : 1;
      if (is_want) {
        saw_wantstuff = 2; /* answer the demand once */
      }
      qpop.result_nation_a = 0;
      qpop.result_nation_b = 1;
      qpop.result_payload = front.payload;
      ai_diplo_apply_popup_result(&qctx, &qpop);
      qpop.has_result = false;
    }
    if (!saw_wantstuff) {
      return fail("153e wantstuff: the goods demand CHOICE must be queued");
    }
    if (qcol.colonies[1].stock[COLONIZE_CARGO_FURS] != 60) {
      fprintf(stderr, "unit_ai_diplo: receiver furs=%d\n", qcol.colonies[1].stock[COLONIZE_CARGO_FURS]);
      return fail("153e wantstuff: accepting must move FURS by the demanded amount (DOS stale index)");
    }
    if (qcol.colonies[0].stock[COLONIZE_CARGO_FURS] != 0) {
      return fail("153e wantstuff: source furs floor at 0 (Linux bound on the DOS underflow)");
    }
    if (qcol.colonies[0].stock[COLONIZE_CARGO_MUSKETS] != 60) {
      return fail("153e wantstuff: the NAMED cargo must not move (DOS transfers Furs)");
    }
  }

  return 0;
}
