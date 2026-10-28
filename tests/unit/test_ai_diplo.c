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
#include "core/colony.h"
#include "core/dos_rng.h"
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

/* Expected status text for a nation pair, built from the same GAME.TXT
 * section and NAMES.TXT adjectives the code reads — never typed in here. */
static const char* diplo_pair_text(const char* section, int nation_a, int nation_b) {
  static char buf[4][256];
  static int next = 0;
  char* out = buf[next];
  next = (next + 1) % 4;
  char a[32], b[32];
  snprintf(a, sizeof(a), "%s", reports_nation_adjective_display_name(nation_a));
  snprintf(b, sizeof(b), "%s", reports_nation_adjective_display_name(nation_b));
  PopupMsgTokens tok = {0};
  tok.string0 = a;
  tok.string1 = b;
  popup_msg_fill(test_game_txt(), section, &tok, "", out, sizeof(buf[0]));
  popup_msg_strip_markup(out);
  return out;
}

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
  if (!ai_diplo_at_war_with_any(&col1, 0) || ai_diplo_at_war_with_any(&col1, 2)) {
    return fail("ai_diplo_at_war_with_any should detect any Euro×Euro war");
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

static int case_trade_deepen_military_score(void) {
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

    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    units_set_occupancy_map(NULL);
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
    units_get(&units, sid)->nation_id = 0;
    units_get(&units, ship)->nation_id = 0;
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    colonies_set_occupancy_map(NULL);
    colonies.colonies[0].active = true;
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].population = 4;
    colonies.colony_count = 1;
    ColonizeCol1Save sc;
    col1_save_init(&sc);
    sc.nation[0].gold = 100; /* must NOT reach the score any more */
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.messages = test_game_txt();
    ctx.units = &units;
    ctx.colonies = &colonies;
    ctx.col1 = &sc;
    ctx.col1_ok = true;
    /*
     * 2026-09-09 (#51): the score is DS:0x941c land_combat_strength — nothing
     * else. Ships, colony population, fortifications, treasury and the 00f8
     * rank must not contribute (the invented blend is retired).
     * 2026-09-10 (sweep-3 D2): computed LIVE (Σ combat_unit_base_x8 over the
     * nation's active land units), no longer read from the stuff mirror —
     * that mirror has no live writer in an OpenCol-started game. The stale
     * mirror values below must therefore NOT reach the score: the fixture's
     * one Soldier (attack 2) is worth 2*8 = 16.
     */
    sc.stuff.land_combat_strength[0] = 137;
    sc.stuff.land_combat_strength[1] = 40;
    ctx.euro_power_rank_ok = true;
    ctx.euro_power_rank[0] = 0;
    ctx.euro_power_rank[1] = 3;
    int score = ai_diplo_military_score(&ctx, 0);
    if (score != 16) {
      fprintf(stderr, "unit_ai_diplo: military_score %d (want 16)\n", score);
      return fail("military_score must be the live land-unit x8 sum, not the mirror");
    }
    if (ai_diplo_military_score(&ctx, 1) != 0) {
      return fail("military_score peer with no units must be 0 despite mirror 40");
    }
    /* Rank must not double-count: the 00f8 rank table is itself built from
     * land_combat_strength, so flipping rank alone changes nothing. */
    ctx.euro_power_rank[0] = 3;
    if (ai_diplo_military_score(&ctx, 0) != 16) {
      return fail("military_score must not add a rank term");
    }
    /* Treasury must not leak in either. */
    sc.nation[0].gold = 100000;
    score = ai_diplo_military_score(&ctx, 0);
    if (score != 16) {
      fprintf(stderr, "unit_ai_diplo: military_score with gold %d (want 16)\n", score);
      return fail("military_score must not blend treasury");
    }
    /* No col1 → gate closed → 0. */
    ctx.col1_ok = false;
    if (ai_diplo_military_score(&ctx, 0) != 0) {
      return fail("military_score without col1 must be 0");
    }
    ctx.col1_ok = true;
  return 0;
}

static int case_war_peace_status_chrome(void) {
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
    ctx_st.messages = test_game_txt();
    ctx_st.col1 = &st;
    ctx_st.col1_ok = true;
    ctx_st.human_nation = 0;
    ctx_st.status = status;
    ctx_st.status_size = sizeof(status);

    ai_diplo_declare_war_ctx(&ctx_st, 1, 0);
    if (!ai_diplo_at_war(&st, 0, 1)) {
      return fail("declare_war_ctx should set WAR");
    }
    if (strcmp(status, diplo_pair_text("DECLAREWAR", 1, 0)) != 0) {
      fprintf(stderr, "unit_ai_diplo: war status '%s'\n", status);
      return fail("declare_war_ctx should use @DECLAREWAR line (no wartime boycott)");
    }
    /* Re-declare: no status rewrite. */
    snprintf(status, sizeof(status), "keep");
    ai_diplo_declare_war_ctx(&ctx_st, 1, 0);
    if (strcmp(status, "keep") != 0) {
      return fail("re-declare_war_ctx should not rewrite status");
    }

    ai_diplo_make_peace(&st, 0, 1);

    /* R12: Sugar/Tobacco/Tools already set → name first newly boycotted cargo
     * (Food idx 0) instead of the war line. Source: @CARGO / NAMES.TXT; FA UI PARKED. */
    st.player[2].country_name[0] = '\0';
    st.nation[0].gold = 200;
    st.nation[2].gold = 200;
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 0; /* relation 100 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
    st.nation[0].boycott_bitmap =
      (uint16_t)(AI_DIPLO_SMOKE_SUGAR_BIT | AI_DIPLO_SMOKE_TOBACCO_BIT |
                 AI_DIPLO_SMOKE_TOOLS_BIT);
    status[0] = '\0';
    ai_diplo_declare_war_ctx(&ctx_st, 0, 2);
    if ((st.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_FOOD_BIT) != 0) {
      return fail("declare_war_ctx must not OR Food onto existing king boycotts");
    }
    if (strcmp(status, diplo_pair_text("DECLAREWAR", 0, 2)) != 0) {
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
    st.nation[0].boycott_bitmap = (uint16_t)AI_DIPLO_SMOKE_WARTIME_MASK;
    status[0] = '\0';
    ai_diplo_declare_war_ctx(&ctx_st, 0, 2);
    if (strcmp(status, diplo_pair_text("DECLAREWAR", 0, 2)) != 0) {
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
      assets_msg_free(&game_txt);
    }
    if (strcmp(status, diplo_pair_text("DECLAREWAR", 0, 2)) != 0) {
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
     * Indian relations) — declare_war_ctx must neither move the matrix nor
     * pop "Natives grow hostile" (bugs: it fired on a Euro-vs-Euro attack). */
    ai_diplo_make_peace(&st, 0, 2);
    for (int i = 0; i < 8; ++i) {
      st.indian[i].alarm_by_player[0] = 70; /* relation 30 */
      st.indian[i].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    }
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
    ctx_ts.messages = test_game_txt();
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
    if (strcmp(status, diplo_pair_text("DECLAREWAR", 0, 1)) != 0) {
      fprintf(stderr, "unit_ai_diplo: Tools set status '%s'\n", status);
      free(ts.colony);
      return fail("declare_war_ctx should use war line (no wartime boycott)");
    }
    ai_diplo_make_peace(&ts, 0, 1);
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
    if (ai_diplo_at_war_with_any(&sg, 0) || ai_diplo_at_war(&sg, 0, 1)) {
      return fail("after peace, at_war / at_war_with_any should be clear");
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
    ctx_st.messages = test_game_txt();
    ctx_st.col1 = &st;
    ctx_st.col1_ok = true;
    ctx_st.human_nation = 0;
    ctx_st.status = status_st;
    ctx_st.status_size = sizeof(status_st);
    ai_diplo_declare_war_ctx(&ctx_st, 0, 1);
    if ((st.nation[0].boycott_bitmap & AI_DIPLO_SMOKE_TOOLS_BIT) != 0) {
      return fail("declare_war must not OR Tools boycott");
    }
    if (strcmp(status_st, diplo_pair_text("DECLAREWAR", 0, 1)) != 0) {
      fprintf(stderr, "unit_ai_diplo: Sugar/Tobacco/Tools status '%s'\n", status_st);
      return fail("declare_war_ctx should use war line (no wartime boycott)");
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

static int case_declare_war_popup_unpark(void) {
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
    ctx_pop.messages = test_game_txt();
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
    if (strcmp(status_pop, diplo_pair_text("DECLAREWAR", 1, 0)) != 0) {
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


    /*
     * R2: war OK both directions (human-as-a / human-as-b) once each;
     * re-declare must not spam. A DIPLO_PEACE result carries no action: DOS
     * treaty notices are OK-only, the old Accept/Refuse handler was dead
     * (port_bugs.md #1104). Cite: FUN_15b3 / FUN_5bfb; FA 3f41 full UI PARKED.
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
      ctx_w2.messages = test_game_txt();
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
        /* human-as-b */
      ai_diplo_declare_war_ctx(&ctx_w2, 1, 0);
      if (pop_w2.queue_count != 1) {
        return fail("R2 war: human-as-b should enqueue one OK");
      }
      ai_diplo_declare_war_ctx(&ctx_w2, 1, 0);
      if (pop_w2.queue_count != 1) {
        return fail("R2 war: re-declare human-as-b must not spam");
      }

      /* A DIPLO_PEACE choice result must not make peace any more. */
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

static int case_153e_phases_2_4_encounter(void) {
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
    const int started = ai_diplo_153e_encounter(&ectx, 0, 1, pa->id);
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
    const int second = ai_diplo_153e_encounter(&ectx, 0, 1, pa->id);
    /* port_bugs.md #453: FUN_5f7a_000e Meet With Mayor pushes param_6 = 1, so the
     * scout's audience opens even inside the cooldown and the same turn. */
    ai_popup_init(&epop);
    const int forced = second ? 0 : ai_diplo_153e_encounter_forced(&ectx, 0, 1, pa->id);
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

static int case_153e_wantstuff_demand(void) {
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
    qctx.messages = test_game_txt();
    qctx.col1 = &q;
    qctx.col1_ok = true;
    qctx.map = &qmap;
    qctx.units = &qunits;
    qctx.colonies = &qcol;
    qctx.human_nation = 0;
    qctx.ai_popups = &qpop;
    qctx.rng = &qrng;
    const int started = ai_diplo_153e_encounter(&qctx, 0, 1, pai->id);
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
    if (!saw_wantstuff) {
      return fail("153e wantstuff: the goods demand CHOICE must be queued");
    }
    /* docs/dos_bugs.md DB-8: the NAMED cargo moves (DOS moves Furs). */
    if (qcol.colonies[1].stock[COLONIZE_CARGO_MUSKETS] != 60 ||
        qcol.colonies[0].stock[COLONIZE_CARGO_MUSKETS] != 0) {
      fprintf(stderr, "unit_ai_diplo: muskets src=%d dst=%d\n",
              qcol.colonies[0].stock[COLONIZE_CARGO_MUSKETS],
              qcol.colonies[1].stock[COLONIZE_CARGO_MUSKETS]);
      return fail("153e wantstuff: accepting must move the demanded cargo by the demanded amount");
    }
    if (qcol.colonies[0].stock[COLONIZE_CARGO_FURS] != 10 ||
        qcol.colonies[1].stock[COLONIZE_CARGO_FURS] != 0) {
      return fail("153e wantstuff: Furs must not move (DB-8)");
    }
  return 0;
}

static int case_153e_worthy_cascade(void) {
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
    if (!ai_diplo_153e_encounter(&rctx, 0, 1, rai->id)) {
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
    if (!ai_diplo_153e_encounter(&rctx, 0, 1, rai->id)) {
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
  (void)ai_diplo_153e_encounter(&ectx, 0, 1, pa->id);
  if (talk474_drain(&ectx, &epop, &saw_worthy, &saw_wise) != 0) {
    return 1;
  }
  ai_popup_init(&epop);
  (void)ai_diplo_153e_encounter_forced(&ectx, 0, 1, pa->id);
  if (talk474_drain(&ectx, &epop, &saw_worthy, &saw_wise) != 0) {
    return 1;
  }
  if (!saw_worthy || !saw_wise) {
    fprintf(stderr, "unit_ai_diplo: #474 saw_worthy=%d saw_wise=%d\n", saw_worthy, saw_wise);
    return fail("#474: fixture must reach both @WORTHY and @OLDPEACEMEEK");
  }
  return 0;
}

/* raw :97431/:97720: a target that won its own independence (nation_flags 0x04)
 * greets with @HELLOUSA, not @HELLOFIRST's "in the name of our Queen" (playtest C10). */
static int case_153e_usa_greeting(void) {
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
  (void)ai_diplo_153e_encounter(&ectx, 0, 2, pa->id);
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

static const TestCase k_cases[] = {
    {"case_relation_writes_preserve_player_bytes", case_relation_writes_preserve_player_bytes},
    {"case_153e_talk_tokens_474", case_153e_talk_tokens_474},
    {"case_declare_peace_narrative", case_declare_peace_narrative},
    {"case_r11_no_indian_war_hit", case_r11_no_indian_war_hit},
    {"case_trade_deepen_military_score", case_trade_deepen_military_score},
    {"case_war_peace_status_chrome", case_war_peace_status_chrome},
    {"case_indian_relation_read", case_indian_relation_read},
    {"case_r2_no_fatigue_tools", case_r2_no_fatigue_tools},
    {"case_r3_r4_sugar_rum_cigars_boycott", case_r3_r4_sugar_rum_cigars_boycott},
    {"case_r6_ore_silver_boycott", case_r6_ore_silver_boycott},
    {"case_r8_lumber_boycott_privateer", case_r8_lumber_boycott_privateer},
    {"case_r9_horses_muskets_boycott", case_r9_horses_muskets_boycott},
    {"case_declare_war_popup_unpark", case_declare_war_popup_unpark},
    {"case_marathon3_franklin_peace_gate", case_marathon3_franklin_peace_gate},
    {"case_indian_grudge_tension_clamp", case_indian_grudge_tension_clamp},
    {"case_153e_phase1_border_probe", case_153e_phase1_border_probe},
    {"case_153e_phases_2_4_encounter", case_153e_phases_2_4_encounter},
    {"case_153e_wantstuff_demand", case_153e_wantstuff_demand},
    {"case_153e_worthy_cascade", case_153e_worthy_cascade},
    {"case_153e_usa_greeting", case_153e_usa_greeting}, /* last: leaves the static talk open */
};
TEST_MAIN(k_cases)
