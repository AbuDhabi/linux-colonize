/* Smoke: bilateral 15b3 diplo bytes, declare-war leaves gold/tax alone
 * (the −100/+1 "sting" was retired 2026-09-09, smell #47),
 * Euro war does not boycott Europe cargos or seed a fatigue timer,
 * peace does not lift a boycott (port_bugs.md #1104), treaty timer
 * decrement, no Indian drift,
 * Sugar/Tobacco/Tools + first newly boycotted cargo status +
 * R14 full wartime mask declare smoke +
 * Marathon3 R1 Benjamin Franklin NW peace gate (declare no-op) + R2 spawn-only Privateer
 * (PARK 8g prize when units set) + Franklin Peace concluded human chrome +
 * R4 Franklin at-war skips the PARK prize (gold unchanged) +
 * Marathon4 R1 Privateer commission is status-only (no INFO OK popup).
 * (OpenCol-only Euro×Euro alliance machinery + its tests retired T2.4
 * 2026-09-06 — DOS has no Euro×Euro alliances.) */
#include "../common/ai_fixture.h"
#include "core/ai_diplo.h"
#include "core/ai_internal.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/col1_stuff_census.h"
#include "core/colony.h"
#include "core/dos_buggy.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/turn.h"
#include "core/units.h"
#include "../common/test_runner.h"

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

#include "../common/test_catalogs.h"
#include "core/popup_msg.h"
#include "core/reports.h"

#define TEST_NAME "unit_ai_diplo"
#include "../common/test_fail.h"

static int case_declare_peace_narrative(void) {
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
   * 2026-09-09 (smell #47): the "thin 153e" −100 gold / +1 tax_rate pair was
   * retired as a fandom invention — no DOS declare path writes either (tax
   * only from FUN_38fd_44a4's @TAXRAISE; declare-adjacent gold moves are
   * transfers, not symmetric drains). The probes below now pin the absence.
   * Indians dislike war: −5 on relation_by_indian[0..7] both sides.
   * Euro war does not boycott Europe cargos (DOS king tea-party only), and
   * declare-war does not seed the 6d8e treaty timer. */
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
  if (col1.nation[0].gold != 250 || col1.nation[1].gold != 80) {
    return fail("declare_war must not drain gold (smell #47: no DOS gold sting)");
  }
  if (col1.nation[2].gold != 500) {
    return fail("war(0,1) must not drain gold of nation 2");
  }
  if (col1.nation[0].tax_rate != 10 || col1.nation[1].tax_rate != 74) {
    return fail("declare_war must not bump tax_rate (smell #47: DOS bumps tax only in 38fd_44a4)");
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
  if (!ai_diplo_at_war(&col1, 0, 1) || ai_diplo_at_war(&col1, 0, 2)) {
    return fail("ai_diplo_at_war should report the declared pair only");
  }
  if (col1.nation[0].unknown26[1] != 0 || col1.nation[1].unknown26[0] != 0) {
    return fail("declare_war must not seed a war-fatigue treaty timer");
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
  /* Re-declare: no Indian hit; still no Europe boycott, still no gold/tax. */
  ai_diplo_declare_war(&col1, 0, 1);
  if (col1.nation[0].gold != 250) {
    return fail("re-declare_war should not touch gold");
  }
  if (col1.nation[0].tax_rate != 10) {
    return fail("re-declare_war should not touch tax");
  }
  if (ai_diplo_indian_relation(&col1, 4 + (0), 0) != 50) {
    return fail("re-declare_war should not re-hit Indian relations");
  }
  if ((col1.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
    return fail("re-declare_war must not boycott Europe cargos");
  }

  /* make_peace (fixture helper): clear WAR both ways, set PEACE. No gold
   * cost, and port_bugs.md #1104: it must not touch boycott_bitmap — DOS has no
   * peace writer of nation+0x20, so a tea-party boycott survives peace. */
  {
    const uint16_t gold0 = col1.nation[0].gold;
    const uint16_t gold1 = col1.nation[1].gold;
    col1.nation[0].boycott_bitmap = (uint16_t)(1u << COLONIZE_CARGO_TOOLS);
    ai_diplo_make_peace(&col1, 0, 1);
    if (ai_diplo_at_war(&col1, 0, 1) || ai_diplo_at_war(&col1, 1, 0)) {
      return fail("make_peace should clear WAR both ways");
    }
    if ((ai_diplo_read(&col1, 0, 1) & AI_DIPLO_PEACE) == 0 ||
        (ai_diplo_read(&col1, 1, 0) & AI_DIPLO_PEACE) == 0) {
      return fail("make_peace should set PEACE both ways");
    }
    if (col1.nation[0].boycott_bitmap != (uint16_t)(1u << COLONIZE_CARGO_TOOLS)) {
      return fail("make_peace must not lift a boycott (port_bugs.md #1104)");
    }
    col1.nation[0].boycott_bitmap = 0;
    if (col1.nation[0].gold != gold0 || col1.nation[1].gold != gold1) {
      return fail("make_peace should not change gold (no cost)");
    }
  }

  /* Re-war for the treaty-timer tests below. */
  ai_diplo_declare_war(&col1, 0, 1);
  if ((col1.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0 ||
      (col1.nation[1].boycott_bitmap & AI_DIPLO_SMOKE_WARTIME_MASK) != 0) {
    return fail("re-war after make_peace must not boycott Europe cargos");
  }

  /* 6d8e step 4: treaty timer decrement (the OpenCol-only ally expiry-break arm
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
    ctx.messages = test_game_txt();
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
    ctx_d.messages = test_game_txt();
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

  /* A second, unrelated pair: declare still leaves tax_rate and gold alone
   * (smell #47 retirement; this used to be the +1-bump "cap at 75" probe). */
  col1.nation[2].gold = 200;
  col1.nation[3].gold = 200;
  col1.nation[2].tax_rate = 75;
  col1.nation[3].tax_rate = 75;
  col1.nation[2].boycott_bitmap = 0;
  col1.nation[3].boycott_bitmap = 0;
  ai_diplo_declare_war(&col1, 2, 3);
  if (col1.nation[2].tax_rate != 75 || col1.nation[3].tax_rate != 75) {
    return fail("declare_war must not raise tax_rate");
  }
  if (col1.nation[2].gold != 200 || col1.nation[3].gold != 200) {
    return fail("declare_war(2,3) must not drain gold");
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
  return 0;
}

static int case_r11_no_indian_war_hit(void) {
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
  return 0;
}

static int case_trade_deepen(void) {
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
    /* No treasury moves on a declare at all: the flat -100 sting went
     * 2026-09-09 (#47) and the colony-gap -25 trade sting with it (audit
     * follow-up A). DOS has no constant gold decrement anywhere. */
    if (tw.nation[0].gold != 400) {
      fprintf(stderr, "unit_ai_diplo: rich gold after trade war %u (want 400)\n",
              (unsigned)tw.nation[0].gold);
      free(tw.colony);
      return fail("colony-gap declare must not drain the richer treasury");
    }
    if (tw.nation[1].gold != 50) {
      free(tw.colony);
      return fail("colony-gap declare must leave the poorer side's gold alone");
    }
    free(tw.colony);
  return 0;
}

static int case_declare_war_side_effects(void) {
    ColonizeCol1Save st;
    col1_save_init(&st);
    memset(st.nation, 0, sizeof(st.nation));
    for (int i = 0; i < 4; ++i) {
      st.player[i].control = 0;
      st.player[i].country_name[0] = '\0';
    }

    /* R12: a declare ORs no boycott bit onto the King's existing ones. */
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    st.nation[0].boycott_bitmap =
      (uint16_t)(AI_DIPLO_SMOKE_SUGAR_BIT | AI_DIPLO_SMOKE_TOBACCO_BIT |
                 AI_DIPLO_SMOKE_TOOLS_BIT);
    ai_diplo_declare_war(&st, 0, 2);
    if (!ai_diplo_at_war(&st, 0, 2)) {
      return fail("declare_war should set WAR");
    }
    if ((st.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_FOOD_BIT) != 0) {
      return fail("declare_war must not OR Food onto existing king boycotts");
    }

    /*
     * @SNEAK ("Sneak attack by the treacherous {attacker}!") — real text
     * confirmed 2026-08-14 (COLONIZE/GAME.TXT), user-confirmed live mechanic
     * (see src/core/ai_euro.c ai_euro_try_attack). Direct primitive check
     * since ai_euro_try_attack itself is static and its dispatcher-turn
     * context is too entangled for an end-to-end assertion, so this pins the exact fallback text and the real GAME.TXT rendering
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
      assets_msg_free(&game_txt_sneak);
    }

    /* 2026-09-03: the −5 war hit is retired (no DOS declare site touches
     * Indian relations). */
    ai_diplo_make_peace(&st, 0, 2);
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 70; /* relation 30 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    ai_diplo_declare_war(&st, 0, 2);
    if (ai_diplo_indian_relation(&st, 4 + (0), 0) != 30) {
      return fail("declare_war must not touch Indian relations (hit retired)");
    }
  return 0;
}

static int case_indian_relation_read(void) {
    ColonizeCol1Save sp;
    col1_save_init(&sp);
    memset(sp.nation, 0, sizeof(sp.nation));
    for (int i = 0; i < 4; ++i) {
      sp.player[i].control = 0;
    }
    sp.indian[0].alarm_by_player[0] = 90; /* DOS bands: relation 10 */
    sp.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    for (int i = 1; i < 8; ++i) {
      sp.indian[i].alarm_by_player[0] = 20; /* relation 80 */
      sp.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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

  return 0;
}

static int case_r2_no_fatigue_tools(void) {
  ColonizeCol1Save wf;
  col1_save_init(&wf);
  memset(wf.nation, 0, sizeof(wf.nation));
  for (int i = 0; i < 4; ++i) {
    wf.player[i].control = 0;
    wf.player[i].country_name[0] = '\0';
  }
  wf.nation[0].gold = 600;
  wf.nation[1].gold = 600;
  ai_diplo_declare_war(&wf, 0, 1);
  if (wf.nation[0].unknown26[1] != 0 || wf.nation[1].unknown26[0] != 0) {
    return fail("declare_war must not seed a war-fatigue treaty timer");
  }

    /* Human declare with a colony gap: still no Tools boycott. */
    ColonizeCol1Save ts;
    col1_save_init(&ts);
    memset(ts.nation, 0, sizeof(ts.nation));
    for (int i = 0; i < 4; ++i) {
      ts.player[i].control = 0;
      ts.player[i].country_name[0] = '\0';
    }
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
    ai_diplo_declare_war(&ts, 0, 1);
    if ((ts.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_TOOLS_BIT) != 0) {
      free(ts.colony);
      return fail("Tools status setup: colony-gap must not set Tools bit");
    }
    free(ts.colony);
  return 0;
}

static int case_r3_r4_sugar_rum_cigars_boycott(void) {
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
    /* A very hostile tribe toward nation 0. */
    r3.indian[0].alarm_by_player[0] = 90; /* DOS bands: relation 10 */
    r3.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    char status[128];
    status[0] = '\0';
    ColonizeDosRng rng_r3;
    dos_rng_seed(&rng_r3, 7);
    uint32_t turn_r3 = 7;
    ColonizeTurnContext ctx_r3;
    memset(&ctx_r3, 0, sizeof(ctx_r3));
    ctx_r3.messages = test_game_txt();
    ctx_r3.col1 = &r3;
    ctx_r3.col1_ok = true;
    ctx_r3.rng = &rng_r3;
    ctx_r3.turn_number = &turn_r3;
    /* 13b0 (2026-08-27) only runs on a 3180 encounter: adjacent units. */
    static ColonizeUnitPool units_r3;
    units_reset(&units_r3);
    units_set_occupancy_map(NULL);
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
    /* Indian hostility does NOT block the 13b0 tick: DOS 153e sends an AI
     * self straight to 13b0 (raw 97406-97411). DOSBox 1557:
     * Spain and France signed on first contact. */
    int signed_deep = 0;
    for (int n = 0; n < 6 && !signed_deep; ++n) {
      r3.nation[0].gold = 600;
      r3.nation[1].gold = 600;
      r3.indian[0].alarm_by_player[0] = 90; /* DOS bands: relation 10 */
      r3.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      ai_diplo_13b0_encounter(&ctx_r3, 0, 1); /* the 3180 encounter */
      signed_deep = (r3.nation[0].euro_relation[1] & AI_DIPLO_PEACE) != 0;
      turn_r3++;
    }
    if (!signed_deep) {
      return fail("Indian hostility must not refuse the 13b0 treaty (raw 97406)");
    }
    r3.nation[0].euro_relation[1] = 0;
    r3.nation[1].euro_relation[0] = 0;
    /* Peaceful tribes: treaty signs under the same near-parity RNG. */
    for (int i = 0; i < 8; ++i) {
      r3.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      r3.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    /* FUN_5bfb_13b0 (2026-08-27): not war-worthy (turn <= 39) and no PEACE →
     * @SIGNTREATY sets PEACE both ways within the (a+turn+b)%3 cadence. */
    int treaty = 0;
    for (int n = 0; n < 6; ++n) {
      r3.nation[0].gold = 600;
      r3.nation[1].gold = 600;
      ai_diplo_13b0_encounter(&ctx_r3, 0, 1); /* the 3180 encounter */
      if ((r3.nation[0].euro_relation[1] & AI_DIPLO_PEACE) &&
          (r3.nation[1].euro_relation[0] & AI_DIPLO_PEACE)) {
        treaty = 1;
        break;
      }
      turn_r3++;
    }
    if (!treaty) {
      return fail("13b0 should sign a peace treaty");
    }

    /* 153e's exit refreshes the other nation's timer even when 13b0's
     * cadence skips its body. Signing itself sets the mover's timer to 1. */
    if (r3.nation[0].treaty_timer[1] != 1 ||
        r3.nation[1].treaty_timer[0] != (6 - r3.head.difficulty) * 2) {
      return fail("AI audience must refresh the other nation's treaty cooldown");
    }
    while ((turn_r3 + 1) % 3 == 0) turn_r3++;
    r3.nation[0].treaty_timer[1] = 7;
    r3.nation[1].treaty_timer[0] = 0;
    r3.nation[0].founding_fathers[FF_BENJAMIN_FRANKLIN / 8] |=
      (uint8_t)(1u << (FF_BENJAMIN_FRANKLIN % 8));
    ai_diplo_13b0_encounter(&ctx_r3, 0, 1);
    if (r3.nation[0].treaty_timer[1] != 7 ||
        r3.nation[1].treaty_timer[0] != 6 - r3.head.difficulty) {
      return fail("audience exit must use mover Franklin and preserve mover timer");
    }

    /* A king (tea-party) Sugar boycott survives both war and peace: neither
     * DOS path writes nation+0x20 (port_bugs.md #1104). */
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
    if ((sg.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_SUGAR_BIT) == 0) {
      return fail("peace must not lift a king Sugar boycott (port_bugs.md #1104)");
    }
    if (ai_diplo_at_war(&sg, 0, 1)) {
      return fail("after peace, at_war should be clear");
    }

    /* R4/R11: no Rum+Cigars+Cotton wartime boycott on declare. */
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

    if ((rc.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_TOOLS_BIT) != 0) {
      return fail("declare_war must not OR Tools boycott");
    }
  return 0;
}

static int case_r6_ore_silver_boycott(void) {
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
  return 0;
}

static int case_r8_lumber_boycott_privateer(void) {
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
  return 0;
}

static int case_r9_horses_muskets_boycott(void) {
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

  return 0;
}

/* A DIPLO_PEACE result carries no action: DOS treaty notices are OK-only, the
 * old Accept/Refuse handler was dead (port_bugs.md #1104). */
static int case_diplo_peace_result_no_action(void) {
    {
      ColonizeCol1Save w2;
      col1_save_init(&w2);
      memset(w2.nation, 0, sizeof(w2.nation));
      for (int i = 0; i < 4; ++i) {
        w2.player[i].control = 0;
        w2.player[i].country_name[0] = '\0';
      }
      AiPopupState pop_w2;
      ai_popup_init(&pop_w2);
      ColonizeTurnContext ctx_w2;
      memset(&ctx_w2, 0, sizeof(ctx_w2));
      ctx_w2.col1 = &w2;
      ctx_w2.col1_ok = true;
      ctx_w2.human_nation = 0;
      ctx_w2.ai_popups = &pop_w2;
      ai_diplo_declare_war(&w2, 1, 0);
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
      if (!ai_diplo_at_war(&w2, 0, 1)) {
        return fail("R2 peace: a DIPLO_PEACE result must not make peace (port_bugs.md #1104)");
      }
      ai_popup_consume_result(&pop_w2);
    }
  return 0;
}
static int case_marathon3_franklin_peace_gate(void) {
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
    if (!founding_fathers_nation_has(&fr, 0, FF_BENJAMIN_FRANKLIN)) {
      return fail("M3R1 Franklin: ownership gate should be true for nation 0");
    }
    if (founding_fathers_nation_has(&fr, 1, FF_BENJAMIN_FRANKLIN)) {
      return fail("M3R1 Franklin: peer without FF must not keep NW peace alone");
    }

    const uint16_t gold0 = fr.nation[0].gold;
    const uint16_t gold1 = fr.nation[1].gold;
    const uint8_t rel0 = ai_diplo_indian_relation(&fr, 4 + (0), 0);
    ai_diplo_declare_war(&fr, 0, 1);
    /* port_bugs.md #472: no DOS declare site reads Franklin -- war sticks. */
    if (!ai_diplo_at_war(&fr, 0, 1)) {
      return fail("M3R1 Franklin: declare_war must stick (no DOS Franklin gate)");
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

    /* port_bugs.md #472: the euro_balance Franklin auto-peace / no-pressure arms
     * were invented (DOS reads FF 0x13 only in 38fd_5930 + 153e); removed. */
    /* Peer owns Franklin: declare still sticks. */
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
    if (!ai_diplo_at_war(&fr2, 0, 1)) {
      return fail("M3R1 Franklin: peer ownership must not block declare_war");
    }

    /* Elect: FUN_4345_0342 (raw 73044) has no case 0x13 -- electing Franklin
     * touches no relation, so a standing WAR survives the elect. */
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
    fr3.nation[0].liberty_bells_pool = 40;
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
    /* port_bugs.md #933: the pool IS nation+0xc (liberty_bells_pool); nothing to seed. */
    founding_fathers_reset();
    uint32_t turn_e = 1;
    char status_e[128];
    status_e[0] = '\0';
    ColonizeTurnContext ctx_e;
    memset(&ctx_e, 0, sizeof(ctx_e));
    ctx_e.messages = test_game_txt();
    ctx_e.col1 = &fr3;
    ctx_e.col1_ok = true;
    ctx_e.turn_number = &turn_e;
    ctx_e.human_nation = 0;
    ctx_e.status = status_e;
    ctx_e.status_size = sizeof(status_e);
    founding_fathers_tick(&ctx_e);
    founding_fathers_tick_human_elect(&ctx_e);
    if (!founding_fathers_nation_has(&fr3, 0, FF_BENJAMIN_FRANKLIN)) {
      return fail("M3R1 Franklin elect: should own Benjamin Franklin");
    }
    if (!ai_diplo_at_war(&fr3, 0, 1)) {
      return fail("M3R1 Franklin elect: must not make_peace (no DOS elect case)");
    }
  return 0;
}

static int case_indian_grudge_tension_clamp(void) {
    ColonizeCol1Save gt;
    col1_save_init(&gt);
    gt.head.tribe_count = 2;
    ColonizeCol1Tribe tribes[2];
    memset(tribes, 0, sizeof(tribes));
    tribes[0].nation_id = 4; /* Indian nation 0 */
    tribes[1].nation_id = 5; /* Indian nation 1 — must stay untouched below */
    gt.tribe = tribes;
    /* DS:0x54f6 slot = the record's own attitude[euro] word, tribe.alarm[]. */
    col1_tribe_attitude_set(&tribes[0], 0, 0x70); /* tribe0/euro0: above both caps */
    col1_tribe_attitude_set(&tribes[0], 1, 0x70); /* tribe0/euro1: untouched control */
    col1_tribe_attitude_set(&tribes[1], 0, 0x70); /* tribe1/euro0: untouched control */
    col1_tribe_attitude_set(&tribes[1], 1, 0x70); /* tribe1/euro1: above both caps */
    gt.owned = false; /* stack-owned fixture, nothing to free */

    /* FUN_4cc6_00f2 gates on a NEGATIVE ALARM delta (tribe cooling). High-alarm
     * tier (new alarm >=50 -> cap 0x60). */
    gt.indian[0].alarm_by_player[0] = 62; /* indian_nation 4, euro 0 */
    gt.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ai_diplo_indian_alarm_delta(&gt, 4, 0, -10); /* 62 -> 52, crosses tier */
    if (ai_diplo_indian_alarm(&gt, 4, 0) != 52) {
      return fail("54f6: alarm delta itself must still apply");
    }
    if (col1_tribe_attitude(&tribes[0], 0) != 0x60) {
      return fail("54f6: tribe0/euro0 should clamp to 0x60 (new relation >=50)");
    }
    if (col1_tribe_attitude(&tribes[0], 1) != 0x70) {
      return fail("54f6: tribe0/euro1 must be untouched (different euro nation)");
    }
    if (col1_tribe_attitude(&tribes[1], 0) != 0x70) {
      return fail("54f6: tribe1/euro0 must be untouched (different Indian nation)");
    }

    /* Low-alarm tier crossing (new alarm <50 -> cap 0x20). */
    gt.indian[1].alarm_by_player[1] = 30; /* indian_nation 5, euro 1 */
    gt.indian[1].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    ai_diplo_indian_alarm_delta(&gt, 5, 1, -20); /* 30 -> 10, crosses tier */
    if (col1_tribe_attitude(&tribes[1], 1) != 0x20) {
      return fail("54f6: tribe1/euro1 should clamp to 0x20 (new relation <50)");
    }

    /* The clamp writes the WHOLE word, so a recorded attack count (high byte)
     * is clamped away with the friction byte. */
    tribes[1].alarm[1].friction = 0x10;
    tribes[1].alarm[1].attacks = 2; /* word 0x210, above cap 0x20 */
    gt.indian[1].alarm_by_player[1] = 30;
    gt.indian[1].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    ai_diplo_indian_alarm_delta(&gt, 5, 1, -20); /* 30 -> 10, crosses tier */
    if (col1_tribe_attitude(&tribes[1], 1) != 0x20 || tribes[1].alarm[1].attacks != 0) {
      return fail("54f6: clamp must rewrite both bytes of the attitude word");
    }

    /* Positive alarm delta never triggers the clamp (DOS gates on delta<0). */
    col1_tribe_attitude_set(&tribes[1], 1, 0x70);
    gt.indian[1].alarm_by_player[1] = 10;
    gt.indian[1].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    ai_diplo_indian_alarm_delta(&gt, 5, 1, 40); /* 10 -> 50, crosses tier */
    if (col1_tribe_attitude(&tribes[1], 1) != 0x70) {
      return fail("54f6: positive delta must not touch the attitude word");
    }
  return 0;
}

static int case_153e_phase1_border_probe(void) {
    ColonizeWorldMap wmap;
    memset(&wmap, 0, sizeof(wmap));
    if (!fx_map_alloc(&wmap, 16, 16, 0, false)) {
      return fail("153e alloc map");
    }
    for (int i = 0; i < 256; ++i) {
      wmap.terrain[i] = 1;
      wmap.layer3[i] = 1; /* continent 1 */
    }
    static ColonizeUnitPool wunits;
    units_reset(&wunits);
    units_set_occupancy_map(NULL);
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
    colonies_set_occupancy_map(NULL);
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
    wctx.messages = test_game_txt();
    wctx.col1 = &w;
    wctx.col1_ok = true;
    wctx.map = &wmap;
    wctx.units = &wunits;
    wctx.colonies = &wcol;
    wctx.human_nation = 0;
    /* 153e reads the FUN_4962_0018 census rows (raw 97437-97492): census
     * the scene, then restore the hand-set nation totals. */
    {
      const ColonizeWorld cw = world_from_turn_ctx(&wctx);
      col1_stuff_census_4962_w(&cw, &w.stuff, 0);
      col1_stuff_census_4962_w(&cw, &w.stuff, 1);
      w.stuff.field_combat_totals[1] = 10;
    }
    const Ai153eWorthinessScore ws = ai_diplo_153e_worthiness_score(&wctx, 0, 1, -1, 1);
    fx_map_free(&wmap);
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
  return 0;
}

/* Raw 97445-97452: the worthy test reads the census rows -0x6a4e (exposed
 * combat) and -0x6ada (skilled units). The census says the target has
 * exposed strength and self has 5 skilled units on continent 1, while the
 * live pools are empty: 153e must go by the census and come out worthy. */
static int case_153e_reads_census_rows(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (!fx_map_alloc(&map, 16, 16, 0, false)) {
    return fail("153e census: alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
    map.layer3[i] = 1;
  }
  static ColonizeUnitPool units;
  units_reset(&units);
  units_set_occupancy_map(NULL);
  static ColonizeColonyPool cols;
  colonies_init(&cols);
  colonies_set_occupancy_map(NULL);
  static ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  col1.head.turn = 100;
  col1.head.difficulty = 1;
  col1.head.human_player = 0;
  col1.stuff.field_combat_strength_by_continent[1 * 16 + 1] = 200; /* target exposed */
  col1.stuff.skilled_unit_counts_by_continent[0 * 16 + 1] = 5;   /* self skilled > 4 */
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.units = &units;
  ctx.colonies = &cols;
  ctx.human_nation = 0;
  const Ai153eWorthinessScore ws = ai_diplo_153e_worthiness_score(&ctx, 0, 1, -1, 1);
  fx_map_free(&map);
  if (!ws.handled || !ws.worthy) {
    fprintf(stderr, "unit_ai_diplo: census handled=%d worthy=%d\n", ws.handled, ws.worthy);
    return fail("153e: the worthy test must read the census rows, not a live recount");
  }
  return 0;
}

static int case_153e_phases_2_4_encounter(void) {
  ai_diplo_talk_reset(); /* a talk left open by an earlier case refuses this one */
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    if (!fx_map_alloc(&emap, 16, 16, 0, false)) {
      return fail("153e talk alloc map");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 1;
      emap.layer3[i] = 1;
    }
    static ColonizeUnitPool eunits;
    units_reset(&eunits);
    units_set_occupancy_map(NULL);
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
    colonies_set_occupancy_map(NULL);
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
    snprintf(e.player[0].name, sizeof(e.player[0].name), "Tsterson");
    AiPopupState epop;
    ai_popup_init(&epop);
    ColonizeDosRng erng;
    dos_rng_seed(&erng, 5);
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.col1 = &e;
    ectx.col1_ok = true;
    ectx.map = &emap;
    ectx.units = &eunits;
    ectx.colonies = &ecol;
    ectx.human_nation = 0;
    ectx.ai_popups = &epop;
    ectx.rng = &erng;
    const int started = ai_diplo_153e_encounter(&ectx, 0, 1, pa->id, -1);
    if (!started || epop.queue_count < 2) {
      fprintf(stderr, "unit_ai_diplo: 153e talk started=%d queued=%d\n", started, epop.queue_count);
      fx_map_free(&emap);
      return fail("153e talk: unmet adjacent AI Euro must open the greeting + partition CHOICE");
    }
    if (epop.queue[0].tag != AI_POPUP_TAG_DIPLO_TALK || epop.queue[0].kind != AI_POPUP_KIND_OK ||
        epop.queue[1].kind != AI_POPUP_KIND_CHOICE) {
      fx_map_free(&emap);
      return fail("153e talk: expected greeting OK then a CHOICE");
    }
    /* port_bugs.md #545: @HELLOFIRST tokens are DOS raw :97696-97704 — %STRING0
     * = difficulty title + human leader name, %STRING1 = TARGET country,
     * %STRING2/3 = @GREATKINGS/@GREATDEEDS line[target]. Compare against
     * the catalog lines themselves (wrap-normalised). */
    {
      char flat[AI_POPUP_BODY_LEN];
      snprintf(flat, sizeof(flat), "%s", epop.queue[0].body);
      for (char* c = flat; *c; ++c) {
        if (*c == '\n') {
          *c = ' ';
        }
      }
      const ColonizeMsgSection* gk = assets_msg_find(test_game_txt(), "GREATKINGS");
      const char* kings = NULL;
      int idx = 0;
      for (int i = 0; gk && i < gk->line_count; ++i) {
        const char* ln = gk->lines[i];
        if (!ln || !ln[0] || ln[0] == ';' || ln[0] == '@') {
          continue;
        }
        if (idx++ == 1) {
          kings = ln;
          break;
        }
      }
      if (!strstr(flat, "Tsterson") || !strstr(flat, "France") || !kings || !strstr(flat, kings)) {
        fprintf(stderr, "unit_ai_diplo: greeting body='%s'\n", flat);
        fx_map_free(&emap);
        return fail("153e greeting: DOS HELLOFIRST tokens (leader, target country, GREATKINGS)");
      }
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
    const bool peace_ok = (e.nation[0].euro_relation[1] & AI_DIPLO_PEACE) &&
                          (e.nation[1].euro_relation[0] & AI_DIPLO_PEACE);
    const bool stamp_ok = e.head.nation_relation[1] == 30 + 0x10;
    const int second = ai_diplo_153e_encounter(&ectx, 0, 1, pa->id, -1);
    /* port_bugs.md #453: FUN_5f7a_000e Meet With Mayor pushes param_6 = 1, so the
     * scout's audience opens even inside the cooldown and the same turn. */
    ai_popup_init(&epop);
    const int forced = second ? 0 : ai_diplo_153e_encounter_forced(&ectx, 0, 1, pa->id, -1);
    const int forced_queued = epop.queue_count;
    /* Drain it so the talk machine is idle for the next block. */
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
    fx_map_free(&emap);
    if (!peace_ok) {
      return fail("153e talk: accepting the partition treaty must sign PEACE both ways");
    }
    if (!stamp_ok) {
      return fail("153e talk: partition must stamp DS:0x53c8[target] = turn + 16");
    }
    if (second) {
      return fail("153e talk: a second encounter the same turn must not reopen the talk");
    }
    if (!forced || forced_queued < 1) {
      return fail("153e talk: scout Meet With Mayor (forced gate) must open inside the cooldown");
    }
  return 0;
}

/* Human England (0) beside computer France (1) on continent 1, turn 30,
 * difficulty 2, both 1000 gold, no colonies. */
typedef struct {
  ColonizeWorldMap map;
  ColonizeUnitPool units;
  ColonizeColonyPool cols;
  ColonizeCol1Save col1;
  AiPopupState pop;
  ColonizeDosRng rng;
  ColonizeTurnContext ctx;
  int unit_id;
} TreatyFx;

static int treaty_fx_init(TreatyFx* f) {
  memset(&f->map, 0, sizeof(f->map));
  if (!fx_map_alloc(&f->map, 16, 16, 0, false)) {
    return fail("treaty fixture: alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    f->map.terrain[i] = 1;
    f->map.layer3[i] = 1;
  }
  units_reset(&f->units);
  units_set_occupancy_map(NULL);
  f->units.type_count = 1;
  snprintf(f->units.types[0].name, sizeof(f->units.types[0].name), "Scouts");
  f->units.types[0].movement = 4;
  f->units.types[0].attack = 1;
  f->units.types[0].defense = 1;
  f->units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  const int ha = units_spawn(&f->units, 0, 5, 5);
  const int hb = units_spawn(&f->units, 0, 6, 5);
  ColonizeUnit* pa = units_get(&f->units, ha);
  ColonizeUnit* pb = units_get(&f->units, hb);
  if (!pa || !pb) {
    fx_map_free(&f->map);
    return fail("treaty fixture: spawn");
  }
  pa->nation_id = 0;
  pb->nation_id = 1;
  f->unit_id = pa->id;
  colonies_init(&f->cols);
  colonies_set_occupancy_map(NULL);
  col1_save_init(&f->col1);
  memset(f->col1.nation, 0, sizeof(f->col1.nation));
  memset(f->col1.head.nation_relation, 0, sizeof(f->col1.head.nation_relation));
  f->col1.head.turn = 30;
  f->col1.head.difficulty = 2;
  f->col1.head.human_player = 0;
  f->col1.player[0].control = 0;
  f->col1.player[1].control = 1;
  f->col1.nation[0].gold = 1000;
  f->col1.nation[1].gold = 1000;
  snprintf(f->col1.player[0].country_name, sizeof(f->col1.player[0].country_name), "England");
  snprintf(f->col1.player[1].country_name, sizeof(f->col1.player[1].country_name), "France");
  snprintf(f->col1.player[0].name, sizeof(f->col1.player[0].name), "Tsterson");
  ai_popup_init(&f->pop);
  dos_rng_seed(&f->rng, 5);
  memset(&f->ctx, 0, sizeof(f->ctx));
  f->ctx.messages = test_game_txt();
  f->ctx.col1 = &f->col1;
  f->ctx.col1_ok = true;
  f->ctx.map = &f->map;
  f->ctx.units = &f->units;
  f->ctx.colonies = &f->cols;
  f->ctx.human_nation = 0;
  f->ctx.ai_popups = &f->pop;
  f->ctx.rng = &f->rng;
  return 0;
}

/* Drain the talk queue; `pick(body)` returns the choice id for each CHOICE. */
static void treaty_fx_drain(TreatyFx* f, int (*pick)(const char* body)) {
  for (int guard = 0; guard < 12 && f->pop.queue_count > 0; ++guard) {
    AiPopupRequest front = f->pop.queue[0];
    memmove(&f->pop.queue[0], &f->pop.queue[1],
            sizeof(f->pop.queue[0]) * (size_t)(f->pop.queue_count - 1));
    f->pop.queue_count--;
    if (front.kind != AI_POPUP_KIND_CHOICE) {
      continue;
    }
    f->pop.has_result = true;
    f->pop.result_cancelled = false;
    f->pop.result_tag = AI_POPUP_TAG_DIPLO_TALK;
    f->pop.result_choice_id = pick(front.body);
    f->pop.result_nation_a = 0;
    f->pop.result_nation_b = 1;
    f->pop.result_payload = front.payload;
    ai_diplo_apply_popup_result(&f->ctx, &f->pop);
    f->pop.has_result = false;
  }
}

static int treaty_pick_first(const char* body) {
  (void)body;
  return 1;
}

/* The @GIVECASH offer is the only CHOICE carrying the 500$ amount. */
static int treaty_pick_cash_only(const char* body) {
  return strstr(body, "500") ? 1 : 2;
}

/*
 * DOS keeps WAR 0x02 through a @WORTHY peace: the accept only ORs PEACE 0x40
 * (FUN_5bfb_153e raw 98003). The 465b band sets 0x02 on a human attack under
 * PEACE; the next 153e audience reads it (raw 97509): no early-game grace and
 * forced hostility.
 */
static int case_153e_war_bit_survives_worthy_peace(void) {
  ai_diplo_talk_reset(); /* a talk left open by an earlier case refuses this one */
  static TreatyFx f;
  if (treaty_fx_init(&f) != 0) {
    return 1;
  }
  ColonizeCol1Save* e = &f.col1;
  ai_diplo_write(e, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
  ai_diplo_write(e, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
  (void)ai_diplo_465b_band_ctx(&f.ctx, 0, 1);
  ai_popup_init(&f.pop); /* drop the @CANCELPEACE notice */
  if ((e->nation[1].euro_relation[0] & AI_DIPLO_WAR) == 0) {
    fx_map_free(&f.map);
    return fail("war bit: human attack under PEACE must set 0x02 in the AI's byte");
  }
  if (!ai_diplo_153e_encounter(&f.ctx, 0, 1, f.unit_id, -1)) {
    fx_map_free(&f.map);
    return fail("war bit: the audience must open");
  }
  treaty_fx_drain(&f, treaty_pick_first); /* WORTHY: Yes */
  const bool peace = (e->nation[0].euro_relation[1] & AI_DIPLO_PEACE) &&
                     (e->nation[1].euro_relation[0] & AI_DIPLO_PEACE);
  const bool kept = (e->nation[1].euro_relation[0] & AI_DIPLO_WAR) != 0;
  if (!peace || !kept) {
    fprintf(stderr, "war bit: human=0x%02x ai=0x%02x\n", e->nation[0].euro_relation[1],
            e->nation[1].euro_relation[0]);
    fx_map_free(&f.map);
    return fail("war bit: @WORTHY peace must sign PEACE and keep the AI's 0x02");
  }
  /* Next audience scoring. Equal field totals make the AI weaker than 3x the
   * human, so 0x02 forces hostility (raw 97511-97517); difficulty 0 keeps the
   * clamped 100 clear of the 16-bit wrap in the scaling. */
  e->head.difficulty = 0;
  e->stuff.field_combat_totals[0] = 1;
  e->stuff.field_combat_totals[1] = 1;
  const int16_t stamp = e->head.nation_relation[1];
  const Ai153eWorthinessScore hostile = ai_diplo_153e_worthiness_score(&f.ctx, 0, 1, -1, 1);
  e->head.nation_relation[1] = stamp;
  e->nation[1].euro_relation[0] &= (uint8_t)~AI_DIPLO_WAR;
  const Ai153eWorthinessScore control = ai_diplo_153e_worthiness_score(&f.ctx, 0, 1, -1, 1);
  fx_map_free(&f.map);
  if (!hostile.at_war || !hostile.worthy || hostile.score <= 0) {
    fprintf(stderr, "war bit: at_war=%d worthy=%d score=%d\n", hostile.at_war, hostile.worthy,
            hostile.score);
    return fail("war bit: 0x02 under PEACE must make the next 153e scoring hostile");
  }
  if (control.at_war || control.worthy) {
    return fail("war bit control: without 0x02, turn 30 is inside the early-game grace");
  }
  return 0;
}

/*
 * @GIVECASH accept (FUN_5bfb_153e raw 98025) also ORs PEACE 0x40 only. The
 * census gives France a colony and no land units on continent 1 against 10 of
 * English field strength: dominance (10 / 1) << 1 = 20, so "No" to @WORTHY
 * brings the (20 - 2) * 2 offer, capped at France's 500 gold / 100. England's
 * 0x02 toward France survives the accepted cash peace.
 */
static int case_153e_war_bit_survives_givecash_peace(void) {
  ai_diplo_talk_reset(); /* a talk left open by an earlier case refuses this one */
  static TreatyFx f;
  if (treaty_fx_init(&f) != 0) {
    return 1;
  }
  ColonizeCol1Save* e = &f.col1;
  e->nation[1].gold = 500;
  e->stuff.colony_counts[1] = 1;
  e->stuff.colony_counts_by_continent[1 * 16 + 1] = 1;
  e->stuff.field_combat_strength_by_continent[0 * 16 + 1] = 10;
  ai_diplo_write(e, 0, 1, AI_DIPLO_MET | AI_DIPLO_WAR);
  ai_diplo_write(e, 1, 0, AI_DIPLO_MET);
  if (!ai_diplo_153e_encounter(&f.ctx, 0, 1, f.unit_id, -1)) {
    fx_map_free(&f.map);
    return fail("cash peace: the audience must open");
  }
  treaty_fx_drain(&f, treaty_pick_cash_only); /* WORTHY: No, GIVECASH: Yes */
  const bool peace = (e->nation[0].euro_relation[1] & AI_DIPLO_PEACE) &&
                     (e->nation[1].euro_relation[0] & AI_DIPLO_PEACE);
  const bool paid = e->nation[0].gold == 1500;
  const bool kept = (e->nation[0].euro_relation[1] & AI_DIPLO_WAR) != 0;
  fx_map_free(&f.map);
  if (!peace || !paid || !kept) {
    fprintf(stderr, "cash peace: human=0x%02x ai=0x%02x gold=%ld\n", e->nation[0].euro_relation[1],
            e->nation[1].euro_relation[0], (long)e->nation[0].gold);
    return fail("cash peace: @GIVECASH must pay, sign PEACE and keep 0x02");
  }
  return 0;
}

/* `tie`: a second AI colony at (5,10), DOS distance 4 from the unit at (5,6)
 * like the first at (8,8) (Chebyshev 4 vs 3). FUN_15eb_0142 (raw 9380-9421,
 * max + min/2, `<=`) picks the later one; a Chebyshev `<` pick took (8,8).
 * no_demand 1: the 3180 direction (S) points away from the human colony,
 * so raw 97601 FUN_281f_07be finds none there and no demand is made.
 * no_demand 2: euro_price 1 -> DS:0x84bc byte 0 (raw 6316-6320), so every
 * value is 0 and misses the threshold. */
static int wantstuff_run(int tie, int no_demand) {
  ai_diplo_talk_reset(); /* a talk left open by an earlier case refuses this one */
    ColonizeWorldMap qmap;
    memset(&qmap, 0, sizeof(qmap));
    if (!fx_map_alloc(&qmap, 16, 16, 0, false)) {
      return fail("153e wantstuff alloc map");
    }
    for (int i = 0; i < 256; ++i) {
      qmap.terrain[i] = 1;
      qmap.layer3[i] = 1;
    }
    static ColonizeUnitPool qunits;
    units_reset(&qunits);
    units_set_occupancy_map(NULL);
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
    colonies_set_occupancy_map(NULL);
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
    if (tie) {
      ColonizeColony* tc = &qcol.colonies[2];
      *tc = *ac;
      tc->id = 2;
      tc->x = 5;
      tc->y = 10;
      qcol.colony_count = 3;
      qcol.next_id = 3;
    }
    const int rcv = tie ? 2 : 1;
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
    q.nation[1].trade.euro_price[COLONIZE_CARGO_MUSKETS] =
      no_demand == 2 ? 1 : 200; /* -0x7b44 weight */
    snprintf(q.player[0].country_name, sizeof(q.player[0].country_name), "England");
    snprintf(q.player[1].country_name, sizeof(q.player[1].country_name), "France");
    AiPopupState qpop;
    ai_popup_init(&qpop);
    ColonizeDosRng qrng;
    dos_rng_seed(&qrng, 7);
    ColonizeTurnContext qctx;
    memset(&qctx, 0, sizeof(qctx));
    qctx.messages = test_game_txt();
    qctx.col1 = &q;
    qctx.col1_ok = true;
    qctx.map = &qmap;
    qctx.units = &qunits;
    qctx.colonies = &qcol;
    qctx.human_nation = 0;
    qctx.ai_popups = &qpop;
    qctx.rng = &qrng;
    const int started =
      ai_diplo_153e_encounter(&qctx, 0, 1, pai->id, no_demand == 1 ? 4 : 0); /* 0 = N */
    fx_map_free(&qmap);
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
      const int is_want = test_body_is_section(front.body, "WANTSTUFF");
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
    if (no_demand) {
      if (saw_wantstuff || qcol.colonies[0].stock[COLONIZE_CARGO_MUSKETS] != 60) {
        return fail(no_demand == 1 ? "153e wantstuff: only the 3180-direction tile is a source"
                                   : "153e wantstuff: value weight is the 0x84bc byte (price-1)");
      }
      return 0;
    }
    if (!saw_wantstuff) {
      return fail("153e wantstuff: the goods demand CHOICE must be queued");
    }
    /* docs/dos_bugs_ai.md DB-8: the NAMED cargo moves (DOS moves Furs). */
    if (qcol.colonies[rcv].stock[COLONIZE_CARGO_MUSKETS] != 60 ||
        qcol.colonies[0].stock[COLONIZE_CARGO_MUSKETS] != 0) {
      fprintf(stderr, "unit_ai_diplo: muskets src=%d dst=%d (colony %d) other=%d\n",
              qcol.colonies[0].stock[COLONIZE_CARGO_MUSKETS],
              qcol.colonies[rcv].stock[COLONIZE_CARGO_MUSKETS], rcv,
              qcol.colonies[3 - rcv].stock[COLONIZE_CARGO_MUSKETS]);
      return fail(tie ? "153e wantstuff: the receiver is the last colony at the lowest DOS distance"
                      : "153e wantstuff: accepting must move the demanded cargo by the demanded amount");
    }
    if (qcol.colonies[0].stock[COLONIZE_CARGO_FURS] != 10 ||
        qcol.colonies[rcv].stock[COLONIZE_CARGO_FURS] != 0) {
      return fail("153e wantstuff: Furs must not move (DB-8)");
    }
  return 0;
}

static int case_153e_wantstuff_demand(void) {
  return wantstuff_run(0, 0);
}

static int case_153e_wantstuff_receiver_dos_distance(void) {
  if (!test_game_txt()) {
    return TR_SKIP;
  }
  return wantstuff_run(1, 0);
}

static int case_153e_wantstuff_source_is_encounter_dir(void) {
  return wantstuff_run(0, 1);
}

static int case_153e_wantstuff_weight_is_84bc_byte(void) {
  return wantstuff_run(0, 2);
}

static int case_153e_worthy_cascade(void) {
  ai_diplo_talk_reset(); /* a talk left open by an earlier case refuses this one */
    ColonizeWorldMap rmap;
    memset(&rmap, 0, sizeof(rmap));
    if (!fx_map_alloc(&rmap, 16, 16, 0, false)) {
      return fail("153e rid alloc map");
    }
    for (int i = 0; i < 256; ++i) {
      rmap.terrain[i] = 1;
      rmap.layer3[i] = 1;
    }
    static ColonizeUnitPool runits;
    units_reset(&runits);
    units_set_occupancy_map(NULL);
    runits.type_count = 1;
    snprintf(runits.types[0].name, sizeof(runits.types[0].name), "Dragoons");
    runits.types[0].movement = 4;
    runits.types[0].attack = 3;
    runits.types[0].defense = 3;
    runits.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    const int raiu = units_spawn(&runits, 0, 5, 6); /* AI unit beside the human colony */
    ColonizeUnit* rai = units_get(&runits, raiu);
    if (!rai) {
      return fail("153e rid spawn");
    }
    rai->nation_id = 1;
    static ColonizeColonyPool rcol;
    colonies_init(&rcol);
    colonies_set_occupancy_map(NULL);
    ColonizeColony* rhc = &rcol.colonies[0];
    rhc->id = 0;
    rhc->active = true;
    rhc->nation_id = 0;
    rhc->x = 5;
    rhc->y = 5;
    rhc->population = 2;
    rhc->colonist_count = 2;
    rhc->building_in_production = -1;
    rhc->stock[COLONIZE_CARGO_MUSKETS] = 60;
    ColonizeColony* rac = &rcol.colonies[1];
    rac->id = 1;
    rac->active = true;
    rac->nation_id = 1;
    rac->x = 8;
    rac->y = 8;
    rac->population = 2;
    rac->colonist_count = 2;
    rac->building_in_production = -1;
    rcol.colony_count = 2;
    rcol.next_id = 2;
    ColonizeCol1Save r;
    col1_save_init(&r);
    memset(r.nation, 0, sizeof(r.nation));
    memset(r.head.nation_relation, 0, sizeof(r.head.nation_relation));
    r.head.turn = 100;
    r.head.difficulty = 1;
    r.head.human_player = 0;
    r.nation[0].gold = 0; /* run 1: cannot afford tribute -> score stays != 999 */
    r.stuff.colony_counts[0] = 1;
    r.stuff.colony_counts[1] = 1;
    r.stuff.field_combat_totals[1] = 10;
    r.stuff.colony_pop_totals[1] = 8;
    r.stuff.census_pop_proxy[0] = 8;
    r.nation[1].trade.euro_price[COLONIZE_CARGO_MUSKETS] = 200;
    snprintf(r.player[0].country_name, sizeof(r.player[0].country_name), "England");
    snprintf(r.player[1].country_name, sizeof(r.player[1].country_name), "France");
    AiPopupState rpop;
    ai_popup_init(&rpop);
    ColonizeDosRng rrng;
    dos_rng_seed(&rrng, 7);
    ColonizeTurnContext rctx;
    memset(&rctx, 0, sizeof(rctx));
    rctx.messages = test_game_txt();
    rctx.col1 = &r;
    rctx.col1_ok = true;
    rctx.map = &rmap;
    rctx.units = &runits;
    rctx.colonies = &rcol;
    rctx.human_nation = 0;
    rctx.ai_popups = &rpop;
    rctx.rng = &rrng;

    int saw_rid = 0;
    int saw_warmanly_run1 = 0;
    if (!ai_diplo_153e_encounter(&rctx, 0, 1, rai->id, 0)) {
      fx_map_free(&rmap);
      return fail("153e rid: the encounter must open");
    }
    for (int guard = 0; guard < 16 && rpop.queue_count > 0; ++guard) {
      AiPopupRequest front = rpop.queue[0];
      memmove(&rpop.queue[0], &rpop.queue[1], sizeof(rpop.queue[0]) * (size_t)(rpop.queue_count - 1));
      rpop.queue_count--;
      if (test_body_is_section(front.body, "RID")) {
        saw_rid = 1;
      }
      if (test_body_is_section(front.body, "WARMANLY")) {
        saw_warmanly_run1 = 1;
      }
      if (front.kind != AI_POPUP_KIND_CHOICE) {
        continue;
      }
      rpop.has_result = true;
      rpop.result_cancelled = false;
      rpop.result_tag = AI_POPUP_TAG_DIPLO_TALK;
      rpop.result_choice_id = 1; /* refuse every demand: `worthy` stays armed */
      rpop.result_nation_a = 0;
      rpop.result_nation_b = 1;
      rpop.result_payload = front.payload;
      ai_diplo_apply_popup_result(&rctx, &rpop);
      rpop.has_result = false;
    }
    if (!saw_rid) {
      fx_map_free(&rmap);
      return fail("153e worthy cascade: a worthy AI with no tribute dialog must issue @RID");
    }
    if (saw_warmanly_run1) {
      fx_map_free(&rmap);
      return fail("153e worthy cascade: @WARMANLY must not fire without the 999 tribute latch");
    }
    if ((r.nation[0].euro_relation[1] & AI_DIPLO_PEACE) != 0) {
      fx_map_free(&rmap);
      return fail("153e worthy cascade: the @RID ultimatum leg must not sign peace");
    }

    /* Run 2: rich human -> @TRIBUTE runs, refusal latches score = 999. */
    ai_popup_init(&rpop);
    r.head.turn = 140; /* past the DS:0x53c8 + 16 re-talk cooldown */
    r.nation[0].gold = 100000;
    int saw_tribute = 0;
    int saw_warmanly = 0;
    int saw_rid_run2 = 0;
    if (!ai_diplo_153e_encounter(&rctx, 0, 1, rai->id, 0)) {
      fx_map_free(&rmap);
      return fail("153e warmanly: the second encounter must open on a later turn");
    }
    for (int guard = 0; guard < 16 && rpop.queue_count > 0; ++guard) {
      AiPopupRequest front = rpop.queue[0];
      memmove(&rpop.queue[0], &rpop.queue[1], sizeof(rpop.queue[0]) * (size_t)(rpop.queue_count - 1));
      rpop.queue_count--;
      if (test_body_is_section(front.body, "TRIBUTE")) {
        saw_tribute = 1;
      }
      if (test_body_is_section(front.body, "WARMANLY")) {
        saw_warmanly = 1;
      }
      if (test_body_is_section(front.body, "RID")) {
        saw_rid_run2 = 1;
      }
      if (front.kind != AI_POPUP_KIND_CHOICE) {
        continue;
      }
      rpop.has_result = true;
      rpop.result_cancelled = false;
      rpop.result_tag = AI_POPUP_TAG_DIPLO_TALK;
      rpop.result_choice_id = 1; /* refuse the tribute: score latches 999 anyway */
      rpop.result_nation_a = 0;
      rpop.result_nation_b = 1;
      rpop.result_payload = front.payload;
      ai_diplo_apply_popup_result(&rctx, &rpop);
      rpop.has_result = false;
    }
    fx_map_free(&rmap);
    if (!saw_tribute) {
      return fail("153e warmanly: an affordable tribute demand must be shown first");
    }
    if (!saw_warmanly) {
      return fail("153e worthy cascade: score==999 after @TRIBUTE must declare war (@WARMANLY)");
    }
    if (saw_rid_run2) {
      return fail("153e worthy cascade: the 999 leg must win over @RID");
    }
    if ((r.nation[0].euro_relation[1] & AI_DIPLO_PEACE) != 0 ||
        (r.nation[1].euro_relation[0] & AI_DIPLO_PEACE) != 0) {
      return fail("153e worthy cascade: @WARMANLY clears PEACE (0a10 mask 0x40), never signs it");
    }
  return 0;
}

/*
 * port_bugs.md #474: 153e talk tokens that used to carry the nation adjective /
 * the @MEEKNESS word. DOS operands:
 *   @WORTHY %STRING0      = @GREATLEADER2[target]   (raw :97987)
 *   @OLDPEACE* %STRING2/3 = difficulty title / human player.name (raw :98076-98079)
 * Drives a first audience (partition accepted) and then a forced one at
 * peace, collecting every body, and checks each shape that shows up.
 */
static void talk474_flat(char* out, size_t cap, const char* body) {
  snprintf(out, cap, "%s", body ? body : "");
  for (char* c = out; *c; ++c) {
    if (*c == '\n') {
      *c = ' ';
    }
  }
}

static int s_talk474_saw_piracy;
static int s_talk474_saw_sphere;

static int talk474_drain(ColonizeTurnContext* ctx, AiPopupState* pop, int* saw_worthy,
                         int* saw_wise) {
  for (int guard = 0; guard < 16 && pop->queue_count > 0; ++guard) {
    AiPopupRequest front = pop->queue[0];
    memmove(&pop->queue[0], &pop->queue[1], sizeof(pop->queue[0]) * (size_t)(pop->queue_count - 1));
    pop->queue_count--;
    char flat[AI_POPUP_BODY_LEN];
    talk474_flat(flat, sizeof(flat), front.body);
    if (strstr(flat, "rightfully ours by order of")) {
      *saw_worthy = 1;
      if (!strstr(flat, "by order of the King") || strstr(flat, "by order of France")) {
        fprintf(stderr, "unit_ai_diplo: #474 WORTHY body='%s'\n", flat);
        return fail("#474: @WORTHY %STRING0 must be @GREATLEADER2[target]");
      }
    }
    if (strstr(flat, "most displeased")) {
      /* raw 97778-97781: %STRING0 = @GREATLEADER[target], %STRING2 = target country. */
      s_talk474_saw_piracy = 1;
      if (!strstr(flat, "Our King is most displeased") || !strstr(flat, "off the coast of France")) {
        fprintf(stderr, "unit_ai_diplo: PIRACY body='%s'\n", flat);
        return fail("@PIRACY %STRING0/2 must be @GREATLEADER[target] + target country");
      }
    }
    if (strstr(flat, "sphere")) {
      s_talk474_saw_sphere = 1;
    }
    if (strstr(flat, "wise leader")) {
      *saw_wise = 1;
      if (!strstr(flat, "Tsterson") || strstr(flat, "request") || strstr(flat, "leader France")) {
        fprintf(stderr, "unit_ai_diplo: #474 OLDPEACE body='%s'\n", flat);
        return fail("#474: @OLDPEACEMEEK %STRING2/3 must be title + human leader name");
      }
    }
    if (front.kind != AI_POPUP_KIND_CHOICE) {
      continue;
    }
    pop->has_result = true;
    pop->result_cancelled = false;
    pop->result_tag = AI_POPUP_TAG_DIPLO_TALK;
    pop->result_choice_id = 1;
    pop->result_nation_a = 0;
    pop->result_nation_b = 1;
    pop->result_payload = front.payload;
    ai_diplo_apply_popup_result(ctx, pop);
    pop->has_result = false;
  }
  return 0;
}

static int case_153e_talk_tokens_474(void) {
  ai_diplo_talk_reset(); /* a talk left open by an earlier case refuses this one */
  ColonizeWorldMap emap;
  memset(&emap, 0, sizeof(emap));
  emap.width = 16;
  emap.height = 16;
  emap.tile_count = 256;
  static uint8_t terr[256], l2[256], l3[256];
  memset(terr, 1, sizeof(terr));
  memset(l2, 0, sizeof(l2));
  memset(l3, 1, sizeof(l3));
  emap.terrain = terr;
  emap.layer2 = l2;
  emap.layer3 = l3;
  static ColonizeUnitPool eunits;
  units_reset(&eunits);
  units_set_occupancy_map(NULL);
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
    return fail("#474 talk spawn");
  }
  pa->nation_id = 0;
  pb->nation_id = 1;
  static ColonizeColonyPool ecol;
  colonies_init(&ecol);
  colonies_set_occupancy_map(NULL);
  static ColonizeCol1Save e;
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
  snprintf(e.player[0].name, sizeof(e.player[0].name), "Tsterson");
  static AiPopupState epop;
  ai_popup_init(&epop);
  ColonizeDosRng erng;
  dos_rng_seed(&erng, 5);
  ColonizeTurnContext ectx;
  memset(&ectx, 0, sizeof(ectx));
  ectx.messages = test_game_txt();
  ectx.col1 = &e;
  ectx.col1_ok = true;
  ectx.map = &emap;
  ectx.units = &eunits;
  ectx.colonies = &ecol;
  ectx.human_nation = 0;
  ectx.ai_popups = &epop;
  ectx.rng = &erng;
  if (!ectx.messages) {
    return 0; /* no shipped GAME.TXT: nothing to compare against */
  }
  int saw_worthy = 0;
  int saw_wise = 0;
  s_talk474_saw_sphere = 0;
  (void)ai_diplo_153e_encounter(&ectx, 0, 1, pa->id, -1);
  if (talk474_drain(&ectx, &epop, &saw_worthy, &saw_wise) != 0) {
    return 1;
  }
  /* port_bugs.md #1473 — raw :97985-97988/:98053-98057: a treaty signed in
   * this audience answers with @PEACEMEEK/@PEACEMANLY (the "sphere" text);
   * the DS:0x1984 "OLDPEACE" prefix needs the 0x40 bit set on entry, so
   * @OLDPEACE* ("wise leader") belongs to the second, at-peace audience. */
  if (saw_wise || (saw_worthy && !s_talk474_saw_sphere)) {
    return fail("#1473: a first treaty must show @PEACE*, not @OLDPEACE*");
  }
  ai_popup_init(&epop);
  /* The PIRACY gate (raw 97776-97777): grievance bit, a privateer of the human's. */
  s_talk474_saw_piracy = 0;
  ai_diplo_write(&e, 1, 0, (uint8_t)(ai_diplo_read(&e, 1, 0) | AI_DIPLO_PRIVATEER_SIGHTED));
  e.stuff.unit_type_counts[0][16] = 1;
  (void)ai_diplo_153e_encounter_forced(&ectx, 0, 1, pa->id, -1);
  if (talk474_drain(&ectx, &epop, &saw_worthy, &saw_wise) != 0) {
    return 1;
  }
  if (!saw_worthy || !saw_wise || !s_talk474_saw_piracy) {
    fprintf(stderr, "unit_ai_diplo: #474 saw_worthy=%d saw_wise=%d saw_piracy=%d\n", saw_worthy, saw_wise,
            s_talk474_saw_piracy);
    return fail("#474: fixture must reach both @WORTHY and @OLDPEACEMEEK");
  }
  return 0;
}

/* raw :97431/:97720: a target that won its own independence (nation_flags 0x04)
 * greets with @HELLOUSA, not @HELLOFIRST's "in the name of our Queen" (playtest C10). */
static int case_153e_usa_greeting(void) {
  ai_diplo_talk_reset(); /* a talk left open by an earlier case refuses this one */
  ColonizeWorldMap emap;
  memset(&emap, 0, sizeof(emap));
  emap.width = 16;
  emap.height = 16;
  emap.tile_count = 256;
  static uint8_t terr[256], l2[256], l3[256];
  memset(terr, 1, sizeof(terr));
  memset(l2, 0, sizeof(l2));
  memset(l3, 1, sizeof(l3));
  emap.terrain = terr;
  emap.layer2 = l2;
  emap.layer3 = l3;
  static ColonizeUnitPool eunits;
  units_reset(&eunits);
  units_set_occupancy_map(NULL);
  eunits.type_count = 1;
  snprintf(eunits.types[0].name, sizeof(eunits.types[0].name), "Scouts");
  eunits.types[0].movement = 4;
  eunits.types[0].attack = 1;
  eunits.types[0].defense = 1;
  eunits.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  ColonizeUnit* pa = units_get(&eunits, units_spawn(&eunits, 0, 5, 5));
  ColonizeUnit* pb = units_get(&eunits, units_spawn(&eunits, 0, 6, 5));
  if (!pa || !pb) {
    return fail("usa talk spawn");
  }
  pa->nation_id = 0;
  pb->nation_id = 2; /* pair (0,2): the talk cooldown table is static */
  static ColonizeColonyPool ecol;
  colonies_init(&ecol);
  colonies_set_occupancy_map(NULL);
  static ColonizeCol1Save e;
  col1_save_init(&e);
  memset(e.nation, 0, sizeof(e.nation));
  memset(e.head.nation_relation, 0, sizeof(e.head.nation_relation));
  e.head.turn = 30;
  e.head.difficulty = 2;
  e.head.human_player = 0;
  e.nation[0].gold = 1000;
  e.nation[2].gold = 1000;
  e.nation[2].nation_flags = 0x04;
  snprintf(e.player[0].country_name, sizeof(e.player[0].country_name), "England");
  snprintf(e.player[2].country_name, sizeof(e.player[2].country_name), "Republic");
  snprintf(e.player[0].name, sizeof(e.player[0].name), "Tsterson");
  static AiPopupState epop;
  ai_popup_init(&epop);
  ColonizeDosRng erng;
  dos_rng_seed(&erng, 5);
  ColonizeTurnContext ectx;
  memset(&ectx, 0, sizeof(ectx));
  ectx.messages = test_game_txt();
  ectx.col1 = &e;
  ectx.col1_ok = true;
  ectx.map = &emap;
  ectx.units = &eunits;
  ectx.colonies = &ecol;
  ectx.human_nation = 0;
  ectx.ai_popups = &epop;
  ectx.rng = &erng;
  if (!ectx.messages) {
    return 0; /* no shipped GAME.TXT */
  }
  (void)ai_diplo_153e_encounter(&ectx, 0, 2, pa->id, -1);
  int saw = 0;
  for (int i = 0; i < epop.queue_count; ++i) {
    char flat[AI_POPUP_BODY_LEN];
    talk474_flat(flat, sizeof(flat), epop.queue[i].body);
    if (strstr(flat, "justly claimed")) {
      fprintf(stderr, "unit_ai_diplo: usa body='%s'\n", flat);
      return fail("USA target greeted with @HELLOFIRST");
    }
    saw |= strstr(flat, "newly elected government") && strstr(flat, "Republic");
  }
  return saw ? 0 : fail("USA target: no @HELLOUSA greeting queued");
}

/* DOS 15b3_0032 only stores the selected relation byte (raw 9069-9080). */
static int case_relation_writes_preserve_player_bytes(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  for (int i = 0; i < 4; ++i) col1.player[i].diplomacy = 0xa0 + i;
  ai_diplo_write(&col1, 1, 2, AI_DIPLO_MET);
  ai_diplo_or_both(&col1, 1, 2, AI_DIPLO_PEACE);
  if (ai_diplo_read(&col1, 1, 2) != (AI_DIPLO_MET | AI_DIPLO_PEACE) ||
      ai_diplo_read(&col1, 2, 1) != AI_DIPLO_PEACE) {
    return fail("relation setters must retain their directional and bilateral stores");
  }
  ai_diplo_clear_both(&col1, 1, 2, AI_DIPLO_PEACE);
  for (int i = 0; i < 4; ++i) {
    if (col1.player[i].diplomacy != 0xa0 + i) {
      return fail("relation setters must preserve the unrelated player byte");
    }
  }
  return 0;
}

/*
 * FUN_5bfb_153e raw 98098 (asm 5bfb:29b6-29c2), peace menu "withdraw your
 * troops": the free @WITHDRAW is chosen by the dominance accumulator (!= 0,
 * no latch), not by the AI's tone. The human has no gold, so the cost path
 * ends in @NOTWITHDRAW and the AI Soldier next to the colony stays.
 */
static int case_153e_peacemenu_withdraw_dominance(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (!fx_map_alloc(&map, 16, 16, 0, false)) {
    return fail("withdraw: alloc map");
  }
  static ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].attack = 2;
  units.types[0].defense = 2;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  static ColonizeColonyPool cols;
  colonies_init(&cols);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* hc = &cols.colonies[0];
  hc->id = 0;
  hc->active = true;
  hc->nation_id = 0;
  hc->x = 5;
  hc->y = 5;
  hc->population = 1;
  hc->colonist_count = 1;
  hc->building_in_production = -1;
  cols.colony_count = 1;
  cols.next_id = 1;
  static ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  col1.head.turn = 100;
  col1.head.difficulty = 1;
  col1.head.human_player = 0;
  AiPopupState pop;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.units = &units;
  ctx.colonies = &cols;
  ctx.human_nation = 0;
  ctx.ai_popups = &pop;
  ctx.rng = &rng;
  int rc = 0;
  /* {dominance, manly, want_free} */
  static const int k_rows[2][3] = {{0, 1, 0}, {2, 0, 1}};
  for (int r = 0; r < 2 && rc == 0; ++r) {
    const int sid = units_spawn(&units, 0, 4, 5);
    ColonizeUnit* s = units_get(&units, sid);
    if (!s) {
      rc = fail("withdraw: spawn");
      break;
    }
    s->nation_id = 1;
    ai_popup_init(&pop);
    ai_diplo_153e_test_peacemenu(&ctx, 0, 1, k_rows[r][0], k_rows[r][1], 0, 2);
    const int freed = s->x != 4;
    if (freed != k_rows[r][2]) {
      fprintf(stderr, "  dominance %d manly %d: withdrawn %d want %d\n", k_rows[r][0],
              k_rows[r][1], freed, k_rows[r][2]);
      rc = fail("peace-menu withdraw: free iff dominance != 0 and no latch");
    }
    units_despawn(&units, sid);
  }
  ai_diplo_talk_reset();
  fx_map_free(&map);
  return rc;
}

/* Shared setup for the 153e withdraw loops: a 16x16 map, Colonist (attack
 * 0) / Soldier (attack 2) / Caravel types, and one colony of
 * `colony_nation` at 5,5. */
typedef struct {
  ColonizeWorldMap map;
  ColonizeUnitPool units;
  ColonizeColonyPool cols;
  ColonizeCol1Save col1;
  AiPopupState pop;
  ColonizeDosRng rng;
  ColonizeTurnContext ctx;
} WithdrawFx;

static int withdraw_fx_init(WithdrawFx* fx, int colony_nation) {
  memset(fx, 0, sizeof(*fx));
  if (!fx_map_alloc(&fx->map, 16, 16, 0, false)) {
    return 0;
  }
  units_reset(&fx->units);
  units_set_occupancy_map(NULL);
  fx->units.type_count = 3;
  static const struct {
    const char* name;
    int movement, attack, space, cargo, domain;
  } k_types[3] = {
    {"Colonist", 1, 0, 1, 0, COLONIZE_UNIT_DOMAIN_LAND},
    {"Soldier", 1, 2, 1, 0, COLONIZE_UNIT_DOMAIN_LAND},
    {"Caravel", 4, 0, 99, 2, COLONIZE_UNIT_DOMAIN_SEA},
  };
  for (int i = 0; i < 3; ++i) {
    ColonizeUnitType* t = &fx->units.types[i];
    snprintf(t->name, sizeof(t->name), "%s", k_types[i].name);
    t->movement = k_types[i].movement;
    t->attack = k_types[i].attack;
    t->defense = 2;
    t->space = k_types[i].space;
    t->cargo = k_types[i].cargo;
    t->domain = k_types[i].domain;
  }
  colonies_init(&fx->cols);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* c = &fx->cols.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = colony_nation;
  c->x = 5;
  c->y = 5;
  c->population = 1;
  c->colonist_count = 1;
  c->building_in_production = -1;
  fx->cols.colony_count = 1;
  fx->cols.next_id = 1;
  col1_save_init(&fx->col1);
  memset(fx->col1.nation, 0, sizeof(fx->col1.nation));
  fx->col1.head.turn = 100;
  fx->col1.head.difficulty = 1;
  fx->col1.head.human_player = 0;
  ai_popup_init(&fx->pop);
  dos_rng_seed(&fx->rng, 7);
  fx->ctx.messages = test_game_txt();
  fx->ctx.col1 = &fx->col1;
  fx->ctx.col1_ok = true;
  fx->ctx.map = &fx->map;
  fx->ctx.units = &fx->units;
  fx->ctx.colonies = &fx->cols;
  fx->ctx.human_nation = 0;
  fx->ctx.ai_popups = &fx->pop;
  fx->ctx.rng = &fx->rng;
  return 1;
}

static int withdraw_fx_spawn(WithdrawFx* fx, int type, int x, int y, int nation, int stack) {
  const int id = stack ? units_spawn_allow_stack(&fx->units, type, x, y)
                       : units_spawn(&fx->units, type, x, y);
  ColonizeUnit* u = units_get(&fx->units, id);
  if (u) {
    u->nation_id = nation;
  }
  return id;
}

/*
 * FUN_5bfb_153e raw 98181-98212 (asm 5bfb:2c17-2c40), the free peace-menu
 * @WITHDRAW: every non-ship AI unit next to a human colony goes, attack byte
 * or not, a passenger included (raw 98190 tests its own x/y, which is its
 * hull's). Each lands on the nation's Europe dock (236+n, FUN_281f_0880 raw
 * 98203) with goto = the nation's landfall (raw 98204-98207); orders stay.
 */
static int case_153e_peacemenu_withdraw_all_land(void) {
  static WithdrawFx fx;
  if (!withdraw_fx_init(&fx, 0)) {
    return fail("withdraw all: alloc map");
  }
  fx.col1.nation[1].return_from_europe_x = 33;
  fx.col1.nation[1].return_from_europe_y = 44;
  const int dock = EUROPE_PARK_XY + 1;
  const int cid = withdraw_fx_spawn(&fx, 0, 5, 6, 1, 0);  /* unarmed, adjacent */
  const int fid = withdraw_fx_spawn(&fx, 1, 12, 12, 1, 0); /* armed, far away */
  const int shid = withdraw_fx_spawn(&fx, 2, 6, 4, 1, 0);  /* hull, adjacent */
  const int pid = withdraw_fx_spawn(&fx, 0, 6, 4, 1, 1);   /* unarmed passenger */
  units_get(&fx.units, cid)->orders = UNITS_ORDER_FORTIFY;
  int rc = 0;
  if (!units_board_stacked_restore(&fx.units, pid, shid)) {
    rc = fail("withdraw all: board the passenger");
  }
  if (rc == 0) {
    ai_diplo_153e_test_peacemenu(&fx.ctx, 0, 1, 2, 0, 0, 2);
    const ColonizeUnit* c = units_get_const(&fx.units, cid);
    const ColonizeUnit* f = units_get_const(&fx.units, fid);
    const ColonizeUnit* sh = units_get_const(&fx.units, shid);
    const ColonizeUnit* p = units_get_const(&fx.units, pid);
    if (c->x != dock || c->y != dock || c->goto_x != 33 || c->goto_y != 44) {
      fprintf(stderr, "  colonist at %d,%d goto %d,%d\n", c->x, c->y, c->goto_x, c->goto_y);
      rc = fail("an unarmed AI unit goes to its Europe dock (236+n), goto = landfall");
    } else if (c->orders != UNITS_ORDER_FORTIFY) {
      rc = fail("DOS leaves the unit's orders (+0x314c) alone");
    } else if (p->aboard_ship_id >= 0 || p->x != dock || sh->cargo_count != 0) {
      rc = fail("a passenger next to the colony leaves its hull for the dock");
    } else if (sh->x != 6 || sh->y != 4 || f->x != 12) {
      rc = fail("hulls and units away from the colony stay");
    }
  }
  ai_diplo_talk_reset();
  fx_map_free(&fx.map);
  return rc;
}

/*
 * FUN_5bfb_153e raw 97411: under WoI (DS:0x5382 & 1) the encounter jumps to
 * the shared exit LAB_5bfb_30de (raw 98418-98427), which re-arms the treaty
 * cooldown at peace: nation[target].treaty_timer[self] = (6 - difficulty) * 2.
 */
static int case_153e_woi_encounter_rearms_timer(void) {
  ai_diplo_talk_reset(); /* a talk left open by an earlier case refuses this one */
  static WithdrawFx fx;
  if (!withdraw_fx_init(&fx, 0)) {
    return fail("woi timer: alloc map");
  }
  fx.col1.head.game_options.woi = 1;
  ai_diplo_make_peace(&fx.col1, 0, 1);
  fx.col1.nation[1].treaty_timer[0] = 0;
  const int uid = withdraw_fx_spawn(&fx, 1, 5, 6, 1, 0);
  const int started = ai_diplo_153e_encounter(&fx.ctx, 0, 1, uid, 0);
  const int timer = fx.col1.nation[1].treaty_timer[0];
  ai_diplo_talk_reset();
  fx_map_free(&fx.map);
  if (started || timer != (6 - 1) * 2) {
    fprintf(stderr, "  started %d timer %d\n", started, timer);
    return fail("woi: the closed gate must still re-arm the treaty timer at peace");
  }
  return 0;
}

/*
 * FUN_5bfb_153e @SIEGES paid (raw 97857-97884): every human unit with attack
 * byte > 1 (raw 97862), not a ship, whose own tile (a passenger's is its
 * hull's) borders the AI colony goes to Europe. There is no chain test, so
 * an armed passenger goes too; an unarmed one stays.
 */
static int case_153e_sieges_takes_passengers(void) {
  static WithdrawFx fx;
  if (!withdraw_fx_init(&fx, 1)) {
    return fail("sieges: alloc map");
  }
  const int shid = withdraw_fx_spawn(&fx, 2, 6, 4, 0, 0); /* human hull beside the colony */
  const int pid = withdraw_fx_spawn(&fx, 1, 6, 4, 0, 1);  /* armed passenger */
  const int cid = withdraw_fx_spawn(&fx, 0, 6, 4, 0, 1);  /* unarmed passenger */
  int rc = 0;
  if (!units_board_stacked_restore(&fx.units, pid, shid) ||
      !units_board_stacked_restore(&fx.units, cid, shid)) {
    rc = fail("sieges: board the passengers");
  }
  if (rc == 0) {
    ai_diplo_153e_test_sieges(&fx.ctx, 0, 1, 2);
    const ColonizeUnit* p = units_get_const(&fx.units, pid);
    const ColonizeUnit* c = units_get_const(&fx.units, cid);
    const ColonizeUnit* sh = units_get_const(&fx.units, shid);
    /* ctx.europe is NULL, so the human unit takes the 236+n park too. */
    if (!p || p->aboard_ship_id >= 0 || p->x != EUROPE_PARK_XY) {
      rc = fail("sieges: an armed human passenger beside the colony goes to Europe");
    } else if (!c || c->aboard_ship_id != shid || !sh || sh->cargo_count != 1 || sh->x != 6) {
      rc = fail("sieges: the unarmed passenger and the hull stay");
    }
  }
  ai_diplo_talk_reset();
  fx_map_free(&fx.map);
  return rc;
}

/*
 * FUN_2b5a_1112 raw 42410-42413: fortify Break Treaty ORs WAR into the
 * partner's byte (unconditionally), clears PEACE both ways and touches
 * nothing else: no 0x01 clear, no DS:0x53c8. The one-row WAR is TB-54, so
 * outside DOS mode WAR lands both ways.
 */
static int case_2b5a_1112_fortify_break_treaty(void) {
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.head.nation_relation[0] = 5;
  col1.head.nation_relation[1] = 5;
  int rc = 0;
  for (int buggy = 1; buggy >= 0 && rc == 0; --buggy) {
    dos_buggy_behavior_set(buggy != 0);
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE | AI_DIPLO_WAR_INTENT);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET);
    ai_diplo_2b5a_1112_break_treaty(&col1, 0, 1);
    const uint8_t self = ai_diplo_read(&col1, 0, 1);
    const uint8_t partner = ai_diplo_read(&col1, 1, 0);
    const uint8_t want_self =
      (uint8_t)(AI_DIPLO_MET | AI_DIPLO_WAR_INTENT | (buggy ? 0 : AI_DIPLO_WAR));
    if (self != want_self || partner != (AI_DIPLO_MET | AI_DIPLO_WAR) ||
        col1.head.nation_relation[0] != 5 || col1.head.nation_relation[1] != 5) {
      fprintf(stderr, "2b5a_1112 buggy=%d: self=0x%02x partner=0x%02x\n", buggy, self, partner);
      rc = fail("2b5a_1112 fortify break: partner |= WAR, PEACE cleared both ways only");
    }
  }
  dos_buggy_behavior_set(true);
  return rc;
}

/* FUN_5bfb_00f8 raw 96518-96522 keeps each score in a 16-bit word (sorted
 * unsigned), so 6.6M gold (gold/100 = 66000) wraps to 464 and ranks below
 * 100k gold (1000). */
static int case_00f8_score_wraps_16bit(void) {
  static ColonizeCol1Save c;
  memset(&c, 0, sizeof(c));
  c.nation[2].gold = 6600000u;
  c.nation[3].gold = 100000u;
  if (ai_diplo_00f8_top_ranked_nation(&c) != 3) {
    return fail("00f8: a score past 65535 must wrap in 16 bits");
  }
  return 0;
}

/*
 * Pop the talk queue: OK bodies go to `oks` (last one kept); the first CHOICE
 * is answered with `choice` (0 = leave it queued and stop). Returns the
 * answered CHOICE's body in `asked`.
 */
static void ally_fx_answer(TreatyFx* f, int choice, char* oks, char* asked) {
  if (oks) oks[0] = '\0';
  if (asked) asked[0] = '\0';
  while (f->pop.queue_count > 0) {
    AiPopupRequest front = f->pop.queue[0];
    if (front.kind == AI_POPUP_KIND_CHOICE && choice == 0) {
      if (asked) snprintf(asked, AI_POPUP_BODY_LEN, "%s", front.body);
      return;
    }
    memmove(&f->pop.queue[0], &f->pop.queue[1],
            sizeof(f->pop.queue[0]) * (size_t)(f->pop.queue_count - 1));
    f->pop.queue_count--;
    if (front.kind != AI_POPUP_KIND_CHOICE) {
      if (oks) snprintf(oks, AI_POPUP_BODY_LEN, "%s", front.body);
      continue;
    }
    if (asked) snprintf(asked, AI_POPUP_BODY_LEN, "%s", front.body);
    f->pop.has_result = true;
    f->pop.result_cancelled = false;
    f->pop.result_tag = AI_POPUP_TAG_DIPLO_TALK;
    f->pop.result_choice_id = choice;
    f->pop.result_nation_a = 0;
    f->pop.result_nation_b = 1;
    f->pop.result_payload = front.payload;
    ai_diplo_apply_popup_result(&f->ctx, &f->pop);
    f->pop.has_result = false;
    return;
  }
}

/*
 * FUN_5bfb_153e Military Assistance (5bfb:2d59 @MILITARY list, then the
 * raw 98340-98393 pay leg = ai_talk_stage_ally_pay): England (0) asks France
 * (1) to smite pick p. France has no contact with p -> @NOCONTACT; no treaty
 * with p -> @ALREADYSMITE; else the price = clamp((field + land strength of
 * p) * (gold/50) / 50, 10, 200) * 50 (tribe: tribe_data_9184 * gold/50 * 3 /
 * 50), Franklin halves it, @SMITEEUROPE / @SMITEINDIANS asks; "Yes" pays and
 * clears France's PEACE with p (WAR both ways), short gold -> @UNFORTUNATE.
 */
static int case_153e_ally_pay(void) {
  static TreatyFx f;
  static ColonizeCol1Tribe tribe[1];
  static char oks[AI_POPUP_BODY_LEN];
  static char asked[AI_POPUP_BODY_LEN];
  if (treaty_fx_init(&f) != 0) {
    return 1;
  }
  ColonizeCol1Save* e = &f.col1;
  snprintf(e->player[2].country_name, sizeof(e->player[2].country_name), "Spain");
  e->tribe = tribe;
  e->head.tribe_count = 1;
  e->indian[0].euro_diplo[0] = COL1_INDIAN_MET_BIT; /* England met tribe slot 4 */
  ai_diplo_write(e, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
  ai_diplo_write(e, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
  ai_diplo_write(e, 0, 2, AI_DIPLO_MET);
  e->stuff.field_combat_totals[2] = 30;
  e->stuff.land_combat_strength[2] = 20;
  e->stuff.tribe_data_9184[0] = 5;
  int rc = 0;

  /* 1: France never met Spain. */
  ai_diplo_153e_test_peacemenu(&f.ctx, 0, 1, 2, 0, 0, 4);
  if (f.pop.queue_count != 1 || f.pop.queue[0].kind != AI_POPUP_KIND_CHOICE ||
      !test_body_is_section(f.pop.queue[0].body, "MILITARY")) {
    rc = fail("ally: peace-menu row 4 must open the @MILITARY list");
  } else {
    /* Rows: Spain (value 3) and the met tribe (value 5); France/England absent. */
    const AiPopupRequest* q = &f.pop.queue[0];
    if (q->choice_count != 2 || q->choice_ids[0] != 3 || q->choice_ids[1] != 5) {
      fprintf(stderr, "ally: %d rows ids %d %d\n", q->choice_count, q->choice_ids[0],
              q->choice_ids[1]);
      rc = fail("ally: @MILITARY rows = met nations / tribes, value p+1");
    }
  }
  if (rc == 0) {
    ally_fx_answer(&f, 3, oks, asked);
    ally_fx_answer(&f, 0, oks, asked);
    if (!test_body_is_section(oks, "NOCONTACT") || f.pop.queue_count != 0) {
      fprintf(stderr, "ally nocontact: ok '%s' queued %d\n", oks, f.pop.queue_count);
      rc = fail("ally: target without contact with the pick -> @NOCONTACT");
    }
  }
  /* 2: France met Spain, no treaty. */
  if (rc == 0) {
    ai_diplo_write(e, 1, 2, AI_DIPLO_MET);
    ai_popup_init(&f.pop);
    ai_diplo_153e_test_peacemenu(&f.ctx, 0, 1, 2, 0, 0, 4);
    ally_fx_answer(&f, 3, oks, asked);
    ally_fx_answer(&f, 0, oks, asked);
    if (!test_body_is_section(oks, "ALREADYSMITE") || f.pop.queue_count != 0) {
      rc = fail("ally: target already at odds with the pick -> @ALREADYSMITE");
    }
  }
  /* 3: treaty: (30 + 20) * (1000 / 50) / 50 = 20 -> 1000; decline, then pay. */
  if (rc == 0) {
    ai_diplo_write(e, 1, 2, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_diplo_write(e, 2, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_popup_init(&f.pop);
    ai_diplo_153e_test_peacemenu(&f.ctx, 0, 1, 2, 0, 0, 4);
    ally_fx_answer(&f, 3, oks, asked);
    ally_fx_answer(&f, 0, oks, asked);
    if (!test_body_is_section(asked, "SMITEEUROPE") || !strstr(asked, "1000")) {
      fprintf(stderr, "ally smite: '%s'\n", asked);
      rc = fail("ally: Euro pick under treaty asks @SMITEEUROPE for 1000");
    } else {
      ally_fx_answer(&f, 2, oks, asked); /* No */
      if (e->nation[0].gold != 1000 || (ai_diplo_read(e, 1, 2) & AI_DIPLO_PEACE) == 0) {
        rc = fail("ally: declining the price changes nothing");
      }
    }
  }
  if (rc == 0) {
    ai_popup_init(&f.pop);
    ai_diplo_153e_test_peacemenu(&f.ctx, 0, 1, 2, 0, 0, 4);
    ally_fx_answer(&f, 3, oks, asked);
    ally_fx_answer(&f, 1, oks, asked); /* Yes */
    ally_fx_answer(&f, 0, oks, asked);
    if (e->nation[0].gold != 0 || e->nation[1].gold != 2000 ||
        (ai_diplo_read(e, 1, 2) & AI_DIPLO_PEACE) != 0 ||
        (ai_diplo_read(e, 2, 1) & AI_DIPLO_PEACE) != 0 ||
        (ai_diplo_read(e, 2, 1) & AI_DIPLO_WAR) == 0 || !test_body_is_section(oks, "MERCENARY")) {
      fprintf(stderr, "ally pay: gold %u/%u rel 1->2 0x%02x 2->1 0x%02x ok '%s'\n",
              (unsigned)e->nation[0].gold, (unsigned)e->nation[1].gold, ai_diplo_read(e, 1, 2),
              ai_diplo_read(e, 2, 1), oks);
      rc = fail("ally: paying hands 1000 to France, breaks its treaty with Spain (@MERCENARY)");
    }
  }
  /* 4: tribe pick, Franklin: 5 * 20 * 3 / 50 = 6 -> floor 10 -> 500 -> 250;
   * England now has 0 gold -> @UNFORTUNATE. */
  if (rc == 0) {
    e->indian[0].euro_diplo[1] = (uint8_t)(COL1_INDIAN_MET_BIT | COL1_INDIAN_PEACE_BIT);
    e->nation[0].founding_fathers[FF_BENJAMIN_FRANKLIN / 8] |=
      (uint8_t)(1u << (FF_BENJAMIN_FRANKLIN % 8));
    e->nation[0].gold = 1000; /* gold/50 = 20 for the price */
    ai_popup_init(&f.pop);
    ai_diplo_153e_test_peacemenu(&f.ctx, 0, 1, 2, 0, 0, 4);
    ally_fx_answer(&f, 5, oks, asked);
    ally_fx_answer(&f, 0, oks, asked);
    if (!test_body_is_section(asked, "SMITEINDIANS") || !strstr(asked, "250")) {
      fprintf(stderr, "ally tribe: '%s'\n", asked);
      rc = fail("ally: tribe pick asks @SMITEINDIANS for 250 (floor 500, Franklin half)");
    } else {
      e->nation[0].gold = 100;
      ally_fx_answer(&f, 1, oks, asked);
      ally_fx_answer(&f, 0, oks, asked);
      if (!test_body_is_section(oks, "UNFORTUNATE") || e->nation[0].gold != 100 ||
          (e->indian[0].euro_diplo[1] & COL1_INDIAN_PEACE_BIT) == 0) {
        rc = fail("ally: short of the price -> @UNFORTUNATE, nothing paid or broken");
      }
    }
  }
  ai_diplo_talk_reset();
  e->tribe = NULL;
  fx_map_free(&f.map);
  return rc;
}

static const TestCase k_cases[] = {
  {"case_153e_ally_pay", case_153e_ally_pay},
    {"case_00f8_score_wraps_16bit", case_00f8_score_wraps_16bit},
    {"case_2b5a_1112_fortify_break_treaty", case_2b5a_1112_fortify_break_treaty},
    {"case_153e_peacemenu_withdraw_dominance", case_153e_peacemenu_withdraw_dominance},
    {"case_153e_peacemenu_withdraw_all_land", case_153e_peacemenu_withdraw_all_land},
    {"case_153e_wantstuff_receiver_dos_distance", case_153e_wantstuff_receiver_dos_distance},
    {"case_153e_woi_encounter_rearms_timer", case_153e_woi_encounter_rearms_timer},
    {"case_153e_reads_census_rows", case_153e_reads_census_rows},
    {"case_153e_war_bit_survives_worthy_peace", case_153e_war_bit_survives_worthy_peace},
    {"case_153e_war_bit_survives_givecash_peace", case_153e_war_bit_survives_givecash_peace},
    {"case_153e_sieges_takes_passengers", case_153e_sieges_takes_passengers},
    {"case_relation_writes_preserve_player_bytes", case_relation_writes_preserve_player_bytes},
    {"case_153e_talk_tokens_474", case_153e_talk_tokens_474},
    {"case_declare_peace_narrative", case_declare_peace_narrative},
    {"case_r11_no_indian_war_hit", case_r11_no_indian_war_hit},
    {"case_trade_deepen", case_trade_deepen},
    {"case_declare_war_side_effects", case_declare_war_side_effects},
    {"case_indian_relation_read", case_indian_relation_read},
    {"case_r2_no_fatigue_tools", case_r2_no_fatigue_tools},
    {"case_r3_r4_sugar_rum_cigars_boycott", case_r3_r4_sugar_rum_cigars_boycott},
    {"case_r6_ore_silver_boycott", case_r6_ore_silver_boycott},
    {"case_r8_lumber_boycott_privateer", case_r8_lumber_boycott_privateer},
    {"case_r9_horses_muskets_boycott", case_r9_horses_muskets_boycott},
    {"case_diplo_peace_result_no_action", case_diplo_peace_result_no_action},
    {"case_marathon3_franklin_peace_gate", case_marathon3_franklin_peace_gate},
    {"case_indian_grudge_tension_clamp", case_indian_grudge_tension_clamp},
    {"case_153e_phase1_border_probe", case_153e_phase1_border_probe},
    {"case_153e_phases_2_4_encounter", case_153e_phases_2_4_encounter},
    {"case_153e_wantstuff_demand", case_153e_wantstuff_demand},
    {"case_153e_wantstuff_source_is_encounter_dir", case_153e_wantstuff_source_is_encounter_dir},
    {"case_153e_wantstuff_weight_is_84bc_byte", case_153e_wantstuff_weight_is_84bc_byte},
    {"case_153e_worthy_cascade", case_153e_worthy_cascade},
    {"case_153e_usa_greeting", case_153e_usa_greeting}, /* last: leaves the static talk open */
};
TEST_MAIN(k_cases)
