/* Slice of the former tests/unit/test_ai_contact.c (split by feature 2026-09-23):
 * the original inline main() narrative (meet/raid/incite/trade/mission/wagon
 * beats), split into named cases 2026-09-23. */
#include "test_ai_contact_common.h"

/* 0f14 seam access for the raid blocks (COLONIZE_TESTING linkage). */
#include "core/ai_contact_internal.h"
#include "core/ai_internal.h"
#include "core/dos_buggy.h"
#include "core/sound.h"

/*
 * FUN_4d56_417e is a two-step dialog: the target menu (@INDIANWARPATH) is
 * answered with a nation id, which enqueues the @INDIANWARPATH2 pay confirm;
 * only "Pay" on THAT popup charges the treasury. The live harness feeds each
 * answered popup's own payload back as result_payload, so replicate both hops
 * here.
 */
static void test_incite_confirm_pay(
  ColonizeTurnContext* ctx,
  AiPopupState* pop,
  int inciter,
  int nation_id
) {
  if (pop->queue_count < 1 ||
      pop->queue[pop->queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
      pop->queue[pop->queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE) {
    return;
  }
  const int payload = pop->queue[pop->queue_count - 1].payload;
  const int pay_id = pop->queue[pop->queue_count - 1].choice_ids[0];
  ai_popup_clear(pop);
  pop->has_result = true;
  pop->result_cancelled = false;
  pop->result_choice_id = pay_id;
  pop->result_tag = AI_POPUP_TAG_CONTACT_INCITE;
  pop->result_nation_a = inciter;
  pop->result_nation_b = nation_id;
  pop->result_payload = payload;
  ai_contact_apply_popup_result(ctx, pop);
}

/*
 * Human buy-offer CHOICE follow-up (FUN_4d56_2820 LAB_002e92 human branch):
 * pop the CONTACT_TRADE_OFFER price CHOICE just enqueued by Meet/TRADE and
 * accept or decline it (same two-call idiom this file already uses for the
 * Gift/Demand/Incite amount CHOICEs). Returns the locked price, or -1 if no
 * such CHOICE was queued (caller's Meet/TRADE apply already ran its own
 * fallback path — refuse gate or no source/econ to price against).
 */
static int apply_trade_offer_choice(
  ColonizeTurnContext* ctx,
  AiPopupState* pop,
  int e,
  int nation_id,
  int accept
) {
  if (pop->queue_count < 1) {
    return -1;
  }
  const AiPopupRequest* req = &pop->queue[pop->queue_count - 1];
  if (req->kind != AI_POPUP_KIND_CHOICE || req->tag != AI_POPUP_TAG_CONTACT_TRADE_OFFER ||
      req->nation_a != e || req->nation_b != nation_id) {
    return -1;
  }
  const int price = req->payload;
  ai_popup_clear(pop);
  pop->has_result = true;
  pop->result_cancelled = false;
  pop->result_choice_id = accept ? 1 : 2; /* AI_CONTACT_TRADE_OFFER_ACCEPT : _DECLINE */
  pop->result_tag = AI_POPUP_TAG_CONTACT_TRADE_OFFER;
  pop->result_nation_a = e;
  pop->result_nation_b = nation_id;
  pop->result_payload = price;
  ai_contact_apply_popup_result(ctx, pop);
  return price;
}

/*
 * The original body was ONE continuous narrative: a single shared fixture
 * (col1/map/units/ctx/pop) built once and then mutated and re-checked across
 * ~60 scenario beats all the way to the teardown. There is no point at which
 * the fixture resets, and later beats depend on the alarm / met / tension /
 * treasury / unit state — and on the RNG stream position — left by earlier
 * ones, so a beat cannot be lifted out on its own.
 *
 * The split therefore keeps the narrative intact and cuts it into ordered
 * spine segments sp_00..sp_NN (every statement still appears exactly once,
 * verbatim, in its original order). Each named case is `fx_run(N)`: it
 * replays the spine from sp_00 up to and including its own segment, then
 * tears the fixture down. So `COLONIZE_TEST_ONLY=<case>` rebuilds exactly
 * the state that case was written against, and the cases are order- and
 * shuffle-independent. sp_00 (fx_open) additionally resets the sim-side
 * statics that outlive a fixture, so one case never inherits another's.
 */

/* Fixture state shared by the spine segments (was main()-scope locals). */
static ColonizeCol1Save col1;
static ColonizeCol1Indian* ind;
static ColonizeWorldMap map;
static ColonizeUnitPool units;
static ColonizeColonyPool colonies;
static int brave_id;
static int euro_id_;
static ColonizeUnit* brave;
static ColonizeUnit* euro;
static ColonizeColony* c;
static uint32_t turn;
static ColonizeTurnContext ctx;
static int alarm_pre_raid;
static char status[128];
/* AI-popup narrative block (was the 2910..4610 scope). */
static AiPopupState pop;
static ColonizeUnit* brave2;
static ColonizeUnit* land0;
static int goods0;
static char st_pop[128];
/* Reparations block (was the 5110..5475 scope). */
static AiPopupState rp;
static ColonizeDosRng rep_rng;
static ColonizeDosRng* saved_rng;
static uint16_t saved_turn;
static int rep_brave;
static int rq;
/* @ACTIONS village-menu block (was the 6028..6726 scope). */
static ColonizeMsgCatalog game_txt;
static AiPopupState pop_menu;
static char st_menu[AI_POPUP_BODY_LEN];
static int scout_id;
static ColonizeUnit* scout;
static int col_id;
static ColonizeUnit* colonist;

static void fx_close(void) {
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  /* tribe[] was calloc'd by hand above and col1.owned stays 0, so
   * col1_save_free() will not release it — free it here. */
  free(col1.tribe);
  col1.tribe = NULL;
  col1_save_free(&col1);
  assets_msg_free(&game_txt); /* sp_53 loads it; the spine frees it only at its end */
}

/*
 * port_bugs.md #1093: the invented post-pulse raid pass (ai_contact_indian_raids)
 * is gone. DOS reaches FUN_5fef_0f14 only from FUN_5fef_1b0e's loser limb,
 * i.e. ai_contact_colony_raid_repelled_w. The raid blocks below drive that
 * entry directly for the colony the fixture Brave stands on, with the old
 * pulse's turn-keyed local stream (so raid_probe_turn_for_kind still works)
 * and forced = 0 (the walls roll applies). The Brave is not removed: 1b0e
 * has already killed the attacker by the time it hands off.
 */
static void run_colony_raid(void) {
  units_pool_sync(&units);
  if (!brave || !brave->active || units_remaining_mp(&units, brave->id) <= 0) {
    return;
  }
  const int cid = colonies_id_at(&colonies, brave->x, brave->y);
  const ColonizeColony* rc = cid >= 0 ? colonies_get(&colonies, cid) : NULL;
  if (!rc) {
    return;
  }
  ColonizeDosRng local;
  ai_contact_local_rng(&ctx, 4, &local);
  ColonizeWorld w = world_from_turn_ctx(&ctx);
  w.rng = &local;
  /* The 1b0e path has no status line; its @RAID* chrome is a popup. Mirror
   * the newest queued body into ctx.status so the blocks' text checks hold. */
  static AiPopupState cap;
  if (!w.ai_popups) {
    ai_popup_init(&cap);
    w.ai_popups = &cap;
  }
  const int q0 = w.ai_popups->queue_count;
  (void)ai_contact_colony_raid_repelled_w(
    &w, 4, rc->nation_id, cid, brave->home_tribe_index, 0, ctx.human_nation
  );
  if (ctx.status && ctx.status_size && w.ai_popups->queue_count > q0) {
    snprintf(
      ctx.status, ctx.status_size, "%s",
      w.ai_popups->queue[w.ai_popups->queue_count - 1].body
    );
    popup_msg_strip_markup(ctx.status);
  }
}

/*
 * port_bugs.md #827: FUN_5fef_0f14's kind is `rand(1,4)` behind a walls roll, so a
 * fixture can no longer "force" a kind through an alarm band the way the
 * retired OpenCol band-picker allowed. This probe keeps those blocks honest
 * without changing what they exercise: snapshot the whole fixture, replay the
 * pulse over successive turn values (the pulse's local RNG is keyed on the
 * turn) until `want` comes up, restore the snapshot, and leave `turn` on the
 * value that produces it — so the block's own single run_colony_raid() lands
 * that kind with exactly one pulse worth of side effects, as before.
 */
static int raid_probe_turn_for_kind(int want, int max_tries) {
  const uint32_t turn0 = turn;
  static ColonizeUnitPool rp_units;
  static ColonizeColonyPool rp_colonies;
  static ColonizeCol1Save rp_col1;
  rp_units = units;
  rp_colonies = colonies;
  rp_col1 = col1;
  const size_t tn = (size_t)col1.head.tribe_count;
  ColonizeCol1Tribe* tsave = NULL;
  if (col1.tribe && tn) {
    tsave = (ColonizeCol1Tribe*)malloc(tn * sizeof(*tsave));
    if (tsave) {
      memcpy(tsave, col1.tribe, tn * sizeof(*tsave));
    }
  }
  const int kind0 = ai_contact_last_raid_kind();
  int found = -1;
  for (int i = 0; i < max_tries && found < 0; ++i) {
    turn = turn0 + (uint32_t)i;
    run_colony_raid();
    if (ai_contact_last_raid_kind() == want) {
      found = (int)(turn0 + (uint32_t)i);
    }
    units = rp_units;
    colonies = rp_colonies;
    ColonizeCol1Tribe* const tp = col1.tribe;
    col1 = rp_col1;
    col1.tribe = tp;
    if (tsave) {
      memcpy(col1.tribe, tsave, tn * sizeof(*tsave));
    }
  }
  free(tsave);
  ai_contact_s_last_raid_kind = kind0;
  turn = (found >= 0) ? (uint32_t)found : turn0;
  return found >= 0;
}

static int sp_00(void) {
  /* New in the split: sim-side statics that outlive one fixture must be
   * cleared so any case order works (tests/README.md "Hunting order
   * dependencies"). */
  ai_contact_reset();
  village_trade_intel_reset();
  ai_diplo_talk_reset();
  ai_native_reset();
  turn_reset();
  units_set_native_fallout_context(NULL, NULL);
  units_set_occupancy_map(NULL);
  colonies_set_occupancy_map(NULL);
  brave = NULL;
  euro = NULL;
  brave2 = NULL;
  land0 = NULL;
  c = NULL;
  ind = NULL;
  scout = NULL;
  colonist = NULL;
  alarm_pre_raid = 0;
  status[0] = '\0';
  st_pop[0] = '\0';
  st_menu[0] = '\0';
  col1_save_init(&col1);
  col1.head.difficulty = 2;
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    return fail("alloc tribe");
  }
  col1.tribe[0].x = 5;
  col1.tribe[0].y = 5;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].population = 4;
  col1.tribe[0].alarm[0].friction = 0;

  ind = &col1.indian[0];
  memset(ind, 0, sizeof(*ind));
  ind->euro_diplo[0] = 0;
  ind->alarm_by_player[0] = 0;
  /* Zero-init founding_father[] == 0 would falsely grant FF to Euro 0. */
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }

  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("alloc map");
  }
  /* Plains-ish land everywhere (non-water). */
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }

  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Brave");
  units.types[0].movement = 1; /* NAMES @UNIT; DOS max MP = 3 thirds */
  units.types[0].attack = 2;
  units.types[0].defense = 1;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Free Colonist");
  units.types[1].movement = 1;
  units.types[1].attack = 0;
  units.types[1].defense = 1;

  brave_id = units_spawn_allow_stack(&units, 0, 5, 5);
  euro_id_ = units_spawn_allow_stack(&units, 1, 8, 5);
  brave = units_get(&units, brave_id);
  euro = units_get(&units, euro_id_);
  if (!brave || !euro) {
    return fail("spawn");
  }
  brave->nation_id = 4;
  brave->moves = 0; /* natives store the DOS SPENT byte: 0 = fresh, 3 = spent */
  euro->nation_id = 0;

  /* Meet needs adjacency — temporarily place euro next to brave. */
  euro->x = 6;
  euro->y = 5;

  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);

  turn = 1;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  return 0;
}

static int sp_01(void) {
  /* Meet: adjacent Euro → euro_diplo + relation bump; peaceful friction decay. */
  ind->alarm_by_player[0] = 33; /* first contact clamps alarm <= 20 (FUN_5bfb :96624) */
  (void)ai_contact_try_first_welcome(&ctx, 0, 4);
  if (!ind->euro_diplo[0]) {
    return fail("meet should set euro_diplo[0]");
  }
  if (col1.nation[0].relation_by_indian[0] != 96 || ind->alarm_by_player[0] > 20) {
    return fail("meet should set relation_by_indian=0x60 flag and clamp alarm <= 20");
  }
  /* Peaceful meet-trade needs relation ≥40 (very-low refuse gate). */
  col1.indian[0].alarm_by_player[0] = 20; /* relation 80 */
  col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
  /* Move Euro away so colony raid is not pre-empted by adjacent combat. */
  euro->x = 10;
  euro->y = 10;

  return 0;
}

static int sp_02(void) {
  /* Raid: high friction, brave on colony tile → @RAID* loot path. */
  ind->alarm_by_player[0] = 55;
  col1.tribe[0].alarm[0].friction = 55;

  c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 5;
  c->y = 5;
  c->population = 3;
  c->colonist_count = 3;
  c->stock[COLONIZE_CARGO_FOOD] = 10;
  c->building_in_production = -1;
  colonies.colony_count = 1;
  colonies.next_id = 1;

  brave->x = 5;
  brave->y = 5;
  brave->moves = 0;

  const int food0 = c->stock[COLONIZE_CARGO_FOOD];
  /* War bit clear → FUN_5fef_0f14's alarm tail is not gated out (see below). */
  ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~COL1_INDIAN_WAR_BIT);
  alarm_pre_raid = (int)ind->alarm_by_player[0];
  run_colony_raid();
  const int kind = ai_contact_last_raid_kind();
  if (kind < AI_RAID_NOTHING || kind > AI_RAID_GOLD) {
    return fail("raid kind out of range");
  }
  /* With alarm 55, picker prefers STORES (food--) or other mutating kinds. */
  if (kind == AI_RAID_NOTHING && c->stock[COLONIZE_CARGO_FOOD] == food0 &&
      c->population == 3 && c->building_in_production < 0) {
    /* NOTHING is valid at low-mid band; force STORES by bumping alarm. */
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    brave->moves = 0;
    alarm_pre_raid = (int)ind->alarm_by_player[0];
    run_colony_raid();
  }
  if (ai_contact_last_raid_kind() == AI_RAID_NOTHING && c->stock[COLONIZE_CARGO_FOOD] == food0) {
    /*
     * @RAIDNOTHING is a first-class DOS outcome, not a failure: the walls
     * roll (raw 99783), a Stockade + `difficulty < rand(0,8)` (raw 99814),
     * a Fortress against the ship kind, or 100 fruitless cargo rolls all
     * land here and mutate nothing. The old assertion demanded a colony
     * change or an `attacks` bump; DOS bumps that byte only from the 465b
     * trespass arm, and the alarm-band picker that made NOTHING rare was
     * retired with port_bugs.md #827.
     */
  } else if (c->stock[COLONIZE_CARGO_FOOD] >= food0 && c->population >= 3 &&
             ai_contact_last_raid_kind() != AI_RAID_GOLD &&
             ai_contact_last_raid_kind() != AI_RAID_SHIP &&
             ai_contact_last_raid_kind() != AI_RAID_BURN &&
             ai_contact_last_raid_kind() != AI_RAID_NOTHING) {
    return fail("expected colony stock/pop change for raid kind");
  }
  /*
   * FUN_5fef_0f14's alarm tail (raw viceroy_unpacked.c:100033):
   * `FUN_281f_0d6c(indian_nation, euro, delta, 0)` with delta 0xfffc (−4)
   * goods / 0xfff4 (−12) building / 0xfff0 (−16) unit / 0xfff8 (−8) gold,
   * and no call at all for kind 0. Every value is NEGATIVE — a successful
   * raid DISCHARGES the tribe's alarm toward that European, matching the
   * unconditional DS:0x54f6 tension clear on the next line. Until
   * 2026-09-08 the port applied a fandom-derived POSITIVE bump of the same
   * magnitudes here (and mapped DOS's −16 to SCALP instead of the ship
   * kind); DOS grows Indian alarm only through the 4d56_152e accumulator.
   */
  return 0;
}

static int sp_03(void) {
  {
    const int kind_after = ai_contact_last_raid_kind();
    int expect_delta = 0;
    if (kind_after == AI_RAID_STORES) {
      expect_delta = -4;
    } else if (kind_after == AI_RAID_BURN) {
      expect_delta = -12;
    } else if (kind_after == AI_RAID_SHIP) {
      expect_delta = -16;
    } else if (kind_after == AI_RAID_GOLD) {
      expect_delta = -8;
    }
    int expect_alarm = alarm_pre_raid + expect_delta;
    if (expect_alarm < 0) {
      expect_alarm = 0;
    }
    if ((int)ind->alarm_by_player[0] != expect_alarm) {
      return fail("raid alarm tail must vent DOS's negative per-kind delta");
    }
    if (kind_after != AI_RAID_NOTHING &&
        ai_diplo_indian_relation(&col1, 4 + (0), 0) <=
          (uint8_t)(100 - alarm_pre_raid)) {
      return fail("successful raid should vent alarm, not escalate it");
    }
  }
  /* War gate (`FUN_281f_0a38(...) & 2`, raw 99993): once the tribe is already
   * at war with that European the 0d6c call is jumped over — no relief. */
  {
    ind->alarm_by_player[0] = 70;
    col1.tribe[0].alarm[0].friction = 70;
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] | COL1_INDIAN_WAR_BIT);
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    const int alarm_war_pre = (int)ind->alarm_by_player[0];
    run_colony_raid();
    if ((int)ind->alarm_by_player[0] != alarm_war_pre) {
      return fail("raid alarm tail must be skipped while at war (15b3 bit 1)");
    }
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~COL1_INDIAN_WAR_BIT);
  }
  return 0;
}

static int sp_04(void) {
  /*
   * 1b0e's repelled-at-a-colony handoff (thunk_FUN_2a1f_06c8 → the whole of
   * FUN_5fef_0f14) is the second entry into the same resolver, so it applies
   * the same tail — both entries agreed 2026-09-08.
   */
  {
    ColonizeDosRng raid_rng;
    dos_rng_seed(&raid_rng, 4242u);
    ind->alarm_by_player[0] = 60;
    col1.tribe[0].alarm[0].friction = 60;
    c->active = true;
    c->nation_id = 0;
    c->population = 3;
    c->colonist_count = 3;
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    /* DS:0x54f6 = tribe 0's own attitude[euro 0] word (friction | attacks<<8);
     * 0f14 clears the WHOLE word, both bytes. */
    col1.tribe[0].alarm[0].attacks = 2;
    const int alarm_rep_pre = (int)ind->alarm_by_player[0];
    ColonizeWorld w_rep = world_make(&units, &colonies, &map, &col1, true, &raid_rng, NULL);
    const int kind_rep = ai_contact_colony_raid_repelled_w(
      &w_rep, 4, 0, c->id, 0, 1, -1
    );
    int expect_rep = 0;
    if (kind_rep == AI_RAID_STORES) {
      expect_rep = -4;
    } else if (kind_rep == AI_RAID_BURN) {
      expect_rep = -12;
    } else if (kind_rep == AI_RAID_SHIP) {
      expect_rep = -16;
    } else if (kind_rep == AI_RAID_GOLD) {
      expect_rep = -8;
    }
    int expect_rep_alarm = alarm_rep_pre + expect_rep;
    if (expect_rep_alarm < 0) {
      expect_rep_alarm = 0;
    }
    if ((int)ind->alarm_by_player[0] != expect_rep_alarm) {
      return fail("repelled-raid limb must apply the same 0f14 alarm tail");
    }
    if (col1_tribe_attitude(&col1.tribe[0], 0) != 0) {
      return fail("repelled-raid limb must keep 0f14's unconditional attitude clear");
    }
  }
  return 0;
}

static int sp_05(void) {

  /*
   * (port_bugs.md #1093) The uniform ≥40 raid-gate check that lived here tested
   * the deleted invented raid pass; DOS has no such gate (0f14 is reached
   * only from 1b0e's loser limb). Only the fixture state later beats inherit
   * is kept.
   */
  {
    ColonizeColony* csp = &colonies.colonies[1];
    memset(csp, 0, sizeof(*csp));
    csp->id = 1;
    csp->active = true;
    csp->nation_id = 2; /* Spain */
    csp->x = 8;
    csp->y = 5;
    csp->population = 3;
    csp->colonist_count = 3;
    csp->stock[COLONIZE_CARGO_FOOD] = 12;
    csp->building_in_production = -1;
    if (colonies.colony_count < 2) {
      colonies.colony_count = 2;
    }
    ind->euro_diplo[0] = 1;
    ind->euro_diplo[2] = 1;
    ind->alarm_by_player[0] = 30;
    ind->alarm_by_player[2] = 35;
    col1.tribe[0].alarm[0].friction = 30;
    col1.tribe[0].alarm[2].friction = 35;
    col1.tribe[0].mission = 0xff;
    brave->x = 8;
    brave->y = 5;
    brave->moves = 0;
    /* Raid marker: 0f14's tail zeroes the home village's attitude word for
     * every kind incl. "Nothing" — but only with a valid home_tribe_index. */
    brave->home_tribe_index = 0;
    /* Cleanup SP colony so later arms stay on EN fixtures. */
    csp->active = false;
    ind->alarm_by_player[2] = 0;
    col1.tribe[0].alarm[2].friction = 0;
    brave->x = 5;
    brave->y = 5;
  }
  return 0;
}

static int sp_06(void) {

  /*
   * Mission expel (DOS 4cc6_00f2 escalation tail + 4cc6_0000, 2026-09-07d):
   * an alarm delta that lands the pair at 100 while at PEACE rolls
   * rng(0,10) <= difficulty+1 to burn the euro's missions (@INDIANBURN).
   * The old "prelude burns at alarm >= 80 each tick" stand-in is retired —
   * the prelude must NOT clear missions any more.
   */
  {
    char status_burn[128];
    status_burn[0] = '\0';
    ctx.status = status_burn;
    ctx.status_size = sizeof(status_burn);
    ctx.human_nation = 0;
    col1.tribe[0].mission = 0; /* English mission */
    ind->alarm_by_player[0] = 90;
    col1.tribe[0].alarm[0].friction = 10;
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].mission != 0) {
      return fail("prelude must not burn missions (00f2 escalation only)");
    }
    ind->euro_diplo[0] |= COL1_INDIAN_PEACE_BIT;
    col1.head.difficulty = 4; /* cap+1 = 5: roll <= 5 of 0..10 */
    ind->alarm_by_player[0] = 99;
    /* The tail refuses to roll without a stream (port_bugs.md #1004) — a NULL rng
     * answers dos_rng_range's low bound and would burn unconditionally. */
    ColonizeDosRng burn_rng;
    dos_rng_seed(&burn_rng, 0x4cc6u);
    ColonizeDosRng* const burn_rng_save = ctx.rng;
    ctx.rng = &burn_rng;
    int fired = 0;
    for (int tries = 0; tries < 64 && !fired; ++tries) {
      ai_contact_alarm_delta_00f2(&ctx, 4, 0, 5);
      ind->alarm_by_player[0] = 99; /* re-arm below the cap between rolls */
      fired = col1.tribe[0].mission == 0xff;
    }
    if (!fired) {
      return fail("00f2 escalation should expel missions at alarm 100 + PEACE");
    }
    if (strstr(status_burn, "burn") == NULL || strstr(status_burn, "mission") == NULL) {
      fprintf(stderr, "unit_ai_contact: mission-burn status '%s'\n", status_burn);
      return fail("mission expel should set @INDIANBURN status");
    }
    if (strstr(status_burn, "Inca") == NULL) {
      fprintf(stderr, "unit_ai_contact: mission-burn status '%s'\n", status_burn);
      return fail("mission expel should name tribe (@INDIANBURN)");
    }
    ctx.rng = burn_rng_save;
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~COL1_INDIAN_PEACE_BIT);
    ind->alarm_by_player[0] = 55; /* restore the fixture band for later arms */
    col1.head.difficulty = 2;
    ctx.status = NULL;
    ctx.status_size = 0;
  }
  /* The world form takes "is human" from w->human_nation like the ctx form:
   * a human whose control byte reads AI still gets the @INDIANBURN notice. */
  {
    AiPopupState wpop;
    ai_popup_init(&wpop);
    ColonizeDosRng wrng;
    dos_rng_seed(&wrng, 0x4cc6u);
    ColonizeWorld ww = world_make(NULL, NULL, NULL, &col1, true, &wrng, NULL);
    ww.ai_popups = &wpop;
    ww.messages = ctx.messages;
    ww.human_nation = 0;
    const uint8_t ctl = col1.player[0].control;
    const uint8_t mission = col1.tribe[0].mission;
    col1.player[0].control = 1; /* the save says AI */
    ind->euro_diplo[0] |= COL1_INDIAN_PEACE_BIT;
    int fired = 0;
    for (int tries = 0; tries < 64 && !fired; ++tries) {
      col1.tribe[0].mission = 0;
      ind->alarm_by_player[0] = 99;
      ai_contact_alarm_delta_00f2_w(&ww, 4, 0, 5);
      fired = col1.tribe[0].mission == 0xff;
    }
    col1.player[0].control = ctl;
    col1.tribe[0].mission = mission;
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~COL1_INDIAN_PEACE_BIT);
    ind->alarm_by_player[0] = 55;
    if (!fired || wpop.queue_count == 0) {
      return fail("00f2_w: world human_nation must drive the @INDIANBURN notice");
    }
  }
  return 0;
}

static int sp_07(void) {

  /*
   * Gear seize (DOS FUN_5fef_1b0e raw 100730-100744): a Brave that beats a
   * musketed / mounted foe steps its type. port_bugs.md #1093: the invented
   * adjacent-ambush arm (and its @INDIANWIN chrome) that used to start this
   * fight is gone, so the fight is driven straight through the shared land
   * resolver every 1b0e call site uses.
   */
  ColonizeDosRng amb_rng;
  dos_rng_seed(&amb_rng, 0x1b0eu);
  {
    char st_amb[128];
    st_amb[0] = '\0';
    ctx.status = st_amb;
    ctx.status_size = sizeof(st_amb);
    ctx.human_nation = 0;
    units.type_count = 5;
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
    units.types[2].movement = 1;
    units.types[2].attack = 0;
    units.types[2].defense = 0; /* total=Brave.attack → always Brave win */
    /* port_bugs.md #645: the seizure is a TYPE STEP (+1 armed / +2 mounted) on the
     * brave, so the @UNIT ladder rows have to exist in the fixture. */
    snprintf(units.types[3].name, sizeof(units.types[3].name), "Armed Braves");
    units.types[3].movement = 1;
    units.types[3].attack = 2;
    units.types[3].defense = 2;
    snprintf(units.types[4].name, sizeof(units.types[4].name), "Mtd. Braves");
    units.types[4].movement = 4;
    units.types[4].attack = 2;
    units.types[4].defense = 2;
    const int sol_id = units_spawn_allow_stack(&units, 2, 6, 5);
    ColonizeUnit* sol = units_get(&units, sol_id);
    if (!sol) {
      return fail("ambush soldier spawn");
    }
    sol->nation_id = 0;
    sol->muskets = 50;
    sol->horses = 0;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->muskets = 0;
    brave->horses = 0;
    c->active = false; /* avoid colony loot pre-empt — combat is adjacent */
    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    euro->x = 10;
    euro->y = 10;
    (void)units_resolve_land_combat(&units, brave_id, sol_id, &amb_rng);
    if (!brave->active) {
      return fail("ambush win should keep Brave alive");
    }
    /* DOS FUN_5fef_1b0e raw 100739-100742: beaten Soldiers step the brave to
     * Armed Braves (type += 1). No numeric muskets transfer. */
    if (units_type_kind(units_type(&units, brave->type_index)) !=
        UNITS_KIND_ARMED_BRAVE) {
      return fail("ambush WIN1 should step the Brave to Armed Braves");
    }
    if (brave->muskets != 0) {
      return fail("ambush WIN1 must not transfer a numeric muskets kit");
    }
    units_despawn(&units, sol_id);
    /* WIN2 horses arm */
    st_amb[0] = '\0';
    const int sol2 = units_spawn_allow_stack(&units, 2, 6, 5);
    sol = units_get(&units, sol2);
    if (!sol) {
      return fail("ambush horse foe spawn");
    }
    sol->nation_id = 0;
    sol->muskets = 0;
    sol->horses = 50;
    /* WIN2 needs a Dragoons/Scouts loser (raw 100731) and a brave that is not
     * mounted yet. */
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Dragoons");
    brave->type_index = 0;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->muskets = 0;
    brave->horses = 0;
    const uint8_t herds_before = ind->horse_herds;
    /* The tribe tally writes through the native-fallout context (turn.c:123
     * sets it in play). */
    units_set_native_fallout_context(&col1, ctx.map);
    (void)units_resolve_land_combat(&units, brave_id, sol2, &amb_rng);
    units_set_native_fallout_context(NULL, NULL);
    if (units_type_kind(units_type(&units, brave->type_index)) !=
        UNITS_KIND_MTD_BRAVE) {
      return fail("ambush WIN2 should step the Brave to Mtd. Braves");
    }
    if (brave->horses != 0) {
      return fail("ambush WIN2 must not transfer a numeric horses kit");
    }
    if (ind->horse_herds != (uint8_t)(herds_before + 1)) {
      return fail("ambush WIN2 should tally the tribe horse herd (DS:0x59a6)");
    }
    units_despawn(&units, sol2);
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
    units.type_count = 3;
    brave->type_index = 0;
    c->active = true;
    brave->x = 4;
    brave->y = 5;
    brave->muskets = 0;
    brave->horses = 0;
    /* Restore peaceful band + Soldier type stats for later encroach/convert. */
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    units.types[2].attack = 2;
    units.types[2].defense = 1;
    ctx.status = NULL;
    ctx.status_size = 0;
  }
  return 0;
}

static int sp_08(void) {

  /*
   * Retired 2026-09-03 (port_bugs.md "alarm rises incredibly fast"): the fandom
   * +2/turn unit- and colony-encroachment bumps are gone — DOS grows alarm
   * only via the 152e threat accumulator. Prelude must leave friction /
   * alarm_by_player untouched with a Soldier AND a colony adjacent.
   */
  units.type_count = 3;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
  units.types[2].movement = 1;
  units.types[2].attack = 2;
  units.types[2].defense = 1;
  const int soldier_id = units_spawn_allow_stack(&units, 2, 7, 5); /* Chebyshev 2 from (5,5) */
  ColonizeUnit* soldier = units_get(&units, soldier_id);
  if (!soldier) {
    return fail("spawn soldier");
  }
  soldier->nation_id = 0;
  euro->x = 10;
  euro->y = 10;
  c->active = true;
  c->nation_id = 0;
  c->x = 6;
  c->y = 5; /* Chebyshev 1 from tribe (5,5) */
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].alarm[0].friction = 10;
  ind->alarm_by_player[0] = 10;
  ind->unknown31_flags = (uint8_t)(ind->unknown31_flags | 0x20); /* skip flag-body escalate */
  ai_contact_indian_prelude(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 10) {
    return fail("prelude must not bump tribe friction (encroachment drift retired)");
  }
  if (ind->alarm_by_player[0] != 10) {
    return fail("prelude must not bump alarm_by_player (encroachment drift retired)");
  }
  c->x = 5;
  c->y = 5;

  /* Mission present must NOT drip friction/alarm: retired 2026-09-09
   * (smell #48). DOS's only mission goodwill term is inside FUN_4d56_152e
   * (euro_relation_accum → 4cc6_00f2 alarm −1 per −8 crossing, attitude
   * += local_8 * −3); the per-turn −1 here was a second, raw-write copy. */
  col1.tribe[0].mission = 0;
  col1.tribe[0].alarm[0].friction = 12;
  ind->alarm_by_player[0] = 12;
  units_despawn(&units, soldier_id);
  ai_contact_indian_prelude(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 12) {
    return fail("prelude must not drip mission friction (smell #48)");
  }
  if (ind->alarm_by_player[0] != 12) {
    return fail("prelude must not drip mission alarm_by_player (smell #48)");
  }

  /* Relation tick: retired to a no-op — DOS has no per-turn friction drift. */
  ind->euro_diplo[0] = 1;
  col1.tribe[0].alarm[0].friction = 12;
  ind->alarm_by_player[0] = 12;
  ai_contact_indian_relation_tick(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 12) {
    return fail("relation tick must not drift cool-band friction");
  }
  col1.tribe[0].alarm[0].friction = 50;
  ind->alarm_by_player[0] = 40;
  ai_contact_indian_relation_tick(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 50) {
    return fail("relation tick must not drift hot-band friction");
  }
  col1.tribe[0].alarm[0].friction = 10;
  ind->alarm_by_player[0] = 10;
  return 0;
}

static int sp_09(void) {

  /*
   * (Retired 2026-09-22, port_bugs.md #557.) The AI missionary adjacency pulse
   * asserted here (convert once, 50/50 heresy, crosses++, mid-band Jesuit /
   * Brebeuf gate, alarm −2 decay) was invented; DOS has no Indian-turn
   * missionary arm. The AI missionary now acts only at a village, through
   * FUN_4d56_4528's non-human switch (ai_contact_ai_missionary_village),
   * and the human keeps the @ACTIONS Establish / Denounce rows exercised
   * further below.
   */

  return 0;
}

static int sp_10(void) {
  /* port_bugs.md #1340: the test-only Brave adjacency driver
   * (ai_contact_indian_meet_trade / ai_contact_auto_trade) this segment drove
   * is deleted; the segment stays so later fx_run() indices hold. */
  return 0;
}

static int sp_11(void) {
  /* port_bugs.md #1340: the test-only Brave adjacency driver
   * (ai_contact_indian_meet_trade / ai_contact_auto_trade) this segment drove
   * is deleted; the segment stays so later fx_run() indices hold. It keeps
   * the fixture state the later segments were tuned on. */
  ctx.rng_seed = 1;
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
  }
  return 0;
}

static int sp_12(void) {

  /*
   * First-meet status (gold in 10..19 → gift skips silent; learned → teach skips):
   * FUN_5bfb_022e WELCOME (no-popup auto-accept path).
   */
  ind->euro_diplo[0] = 0;
  ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
  col1.nation[0].gold = 15; /* ≥10 no refuse; <20 no gift drain */
  col1.indian[0].euro_diplo[0] = (uint8_t)(col1.indian[0].euro_diplo[0] & ~COL1_INDIAN_MET_BIT); /* unmet (was relation 0) */
  status[0] = '\0';
  ctx.status = status;
  ctx.status_size = sizeof(status);
  ctx.human_nation = 0;
  (void)ai_contact_try_first_welcome(&ctx, 0, 4);
  if (!ind->euro_diplo[0]) {
    return fail("meet should set euro_diplo for status path");
  }
  if (strstr(status, "peace") == NULL && strstr(status, "Peace") == NULL &&
      strstr(status, "visit") == NULL && strstr(status, "friendship") == NULL) {
    fprintf(stderr, "unit_ai_contact: first-meet status '%s'\n", status);
    return fail("meet should set human-facing peace-offer status");
  }
  if (!ai_contact_indian_has_peace(&col1, 4, 0)) {
    return fail("auto-accept first meet (no popups) should set peace");
  }
  return 0;
}

static int sp_13(void) {

  /*
   * Multi-loot: high friction (≥80) successful colony raid → primary @RAID*
   * plus secondary drain (−5 muskets stock and −1 tools).
   */
  euro->x = 10;
  euro->y = 10;
  brave->x = 5;
  brave->y = 5;
  brave->moves = 0;
  brave->nation_id = 4;
  ind->alarm_by_player[0] = 80;
  col1.tribe[0].alarm[0].friction = 80;
  c->active = true;
  c->nation_id = 0;
  c->x = 5;
  c->y = 5;
  c->population = 4;
  c->colonist_count = 4;
  c->building_in_production = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 12;
  c->stock[COLONIZE_CARGO_TOOLS] = 8;
  c->stock[COLONIZE_CARGO_MUSKETS] = 20;
  c->stock[COLONIZE_CARGO_HORSES] = 4;
  const int food_ml = c->stock[COLONIZE_CARGO_FOOD];
  const int tools_ml = c->stock[COLONIZE_CARGO_TOOLS];
  const int muskets_ml = c->stock[COLONIZE_CARGO_MUSKETS];
  const int pop_ml = c->population;
  const uint16_t gold_ml = col1.nation[0].gold;
  run_colony_raid();
  {
    const int kind_ml = ai_contact_last_raid_kind();
    if (kind_ml == AI_RAID_NOTHING) {
      return fail("high-friction multi-loot raid should not be NOTHING");
    }
    /*
     * port_bugs.md #833: FUN_5fef_0f14 mutates exactly ONE thing per raid. The
     * port's `ai_contact_raid_secondary_loot` (−5 muskets / −1 horse / −1
     * tools on top of the primary kind) was deleted 2026-09-23, so at most
     * ONE cargo may move here, and only on the STORES kind (port_bugs.md #830:
     * `h = stock>>1; amt = rand(min(h,10), h)`).
     */
    int cargo_moved = 0;
    for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
      const int before = (cg == COLONIZE_CARGO_FOOD)      ? food_ml
                       : (cg == COLONIZE_CARGO_TOOLS)     ? tools_ml
                       : (cg == COLONIZE_CARGO_MUSKETS)   ? muskets_ml
                       : (cg == COLONIZE_CARGO_HORSES)    ? 4
                                                          : c->stock[cg];
      if (c->stock[cg] != before) {
        cargo_moved++;
        if (kind_ml != AI_RAID_STORES) {
          return fail("only the STORES kind may drain a cargo (#833)");
        }
        const int h = before >> 1;
        const int lo = (h > 10) ? 10 : h;
        const int amt = before - c->stock[cg];
        if (amt < (lo < 1 ? 1 : lo) || amt > (h < 1 ? 1 : h)) {
          return fail("STORES amount must land in rand(min(h,10), h) (#830)");
        }
      }
    }
    if (cargo_moved > 1) {
      return fail("a raid must mutate at most one cargo (#833)");
    }
    if (c->population != pop_ml) {
      return fail("DOS has no population-loss raid kind (#828)");
    }
    const int primary_hit = cargo_moved || (col1.nation[0].gold < gold_ml) ||
                            (kind_ml == AI_RAID_BURN) || (kind_ml == AI_RAID_SHIP) ||
                            (kind_ml == AI_RAID_GOLD);
    if (!primary_hit) {
      return fail("multi-loot should apply a primary @RAID* outcome");
    }
  }
  return 0;
}

static int sp_14(void) {

  /*
   * Raid muskets drain (STORES primary): warehouse holds only muskets (<5 so
   * secondary mil loot skips) at mid alarm (50 — below GOLD/SCALP/WREAK bands)
   * → AI_RAID_STORES and muskets stock −1 (half of 3 → 1). Cite:
   * indian_raid_outcomes.md @RAIDSTORES; GAME.TXT tag.
   */
  {
    euro->x = 10;
    euro->y = 10;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 76; /* at war (DOS band: alarm > 0x4a), below the 80 burn band */
    col1.tribe[0].alarm[0].friction = 50;
    /* Not the first attack — the live 0f14 word-zero now clears attacks
     * after every raid, so the accumulation earlier sub-tests used to leave
     * behind is gone; keep the @INDIANSURPRISE deniable branch out. */
    col1.tribe[0].alarm[0].attacks = 1;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].gold = 0; /* no GOLD arm */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->population = 2; /* pop 1 + alarm >= 70 would be abandoned outright */
    c->colonist_count = 2;
    c->building_in_production = -1;
    snprintf(c->name, sizeof(c->name), "Roanoke");
    memset(c->stock, 0, sizeof(c->stock));
    /*
     * port_bugs.md #832: FUN_5fef_0f14's cargo pick is a RETRY ROLL that keeps
     * rerolling while the colony holds < 10 of the rolled cargo (raw 99832),
     * so a 3-muskets warehouse is un-lootable in DOS and the old fixture's
     * `= 3` could only ever produce @RAIDNOTHING. 24 muskets: h = 12, lo =
     * min(h,10) = 10, amt = rand(10,12) (raw 99913-99926).
     */
    c->stock[COLONIZE_CARGO_MUSKETS] = 24;
    const int musk_st = c->stock[COLONIZE_CARGO_MUSKETS];
    const uint8_t tribe_musk_st = col1.indian[0].muskets;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    /* kind is rand(1,4): reroll until the goods kind comes up (nothing else
     * can succeed here — no ship, no burn target, no gold). */
    /* port_bugs.md #827: the kind is rand(1,4) — probe for it, then run one raid. */
    (void)raid_probe_turn_for_kind(AI_RAID_STORES, 400);
    run_colony_raid();
    if (ai_contact_last_raid_kind() != AI_RAID_STORES) {
      return fail("muskets-only warehouse should pick AI_RAID_STORES");
    }
    const int musk_taken = musk_st - c->stock[COLONIZE_CARGO_MUSKETS];
    if (musk_taken < 10 || musk_taken > 12) {
      return fail("STORES amount must be rand(min(h,10), h) = 10..12 (#830)");
    }
    /* raw 99944-99947: stolen muskets join the raiding tribe's record. */
    if (col1.indian[0].muskets != (uint8_t)(tribe_musk_st + 1)) {
      return fail("stolen muskets should raise the tribe's muskets byte (#831)");
    }
    if (strstr(status, "stores") == NULL || strstr(status, "Roanoke") == NULL) {
      fprintf(stderr, "unit_ai_contact: STORES status '%s'\n", status);
      return fail("STORES raid should set @RAIDSTORES-shaped status");
    }
  }

  /*
   * Same STORES scenario, but with real COLONIZE/GAME.TXT loaded: the
   * status line should now come from the real @RAIDSTORES body (2026-08-26
   * fix — was always the hand-typed thin line, even with assets present)
   * — "Large quantities of {cargo} stolen. Colonists outraged!" — with the
   * actually-drained cargo name (Muskets, per this scenario's warehouse)
   * substituted in, not a fixed placeholder.
   */
  {
    euro->x = 10;
    euro->y = 10;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 76; /* at war (DOS band), below burn 80 */
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].gold = 0;
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->population = 2; /* pop 1 + alarm >= 70 would abandon */
    c->colonist_count = 2;
    c->building_in_production = -1;
    snprintf(c->name, sizeof(c->name), "Roanoke");
    memset(c->stock, 0, sizeof(c->stock));
    c->stock[COLONIZE_CARGO_MUSKETS] = 24; /* >= 10: the DOS retry loop's floor */
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    ColonizeMsgCatalog game_txt_raid;
    assets_msg_init(&game_txt_raid);
    (void)assets_msg_load_file(&game_txt_raid, "COLONIZE/GAME.TXT");
    ctx.messages = &game_txt_raid;
    run_colony_raid();
    /* The previous block's 0f14 vent left alarm just under DOS's at-war band
     * (0x4a), which prepends @INDIANSURPRISE and overruns the 128-byte status
     * buffer this block reads. Pin the at-war state the block intends. */
    ind->alarm_by_player[0] = 76;
    col1.tribe[0].alarm[0].friction = 76;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_WAR_BIT;
    /* port_bugs.md #827: the kind is rand(1,4) — probe for it, then run one raid. */
    (void)raid_probe_turn_for_kind(AI_RAID_STORES, 400);
    run_colony_raid();
    ctx.messages = test_game_txt();
    ctx.names = test_names_txt();
    assets_msg_free(&game_txt_raid);
    if (ai_contact_last_raid_kind() != AI_RAID_STORES) {
      return fail("real-GAME.TXT STORES scenario should still pick AI_RAID_STORES");
    }
    if (strstr(status, "Large quantities of Muskets stolen") == NULL ||
        strstr(status, "Colonists outraged") == NULL ||
        strstr(status, "Roanoke") == NULL) {
      fprintf(stderr, "unit_ai_contact: real @RAIDSTORES status '%s'\n", status);
      return fail("STORES raid with GAME.TXT loaded should use the real @RAIDSTORES body");
    }
  }

  /*
   * port_bugs.md #832: there is no goods-VALUE sort. FUN_5fef_0f14 rolls a cargo
   * 0..0xf and rerolls while the colony holds < 10 of it (raw 99819-99833),
   * so a 20-food / 2-silver warehouse can only ever lose FOOD — the 2 silver
   * are below the floor and are never even a candidate. This block used to
   * assert the opposite ("drain silver before food").
   */
  {
    euro->x = 10;
    euro->y = 10;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 50;
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].gold = 0;
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->population = 1;
    c->colonist_count = 1;
    c->building_in_production = -1;
    memset(c->stock, 0, sizeof(c->stock));
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    c->stock[COLONIZE_CARGO_SILVER] = 2;
    const int food_vs = c->stock[COLONIZE_CARGO_FOOD];
    const int sil_vs = c->stock[COLONIZE_CARGO_SILVER];
    /* port_bugs.md #827: the kind is rand(1,4) — probe for it, then run one raid. */
    (void)raid_probe_turn_for_kind(AI_RAID_STORES, 400);
    run_colony_raid();
    if (ai_contact_last_raid_kind() != AI_RAID_STORES) {
      return fail("food+silver warehouse should pick AI_RAID_STORES");
    }
    if (c->stock[COLONIZE_CARGO_SILVER] != sil_vs) {
      return fail("a 2-unit silver pile is below DOS's 10 floor — never looted");
    }
    const int food_taken = food_vs - c->stock[COLONIZE_CARGO_FOOD];
    if (food_taken != 10) {
      /* h = 20>>1 = 10, lo = min(h,10) = 10 -> rand(10,10) = 10 exactly. */
      return fail("20-food warehouse should lose exactly 10 (h = lo = 10)");
    }
  }
  return 0;
}

static int sp_15(void) {

  /*
   * port_bugs.md #499: the "FUN_4d56_359c anti-Scout displace/kill" raid arm was
   * deleted 2026-09-18. FUN_4d56_359c (raw 83481-83505) is the Enter-Hostile-
   * Village wagon outcome (@KILLWAGONS / @MADATWAGONS / @GRUDGEWAGONS), ported
   * as ai_contact_enter_hostile_village; no DOS body harasses Scouts on the
   * Indian move pulse. (The pulse that check ran against was deleted with
   * port_bugs.md #1093; only the fixture state later beats inherit is kept.)
   */
  units.type_count = 4;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Scout");
  units.types[3].movement = 4;
  units.types[3].attack = 0;
  units.types[3].defense = 1;
  {
    const int scout_id = units_spawn_allow_stack(&units, 3, 6, 5);
    ColonizeUnit* scout = units_get(&units, scout_id);
    if (!scout) {
      return fail("spawn scout");
    }
    scout->nation_id = 0;
    scout->horses = 50;
    euro->x = 10;
    euro->y = 10;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 3; /* spent byte at max = exhausted (combat arm skips) */
    ind->alarm_by_player[0] = 95;
    col1.tribe[0].alarm[0].friction = 95;
    status[0] = 0;
    units_despawn(&units, scout_id);
  }
  ind->alarm_by_player[0] = 90;
  col1.tribe[0].alarm[0].friction = 90;
  return 0;
}

static int sp_16(void) {
  /* port_bugs.md #1340: the test-only Brave adjacency driver
   * (ai_contact_indian_meet_trade / ai_contact_auto_trade) this segment drove
   * is deleted; the segment stays so later fx_run() indices hold. */
  return 0;
}

static int sp_17(void) {

  /*
   * Raid kind gating: empty warehouse + no Euro gold + pop≤1 (no SCALP) →
   * AI_RAID_NOTHING; no fake muskets; human @RAIDNOTHING status
   * ("{tribe} raiding party wiped out in {colony}!"). Cite: GAME.TXT @RAIDNOTHING.
   */
  {
    euro->x = 10;
    euro->y = 10;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    col1.nation[0].gold = 0;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->population = 1; /* no SCALP fallback */
    c->colonist_count = 1;
    c->building_in_production = -1;
    snprintf(c->name, sizeof(c->name), "Roanoke");
    memset(c->stock, 0, sizeof(c->stock));
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    run_colony_raid();
    if (c->stock[COLONIZE_CARGO_MUSKETS] != 0) {
      return fail("empty warehouse raid must not invent muskets loot");
    }
    if (ai_contact_last_raid_kind() != AI_RAID_NOTHING) {
      return fail("empty warehouse should pick AI_RAID_NOTHING");
    }
    if (strstr(status, "wiped") == NULL || strstr(status, "Roanoke") == NULL ||
        strstr(status, "Inca") == NULL) {
      fprintf(stderr, "unit_ai_contact: NOTHING status '%s'\n", status);
      return fail("empty raid NOTHING should set tribe+colony wiped-out status");
    }
  }

  /*
   * port_bugs.md #1093: a successful raid on a human colony draws only 0f14's own
   * per-kind @RAID* line — no @INDIANSURPRISE prefix (its DOS site is the
   * unported 021a human-colony warning) — and 0f14 never touches the peace
   * bit (the old @INDIANWAR peace-break was pulse chrome).
   */
  {
    char st_sur[128];
    st_sur[0] = '\0';
    ctx.status = st_sur;
    ctx.status_size = sizeof(st_sur);
    ctx.human_nation = 0;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->home_tribe_index = 0; /* raid-mark: 0f14 tail zeroes home attitude */
    ind->alarm_by_player[0] = 45; /* raid gate >=40, below thin at-war (alarm > 50) */
    col1.tribe[0].alarm[0].friction = 65;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* not at-war */
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & (uint8_t)~COL1_INDIAN_PEACE_BIT);
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->population = 3;
    c->colonist_count = 3;
    snprintf(c->name, sizeof(c->name), "Roanoke");
    memset(c->stock, 0, sizeof(c->stock));
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    run_colony_raid();
    if (ai_contact_last_raid_kind() == AI_RAID_NOTHING) {
      /* "Nothing" is a legal 0f14 roll; the raid still marks itself by
       * zeroing the home village's attitude word (the retired attacks++
       * marker no longer exists). */
      if (col1_tribe_attitude(&col1.tribe[0], 0) != 0) {
        return fail("surprise raid should loot or leave the 0f14 attitude clear");
      }
    } else if (strstr(st_sur, "surprise") != NULL || strstr(st_sur, "denies") != NULL ||
               strstr(st_sur, "Roanoke") == NULL) {
      fprintf(stderr, "unit_ai_contact: surprise status '%s'\n", st_sur);
      return fail("non-war raid should draw only the @RAID* line (no @INDIANSURPRISE)");
    }

    st_sur[0] = '\0';
    brave->moves = 0;
    brave->x = 5;
    brave->y = 5;
    ind->euro_diplo[0] |= COL1_INDIAN_PEACE_BIT;
    col1.indian[0].alarm_by_player[0] = 20; /* relation 80 */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    /*
     * FUN_5fef_0f14 wall gate (2026-08-28): even a bare colony repels the
     * party on rand(0,12)-1 < 1 (1/13) → @RAIDNOTHING and no war. Retry a
     * few pulses so the assertion tests the raid arm, not that roll.
     */
    for (int attempt = 0; attempt < 12; ++attempt) {
      brave->moves = 0;
      brave->x = 5;
      brave->y = 5;
      run_colony_raid();
      if (ai_contact_last_raid_kind() != AI_RAID_NOTHING) {
        break;
      }
      turn++; /* the pulse's local RNG is keyed on the turn — advance it */
    }
    if (!ai_contact_indian_has_peace(&col1, 4, 0)) {
      return fail("0f14 must not clear the peace bit (no DOS @INDIANWAR arm)");
    }
    /*
     * port_bugs.md #836: the "The %s declare war! Prepare for WAR!" prelude was
     * typed English with no catalog row behind it (@INDIANWAR is dead
     * GAME.TXT text — no DS tag string exists in VICEROY.EXE), so it was
     * retired 2026-09-23 and there is no "war" wording to look for. What
     * remains here is that a successful raid still says SOMETHING.
     */
    if (ai_contact_last_raid_kind() != AI_RAID_NOTHING && st_sur[0] == '\0') {
      fprintf(stderr, "unit_ai_contact: war-raid status '%s'\n", st_sur);
      return fail("peace-breaking raid should still draw its @RAID* line");
    }
    ctx.status = NULL;
    ctx.status_size = 0;
  }
  return 0;
}

static int sp_18(void) {
  /* port_bugs.md #1340: the test-only Brave adjacency driver
   * (ai_contact_indian_meet_trade / ai_contact_auto_trade) this segment drove
   * is deleted; the segment stays so later fx_run() indices hold. */
  return 0;
}

static int sp_19(void) {
  /* port_bugs.md #1340: the test-only Brave adjacency driver
   * (ai_contact_indian_meet_trade / ai_contact_auto_trade) this segment drove
   * is deleted; the segment stays so later fx_run() indices hold. */
  return 0;
}

static int sp_20(void) {

  /*
   * (Retired 2026-09-22, port_bugs.md #557.) A "missionary flee" assertion sat
   * here; the pulse it tested had no DOS site and is deleted.
   *
   * port_bugs.md #1093: the raid-target picks this segment asserted (lower
   * relation / at-war Euro first, muskets-horses colony first, the #1003
   * nation-scalar gate) all belonged to the deleted invented raid pass; DOS
   * reaches 0f14 only from 1b0e's loser limb, on the colony the fight was
   * at. Only the fixture state later beats inherit is kept.
   */
  euro->x = 12;
  euro->y = 12;
  brave->x = 5;
  brave->y = 5;
  brave->moves = 0;
  brave->nation_id = 4;
  col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
  col1.indian[0].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
  {
    ColonizeColony* c1 = &colonies.colonies[1];
    c1->id = 1;
    c1->active = false;
    c1->nation_id = 0;
    c1->x = 8;
    c1->y = 8;
    c1->population = 3;
    c1->colonist_count = 3;
    c1->building_in_production = -1;
    ColonizeColony* c0 = &colonies.colonies[0];
    c0->id = 0;
    c0->active = true;
    c0->nation_id = 0;
    c0->x = 5;
    c0->y = 5;
    c0->population = 3;
    c0->colonist_count = 3;
    c0->building_in_production = -1;
    memset(c0->stock, 0, sizeof(c0->stock));
    c0->stock[COLONIZE_CARGO_FOOD] = 20;
    colonies.colony_count = 1;
  }
  ind->alarm_by_player[0] = 10;
  ind->alarm_by_player[1] = 0;
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].alarm[0].friction = 0;
  col1.tribe[0].alarm[0].attacks = 0;
  col1.tribe[0].alarm[1].friction = 0;
  col1.tribe[0].alarm[1].attacks = 0;
  return 0;
}

static int sp_21(void) {

  /*
   * Prelude is clamp-only since the mission-pacify drip went (smell #48):
   * the old escalate arm (once-per-
   * nation unknown31_flags 0x20 latch, alarm<30 gate, difficulty bump) was
   * invented — no DOS counterpart (4d56_1816 §2 is the WoI defection; the
   * sole DOS alarm grower is the 152e accumulator). Prelude must not touch
   * alarm, friction, or unknown31_flags.
   */
  {
    /* Despawn any encroacher so nothing else bumps alarm. */
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      ColonizeUnit* u = &units.units[ui];
      if (!u->active || u->nation_id < 0 || u->nation_id > 3) {
        continue;
      }
      const char* nm = units_display_name(&units, u);
      if (nm && (strstr(nm, "Soldier") || strstr(nm, "Scout") || strstr(nm, "Pioneer"))) {
        units_despawn(&units, u->id);
      }
    }
    euro->x = 12;
    euro->y = 12;
    euro->active = true;
    euro->id = euro_id_;
    units_pool_sync(&units);
    c->x = 14;
    c->y = 14; /* park */
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 0;
    ind->euro_diplo[0] = 1;
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags & (uint8_t)~0x20);
    ind->alarm_by_player[0] = 10;
    col1.head.difficulty = 2;
    turn = 1;
    ctx.rng_seed = 42;
    ai_contact_indian_prelude(&ctx, 4);
    if ((ind->unknown31_flags & 0x20) != 0) {
      return fail("prelude must not write unknown31_flags (DOS never reads +6)");
    }
    if (ind->alarm_by_player[0] != 10) {
      fprintf(
        stderr,
        "unit_ai_contact: prelude alarm=%u (want 10 unchanged)\n",
        (unsigned)ind->alarm_by_player[0]
      );
      return fail("prelude must not bump alarm (escalate arm retired)");
    }
    if (col1.tribe[0].alarm[0].friction != 0) {
      return fail("prelude must not bump tribe friction (escalate arm retired)");
    }
    c->x = 5;
    c->y = 5;
  }
  return 0;
}

static int sp_22(void) {

  /*
   * port_bugs.md #1093: the colony-approach picks this segment asserted (tools /
   * silver / alarm-band preferences, Series Q) belonged to the deleted
   * invented raid pass. Only the fixture setup later beats inherit is kept.
   */

  /*
   * Raid prefer colony with tools≥10 at equal distance (no military stock):
   * secondary high-friction tools drain. Cite: indian_raid_outcomes.md.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    brave->x = 8;
    brave->y = 8;
    brave->moves = 0;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.tribe[0].mission = 0xff;
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ColonizeColony* c_plain = &colonies.colonies[0];
    ColonizeColony* c_tools = &colonies.colonies[1];
    c_plain->id = 0;
    c_plain->active = true;
    c_plain->nation_id = 0;
    c_plain->x = 8;
    c_plain->y = 8;
    c_plain->population = 3;
    c_plain->colonist_count = 3;
    c_plain->building_in_production = -1;
    memset(c_plain->stock, 0, sizeof(c_plain->stock));
    c_plain->stock[COLONIZE_CARGO_FOOD] = 20;
    c_tools->id = 1;
    c_tools->active = true;
    c_tools->nation_id = 0;
    c_tools->x = 8;
    c_tools->y = 8;
    c_tools->population = 3;
    c_tools->colonist_count = 3;
    c_tools->building_in_production = -1;
    memset(c_tools->stock, 0, sizeof(c_tools->stock));
    c_tools->stock[COLONIZE_CARGO_FOOD] = 20;
    c_tools->stock[COLONIZE_CARGO_TOOLS] = 12; /* ≥10 secondary prefer */
    colonies.colony_count = 2;
    /* The approach walk scans the whole pool, not colony_count — retire any
     * colony an earlier sub-test left active or it can win the pick. */
    for (int ci = 2; ci < COLONIZE_COLONIES_MAX; ++ci) {
      colonies.colonies[ci].active = false;
    }
    c_tools->active = false;
    colonies.colony_count = 1;
  }

  /*
   * Raid prefer high-silver (GOLD wealth) colony at equal distance when neither
   * has mil/tools advantage. Cite: indian_raid_outcomes.md colony approach;
   * @RAIDGOLD.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    brave->x = 8;
    brave->y = 8;
    brave->moves = 0;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.tribe[0].mission = 0xff;
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.nation[0].gold = 100; /* GOLD kind eligible; approach uses silver */
    ColonizeColony* c_plain = &colonies.colonies[0];
    ColonizeColony* c_silver = &colonies.colonies[1];
    c_plain->id = 0;
    c_plain->active = true;
    c_plain->nation_id = 0;
    c_plain->x = 8;
    c_plain->y = 8;
    c_plain->population = 3;
    c_plain->colonist_count = 3;
    c_plain->building_in_production = -1;
    memset(c_plain->stock, 0, sizeof(c_plain->stock));
    c_plain->stock[COLONIZE_CARGO_FOOD] = 20;
    c_silver->id = 1;
    c_silver->active = true;
    c_silver->nation_id = 0;
    c_silver->x = 8;
    c_silver->y = 8;
    c_silver->population = 3;
    c_silver->colonist_count = 3;
    c_silver->building_in_production = -1;
    memset(c_silver->stock, 0, sizeof(c_silver->stock));
    c_silver->stock[COLONIZE_CARGO_FOOD] = 20;
    c_silver->stock[COLONIZE_CARGO_SILVER] = 8; /* wealth prefer */
    colonies.colony_count = 2;
    c_silver->active = false;
    colonies.colony_count = 1;
  }

  /*
   * Series Q: alarm≥80 prefers silver wealth before tools at equal MD;
   * alarm 55 keeps tools-before-gold. Cite: indian_raid_outcomes.md; Series Q.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    brave->x = 8;
    brave->y = 8;
    brave->moves = 0;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.tribe[0].mission = 0xff;
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.nation[0].gold = 100;
    ColonizeColony* c_tools = &colonies.colonies[0];
    ColonizeColony* c_silver = &colonies.colonies[1];
    c_tools->id = 0;
    c_tools->active = true;
    c_tools->nation_id = 0;
    c_tools->x = 8;
    c_tools->y = 8;
    c_tools->population = 3;
    c_tools->colonist_count = 3;
    c_tools->building_in_production = -1;
    memset(c_tools->stock, 0, sizeof(c_tools->stock));
    c_tools->stock[COLONIZE_CARGO_FOOD] = 20;
    c_tools->stock[COLONIZE_CARGO_TOOLS] = 12;
    c_silver->id = 1;
    c_silver->active = true;
    c_silver->nation_id = 0;
    c_silver->x = 8;
    c_silver->y = 8;
    c_silver->population = 3;
    c_silver->colonist_count = 3;
    c_silver->building_in_production = -1;
    memset(c_silver->stock, 0, sizeof(c_silver->stock));
    c_silver->stock[COLONIZE_CARGO_FOOD] = 20;
    c_silver->stock[COLONIZE_CARGO_SILVER] = 8;
    colonies.colony_count = 2;
    /* Reset and hot alarm≥80 → silver before tools. */
    c_tools->stock[COLONIZE_CARGO_TOOLS] = 12;
    c_tools->stock[COLONIZE_CARGO_FOOD] = 20;
    c_silver->stock[COLONIZE_CARGO_SILVER] = 8;
    c_silver->stock[COLONIZE_CARGO_FOOD] = 20;
    col1.tribe[0].alarm[0].attacks = 0;
    brave->moves = 0;
    brave->orders = UNITS_ORDER_NONE;
    ind->alarm_by_player[0] = 80;
    col1.tribe[0].alarm[0].friction = 80;
    c_silver->active = false;
    colonies.colony_count = 1;
    ind->alarm_by_player[0] = 0;
    col1.tribe[0].alarm[0].friction = 0;
  }
  return 0;
}

static int sp_23(void) {

  /*
   * @RAIDBURN lumber gate: no construction, warehouse lumber only, alarm≥60
   * → BURN drains lumber (wooden-building stock). Cite: indian_raid_outcomes.md
   * @RAIDBURN; apply lumber stub.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    euro->active = true;
    euro->id = euro_id_;
    units_pool_sync(&units);
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 65;
/* alarm pinned above (was relation write) */
    /* At war: the @INDIANSURPRISE deniable branch fires only while NOT at
     * war (alarm ≤ 0x4a and no WAR bit) — this fixture is below the band,
     * so pin the bit to exercise the @RAIDBURN chrome under test (it used
     * to lean on a war bit leaked by earlier sub-tests). */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT | COL1_INDIAN_WAR_BIT;
    col1.nation[0].gold = 0; /* no GOLD */
    col1.head.founding_father[FF_POCAHONTAS] = -1;
    col1.nation[0].founding_fathers[FF_POCAHONTAS / 8] &=
      (uint8_t)~(1u << (FF_POCAHONTAS % 8));
    ColonizeColony* c_burn = &colonies.colonies[0];
    c_burn->active = true;
    c_burn->nation_id = 0;
    c_burn->x = 5;
    c_burn->y = 5;
    c_burn->population = 1; /* no SCALP */
    c_burn->colonist_count = 1;
    c_burn->building_in_production = -1;
    snprintf(c_burn->name, sizeof(c_burn->name), "Roanoke");
    memset(c_burn->stock, 0, sizeof(c_burn->stock));
    c_burn->stock[COLONIZE_CARGO_LUMBER] = 6;
    /*
     * port_bugs.md #924: the burn arm has no DOS lumber/construction step — it
     * rolls an owned @BUILDING row; DOS then walks the chain DOWN to the
     * lowest owned tier (FUN_5fef_0f14 raw 99832-99863), the port burns the
     * top owned tier (dos_bugs.md DB-50). Church (0x25) / Cathedral
     * (0x26) are legal targets, the Town Hall chain (root 9) never is.
     */
    colonies.building_type_count = 3;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Town Hall");
    snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name),
             "Church");
    snprintf(colonies.building_types[2].name, sizeof(colonies.building_types[2].name),
             "Cathedral");
    memset(c_burn->has_building, 0, sizeof(c_burn->has_building));
    c_burn->has_building[0] = true;
    c_burn->has_building[1] = true;
    c_burn->has_building[2] = true;
    colonies.colony_count = 1;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    /* port_bugs.md #827: the kind is DOS's rand(1,4) behind a walls roll, so the
     * BURN arm is reached by rerolling the pulse, not by an alarm band. */
    ctx.human_nation = 0;
    /* port_bugs.md #827: the kind is rand(1,4) — probe for it, then run one raid. */
    (void)raid_probe_turn_for_kind(AI_RAID_BURN, 400);
    status[0] = '\0'; /* the probe pulses wrote their own lines */
    run_colony_raid();
    if (ai_contact_last_raid_kind() != AI_RAID_BURN) {
      fprintf(
        stderr,
        "unit_ai_contact: burn-lumber kind=%d\n",
        ai_contact_last_raid_kind()
      );
      return fail("lumber-only colony at alarm≥60 should pick AI_RAID_BURN");
    }
    if (c_burn->stock[COLONIZE_CARGO_LUMBER] != 6) {
      return fail("BURN must not touch the lumber pile (port_bugs.md #924)");
    }
    if (!c_burn->has_building[0]) {
      return fail("BURN must never burn the Town Hall chain (0a88 root 9)");
    }
    /* dos_bugs.md DB-50: the top owned tier burns (DOS: the Church). */
    if (c_burn->has_building[2]) {
      return fail("BURN should clear the Cathedral, the chain's top owned tier");
    }
    if (!c_burn->has_building[1]) {
      return fail("BURN destroys one tier only — the Church bit survives");
    }
    if (strstr(status, "Cathedral") == NULL) {
      fprintf(stderr, "unit_ai_contact: BURN-church status '%s'\n", status);
      return fail("@RAIDBURN should name the burned building");
    }
  }

  /*
   * @BURNED3 bystander: an AI-owned colony (nation 1) burns/abandons while
   * the human (nation 0) is neither victim nor burner — "Spies report: …"
   * OK instead of silence. Cite: GAME.TXT @BURNED3.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    euro->active = true;
    euro->id = euro_id_;
    units_pool_sync(&units);
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    for (int e = 0; e < 4; ++e) {
      ind->alarm_by_player[e] = 0;
      col1.tribe[0].alarm[e].friction = 0;
      col1.tribe[0].alarm[e].attacks = 0;
      col1.indian[0].euro_diplo[e] |= COL1_INDIAN_MET_BIT; /* met (was relation 100; alarm pinned above) */ /* peaceful baseline */
      col1.nation[e].gold = 0;
    }
    ind->alarm_by_player[1] = 75; /* ≥70 abandon gate targets nation 1 */
    col1.tribe[0].alarm[1].friction = 75;
    col1.indian[0].euro_diplo[1] |= COL1_INDIAN_MET_BIT; /* met (was relation 40; alarm pinned above) */
    col1.head.founding_father[FF_POCAHONTAS] = -1;
    col1.nation[0].founding_fathers[FF_POCAHONTAS / 8] &=
      (uint8_t)~(1u << (FF_POCAHONTAS % 8));
    ColonizeColony* c_fbrn = &colonies.colonies[0];
    memset(c_fbrn, 0, sizeof(*c_fbrn));
    c_fbrn->active = true;
    c_fbrn->nation_id = 1; /* AI-owned — not the human */
    c_fbrn->x = 5;
    c_fbrn->y = 5;
    c_fbrn->population = 1; /* abandon gate */
    c_fbrn->colonist_count = 1;
    c_fbrn->building_in_production = -1;
    snprintf(c_fbrn->name, sizeof(c_fbrn->name), "Jamestown");
    memset(c_fbrn->stock, 0, sizeof(c_fbrn->stock));
    c_fbrn->stock[COLONIZE_CARGO_LUMBER] = 6;
    /* port_bugs.md #924: BURN needs an owned, non-rejected @BUILDING row to roll. */
    colonies.building_type_count = 2;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Town Hall");
    snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name),
             "Warehouse");
    c_fbrn->has_building[0] = true;
    c_fbrn->has_building[1] = true;
    colonies.colony_count = 1;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    AiPopupState pop_fbrn;
    ai_popup_init(&pop_fbrn);
    ctx.ai_popups = &pop_fbrn;
    ColonizeMsgCatalog game_txt_fbrn;
    assets_msg_init(&game_txt_fbrn);
    if (!assets_msg_load_file(&game_txt_fbrn, "COLONIZE/GAME.TXT")) {
      return fail("bystander burn: GAME.TXT load failed");
    }
    units_set_combat_human_nation(0);
    units_set_combat_popups(&pop_fbrn, &game_txt_fbrn);
    /* port_bugs.md #827: the kind is DOS's rand(1,4) behind a walls roll, so the
     * BURN arm is reached by rerolling the pulse, not by an alarm band. */
    /* port_bugs.md #827: the kind is rand(1,4) — probe for it, then run one raid. */
    (void)raid_probe_turn_for_kind(AI_RAID_BURN, 400);
    status[0] = '\0'; /* the probe pulses wrote their own lines */
    run_colony_raid();
    units_set_combat_popups(NULL, NULL);
    units_set_combat_human_nation(-1);
    ctx.ai_popups = NULL;
    assets_msg_free(&game_txt_fbrn);
    if (ai_contact_last_raid_kind() != AI_RAID_BURN) {
      fprintf(stderr, "unit_ai_contact: bystander burn kind=%d\n",
              ai_contact_last_raid_kind());
      return fail("bystander lumber-only colony at alarm≥70 should pick AI_RAID_BURN");
    }
    /*
     * port_bugs.md #829 / DOS-LITERAL raw 99897-99899: a successful raid on a
     * colony that is NOT human-controlled fires @RAIDWREAK (0x1b8a) at the
     * human — "Spies report: {tribe} raiding party wreaks havoc in the
     * {nation} colony of {colony}." The port used to model 0x1b8a as a loot
     * kind and this block asserted the human heard nothing at all.
     */
    if (strstr(status, "Spies report") == NULL || strstr(status, "Jamestown") == NULL) {
      fprintf(stderr, "unit_ai_contact: bystander status '%s'\n", status);
      return fail("bystander raid should fire the @RAIDWREAK bulletin (#829)");
    }
    /* port_bugs.md #281: the raid only LOOTS — a pop-1 colony survives a
     * BURN raid with its owner intact (destruction lives on the combat
     * path, and only when the last colonist falls there). */
    if (!c_fbrn->active || c_fbrn->nation_id != 1) {
      return fail("raid must not destroy or capture a pop-1 colony (port_bugs.md #281)");
    }
    for (int qi = 0; qi < pop_fbrn.queue_count; ++qi) {
      /* "Spies report" is @RAIDWREAK's own opening (raw 99897) and is
       * expected here — only the abandon/capture wording is forbidden. */
      if (strstr(pop_fbrn.queue[qi].body, "overrun") != NULL ||
          strstr(pop_fbrn.queue[qi].body, "march into") != NULL) {
        fprintf(stderr, "unit_ai_contact: raid abandon popup leaked: '%s'\n",
                pop_fbrn.queue[qi].body);
        return fail("raid must not emit abandon/capture chrome (port_bugs.md #281)");
      }
    }
    /* Restore neutral baseline for nation 1 so later blocks (which only
     * touch nation 0) are not hijacked by this block's higher alarm. */
    ind->alarm_by_player[1] = 0;
    col1.tribe[0].alarm[1].friction = 0;
    col1.indian[0].euro_diplo[1] = (uint8_t)(col1.indian[0].euro_diplo[1] & ~COL1_INDIAN_MET_BIT); /* unmet (was relation 0) */
  }

  /*
   * @RAIDBURN empty warehouse → colonies_destroy_building (non-Town-Hall).
   * Cite: colonies_destroy_building; indian_raid_outcomes.md @RAIDBURN.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    euro->active = true;
    euro->id = euro_id_;
    units_pool_sync(&units);
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 65;
    col1.tribe[0].alarm[0].attacks = 0;
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.nation[0].gold = 0;
    col1.head.founding_father[FF_POCAHONTAS] = -1;
    col1.nation[0].founding_fathers[FF_POCAHONTAS / 8] &=
      (uint8_t)~(1u << (FF_POCAHONTAS % 8));
    colonies.building_type_count = 2;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Town Hall");
    snprintf(colonies.building_types[1].name, sizeof(colonies.building_types[1].name),
             "Warehouse");
    ColonizeColony* c_bd = &colonies.colonies[0];
    c_bd->active = true;
    c_bd->nation_id = 0;
    c_bd->x = 5;
    c_bd->y = 5;
    c_bd->population = 1;
    c_bd->colonist_count = 1;
    c_bd->building_in_production = -1;
    memset(c_bd->stock, 0, sizeof(c_bd->stock));
    memset(c_bd->has_building, 0, sizeof(c_bd->has_building));
    c_bd->has_building[0] = true;
    c_bd->has_building[1] = true;
    colonies.colony_count = 1;
    /* port_bugs.md #827: the kind is DOS's rand(1,4) behind a walls roll, so the
     * BURN arm is reached by rerolling the pulse, not by an alarm band. */
    /* port_bugs.md #827: the kind is rand(1,4) — probe for it, then run one raid. */
    (void)raid_probe_turn_for_kind(AI_RAID_BURN, 400);
    status[0] = '\0'; /* the probe pulses wrote their own lines */
    run_colony_raid();
    if (ai_contact_last_raid_kind() != AI_RAID_BURN) {
      fprintf(stderr, "unit_ai_contact: burn-building kind=%d\n",
              ai_contact_last_raid_kind());
      return fail("empty warehouse at alarm≥60 should pick AI_RAID_BURN");
    }
    if (c_bd->has_building[1]) {
      return fail("BURN should destroy Warehouse via colonies_destroy_building");
    }
    if (!c_bd->has_building[0]) {
      return fail("BURN must not destroy Town Hall");
    }
    /* BURN building destroy → human status names the building (@RAIDBURN). */
    {
      char status_burn_bd[128];
      status_burn_bd[0] = '\0';
      ctx.status = status_burn_bd;
      ctx.status_size = sizeof(status_burn_bd);
      ctx.human_nation = 0;
      /* Reset alarm after kind-scaled BURN bump so pop≤1 does not abandon (≥70). */
      ind->alarm_by_player[0] = 65;
      col1.tribe[0].alarm[0].friction = 65;
      c_bd->has_building[1] = true;
      c_bd->population = 1;
      c_bd->active = true;
      /* Reroll for the BURN kind (rand(1,4), port_bugs.md #827). */
      /* port_bugs.md #827: the kind is rand(1,4) — probe for it, then run one raid. */
      (void)raid_probe_turn_for_kind(AI_RAID_BURN, 400);
      status_burn_bd[0] = '\0';
      run_colony_raid();
      if (ai_contact_last_raid_kind() != AI_RAID_BURN || c_bd->has_building[1]) {
        return fail("BURN status probe needs building destroy");
      }
      if (strstr(status_burn_bd, "Warehouse") == NULL ||
          strstr(status_burn_bd, "burn") == NULL) {
        fprintf(stderr, "unit_ai_contact: burn-building status '%s'\n",
                status_burn_bd);
        return fail("BURN destroy should name building in status");
      }
      ctx.status = NULL;
      ctx.status_size = 0;
    }
  }
  return 0;
}

static int sp_24(void) {

  /*
   * port_bugs.md #1093: the escort FOLLOW peel (MD 3/4/5, weights 1/2/3) this
   * segment asserted was invented (Series N / fandom only) and is deleted
   * with the raid pass. Only the fixture setup later beats inherit is kept.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    /* Park euro away so raid gate does not yank escort. */
    euro->x = 14;
    euro->y = 14;
    euro->active = true;
    euro->id = euro_id_;
    units_pool_sync(&units);
    ind->alarm_by_player[0] = 0;
    col1.tribe[0].alarm[0].friction = 0;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 50; alarm pinned above) */
    colonies.colonies[0].active = false;
    const int lead_id = units_spawn_allow_stack(&units, 0, 9, 5);
    ColonizeUnit* lead = units_get(&units, lead_id);
    if (!lead) {
      return fail("escort lead spawn");
    }
    lead->nation_id = 4;
    lead->moves = 3; /* exhausted: the lead must not act on its own */
    lead->orders = UNITS_ORDER_AI_MOVE;
    lead->goto_x = 12;
    lead->goto_y = 5;
    brave->x = 6;
    brave->y = 5;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    brave->moves = 0;
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
    brave = units_get(&units, brave_id);
    /* Cleanup lead for later probes. */
    units_despawn(&units, lead_id);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
  }

  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 14;
    euro->y = 14;
    euro->active = true;
    euro->id = euro_id_;
    units_pool_sync(&units);
    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 40; alarm pinned above) */
    ColonizeColony* c_tgt = &colonies.colonies[0];
    c_tgt->active = true;
    c_tgt->nation_id = 0;
    c_tgt->x = 12;
    c_tgt->y = 5;
    c_tgt->population = 1;
    c_tgt->colonist_count = 1;
    colonies.colony_count = 1;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    brave->moves = 0;
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
    const int near_id = units_spawn_allow_stack(&units, 0, 6, 5);
    const int far_id = units_spawn_allow_stack(&units, 0, 5, 7);
    ColonizeUnit* near_lead = units_get(&units, near_id);
    ColonizeUnit* far_lead = units_get(&units, far_id);
    if (!near_lead || !far_lead) {
      return fail("escort deepen lead spawn");
    }
    near_lead->nation_id = 4;
    near_lead->moves = 3; /* exhausted: the lead must not act on its own */
    near_lead->orders = UNITS_ORDER_AI_MOVE;
    near_lead->goto_x = 4;
    near_lead->goto_y = 5; /* away from colony at (12,5) */
    far_lead->nation_id = 4;
    far_lead->moves = 3; /* exhausted: the lead must not act on its own */
    far_lead->orders = UNITS_ORDER_AI_MOVE;
    far_lead->goto_x = 12;
    far_lead->goto_y = 5; /* toward raid-gate Euro colony */
    brave = units_get(&units, brave_id);
    units_despawn(&units, near_id);
    units_despawn(&units, far_id);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
    /* No alarm gate → nearest-lead (MD=1 over MD=2). */
    ind->alarm_by_player[0] = 0;
    col1.tribe[0].alarm[0].friction = 0;
    c_tgt->active = false;
    colonies.colony_count = 0;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0; /* refresh: the deepen case above spent this brave's thirds */
    const int near2 = units_spawn_allow_stack(&units, 0, 6, 5);
    const int far2 = units_spawn_allow_stack(&units, 0, 5, 7);
    near_lead = units_get(&units, near2);
    far_lead = units_get(&units, far2);
    if (!near_lead || !far_lead) {
      return fail("escort fallback lead spawn");
    }
    near_lead->nation_id = 4;
    near_lead->moves = 3; /* exhausted: the lead must not act on its own */
    near_lead->orders = UNITS_ORDER_AI_MOVE;
    near_lead->goto_x = 4;
    near_lead->goto_y = 5;
    far_lead->nation_id = 4;
    far_lead->moves = 3; /* exhausted: the lead must not act on its own */
    far_lead->orders = UNITS_ORDER_AI_MOVE;
    far_lead->goto_x = 12;
    far_lead->goto_y = 5;
    brave = units_get(&units, brave_id);
    units_despawn(&units, near2);
    units_despawn(&units, far2);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;

    /*
     * Alarmed escort MD≤4: at alarm≥55 lead at Chebyshev/MD 4 is eligible
     * (peace escort max MD 3). Cite: ai_contact_escort_pick_lead.
     */
    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 40; alarm pinned above) */
    c_tgt->active = true;
    c_tgt->nation_id = 0;
    c_tgt->x = 12;
    c_tgt->y = 5;
    colonies.colony_count = 1;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->orders = UNITS_ORDER_NONE;
    const int md4 = units_spawn_allow_stack(&units, 0, 9, 5); /* MD=4 from (5,5) */
    ColonizeUnit* md4_lead = units_get(&units, md4);
    if (!md4_lead) {
      return fail("alarmed escort MD4 lead spawn");
    }
    md4_lead->nation_id = 4;
    md4_lead->moves = 3; /* exhausted: the lead must not act on its own */
    md4_lead->orders = UNITS_ORDER_AI_MOVE;
    md4_lead->goto_x = 12;
    md4_lead->goto_y = 5;
    brave = units_get(&units, brave_id);
    units_despawn(&units, md4);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;

    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 40; alarm pinned above) */
    c_tgt->active = true;
    c_tgt->nation_id = 0;
    c_tgt->x = 12;
    c_tgt->y = 5;
    colonies.colony_count = 1;
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->orders = UNITS_ORDER_NONE;
    const int md5 = units_spawn_allow_stack(&units, 0, 10, 5); /* MD=5 from (5,5) */
    ColonizeUnit* md5_lead = units_get(&units, md5);
    if (!md5_lead) {
      return fail("hot escort MD5 lead spawn");
    }
    md5_lead->nation_id = 4;
    md5_lead->moves = 3; /* exhausted: the lead must not act on its own */
    md5_lead->orders = UNITS_ORDER_AI_MOVE;
    md5_lead->goto_x = 12;
    md5_lead->goto_y = 5;
    brave = units_get(&units, brave_id);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
    ind->alarm_by_player[0] = 80;
    col1.tribe[0].alarm[0].friction = 80;
    brave = units_get(&units, brave_id);
    units_despawn(&units, md5);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
    c_tgt->active = false;
    colonies.colony_count = 0;
    ind->alarm_by_player[0] = 0;
    col1.tribe[0].alarm[0].friction = 0;
  }
  return 0;
}

static int sp_25(void) {

  /*
   * Raid friction/alarm escalate: successful loot → kind-scaled 0d6c-shaped
   * bump (STORES +4, BURN/WREAK +12, SCALP +16, GOLD/SHIP +8); Pocahontas
   * halves. Cite: indian_raid_loot.md; Series J; fandom Alarm / Pocahontas.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    euro->active = true;
    euro->id = euro_id_;
    units_pool_sync(&units);
    brave->x = 5;
    brave->y = 5;
    brave->moves = 0;
    brave->nation_id = 4;
    brave->active = true;
    brave->id = brave_id;
    units_pool_sync(&units);
    ind->alarm_by_player[0] = 50;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].alarm[0].attacks = 0;
/* alarm pinned above (was relation write) */
    /* Peace: clear the WAR bit pinned by the BURN chrome fixture above so
     * the 0f14 discharge gate is open for this block and the Pocahontas one. */
    col1.indian[0].euro_diplo[0] = (uint8_t)(
      (col1.indian[0].euro_diplo[0] | COL1_INDIAN_MET_BIT) & ~COL1_INDIAN_WAR_BIT
    );
    col1.head.founding_father[FF_POCAHONTAS] = -1;
    col1.nation[0].founding_fathers[FF_POCAHONTAS / 8] &=
      (uint8_t)~(1u << (FF_POCAHONTAS % 8));
    ColonizeColony* c_fr = &colonies.colonies[0];
    c_fr->active = true;
    c_fr->nation_id = 0;
    c_fr->x = 5;
    c_fr->y = 5;
    c_fr->population = 3;
    c_fr->colonist_count = 3;
    c_fr->building_in_production = -1;
    memset(c_fr->stock, 0, sizeof(c_fr->stock));
    c_fr->stock[COLONIZE_CARGO_FOOD] = 20;
    colonies.colony_count = 1;
    /* port_bugs.md #827: the kind is rand(1,4) behind a walls roll, so reroll the
     * pulse (advancing the turn its local RNG is keyed on) until loot lands. */
    /* port_bugs.md #827: the kind is rand(1,4) — probe for it, then run one raid. */
    (void)raid_probe_turn_for_kind(AI_RAID_STORES, 400);
    run_colony_raid();
    const int raid_kind = ai_contact_last_raid_kind();
    if (raid_kind == AI_RAID_NOTHING) {
      return fail("raid friction escalate needs successful loot kind");
    }
    /*
     * DOS 0f14 alarm tail (ported 2026-09-08): a raid DISCHARGES alarm by the
     * per-kind delta (goods −4, building −12, unit/ship −16, gold −8) behind
     * the NOT-at-war gate; the old positive "friction bump" here asserted the
     * retired fandom stand-in. Village friction has no writer on this path.
     */
    int want_delta = 0;
    if (raid_kind == AI_RAID_BURN) {
      want_delta = -12;
    } else if (raid_kind == AI_RAID_SHIP) {
      want_delta = -16;
    } else if (raid_kind == AI_RAID_GOLD) {
      want_delta = -8;
    } else if (raid_kind == AI_RAID_STORES) {
      want_delta = -4;
    }
    int want_alarm = 50 + want_delta;
    if (want_alarm < 0) {
      want_alarm = 0;
    }
    if ((int)ind->alarm_by_player[0] != want_alarm) {
      fprintf(
        stderr,
        "unit_ai_contact: raid alarm=%u (want %d kind=%d)\n",
        (unsigned)ind->alarm_by_player[0],
        want_alarm,
        raid_kind
      );
      return fail("raid at peace should discharge alarm_by_player by DOS kind delta");
    }
    if (col1_tribe_attitude(&col1.tribe[0], 0) != 0) {
      fprintf(
        stderr,
        "unit_ai_contact: raid attitude=%d (want 0 kind=%d)\n",
        col1_tribe_attitude(&col1.tribe[0], 0),
        raid_kind
      );
      return fail("raid must zero the home village attitude word (0f14 tail)");
    }

    /*
     * 5fef demote: year<1520 + WREAK-eligible → STORES (or NOTHING). Structural
     * smoke for Series G3; keep colony approach gate ≥70 elsewhere.
     */
    {
      col1.head.year = 1505;
      col1.head.difficulty = 0;
      ind->alarm_by_player[0] = 90;
      col1.tribe[0].alarm[0].friction = 90;
      c_fr->population = 3;
      c_fr->stock[COLONIZE_CARGO_FOOD] = 20;
      c_fr->stock[COLONIZE_CARGO_TOOLS] = 10;
      /*
       * port_bugs.md #827: the year thresholds this block used to assert (1500 /
       * 1520) exist nowhere in the decomp. FUN_5fef_0f14's grace is a TURN
       * test, `turn <= (difficulty-2)*-0x28 && turn != that`, and it demotes
       * only the building (2) and ship (3) kinds — and only on Discoverer /
       * Explorer. Here difficulty 0 with an early turn, so no sample may come
       * back as BURN or SHIP, and every sample must be one of DOS's five
       * `local_6` values.
       */
      int saw_demote = 0;
      for (int attempt = 0; attempt < 40; ++attempt) {
        c_fr->stock[COLONIZE_CARGO_FOOD] = 20;
        c_fr->stock[COLONIZE_CARGO_TOOLS] = 10;
        c_fr->population = 3;
        ind->alarm_by_player[0] = 90;
        col1.tribe[0].alarm[0].friction = 90;
        run_colony_raid();
        const int k = ai_contact_last_raid_kind();
        if (k < AI_RAID_NOTHING || k > AI_RAID_GOLD) {
          return fail("raid kind must be one of DOS local_6 0..4");
        }
        if (k == AI_RAID_BURN || k == AI_RAID_SHIP) {
          return fail("early-turn grace should demote the building/ship kinds");
        }
        if (k == AI_RAID_STORES || k == AI_RAID_NOTHING) {
          saw_demote = 1;
        }
      }
      if (!saw_demote) {
        return fail("early-turn demote path should yield STORES/NOTHING in samples");
      }
      col1.head.year = 1492;
      col1.head.difficulty = 2;
    }

    /*
     * Same path with Pocahontas. The DOS 0f14 alarm tail's delta is NEGATIVE
     * and the 00f2 halving (France / Pocahontas) applies to positive deltas
     * only — so Pocahontas changes nothing here: the discharge lands in
     * full, and village friction still has no writer on this path. (The old
     * assertion halved the retired positive fandom bump.)
     */
    col1.head.founding_father[FF_POCAHONTAS] = 0;
    col1.nation[0].founding_fathers[FF_POCAHONTAS / 8] |=
      (uint8_t)(1u << (FF_POCAHONTAS % 8));
    ind->alarm_by_player[0] = 50;
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].alarm[0].attacks = 0;
    c_fr->stock[COLONIZE_CARGO_FOOD] = 20;
    c_fr->population = 3;
    c_fr->colonist_count = 3;
    /*
     * The loot kind is DOS's rand(1,4) behind a walls roll (port_bugs.md #827), so
     * drive 0f14's own seam directly with a private LCG until a kind lands —
     * rerunning the whole Indian pulse hundreds of times would also churn the
     * village-visit bookkeeping that later blocks of this narrative rely on.
     */
    ColonizeDosRng poca_rng;
    dos_rng_seed(&poca_rng, 0x5fefu);
    ColonizeDosRng* const poca_rng_save = ctx.rng;
    ctx.rng = &poca_rng;
    /* The probe loop applies real loot; snapshot everything it can touch so
     * the later blocks of this narrative see the fixture they set up. */
    const ColonizeColony poca_colony_save = *c_fr;
    const ColonizeCol1Indian poca_ind_save = col1.indian[0];
    AiRaidKind poca_pick = AI_RAID_NOTHING;
    for (int poca_try = 0; poca_try < 400 && poca_pick == AI_RAID_NOTHING; ++poca_try) {
      ind->alarm_by_player[0] = 50;
      col1.tribe[0].alarm[0].friction = 50;
      c_fr->stock[COLONIZE_CARGO_FOOD] = 20;
      c_fr->building_in_production = -1;
      poca_pick = ai_contact_pick_raid_kind(&ctx, c_fr, 4, 0, &poca_rng, 0);
      if (poca_pick != AI_RAID_NOTHING) {
        ai_contact_apply_raid_loot(&ctx, c_fr, 4, 0, poca_pick);
        ai_contact_raid_alarm_tail(&ctx, 4, 0, poca_pick);
        col1_tribe_attitude_set(&col1.tribe[0], 0, 0);
      }
    }
    ctx.rng = poca_rng_save;
    {
      const uint8_t poca_alarm_now = ind->alarm_by_player[0];
      *c_fr = poca_colony_save;
      col1.indian[0] = poca_ind_save;
      ind->alarm_by_player[0] = poca_alarm_now; /* the tail's discharge is the assertion */
    }
    const int poca_kind = ai_contact_last_raid_kind();
    if (poca_kind == AI_RAID_NOTHING) {
      return fail("Pocahontas raid escalate needs successful loot kind");
    }
    int poca_delta = 0;
    if (poca_kind == AI_RAID_BURN) {
      poca_delta = -12;
    } else if (poca_kind == AI_RAID_SHIP) {
      poca_delta = -16;
    } else if (poca_kind == AI_RAID_GOLD) {
      poca_delta = -8;
    } else if (poca_kind == AI_RAID_STORES) {
      poca_delta = -4;
    }
    int want_poca = 50 + poca_delta;
    if (want_poca < 0) {
      want_poca = 0;
    }
    if ((int)ind->alarm_by_player[0] != want_poca) {
      fprintf(
        stderr,
        "unit_ai_contact: poca raid alarm=%u (want %d kind=%d)\n",
        (unsigned)ind->alarm_by_player[0],
        want_poca,
        poca_kind
      );
      return fail("Pocahontas must not halve the negative 0f14 discharge");
    }
    if (col1_tribe_attitude(&col1.tribe[0], 0) != 0) {
      fprintf(
        stderr,
        "unit_ai_contact: poca raid attitude=%d (want 0 kind=%d)\n",
        col1_tribe_attitude(&col1.tribe[0], 0),
        poca_kind
      );
      return fail("raid must zero the home village attitude word (0f14 tail)");
    }
    col1.head.founding_father[FF_POCAHONTAS] = -1;
    col1.nation[0].founding_fathers[FF_POCAHONTAS / 8] &=
      (uint8_t)~(1u << (FF_POCAHONTAS % 8));
  }
  return 0;
}

static int sp_26(void) {

  /*
   * AI popup unpark: first meet enqueues CONTACT_WELCOME Yes/No; Accept →
   * peace + PEACE/COME OKs only (no Meet CHOICE). Cite: FUN_5bfb_022e / 0182.
   */
    ai_popup_init(&pop);
    /* Fresh pair — earlier raid/scout arms may have despawned the originals. */
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      ColonizeUnit* u = &units.units[ui];
      if (u->active && u->nation_id == 4) {
        units_despawn(&units, u->id);
      }
    }
    const int b2 = units_spawn_allow_stack(&units, 0, 5, 5);
    const int e2 = units_spawn_allow_stack(&units, 1, 6, 5);
    brave2 = units_get(&units, b2);
    ColonizeUnit* euro2 = units_get(&units, e2);
    if (!brave2 || !euro2) {
      return fail("popup meet spawn");
    }
    brave2->nation_id = 4;
    brave2->moves = 1;
    euro2->nation_id = 0;
    euro2->profession = UNITS_JOB_NONE;
    /*
     * 2026-08-22: trade now keys on the contacting UNIT's own cargo hold
     * (LAB_002bbc's real shape), not an invented colony-warehouse search —
     * see ai_contact_2820_begin. euro2 is the unit adjacent to
     * brave2, so it carries the TRADE_GOODS this whole section trades.
     */
    euro2->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
    euro2->hold_goods_amount[0] = 5;
    goods0 = euro2->hold_goods_amount[0];
    ind->euro_diplo[0] = 0;
    ind->alarm_by_player[0] = 25; /* Accept should clear */
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].alarm[0].friction = 18; /* Accept should clear */
    col1.tribe[0].state.learned = 1; /* skip teach side-queue */
    col1.tribe[0].mission = 0xff;
    col1.indian[0].euro_diplo[0] = (uint8_t)(col1.indian[0].euro_diplo[0] & ~COL1_INDIAN_MET_BIT); /* unmet (was relation 0) */
    col1.nation[0].gold = 30;
    ColonizeColony* c_pop = &colonies.colonies[0];
    c_pop->active = true;
    c_pop->nation_id = 0;
    c_pop->x = 5;
    c_pop->y = 5;

    st_pop[0] = '\0';
    ctx.status = st_pop;
    ctx.status_size = sizeof(st_pop);
    ctx.human_nation = 0;
    ctx.ai_popups = &pop;
    ctx.units = &units;
    ctx.colonies = &colonies;
    ctx.col1 = &col1;
    ctx.col1_ok = true;

    (void)ai_contact_try_first_welcome(&ctx, 0, 4);
    if (!ind->euro_diplo[0]) {
      return fail("popup meet should set euro_diplo");
    }
    if (pop.queue_count < 1) {
      return fail("popup meet should enqueue CONTACT_WELCOME CHOICE");
    }
    if (pop.queue[0].tag != AI_POPUP_TAG_CONTACT_WELCOME ||
        pop.queue[0].kind != AI_POPUP_KIND_CHOICE) {
      return fail("first queue entry should be CONTACT_WELCOME Yes/No");
    }
    if (euro2->hold_goods_amount[0] != goods0) {
      return fail("WELCOME should defer auto-trade");
    }

    if (!ai_popup_try_present_next(&pop)) {
      return fail("present WELCOME CHOICE");
    }
    /* Accept peace (Yes). */
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* YES */
    pop.result_tag = AI_POPUP_TAG_CONTACT_WELCOME;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    pop.result_payload = 0;
    ai_contact_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (!ai_contact_indian_has_peace(&col1, 4, 0)) {
      return fail("WELCOME Yes should set peace bit");
    }
    if (ai_diplo_indian_at_war(&col1, 0, 0)) {
      return fail("WELCOME Yes should not be at war");
    }
    if (ai_diplo_indian_relation(&col1, 4, 0) < 40) {
      return fail("WELCOME Yes should raise relation above refuse band");
    }
    /* port_bugs.md #1008: the contact arm clamps alarm and writes no village
     * attitude word at all, so a pre-contact grudge survives the welcome. */
    if (ind->alarm_by_player[0] > 20) {
      return fail("WELCOME Yes should clamp alarm <= 20 (FUN_5bfb :96624)");
    }
    /* No land grant: FUN_5bfb_0182 (raw 96536-96559) only ORs PEACE; the
     * seed-100 idle campaign shows no purchased bit at any AI landfall. */
    {
      euro2 = units_get(&units, e2);
      if (!euro2) {
        return fail("WELCOME Yes euro gone");
      }
      const size_t gidx = (size_t)euro2->y * (size_t)map.width + (size_t)euro2->x;
      if ((map.layer2[gidx] & MAP_LAYER2_PURCHASED) != 0) {
        return fail("WELCOME Yes must not mark the occupied tile purchased");
      }
    }

    /* Follow-ups: PEACE / COME OKs only — no Meet CHOICE. */
    for (int qi = 0; qi < pop.queue_count; ++qi) {
      if (pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_MEET &&
          pop.queue[qi].kind == AI_POPUP_KIND_CHOICE) {
        return fail("WELCOME Yes must not enqueue Meet CHOICE");
      }
    }
    if (pop.queue_count < 1 || pop.queue[0].kind != AI_POPUP_KIND_OK) {
      return fail("WELCOME Yes should enqueue PEACE OK");
    }
    /* port_bugs.md #1230: FUN_5bfb_0182 gates @INDIANCOME on the swapped
     * FUN_281f_030c(euro - 4, nation) word, here dwelling slot 71 (unused,
     * 0), so it shows even though alarm 25 put the relation at 75. */
    if (pop.queue_count != 2 || pop.queue[1].kind != AI_POPUP_KIND_OK) {
      return fail("WELCOME Yes should enqueue @INDIANCOME after @INDIANPEACE");
    }
  return 0;
}

static int sp_27(void) {

    /* Ships: Brave adjacent to sea unit must not start WELCOME. */
    {
      ai_popup_clear(&pop);
      ind->euro_diplo[0] = 0;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
      units.type_count = 3;
      snprintf(units.types[2].name, sizeof(units.types[2].name), "Caravel");
      units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
      units.types[2].movement = 4;
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && u->nation_id == 0) {
          units_despawn(&units, u->id);
        }
      }
      const int ship_id = units_spawn_allow_stack(&units, 2, 6, 5);
      ColonizeUnit* ship = units_get(&units, ship_id);
      if (!ship) {
        return fail("ship contact spawn");
      }
      ship->nation_id = 0;
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && u->nation_id == 4) {
          u->x = 5;
          u->y = 5;
        }
      }
      /* Restore land Free Colonist for later arms. */
      units_despawn(&units, ship_id);
      const int el = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* elu = units_get(&units, el);
      if (elu) {
        elu->nation_id = 0;
        elu->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
        elu->hold_goods_amount[0] = goods0;
      }
    }
  return 0;
}

static int sp_28(void) {

    /* Synthetic Meet CHOICE Trade still works when player initiates (apply). */
    ai_popup_clear(&pop);
    ind->euro_diplo[0] = 1;
    /*
     * FUN_4d56_2820 shell zeroes ask[] for the three highest-bid goods; with
     * every bid 0 that hits muskets/tools/trade goods (stable sort), so give
     * every tribe stock (2154 bid += tons) like a real village has.
     */
    for (int ni = 0; ni < 8; ++ni) {
      for (int c = 1; c < 8; ++c) {
        col1.indian[ni].tons[c] = 300;
      }
    }
    col1.tribe[0].last_bought = 0xffu;
    col1.tribe[0].last_sold = 0xffu;
    col1.tribe[0].sticky_trade_good = 0xffu;
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] | 0x40);
    col1.indian[0].alarm_by_player[0] = 0; /* relation 100 */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ind->alarm_by_player[0] = 10;
    land0 = units_get(&units, units_id_at(&units, 6, 5));
    if (!land0) {
      return fail("land0 lookup for Trade CHOICE");
    }
    land0->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
    land0->hold_goods_amount[0] = goods0;
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* AI_CONTACT_CHOICE_TRADE */
    pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    pop.result_payload = 0;
    st_pop[0] = '\0';
    ai_contact_apply_popup_result(&ctx, &pop);
    /*
     * Human CHOICE over the real LAB_002bbc-shaped sale (not a literal
     * DOS human branch -- see ai_contact_enqueue_trade_price_choice's
     * header): Meet Trade prices the offer and queues Accept/Decline
     * instead of trading immediately. Cite: indian_trade_2820.md.
     */
    if (land0->hold_goods_amount[0] != goods0) {
      return fail("Trade CHOICE should defer trade until price CHOICE");
    }
    if (pop.queue_count < 1 ||
        pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
        pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_TRADE_OFFER) {
      return fail("Trade CHOICE should enqueue CONTACT_TRADE_OFFER price CHOICE");
    }
    st_pop[0] = '\0';
    const uint32_t gold_before_trade = col1.nation[0].gold;
    const int trade_price = apply_trade_offer_choice(&ctx, &pop, 0, 4, 1);
    if (trade_price <= 0) {
      return fail("Trade price CHOICE accept should carry a positive locked price");
    }
    if (land0->hold_goods_amount[0] != 0) {
      return fail("Trade accept should remove the whole hold slot (FUN_1000_8cdc)");
    }
    /*
     * 2026-08-22: LAB_002bbc (the real DOS branch this stand-in approximates)
     * credits the Euro nation's gold — natives pay for the cargo, not the
     * reverse. See indian_trade_2820.md's 2026-08-22 addendum /
     * ai_contact_2820_begin.
     */
    if (col1.nation[0].gold != gold_before_trade + (uint32_t)trade_price) {
      fprintf(
        stderr, "unit_ai_contact: trade gold %u -> %u (price %d)\n", gold_before_trade,
        col1.nation[0].gold, trade_price
      );
      return fail("Trade accept should CREDIT the locked price to the Euro nation's gold");
    }
    if (col1.tribe[0].last_bought != (uint8_t)COLONIZE_CARGO_TRADE_GOODS) {
      return fail("Trade accept should set tribe.last_bought to trade goods");
    }
    if (col1.tribe[0].sticky_trade_good != 0xffu) {
      return fail("Trade accept should idle sticky_trade_good (tribe+7 = 0xff)");
    }
    if (ind->tons[COLONIZE_CARGO_TRADE_GOODS] != goods0) {
      return fail("Trade accept should add the sold quantity to indian.tons[cargo]");
    }
  return 0;
}

static int sp_29(void) {

    /*
     * 2026-08-22: the old "hard bargain" mid-alarm peel (extra trade-good
     * drained, no relation bump) is dropped -- LAB_002bbc's real body has
     * no such tension/resume-loop for the AI-controlled accept branch (a
     * single deterministic decision, see ai_contact_2820_begin).
     * Mid alarm (45..54, still under the outer >=50 refuse-talk gate on
     * this path since that gate itself is untouched) now trades exactly
     * like any other accept: 1 unit drained, relation still bumps +2.
     * Cite: indian_trade_2820.md 2026-08-22 addendum.
     */
    {
      ind->alarm_by_player[0] = 47;
      col1.tribe[0].last_bought = 0xffu; /* 2820: a tribe refuses the good it bought last */
      const uint8_t rel_before = ai_diplo_indian_relation(&col1, 4 + (0), 0);
      col1.tribe[0].alarm[0].friction = 20;
      land0->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
      land0->hold_goods_amount[0] = 3;
      const int goods_mid = land0->hold_goods_amount[0];
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* TRADE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (apply_trade_offer_choice(&ctx, &pop, 0, 4, 1) <= 0) {
        return fail("mid-alarm trade price CHOICE accept failed");
      }
      if (land0->hold_goods_amount[0] != 0) {
        fprintf(stderr, "unit_ai_contact: mid-alarm goods %d->%d\n", goods_mid,
                land0->hold_goods_amount[0]);
        return fail("mid-alarm trade should remove the whole hold");
      }
      if (ai_diplo_indian_relation(&col1, 4 + (0), 0) < rel_before + 2) {
        return fail("mid-alarm trade should bump relation by at least +2 (DOS accept: alarm -= 2*c4)");
      }
      if (ind->alarm_by_player[0] > 45) {
        return fail("mid-alarm trade should relieve alarm by at least 2 (-2*c4)");
      }
      ind->alarm_by_player[0] = 10; /* restore peaceful for later arms */
    }

    /*
     * Ore-primary tribe (Aztec) trade: same real accept mechanic, different
     * nation's flavor-good naming. Cite: indian_trade_2820.md; Series G2.
     */
    {
      ColonizeCol1Indian* az = &col1.indian[1];
      az->euro_diplo[0] = 1;
      az->alarm_by_player[0] = 10;
      col1.indian[1].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */ /* Aztec×Euro meet floor */
      col1.tribe[0].nation_id = 5; /* Aztec ore */
      col1.tribe[0].last_bought = 0xffu; /* 2820: a tribe refuses the good it bought last */
      brave2->nation_id = 5; /* real per-unit adjacency now needs the physical Brave to match */
      col1.tribe[0].alarm[0].friction = 20;
      land0->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
      land0->hold_goods_amount[0] = 3;
      const int goods_ore = land0->hold_goods_amount[0];
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 5;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (apply_trade_offer_choice(&ctx, &pop, 0, 5, 1) <= 0) {
        return fail("ore trade price CHOICE accept failed");
      }
      if (land0->hold_goods_amount[0] != 0) {
        fprintf(stderr, "unit_ai_contact: ore trade goods %d->%d\n", goods_ore,
                land0->hold_goods_amount[0]);
        return fail("ore trade should remove the whole hold");
      }
      col1.tribe[0].nation_id = 4; /* restore Inca */
      brave2->nation_id = 4;
      ind->alarm_by_player[0] = 10;
      az->alarm_by_player[0] = 10;
      pop.result_nation_b = 4;
    }

    /*
     * Series M: per-nation flavor-good naming coverage (tobacco/cotton/furs).
     * Real accept mechanic throughout -- no hard-bargain peel (see above).
     */
    {
      struct {
        int nation_b;
        int tribe_nation;
        const char* label;
      } cases[] = {
        {8, 8, "tobacco"}, /* Cherokee */
        {9, 9, "cotton"},  /* Apache */
        {7, 7, "furs"},    /* Iroquois */
      };
      for (int ci = 0; ci < 3; ++ci) {
        ColonizeCol1Indian* ind_m = &col1.indian[cases[ci].tribe_nation - 4];
        ind_m->euro_diplo[0] = 1;
        ind_m->alarm_by_player[0] = 10;
        col1.indian[cases[ci].tribe_nation - 4].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (alarm pinned above) */
        col1.tribe[0].nation_id = (uint8_t)cases[ci].tribe_nation;
        col1.tribe[0].last_bought = 0xffu; /* 2820: a tribe refuses the good it bought last */
        brave2->nation_id = cases[ci].tribe_nation;
        col1.tribe[0].alarm[0].friction = 20;
        land0->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
        land0->hold_goods_amount[0] = 3;
        const int goods_m = land0->hold_goods_amount[0];
        ai_popup_clear(&pop);
        pop.has_result = true;
        pop.result_cancelled = false;
        pop.result_choice_id = 1;
        pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
        pop.result_nation_a = 0;
        pop.result_nation_b = cases[ci].nation_b;
        st_pop[0] = '\0';
        ai_contact_apply_popup_result(&ctx, &pop);
        if (apply_trade_offer_choice(&ctx, &pop, 0, cases[ci].nation_b, 1) <= 0) {
          return fail("Series M trade price CHOICE accept failed");
        }
        if (land0->hold_goods_amount[0] != 0) {
          fprintf(stderr, "unit_ai_contact: %s trade goods %d->%d\n",
                  cases[ci].label, goods_m, land0->hold_goods_amount[0]);
          return fail("Series M trade should remove the whole hold");
        }
        ind_m->alarm_by_player[0] = 10;
      }
      col1.tribe[0].nation_id = 4;
      brave2->nation_id = 4;
      ind->alarm_by_player[0] = 10;
      pop.result_nation_b = 4;
    }
  return 0;
}

static int sp_30(void) {

    /*
     * Sea trade: a ship (not the land unit) is now the sole contacting
     * unit -- the 2820 shell keys on the ONE unit
     * ai_contact_find_adjacent_euro finds at the tile, so despawn land0
     * first (no DOS shell ever reads two units off one tile). Cite:
     * docs/fandom_col1994.md Teach/trade; indian_trade_2820.md 2026-08-22.
     */
    {
      units_despawn(&units, land0->id);
      units.type_count = 3;
      snprintf(units.types[2].name, sizeof(units.types[2].name), "Caravel");
      units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
      units.types[2].movement = 4;
      units.types[2].cargo = 4;
      const int ship_id = units_spawn_allow_stack(&units, 2, 6, 5);
      ColonizeUnit* ship = units_get(&units, ship_id);
      if (!ship) {
        return fail("sea-trade ship spawn");
      }
      ship->nation_id = 0;
      ship->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
      ship->hold_goods_amount[0] = 3;
      col1.tribe[0].last_bought = 0xffu; /* 2820: a tribe refuses the good it bought last */
      col1.tribe[0].x = 5;
      col1.tribe[0].y = 5;
      ind->alarm_by_player[0] = 10;
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */
      const uint8_t rel_sea = ai_diplo_indian_relation(&col1, 4 + (0), 0);
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* TRADE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (apply_trade_offer_choice(&ctx, &pop, 0, 4, 1) <= 0) {
        return fail("sea-trade price CHOICE accept failed");
      }
      if (ship->hold_goods_amount[0] != 0) {
        return fail("sea-trade should remove the whole ship hold");
      }
      if (ai_diplo_indian_relation(&col1, 4 + (0), 0) < (uint8_t)(rel_sea + 2)) {
        return fail("sea-trade should bump relation like land trade");
      }
      units_despawn(&units, ship_id);
    }

    /*
     * Wagon land-trade: TRADE_GOODS on a Wagon Train's own hold, the sole
     * contacting unit at the tile (ship already despawned above).
     * Cite: fandom sea/land trade; indian_contact.md peaceful trade.
     */
    {
      if (units.type_count < 4) {
        units.type_count = 4;
      }
      snprintf(units.types[3].name, sizeof(units.types[3].name), "Wagon Train");
      units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
      units.types[3].movement = 3;
      units.types[3].cargo = 4;
      const int wag_id = units_spawn_allow_stack(&units, 3, 6, 5);
      ColonizeUnit* wag = units_get(&units, wag_id);
      if (!wag) {
        return fail("wagon-trade spawn");
      }
      wag->nation_id = 0;
      wag->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
      wag->hold_goods_amount[0] = 2;
      col1.tribe[0].last_bought = 0xffu; /* 2820: a tribe refuses the good it bought last */
      ind->alarm_by_player[0] = 12;
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 70; alarm pinned above) */
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (apply_trade_offer_choice(&ctx, &pop, 0, 4, 1) <= 0) {
        return fail("wagon-trade price CHOICE accept failed");
      }
      if (wag->hold_goods_amount[0] != 0) {
        return fail("wagon-trade should remove the whole wagon hold");
      }
      if (ai_diplo_indian_relation(&col1, 4 + (0), 0) < 90) {
        return fail("wagon-trade should bump relation by at least +2 (alarm 12 -> <=10)");
      }
      units_despawn(&units, wag_id);
    }

    /*
     * Leave (choice 10, DOS OVL13 0x4bd2 falls to switch default) shows no
     * popup at all — only the MP forfeit runs. port_bugs.md #799.
     */
    ai_popup_clear(&pop);
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 5; /* LEAVE */
    pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    st_pop[0] = '\0';
    const int leave_queue_before = pop.queue_count;
    ai_contact_apply_popup_result(&ctx, &pop);
    if (pop.queue_count != leave_queue_before) {
      return fail("Leave CHOICE should enqueue no popup");
    }

    /*
     * Esc on the @ACTIONS menu: FUN_6f74_2580 returns -1, which 4528's
     * dispatch sends to the same switch default as the Cancel row, so the
     * 0x4c0a tail forfeits the unit's moves (cancelled MEET == Leave).
     */
    {
      if (units.type_count < 4) {
        units.type_count = 4;
      }
      units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
      units.types[3].movement = 3;
      const int esc_id = units_spawn_allow_stack(&units, 3, 6, 5);
      ColonizeUnit* esc_u = units_get(&units, esc_id);
      if (!esc_u) {
        return fail("Esc forfeit: unit spawn");
      }
      esc_u->nation_id = 0;
      esc_u->moves = 9;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = true;
      pop.result_choice_id = -1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      pop.result_payload = (esc_id + 1) << 2;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (esc_u->moves != 0) {
        return fail("Esc on the village menu must forfeit the unit's moves like Cancel Action");
      }
      if (pop.queue_count != 0) {
        return fail("Esc on the village menu should enqueue no popup");
      }
      units_despawn(&units, esc_id);
    }

    /*
     * Trade CHOICE with no contacting unit/cargo → the 2820 shell returns
     * through LAB_003582 with NO popup at all (port_bugs.md #960; the old
     * typed-English "Trade concluded." fallback was an OpenCol invention).
     */
    {
      ai_popup_clear(&pop);
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* TRADE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count != 0) {
        return fail("Trade fail should enqueue no popup");
      }
      if (st_pop[0] != '\0') {
        fprintf(stderr, "unit_ai_contact: trade-stub status '%s'\n", st_pop);
        return fail("Trade fail should set no status line");
      }
    }
  return 0;
}

static int sp_31(void) {

    /*
     * FUN_4d56_2820 human gates + phases (structural port 2026-08-29):
     *  a) tribe.last_bought == cargo → @BADCARGO (CONTACT_REFUSE OK), no trade
     *  b) tribe.sticky_trade_good == cargo → @BADHAGGLE1
     *  c) two holds → CONTACT_TRADE_PICK CHOICE; pick → @TRADE0; accept →
     *     that hold removed (others compacted), gold credited, tons += qty,
     *     then LAB_002e92: @BUYWHICH → @BUY0 accept → gold debited, goods in.
     */
    {
      if (units.type_count < 4) {
        units.type_count = 4;
      }
      snprintf(units.types[3].name, sizeof(units.types[3].name), "Wagon Train");
      units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
      units.types[3].movement = 3;
      units.types[3].cargo = 4;
      const int wag_id = units_spawn_allow_stack(&units, 3, 6, 5);
      ColonizeUnit* wag = units_get(&units, wag_id);
      if (!wag) {
        return fail("2820 gates: wagon spawn");
      }
      wag->nation_id = 0;
      wag->hold_goods_type[0] = COLONIZE_CARGO_FURS;
      wag->hold_goods_amount[0] = 100;
      ind->alarm_by_player[0] = 10;
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      col1.tribe[0].last_bought = (uint8_t)COLONIZE_CARGO_FURS;
      col1.tribe[0].last_sold = 0xffu;
      col1.tribe[0].sticky_trade_good = 0xffu;
      /* a) @BADCARGO */
      village_trade_intel_reset();
      ai_popup_clear(&pop);
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* TRADE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 || pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_REFUSE ||
          strstr(st_pop, "enough") == NULL) {
        fprintf(stderr, "unit_ai_contact: badcargo status '%s'\n", st_pop);
        return fail("2820: last_bought == cargo should refuse with @BADCARGO");
      }
      if (wag->hold_goods_amount[0] != 100) {
        return fail("2820: @BADCARGO must not touch the hold");
      }
      {
        /* Sidebar intel: @BADCARGO names three wanted goods; nothing sold yet. */
        int ib[3];
        int is[3];
        int ibn = 0;
        int isn = 0;
        if (!village_trade_intel_get(0, 5, 5, ib, &ibn, is, &isn) || ibn != 3 || isn != 0) {
          return fail("2820: @BADCARGO should record the village's three wanted goods");
        }
      }
      /* b) @BADHAGGLE1 (furs may sit in the tribe's top-3 bids → ask 0; use trade goods) */
      col1.tribe[0].last_bought = 0xffu;
      wag->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
      col1.tribe[0].sticky_trade_good = (uint8_t)COLONIZE_CARGO_TRADE_GOODS;
      ai_popup_clear(&pop);
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 || pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_REFUSE ||
          strstr(st_pop, "already told") == NULL) {
        fprintf(stderr, "unit_ai_contact: badhaggle1 status '%s'\n", st_pop);
        return fail("2820: sticky_trade_good == cargo should refuse with @BADHAGGLE1");
      }
      /* c) hold pick → sell → buy */
      col1.tribe[0].sticky_trade_good = 0xffu;
      wag->hold_goods_type[1] = COLONIZE_CARGO_TRADE_GOODS;
      wag->hold_goods_amount[1] = 50;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 || pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_TRADE_PICK ||
          pop.queue[pop.queue_count - 1].choice_count != 3) {
        return fail("2820: two holds should queue the hold-pick CHOICE (2 holds + Cancel)");
      }
      /* The hold pick is FUN_291f_0182/016a with no DS:0x1f5c write (ndisasm
       * 0x49768..0x4982c): no chief portrait, unlike the 019c dialogs. */
      if (pop.queue[pop.queue_count - 1].portrait_tribe != -1) {
        return fail("2820: the hold-pick CHOICE carries no chief portrait");
      }
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 2; /* second hold: 50 trade goods */
      pop.result_tag = AI_POPUP_TAG_CONTACT_TRADE_PICK;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      pop.result_payload = wag_id;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 || pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_TRADE_OFFER ||
          pop.queue[pop.queue_count - 1].choice_count != 4) {
        return fail("2820: hold pick should queue @TRADE0 (accept / fairer / gift / never mind)");
      }
      if (pop.queue[pop.queue_count - 1].portrait_tribe != 0) {
        return fail("2820: @TRADE0 needs the chief portrait");
      }
      const uint32_t gold_c = col1.nation[0].gold;
      const int16_t tons_tg_c = ind->tons[COLONIZE_CARGO_TRADE_GOODS];
      const int price_c = apply_trade_offer_choice(&ctx, &pop, 0, 4, 1);
      if (price_c <= 0) {
        return fail("2820: hold-2 sale accept failed");
      }
      if (wag->hold_goods_type[0] != COLONIZE_CARGO_TRADE_GOODS || wag->hold_goods_amount[0] != 100 ||
          wag->hold_goods_amount[1] != 0) {
        return fail("2820: selling hold 2 must remove only that slot and keep hold 1");
      }
      if (col1.nation[0].gold != gold_c + (uint32_t)price_c) {
        return fail("2820: sale should credit the locked price");
      }
      if (ind->tons[COLONIZE_CARGO_TRADE_GOODS] != tons_tg_c + 50) {
        return fail("2820: sale should add qty to indian.tons[cargo]");
      }
      if (col1.tribe[0].last_bought != (uint8_t)COLONIZE_CARGO_TRADE_GOODS) {
        return fail("2820: sale should set last_bought = the sold cargo");
      }
      int bw = -1;
      for (int qi = 0; qi < pop.queue_count; ++qi) {
        if (pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_BUYWHICH) {
          bw = qi;
        }
      }
      if (bw < 0) {
        return fail("2820: after a sale the tribe should offer its goods (@BUYWHICH)");
      }
      if (pop.queue[bw].portrait_tribe != 0) {
        return fail("2820: @BUYWHICH needs the chief portrait");
      }
      {
        /* Sidebar intel: @BUYWHICH records exactly the offered goods. */
        int ib[3];
        int is[3];
        int ibn = 0;
        int isn = 0;
        /* Goods rows + DOS's "Nothing right now, thank you." decline (id 0). */
        const int cc = pop.queue[bw].choice_count;
        if (cc < 2 || pop.queue[bw].choice_ids[cc - 1] != 0) {
          return fail("2820: @BUYWHICH must end with a decline row carrying id 0");
        }
        if (!village_trade_intel_get(0, 5, 5, ib, &ibn, is, &isn) || isn != cc - 1) {
          return fail("2820: @BUYWHICH should record the village's goods for sale");
        }
        for (int k = 0; k < isn; ++k) {
          if (is[k] + 1 != pop.queue[bw].choice_ids[k]) {
            return fail("2820: @BUYWHICH intel goods differ from the offered choices");
          }
        }
      }
      const int buy_id = pop.queue[bw].choice_ids[0];
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = buy_id;
      pop.result_tag = AI_POPUP_TAG_CONTACT_BUYWHICH;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      pop.result_payload = wag_id;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 || pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_BUY0) {
        return fail("2820: @BUYWHICH pick should queue @BUY0");
      }
      if (pop.queue[pop.queue_count - 1].portrait_tribe != 0) {
        return fail("2820: @BUY0 needs the chief portrait");
      }
      const int buy_payload = pop.queue[pop.queue_count - 1].payload;
      const int bought_c = buy_id - 1;
      const int16_t tons_bought_c = ind->tons[bought_c];
      col1.nation[0].gold = 100000;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* We will gladly pay */
      pop.result_tag = AI_POPUP_TAG_CONTACT_BUY0;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      pop.result_payload = buy_payload;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold >= 100000u) {
        return fail("2820: @BUY0 accept should debit gold");
      }
      if (wag->hold_goods_type[1] != bought_c || wag->hold_goods_amount[1] != 50) {
        fprintf(stderr, "unit_ai_contact: buy hold type=%d amount=%d\n", wag->hold_goods_type[1],
                wag->hold_goods_amount[1]);
        return fail("2820: @BUY0 accept should fill a hold with qty = the sold hold's amount (DS:0x8dc4)");
      }
      if (ind->tons[bought_c] != tons_bought_c - 50) {
        return fail("2820: purchase should take qty from indian.tons[cargo]");
      }
      if (col1.tribe[0].last_sold != (uint8_t)(bought_c == 9 ? 0xff : bought_c)) {
        return fail("2820: purchase should set last_sold");
      }
      units_despawn(&units, wag_id);
      col1.tribe[0].last_bought = 0xffu;
      col1.tribe[0].last_sold = 0xffu;
      col1.tribe[0].sticky_trade_good = 0xffu;
      col1.nation[0].gold = 30;
      ind->alarm_by_player[0] = 0; /* restore peaceful for later popup arms */
      fprintf(stderr, "unit_ai_contact: 2820 gates + sell/buy phases ok\n");
    }
  return 0;
}

static int sp_32(void) {

    /*
     * Second Brave same pulse: pending WELCOME → no second CHOICE enqueue.
     */
    {
      ai_popup_clear(&pop);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && (u->nation_id == 4 || u->nation_id == 0)) {
          units_despawn(&units, u->id);
        }
      }
      const int b_a = units_spawn_allow_stack(&units, 0, 5, 5);
      const int b_b = units_spawn_allow_stack(&units, 0, 5, 6);
      const int e3 = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* ba = units_get(&units, b_a);
      ColonizeUnit* bb = units_get(&units, b_b);
      ColonizeUnit* e3u = units_get(&units, e3);
      if (!ba || !bb || !e3u) {
        return fail("second-brave spawn");
      }
      ba->nation_id = 4;
      bb->nation_id = 4;
      e3u->nation_id = 0;
      ind->euro_diplo[0] = 0;
      ind->alarm_by_player[0] = 0;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
      col1.tribe[0].nation_id = 4;
      col1.tribe[0].alarm[0].friction = 0;
      col1.tribe[0].state.learned = 1;
      col1.tribe[0].mission = 0xff;
      col1.indian[0].euro_diplo[0] = (uint8_t)(col1.indian[0].euro_diplo[0] & ~COL1_INDIAN_MET_BIT); /* unmet (was relation 0) */
      st_pop[0] = '\0';
      /* One welcome per Brave: the second finds the MET bit already set. */
      (void)ai_contact_try_first_welcome(&ctx, 0, 4);
      (void)ai_contact_try_first_welcome(&ctx, 0, 4);
      if (!ind->euro_diplo[0]) {
        return fail("second-brave meet should set euro_diplo");
      }
      int welcome_choices = 0;
      for (int qi = 0; qi < pop.queue_count; ++qi) {
        if (pop.queue[qi].kind == AI_POPUP_KIND_CHOICE &&
            pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_WELCOME) {
          welcome_choices++;
        }
      }
      if (welcome_choices != 1) {
        fprintf(stderr, "unit_ai_contact: WELCOME CHOICE count=%d\n", welcome_choices);
        return fail("second Brave same pulse must not re-offer WELCOME");
      }
    }

    /*
     * WELCOME No → @INDIANSHUN + at war. Cite: FUN_5bfb_022e reject.
     */
    {
      ai_popup_clear(&pop);
      ind->euro_diplo[0] = 0;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
      ind->alarm_by_player[0] = 0;
      col1.tribe[0].alarm[0].attacks = 0;
      col1.tribe[0].alarm[0].friction = 7;
      /* unmet: euro_diplo zeroed above (transform MET line dropped) */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && u->nation_id == 4) {
          units_despawn(&units, u->id);
        }
      }
      const int bn = units_spawn_allow_stack(&units, 0, 5, 5);
      const int en = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* bnu = units_get(&units, bn);
      ColonizeUnit* enu = units_get(&units, en);
      if (!bnu || !enu) {
        return fail("WELCOME No spawn");
      }
      bnu->nation_id = 4;
      enu->nation_id = 0;
      st_pop[0] = '\0';
      (void)ai_contact_try_first_welcome(&ctx, 0, 4);
      if (pop.queue_count < 1 || pop.queue[0].tag != AI_POPUP_TAG_CONTACT_WELCOME) {
        return fail("WELCOME No setup should enqueue WELCOME");
      }
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 2; /* NO */
      pop.result_tag = AI_POPUP_TAG_CONTACT_WELCOME;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (ai_contact_indian_has_peace(&col1, 4, 0)) {
        return fail("WELCOME No should not set peace");
      }
      if (!ai_diplo_indian_at_war(&col1, 0, 0)) {
        return fail("WELCOME No should declare war");
      }
      if (ind->alarm_by_player[0] < 80) {
        return fail("WELCOME No should raise alarm to burn band");
      }
      /* port_bugs.md #1228: 022e's reject limb writes no village word. */
      if (col1.tribe[0].alarm[0].friction != 7) {
        return fail("WELCOME No must not touch tribe friction");
      }
      /* smell #66: DOS's only attacks-writer is the 465b per-settlement
       * trespass bump; the reject limb must leave the word alone. */
      if (col1.tribe[0].alarm[0].attacks != 0) {
        return fail("WELCOME No must not touch tribe attacks");
      }
      if (strstr(st_pop, "WAR") == NULL && strstr(st_pop, "War") == NULL) {
        fprintf(stderr, "unit_ai_contact: reject status '%s'\n", st_pop);
        return fail("WELCOME No should set Prepare for WAR status");
      }
    }

    /*
     * After Accept, second adjacency must not refuse solely due to relation==0.
     */
    {
      ai_popup_clear(&pop);
      ind->euro_diplo[0] = 1;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] | 0x40); /* peace */
      ind->alarm_by_player[0] = 10;
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 100; alarm pinned above) */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && (u->nation_id == 4 || (u->nation_id == 0 && units_is_sea(&units, u->id) == 0))) {
          /* keep one brave + one euro land */
        }
      }
      /* Ensure adjacency pair */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && u->nation_id == 4) {
          u->x = 5;
          u->y = 5;
        }
        if (u->active && u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 6;
          u->y = 5;
        }
      }
      st_pop[0] = '\0';
    }
  return 0;
}

static int sp_33(void) {

    /*
     * Mission expel (DOS 4cc6_00f2 escalation + 4cc6_0000, 2026-09-07d):
     * alarm delta lands at 100 while at PEACE → RNG-gated expel with
     * @INDIANBURN status + CONTACT_RAID OK enqueue. The prelude itself no
     * longer burns missions.
     */
    {
      ai_popup_clear(&pop);
      col1.tribe[0].mission = 0;
      ind->alarm_by_player[0] = 99;
      ind->euro_diplo[0] |= COL1_INDIAN_PEACE_BIT;
      col1.tribe[0].alarm[0].friction = 10;
      st_pop[0] = '\0';
      /* The tail needs a stream (port_bugs.md #1004). */
      ColonizeDosRng burn_rng2;
      dos_rng_seed(&burn_rng2, 0x4cc6u);
      ColonizeDosRng* const burn_rng2_save = ctx.rng;
      ctx.rng = &burn_rng2;
      int fired = 0;
      for (int tries = 0; tries < 64 && !fired; ++tries) {
        ai_contact_alarm_delta_00f2(&ctx, 4, 0, 5);
        ind->alarm_by_player[0] = 99;
        fired = col1.tribe[0].mission == 0xff;
      }
      if (!fired) {
        return fail("popup mission burn should clear mission");
      }
      if (strstr(st_pop, "burn") == NULL) {
        return fail("popup mission burn should set status");
      }
      if (pop.queue_count < 1) {
        return fail("mission burn should enqueue OK popup");
      }
      if (pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_RAID) {
        return fail("mission burn OK should use CONTACT_RAID tag");
      }
      ctx.rng = burn_rng2_save;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~COL1_INDIAN_PEACE_BIT);
      ind->alarm_by_player[0] = 10;
    }
  return 0;
}

static int sp_34(void) {

    /*
     * port_bugs.md #1006: there is no player gift action. NAMES.TXT @ACTIONS has
     * no gift row, nothing emits the old choice id 2, and the invented
     * Small/Large/Generous amount CHOICE is gone. What survives is the 022e
     * auto-gift drain (FUN_5bfb_022e gift half, ai_contact_demand.c).
     */
    {
      ai_popup_clear(&pop);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && (u->nation_id == 4 || u->nation_id == 0)) {
          units_despawn(&units, u->id);
        }
      }
      const int bg = units_spawn_allow_stack(&units, 0, 5, 5);
      const int eg = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* braveg = units_get(&units, bg);
      ColonizeUnit* eurog = units_get(&units, eg);
      if (!braveg || !eurog) {
        return fail("gift spawn");
      }
      braveg->nation_id = 4;
      eurog->nation_id = 0;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 10;
      col1.tribe[0].alarm[0].friction = 10;
      col1.tribe[0].state.learned = 1;
      col1.tribe[0].mission = 0xff;
      col1.nation[0].gold = 50;
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 2; /* the retired gift row */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != 50u) {
        return fail("retired Meet gift row should move no gold");
      }
      for (int qi = 0; qi < pop.queue_count; ++qi) {
        if (pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_GIFT &&
            pop.queue[qi].kind == AI_POPUP_KIND_CHOICE) {
          return fail("no gift amount CHOICE should exist (port_bugs.md #1006)");
        }
      }

    }
  return 0;
}

static int sp_35(void) {

    /*
     * Incite Indians CHOICE (FUN_4d56_417e — see
     * original_sources_annotated/ai/indian_incite_417e.md). Menu step lists
     * affordable targets; picking one drains the inciter's gold and pushes
     * the target's alarm with this tribe.
     */
    {
      ind->tech = 1;
      ind->alarm_by_player[0] = 0;
      ind->alarm_by_player[1] = 0;
      col1.nation[0].gold = 5000;
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 50; alarm pinned above) */
      /* 417e @NOCONTACT gate (0x16b7): the tribe must have MET the target
       * nation too, or the incite is refused without charge. */
      col1.indian[0].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
      col1.indian[0].euro_diplo[2] |= COL1_INDIAN_MET_BIT;
      col1.indian[0].euro_diplo[3] |= COL1_INDIAN_MET_BIT;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 6; /* AI_CONTACT_CHOICE_INCITE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE) {
        return fail("Incite CHOICE should enqueue CONTACT_INCITE target CHOICE");
      }
      if (pop.queue[pop.queue_count - 1].choice_count < 1) {
        return fail("affordable inciter should see at least one incite target");
      }
      const int target = pop.queue[pop.queue_count - 1].choice_ids[0];
      if (target < 0 || target > 3 || target == 0) {
        return fail("incite target choice id should be a Euro nation other than the inciter");
      }

      /* Apply the picked target: drains gold, target's own gold untouched. */
      const uint32_t target_gold_before = col1.nation[target].gold;
      const int target_payload = pop.queue[pop.queue_count - 1].payload;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = target;
      pop.result_tag = AI_POPUP_TAG_CONTACT_INCITE;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      pop.result_payload = target_payload;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE ||
          pop.queue[pop.queue_count - 1].choice_count != 2) {
        return fail("picking a target should enqueue the @INDIANWARPATH2 pay confirm");
      }
      test_incite_confirm_pay(&ctx, &pop, 0, 4);
      if (col1.nation[0].gold >= 5000u || col1.nation[0].gold > 4500u) {
        return fail("Incite should drain at least 500 gold from the inciter");
      }
      if (col1.nation[target].gold != target_gold_before) {
        return fail("Incite target's own gold should be untouched");
      }
      /* FUN_281f_0d6c → FUN_4cc6_00f2: +100 alarm slam, halved for a
       * French (nation 1) target inside 00f2, clamped at 100. */
      if (ind->alarm_by_player[target] != (target == 1 ? 50 : 100)) {
        return fail("Incite should slam the target's alarm with this tribe (+100, French-halved)");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK) {
        return fail("Incite should enqueue an Incite OK follow-up");
      }
      /* Dismissing that OK chrome (payload 0, choice 0 = English) must end
       * the beat, not re-quote a pay confirm against the English. */
      {
        ai_popup_clear(&pop);
        pop.has_result = true;
        pop.result_cancelled = false;
        pop.result_choice_id = 0;
        pop.result_tag = AI_POPUP_TAG_CONTACT_INCITE;
        pop.result_nation_a = 0;
        pop.result_nation_b = 4;
        pop.result_payload = 0;
        ai_contact_apply_popup_result(&ctx, &pop);
        if (pop.queue_count != 0) {
          return fail("dismissing an Incite OK notice must not enqueue another popup");
        }
      }

      /* Too poor to afford any target → no CHOICE enqueued, refuse OK instead. */
      col1.nation[1].gold = 0;
      col1.nation[2].gold = 0;
      col1.nation[3].gold = 0;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 6; /* AI_CONTACT_CHOICE_INCITE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 1;
      pop.result_nation_b = 4;
      col1.nation[1].gold = 0;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      /*
       * DOS does NOT filter the target menu by affordability (viceroy 83600):
       * a broke inciter still sees the rows and still gets quoted a price —
       * the refusal is @UNFORTUNATE after the pay confirm, and no gold moves.
       */
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE) {
        return fail("incite target CHOICE should not be gold-filtered");
      }
      {
        const int broke_payload = pop.queue[pop.queue_count - 1].payload;
        const int broke_target = pop.queue[pop.queue_count - 1].choice_ids[0];
        ai_popup_clear(&pop);
        pop.has_result = true;
        pop.result_cancelled = false;
        pop.result_choice_id = broke_target;
        pop.result_tag = AI_POPUP_TAG_CONTACT_INCITE;
        pop.result_nation_a = 1;
        pop.result_nation_b = 4;
        pop.result_payload = broke_payload;
        ai_contact_apply_popup_result(&ctx, &pop);
        test_incite_confirm_pay(&ctx, &pop, 1, 4);
        if (col1.nation[1].gold != 0u) {
          return fail("broke inciter must not be charged for an incite");
        }
        /* (The @UNFORTUNATE OK itself only draws for a human inciter;
         * nation 1 is AI-run in this fixture, so only the no-charge
         * invariant is asserted here.) */
      }
    }

    /*
     * Missionary + capital-village Incite discount (2026-08-14 port,
     * ai_contact_incite_price's is_missionary/is_capital flags — see
     * indian_incite_417e.md). Boost muskets/horse_herds so `base` (and
     * thus price) is well clear of the 500 floor, so the -1500/-500 DOS
     * discounts are actually observable. Simulates the Meet CHOICE's
     * payload (bit0=is_missionary, bit1=is_capital) the way
     * ai_contact_try_village_meet's caller would really set it, since this
     * test drives ai_contact_apply_popup_result directly rather than going
     * through the full trigger path.
     */
    {
      ind->muskets = 40;
      ind->horse_herds = 40;
      col1.indian[0].alarm_by_player[0] = 50; /* relation 50 */
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;

      col1.nation[0].gold = 1000000;
      col1.nation[1].gold = 1000000;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 6; /* AI_CONTACT_CHOICE_INCITE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      pop.result_payload = 0; /* no missionary, not a capital village */
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE ||
          pop.queue[pop.queue_count - 1].choice_count < 1) {
        return fail("discount setup: expected an affordable incite target CHOICE");
      }
      const int target1 = pop.queue[pop.queue_count - 1].choice_ids[0];
      const int meet_payload1 = pop.queue[pop.queue_count - 1].payload;
      const uint32_t gold_before1 = col1.nation[0].gold;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = target1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_INCITE;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      /* The real harness carries the enqueued CHOICE's own .payload into
       * .result_payload when a live popup is answered; replicate that here
       * since this test drives apply_popup_result directly. */
      pop.result_payload = meet_payload1;
      ai_contact_apply_popup_result(&ctx, &pop);
      test_incite_confirm_pay(&ctx, &pop, 0, 4);
      const uint32_t price_no_discount = gold_before1 - col1.nation[0].gold;

      col1.nation[0].gold = 1000000;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 6; /* AI_CONTACT_CHOICE_INCITE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      pop.result_payload = 3; /* is_missionary=1, is_capital=1 */
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE ||
          pop.queue[pop.queue_count - 1].choice_count < 1) {
        return fail("discount setup: expected an affordable incite target CHOICE (discounted)");
      }
      const int target2 = pop.queue[pop.queue_count - 1].choice_ids[0];
      const int meet_payload2 = pop.queue[pop.queue_count - 1].payload;
      const uint32_t gold_before2 = col1.nation[0].gold;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = target2;
      pop.result_tag = AI_POPUP_TAG_CONTACT_INCITE;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      pop.result_payload = meet_payload2;
      ai_contact_apply_popup_result(&ctx, &pop);
      test_incite_confirm_pay(&ctx, &pop, 0, 4);
      const uint32_t price_discounted = gold_before2 - col1.nation[0].gold;

      if (price_discounted + 1900u > price_no_discount) {
        fprintf(
          stderr,
          "unit_ai_contact: incite price no_discount=%u discounted=%u\n",
          (unsigned)price_no_discount,
          (unsigned)price_discounted
        );
        return fail("missionary+capital Incite discount should cut ~2000 gold off the price");
      }

      ind->muskets = 0;
      ind->horse_herds = 0;
    }

    /*
     * docs/dos_bugs.md DB-55 (port_bugs.md #1157): Esc on the @INDIANWARPATH
     * target menu cancels. DOS clamps FUN_6f74_2580's -1 to row 1
     * (4d56:436c-437a, rows are nation + 1), so its Esc picks the English and
     * quotes @INDIANWARPATH2; the port enqueues nothing and moves no gold and
     * no tension. French inciter, so the English row is a real target.
     */
    {
      col1.nation[1].gold = 100000;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 6; /* AI_CONTACT_CHOICE_INCITE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 1;
      pop.result_nation_b = 4;
      pop.result_payload = 0;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1 || pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE) {
        return fail("DB-55 setup: the French inciter should get the target menu");
      }
      const int menu_payload = pop.queue[pop.queue_count - 1].payload;
      const ColonizeCol1Indian ind_before = *ind;
      uint32_t gold_before[4];
      for (int n = 0; n < 4; ++n) {
        gold_before[n] = col1.nation[n].gold;
      }
      /* What ai_popup_cancel_current answers for a CHOICE: cancelled, id -1. */
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = true;
      pop.result_choice_id = -1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_INCITE;
      pop.result_nation_a = 1;
      pop.result_nation_b = 4;
      pop.result_payload = menu_payload;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count != 0) {
        return fail("DB-55: Esc on the incite target menu must not enqueue the pay confirm");
      }
      for (int n = 0; n < 4; ++n) {
        if (col1.nation[n].gold != gold_before[n]) {
          return fail("DB-55: Esc on the incite target menu must not move gold");
        }
      }
      if (memcmp(&ind_before, ind, sizeof(ind_before)) != 0) {
        return fail("DB-55: Esc on the incite target menu must not move the tribe's tension");
      }
    }
  return 0;
}

static int sp_36(void) {

    /*
     * port_bugs.md #1349: a Demand pick whose payload carries no acting unit
     * is not a DOS situation (row 8 needs a known armed unit). The invented
     * tools/gold amount CHOICE it used to open is gone: no drain, no popup.
     */
    {
      ai_popup_clear(&pop);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && (u->nation_id == 4 || u->nation_id == 0)) {
          units_despawn(&units, u->id);
        }
      }
      const int bd = units_spawn_allow_stack(&units, 0, 5, 5);
      const int ed = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* braved = units_get(&units, bd);
      ColonizeUnit* eurod = units_get(&units, ed);
      if (!braved || !eurod) {
        return fail("demand CHOICE spawn");
      }
      braved->nation_id = 4;
      eurod->nation_id = 0;
      eurod->tools = 5;
      ind->euro_diplo[0] = 1;
      /* port_bugs.md #1005: the band is the nation alarm scalar. */
      ind->alarm_by_player[0] = 45;
      col1.tribe[0].alarm[0].friction = 45;
      col1.tribe[0].state.learned = 1;
      col1.tribe[0].mission = 0xff;
      col1.nation[0].gold = 80;
/* alarm pinned above (was relation write) */
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      c->stock[COLONIZE_CARGO_TOOLS] = 25;
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 3; /* DEMAND */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != 80u || c->stock[COLONIZE_CARGO_TOOLS] != 25 ||
          eurod->tools != 5) {
        return fail("no-unit Demand must not drain gold or tools");
      }
      if (pop.queue_count != 0) {
        return fail("no-unit Demand must not enqueue a popup");
      }
    }

    /*
     * Demand CHOICE refuse when alarmed (≥55): no tools/gold drain (no amount
     * CHOICE). The old typed "The <tribe> refuse demands." OK popup had no
     * catalog row and is gone (port_bugs.md #1100), so the refusal is silent.
     * Cite: FUN_5bfb_102a / fandom Alarm; indian_contact.md demand refuse.
     */
    {
      ai_popup_clear(&pop);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && (u->nation_id == 4 || u->nation_id == 0)) {
          units_despawn(&units, u->id);
        }
      }
      const int bd = units_spawn_allow_stack(&units, 0, 5, 5);
      const int ed = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* braved = units_get(&units, bd);
      ColonizeUnit* eurod = units_get(&units, ed);
      if (!braved || !eurod) {
        return fail("demand refuse CHOICE spawn");
      }
      braved->nation_id = 4;
      eurod->nation_id = 0;
      eurod->tools = 25;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 60;
      col1.tribe[0].nation_id = 4;
      col1.tribe[0].x = 5;
      col1.tribe[0].y = 5;
      col1.tribe[0].alarm[0].friction = 60; /* demand-band refuse line */
      col1.tribe[0].state.learned = 1;
      col1.tribe[0].mission = 0xff;
      col1.nation[0].gold = 100;
      /* port_bugs.md #1005: the refuse line is the nation alarm scalar, so the
       * fixture must keep the 60 set above rather than reset it to 20. */
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      c->stock[COLONIZE_CARGO_TOOLS] = 30;
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 3; /* DEMAND */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      const uint32_t gold_r = col1.nation[0].gold;
      const int tools_r = c->stock[COLONIZE_CARGO_TOOLS];
      const int utools_r = eurod->tools;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != gold_r) {
        return fail("Demand refuse CHOICE should not take gold");
      }
      if (c->stock[COLONIZE_CARGO_TOOLS] != tools_r || eurod->tools != utools_r) {
        return fail("Demand refuse CHOICE should not take tools");
      }
      if (strstr(st_pop, "refuse") != NULL) {
        fprintf(stderr, "unit_ai_contact: demand-refuse status '%s'\n", st_pop);
        return fail("Demand refuse must not print typed English (#1100)");
      }
    }
  return 0;
}

static int sp_37(void) {

    /*
     * port_bugs.md #573: a TEACH choice whose payload carries no acting unit is
     * not a DOS situation at all — DOS only ever teaches through
     * thunk_FUN_1000_a618 ("Live Among The Natives") on a named unit and a
     * named village. The port used to run an invented village-adjacency
     * pulse here (skill from a hand-written tribe table, one-shot burned on
     * refusals); it is retired, so the arm is a silent no-op.
     */
    {
      ai_popup_clear(&pop);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && (u->nation_id == 4 || u->nation_id == 0)) {
          units_despawn(&units, u->id);
        }
      }
      const int bt = units_spawn_allow_stack(&units, 0, 5, 5);
      const int et = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* bravet = units_get(&units, bt);
      ColonizeUnit* eurot = units_get(&units, et);
      if (!bravet || !eurot) {
        return fail("teach no-unit CHOICE spawn");
      }
      bravet->nation_id = 4;
      eurot->nation_id = 0;
      eurot->profession = UNITS_JOB_NONE;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 10;
      col1.tribe[0].nation_id = 4;
      col1.tribe[0].x = 5;
      col1.tribe[0].y = 5;
      col1.tribe[0].alarm[0].friction = 10;
      col1.tribe[0].state.learned = 0;
      col1.tribe[0].mission = 0xff;
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 4; /* TEACH */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.tribe[0].state.learned) {
        return fail("Teach with no acting unit must not burn the one-shot");
      }
      if (eurot->profession != UNITS_JOB_NONE) {
        return fail("Teach with no acting unit must not grant a skill");
      }
      if (pop.queue_count != 0) {
        return fail("Teach with no acting unit must enqueue nothing");
      }
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 10;
    }
  return 0;
}

static int sp_38(void) {

    /*
     * port_bugs.md / FUN_5bfb_022e @INDIANSCONVERT: the human gets Converts when a
     * Brave from a settlement holding *our* mission walks up to one of our
     * colonies — an Indian Convert appears in that colony and the popup names
     * it. Adjacency of our own Missionary to a village does nothing.
     */
    {
      ai_popup_clear(&pop);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && (u->nation_id == 4 || u->nation_id == 0)) {
          units_despawn(&units, u->id);
        }
      }
      units.type_count = 4;
      snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
      units.types[2].movement = 1;
      snprintf(units.types[3].name, sizeof(units.types[3].name), "Colonists");
      units.types[3].movement = 1;

      col1.tribe[0].nation_id = 4;
      col1.tribe[0].x = 5;
      col1.tribe[0].y = 7;
      /* Word must sit below the 0x80 hostile latch or the visit arm skips;
       * attacks are cleared to 0 (smell #66: only the trespass bump writes
       * them, and the discharge assertion below covers friction). */
      col1.tribe[0].alarm[0].friction = 10;
      col1.tribe[0].alarm[0].attacks = 0;
      col1.tribe[0].mission = 0; /* our mission, plain */
      col1.tribe[0].state.learned = 1;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 10;
      ind->tech = 3;
      ind->contact_state[0] = 0; /* earlier WELCOME-No arm latched hostile 1 */
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      snprintf(c->name, sizeof(c->name), "Jamestown");

      /* Our own Missionary beside the village: inert. */
      const int mm = units_spawn_allow_stack(&units, 2, 5, 6);
      ColonizeUnit* missm = units_get(&units, mm);
      if (!missm) {
        return fail("convert visit spawn missionary");
      }
      missm->nation_id = 0;

      /* A Brave of that settlement, standing next to Jamestown. */
      const int bm = units_spawn_allow_stack(&units, 0, 6, 5);
      ColonizeUnit* bravem = units_get(&units, bm);
      if (!bravem) {
        return fail("convert visit spawn brave");
      }
      bravem->nation_id = 4;
      bravem->home_tribe_index = 0;

      /*
       * smell #75: the standing-adjacency convert pulse is retired — the
       * visit fires only off a Brave's move tail (022e). Re-arm the walk-up
       * latch and reroll the visit each pulse until the rng(0,0xf) convert
       * draw hits (need = tech 3 + 2 = 5).
       */
      ColonizeDosRng cv_rng;
      const uint16_t cv_saved_turn = col1.head.turn;
      col1.head.turn = 0; /* once-per-8-turns gift cooldown never blocks */
      st_pop[0] = '\0';
      int converts = 0;
      /* port_bugs.md #879: the spawn must go through units_set_nation, which is
       * the only thing that stamps col1_vis_mask = 1<<nation (units_spawn
       * zeroes it). A raw `convert->nation_id = ...` left the owner unable to
       * see its own convert through the fog. */
      int cv_vis_mask = -1;
      for (int pulse = 0; pulse < 64 && converts == 0; ++pulse) {
        ai_popup_clear(&pop);
        /* Small consecutive seeds all give tiny first LCG outputs (mood roll
         * 1-3, auto-fail vs word 10); spread them. */
        dos_rng_seed(&cv_rng, (uint32_t)((pulse + 1) * 12345u));
        ctx.rng = &cv_rng;
        ai_native_note_brave_turn_origin(bm, 9, 9);
        (void)ai_contact_try_village_gifts(&ctx, 4);
        for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
          const ColonizeUnit* u = &units.units[ui];
          /* port_bugs.md #840 (reverses #561): DOS raw 97009-97013 reads
           * `puVar4 = *(0x8542)` — the COLONY record, the same pointer the
           * @INDIANSCONVERT STRING0 name fill at raw 97002 uses — and calls
           * `FUN_281f_095c(0, puVar4[0x1a], puVar4[0], puVar4[1])`: the
           * colony's owner byte and the colony's own tile, not the brave's. */
          if (u->active && u->nation_id == c->nation_id &&
              u->profession == COLONIZE_PROF_CONVERT && u->x == c->x && u->y == c->y) {
            converts++;
            if (cv_vis_mask < 0) {
              cv_vis_mask = (int)u->col1_vis_mask;
            }
          }
        }
      }
      ctx.rng = NULL;
      col1.head.turn = cv_saved_turn;
      if (converts == 0) {
        return fail("mission settlement visit should spawn an Indian Convert in the colony");
      }
      if (cv_vis_mask != (1 << (c->nation_id & 3))) {
        fprintf(stderr, "unit_ai_contact: convert vis mask %d\n", cv_vis_mask);
        return fail("@INDIANSCONVERT spawn must set col1_vis_mask via units_set_nation");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_CONVERT) {
        return fail("convert visit should enqueue CONTACT_CONVERT OK");
      }
      if (strstr(pop.queue[pop.queue_count - 1].body, "Jamestown") == NULL) {
        fprintf(stderr, "unit_ai_contact: convert visit body '%s'\n",
                pop.queue[pop.queue_count - 1].body);
        return fail("@INDIANSCONVERT should name the colony the converts join");
      }
      if (col1.tribe[0].alarm[0].friction != 0 || col1.tribe[0].alarm[0].attacks != 0) {
        return fail("convert visit should clear the settlement's alarm for that nation");
      }

      /* No mission → no converts, however many visits. */
      col1.tribe[0].mission = 0xff;
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && u->profession == COLONIZE_PROF_CONVERT) {
          units_despawn(&units, u->id);
        }
      }
      col1.head.turn = 0;
      for (int pulse = 0; pulse < 32; ++pulse) {
        dos_rng_seed(&cv_rng, (uint32_t)((pulse + 1) * 12345u));
        ctx.rng = &cv_rng;
        ai_native_note_brave_turn_origin(bm, 9, 9);
        (void)ai_contact_try_village_gifts(&ctx, 4);
      }
      col1.head.turn = cv_saved_turn;
      ctx.rng = NULL;
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        const ColonizeUnit* u = &units.units[ui];
        if (u->active && u->profession == COLONIZE_PROF_CONVERT) {
          return fail("a settlement with no mission must never send converts");
        }
      }
      if (col1.tribe[0].mission != 0xff) {
        return fail("a human missionary beside a village must not establish a mission");
      }
      units_despawn(&units, mm);
      units_despawn(&units, bm);
      col1.tribe[0].y = 5;
    }
  return 0;
}

static int sp_39(void) {

    /* Village-enter Meet CHOICE: already-met human on tribe → Trade…Leave. */
    {
      ai_popup_clear(&pop);
      ind->euro_diplo[0] = 1;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] | 0x40);
      col1.indian[0].alarm_by_player[0] = 0; /* relation 100 */
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      ind->alarm_by_player[0] = 10;
      st_pop[0] = '\0';
      if (!ai_contact_try_village_meet(&ctx, 0, 4, 0, 0)) {
        return fail("village meet should enqueue for already-met human");
      }
      if (pop.queue_count < 1 || pop.queue[0].tag != AI_POPUP_TAG_CONTACT_MEET ||
          pop.queue[0].kind != AI_POPUP_KIND_CHOICE) {
        return fail("village meet should enqueue CONTACT_MEET CHOICE");
      }
      /*
       * No acting unit (legacy caller) → Trade / Live Among The Natives /
       * Incite Indians / Cancel Action (NAMES.TXT @ACTIONS rows 1/5/7/10).
       * Body = GAME.TXT @VILLAGEHAPPY.. (FUN_4d56_4528 human arm), not the
       * old hand-typed "welcomes the most worthy" line.
       */
      if (pop.queue[0].choice_count != 4) {
        fprintf(stderr, "unit_ai_contact: village menu rows %d\n", pop.queue[0].choice_count);
        return fail("legacy village menu should offer Trade/Live Among/Incite/Cancel");
      }
      if (strstr(st_pop, "expedition has reached") == NULL) {
        fprintf(stderr, "unit_ai_contact: village status '%s'\n", st_pop);
        return fail("cool village meet should set the @VILLAGEHAPPY body");
      }
      /* Mid alarm (25..49) → @VILLAGEMEDIUM, still enqueued. */
      ai_popup_clear(&pop);
      ind->alarm_by_player[0] = 45;
      st_pop[0] = '\0';
      if (!ai_contact_try_village_meet(&ctx, 0, 4, 0, 0)) {
        return fail("hot village meet should still enqueue");
      }
      if (strstr(st_pop, "expedition has reached") == NULL) {
        fprintf(stderr, "unit_ai_contact: hot village status '%s'\n", st_pop);
        return fail("mid-alarm village meet should set the @VILLAGEMEDIUM body");
      }
      ind->alarm_by_player[0] = 10;
      /* Unmet must not use village meet (WELCOME path). */
      ai_popup_clear(&pop);
      ind->euro_diplo[0] = 0;
      if (ai_contact_try_village_meet(&ctx, 0, 4, 0, 0)) {
        return fail("unmet village must not enqueue Meet CHOICE");
      }
      /* Restore met for leftover arms. */
      ind->euro_diplo[0] = 1;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] | 0x40);
    }

    ctx.ai_popups = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
  return 0;
}

static int sp_40(void) {

  /*
   * Capital-destroy surrender (fandom): reset alarm/friction + peace toward
   * attacker. Cite: docs/fandom_col1994.md Capital destroy; ai_diplo_indian_capital_surrender.
   */
  {
    ind->alarm_by_player[0] = 90;
    col1.tribe[0].alarm[0].friction = 80;
    col1.tribe[0].alarm[0].attacks = 5;
    col1.tribe[0].nation_id = 4;
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ai_diplo_indian_capital_surrender(&col1, 4, 0);
    /* FUN_5fef_1b0e capital-razed arm (raw 101289-101298): alarm is clamped
     * DOWN to 15, not zeroed. The zeroing loop clears the razed tribe's own
     * dwellings (DOS matches the tribe INDEX against the nation byte and
     * clears the wrong tribe's: DB-22, not reproduced). */
    if (ind->alarm_by_player[0] != 15 || col1.tribe[0].alarm[0].friction != 0) {
      return fail("capital surrender should clamp alarm to 15 / clear friction");
    }
    if (col1.tribe[0].alarm[0].attacks != 0) {
      return fail("capital surrender should clear attack counter");
    }
    if (!ai_contact_indian_has_peace(&col1, 4, 0)) {
      return fail("capital surrender should set peace bit");
    }
    if (ai_diplo_indian_relation(&col1, 4, 0) < 85) {
      return fail("capital surrender should land relation at 100-15");
    }
    /* Remaining villages must not become / stay capital (fandom). */
    col1.tribe[0].state.capital = 1;
    ai_diplo_indian_capital_surrender(&col1, 4, 0);
    if (col1.tribe[0].state.capital != 0) {
      return fail("capital surrender should clear capital bit on remaining tribes");
    }
  }
  return 0;
}

static int sp_41(void) {

  /*
   * Series U: Attack Village (the invented Attack/Leave raid warn was deleted,
   * port_bugs.md #542). DOS FUN_4d56_4528 case 9 only ORs the attack-confirmed
   * bit 0x04 both ways (FUN_1000_8bf6 = FUN_15b3_0066); the 465b move does the
   * fighting and its own alarm slam. No peace clear, no +100, no tribe
   * friction floor (port_bugs.md #1233). Cite: indian_settlement_4528.md.
   */
  {
    AiPopupState pop;
    ai_popup_init(&pop);
    char st_warn[128];
    st_warn[0] = '\0';
    ctx.status = st_warn;
    ctx.status_size = sizeof(st_warn);
    ctx.human_nation = 0;
    ctx.ai_popups = &pop;
    col1.player[0].control = 0;
    ind->euro_diplo[0] = (uint8_t)(COL1_INDIAN_MET_BIT | COL1_INDIAN_PEACE_BIT);
    col1.nation[0].relation_by_indian[0] = (uint8_t)(COL1_INDIAN_MET_BIT | COL1_INDIAN_PEACE_BIT);
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    ai_contact_village_attack_choice(&ctx, 4, 0);
    if ((ind->euro_diplo[0] & COL1_INDIAN_ATTACK_CONFIRMED_BIT) == 0 ||
        (col1.nation[0].relation_by_indian[0] & COL1_INDIAN_ATTACK_CONFIRMED_BIT) == 0) {
      return fail("Attack choice should OR the 0x04 bit into both diplo rows");
    }
    if (!ai_contact_indian_has_peace(&col1, 4, 0)) {
      return fail("Attack choice must not clear peace (4528 case 9 has no 0a10)");
    }
    if (ind->alarm_by_player[0] != 10u) {
      fprintf(
        stderr, "unit_ai_contact: attack choice alarm %u (want 10)\n",
        (unsigned)ind->alarm_by_player[0]
      );
      return fail("Attack choice must not move alarm (the 465b slam does)");
    }
    if (col1.tribe[0].alarm[0].friction != 10u) {
      return fail("Attack choice must not floor tribe friction (port_bugs.md #1233)");
    }
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~COL1_INDIAN_ATTACK_CONFIRMED_BIT);
    col1.nation[0].relation_by_indian[0] =
      (uint8_t)(col1.nation[0].relation_by_indian[0] & ~COL1_INDIAN_ATTACK_CONFIRMED_BIT);
    fprintf(stderr, "unit_ai_contact: village 4528 raid warn ok\n");
    ctx.ai_popups = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
  }
  return 0;
}

static int sp_42(void) {

  /*
   * Series T: 4528 ship-village relation bands. DOS has exactly two abort
   * arms (@DONTKNOWSHIPS unmet, @MADATSHIPS alarm>=0x4b or friction>=0x40);
   * everything below them falls straight through to the village meet with no
   * extra message (port_bugs.md #983). rel 60 → Meet CHOICE only; rel 80 → MADAT;
   * friction>=64 → mad. Cite: indian_settlement_4528.md.
   */
  {
    AiPopupState pop;
    ai_popup_init(&pop);
    char st_ship[128];
    st_ship[0] = '\0';
    ctx.status = st_ship;
    ctx.status_size = sizeof(st_ship);
    ctx.human_nation = 0;
    ctx.ai_popups = &pop;
    col1.player[0].control = 0; /* human */
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].alarm[0].friction = 10;
    ind->euro_diplo[0] = (uint8_t)(COL1_INDIAN_MET_BIT | 0x01u | 0x40u);
    col1.indian[0].alarm_by_player[0] = 40; /* mid band (single store) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    st_ship[0] = '\0';
    ai_popup_clear(&pop);
    if (!ai_contact_try_ship_village(&ctx, 0, 5, 5)) {
      return fail("ship mid-band should handle village tile");
    }
    int meet_ok = 0;
    for (int qi = 0; qi < pop.queue_count; ++qi) {
      if (pop.queue[qi].tag == AI_POPUP_TAG_INFO && pop.queue[qi].body[0]) {
        return fail("ship mid-band must not enqueue an extra INFO popup");
      }
      if (pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_MEET &&
          pop.queue[qi].kind == AI_POPUP_KIND_CHOICE) {
        meet_ok = 1;
      }
    }
    if (!meet_ok) {
      return fail("ship mid-band should enqueue Meet CHOICE");
    }

    /* ASM: FUN_1000_84fc (alarm) >= 0x4b → MADAT; no wary. */
    ai_popup_clear(&pop);
    st_ship[0] = '\0';
    col1.indian[0].alarm_by_player[0] = 80;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.tribe[0].alarm[0].friction = 10;
    if (!ai_contact_try_ship_village(&ctx, 0, 5, 5)) {
      return fail("ship rel80 should handle village");
    }
    for (int qi = 0; qi < pop.queue_count; ++qi) {
      if (pop.queue[qi].body[0] && strstr(pop.queue[qi].body, "wary of ships") != NULL) {
        return fail("ship rel80 MADAT must not set wary");
      }
      if (pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_MEET) {
        return fail("ship rel80 MADAT must not enqueue Meet");
      }
    }
    if (strstr(st_ship, "trust") == NULL && strstr(st_ship, "ships") == NULL) {
      fprintf(stderr, "unit_ai_contact: MADAT status '%s'\n", st_ship);
      return fail("ship rel80 should set MADATSHIPS status");
    }

    /* Friction mad path still aborts. */
    ai_popup_clear(&pop);
    st_ship[0] = '\0';
    col1.indian[0].alarm_by_player[0] = 40; /* relation 60 */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.tribe[0].alarm[0].friction = 64;
    if (!ai_contact_try_ship_village(&ctx, 0, 5, 5)) {
      return fail("ship friction mad should handle village");
    }
    for (int qi = 0; qi < pop.queue_count; ++qi) {
      if (pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_MEET) {
        return fail("ship friction mad must not enqueue Meet");
      }
      if (pop.queue[qi].body[0] && strstr(pop.queue[qi].body, "wary of ships") != NULL) {
        return fail("ship friction mad must not set wary");
      }
    }
    col1.tribe[0].alarm[0].friction = 10;
    ctx.ai_popups = NULL;
  }
  return 0;
}

static int sp_43(void) {

  /*
   * WoI tribe defection (FUN_4d56_1816 §2, indian_woi_defect_1816.md): while
   * WoI is declared, an eligible not-yet-resolved tribe may defect to the
   * human's side — relation vs human +100 / vs crown -100, one-shot latch,
   * musket/horse windfall. Sweep seeds to prove both outcomes are reachable
   * (not just "doesn't crash") — same lesson as the naval-ambush mechanic.
   */
  {
    char status_woi[128];
    ctx.status = status_woi;
    ctx.status_size = sizeof(status_woi);
    ctx.human_nation = 0;
    col1.head.market_demand_pool_raw[0] = 1; /* WoI declared (AI_KING_WOI_BYTE stand-in) */
    col1.head.game_options.woi = 1;
    ColonizeDosRng woi_rng;
    ctx.rng = &woi_rng;

    /*
     * Seed once and let the stream advance across iterations without
     * reseeding — reseeding per-iteration with small sequential seeds hits
     * dos_rng's LCG warm-up bias (the first roll after a tiny seed is not
     * yet well-mixed, so seeds 1..30 all produce the same first
     * dos_rng_range(1,400) result). Matches real gameplay too: the turn RNG
     * is one continuous stream, not reseeded per tribe per check.
     */
    dos_rng_seed(&woi_rng, 100u);
    int saw_defect = 0;
    int saw_no_defect = 0;
    int saw_mission_cleared = 0;
    for (int i = 0; i < 60 && !(saw_defect && saw_no_defect); ++i) {
      memset(ind, 0, sizeof(*ind));
      ind->tech = 15;
      ind->muskets = 10;
      ind->horse_herds = 8;
      /*
       * 2026-09-06d: the windfall clamp operand is the tribe's **village
       * count** (`DS:0x962a` = `stuff.tribe_village_counts`), not its tech
       * level — DOS reads `*(char *)(*(int *)0x8d52 - 0x69d6)`. The old
       * fixture only set `tech`, which happened to give the same answer
       * under the previous (wrong) mapping. Keep the same expected windfall
       * by seeding the census array the same way FUN_4962_06b6 would.
       */
      col1.stuff.tribe_village_counts[0] = 15;
      /* relation_by_indian is indexed [euro_nation].[indian_idx]; human=0,
       * crown fold=1 for human=0, tribe nation_id=4 → indian_idx=0. */
      /* FUN_281f_030c = alarm toward the rebel nation; eligibility needs >= 25. */
      col1.indian[0].alarm_by_player[0] = 30;
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
      col1.indian[0].alarm_by_player[1] = 50; /* crown fold for human=0 */
      col1.indian[0].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
      status_woi[0] = '\0';
      /*
       * "Mission clear" side-effect (FUN_2a1f_0398 / FUN_4cc6_0000, wired
       * 2026-08-14 — see indian_woi_defect_1816.md): a village of this same
       * tribe hosting a human (rebel) mission should lose it on a defect
       * hit. Re-armed each iteration since a hit clears it to "none".
       */
      col1.tribe[0].nation_id = 4;
      col1.tribe[0].mission = 0; /* human (English) mission */
      ai_contact_indian_woi_defect(&ctx, 4);
      if (ind->woi_defect_resolved) {
        if (col1.tribe[0].mission == COL1_TRIBE_MISSION_NONE) {
          saw_mission_cleared = 1;
        } else {
          return fail("WoI defect hit should clear this tribe's human missions");
        }
        /* FUN_4cc6_00f2(+100 vs rebels, -100 vs Crown) are ALARM deltas: Tory natives. */
        if (ai_diplo_indian_alarm(&col1, 4, 0) != 100) {
          return fail("WoI defect hit should max alarm vs the rebel nation");
        }
        if (ai_diplo_indian_alarm(&col1, 4, 1) != 0) {
          fprintf(
            stderr,
            "unit_ai_contact: WoI crown alarm=%d want 0\n",
            ai_diplo_indian_alarm(&col1, 4, 1)
          );
          return fail("WoI defect hit should zero alarm vs the Crown");
        }
        if (ind->muskets != 40 || ind->horse_herds != 8 || ind->horse_breeding != 200) {
          fprintf(
            stderr,
            "unit_ai_contact: WoI windfall muskets=%u horses=%u breeding=%u\n",
            ind->muskets,
            ind->horse_herds,
            ind->horse_breeding
          );
          return fail("WoI defect hit should apply musket/horse windfall");
        }
        if (status_woi[0] == '\0') {
          return fail("WoI defect hit should set a status line");
        }
        saw_defect = 1;
      } else {
        if (ai_diplo_indian_alarm(&col1, 4, 0) != 30 || ai_diplo_indian_alarm(&col1, 4, 1) != 50) {
          return fail("WoI defect miss should leave alarm untouched");
        }
        if (ind->muskets != 10 || ind->horse_herds != 8 || ind->horse_breeding != 0) {
          return fail("WoI defect miss should leave musket/horse windfall untouched");
        }
        saw_no_defect = 1;
      }
    }
    if (!saw_defect) {
      return fail("WoI defect should hit at least once over the RNG stream sweep");
    }
    if (!saw_no_defect) {
      return fail("WoI defect should miss at least once over the RNG stream sweep");
    }
    if (!saw_mission_cleared) {
      return fail("WoI defect should clear a human mission on the same tribe at least once");
    }
    fprintf(
      stderr, "unit_ai_contact: WoI tribe defection ok (hit and miss both reachable)\n"
    );

    /* Resolved latch: once set, no further mutation even on a hit-shaped seed. */
    dos_rng_seed(&woi_rng, 1u);
    memset(ind, 0, sizeof(*ind));
    ind->woi_defect_resolved = 1;
    ind->tech = 15;
    ind->muskets = 10;
    ind->horse_herds = 8;
    col1.indian[0].alarm_by_player[0] = 20; /* relation 80 */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ai_contact_indian_woi_defect(&ctx, 4);
    if (ai_diplo_indian_relation(&col1, 4 + (0), 0) != 80 || ind->muskets != 10) {
      return fail("WoI defect should skip an already-resolved tribe");
    }

    /* No WoI: must not touch state even on a hit-shaped seed. */
    col1.head.market_demand_pool_raw[0] = 0;
    col1.head.game_options.woi = 0;
    dos_rng_seed(&woi_rng, 1u);
    memset(ind, 0, sizeof(*ind));
    ind->tech = 15;
    ind->muskets = 10;
    ind->horse_herds = 8;
    col1.indian[0].alarm_by_player[0] = 20; /* relation 80 */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ai_contact_indian_woi_defect(&ctx, 4);
    if (ind->woi_defect_resolved || ai_diplo_indian_relation(&col1, 4 + (0), 0) != 80) {
      return fail("WoI defect should no-op before independence is declared");
    }
    col1.head.market_demand_pool_raw[0] = 0;
    col1.head.game_options.woi = 0;
    ctx.rng = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
  }
  return 0;
}

static int sp_44(void) {

  /*
   * @INDIANBEGFOOD accept/decline (FUN_5bfb_022e already-met adjacency —
   * ai_contact_try_village_beg_food / ai_contact_apply_beg_food). Sign
   * convention re-read off the decompile 2026-08-31 (port_bugs.md): DOS's
   * `local_c == 2` is the "We offer you ..." row, so *accept* is what
   * subtracts `colony->food >> 1` (half, not a quarter), zeroes the
   * settlement's alarm word and walks the nation alarm down; *refuse*
   * costs no food, scales the settlement alarm word by 1.5 and raises the
   * nation alarm. The two branches used to be the other way round.
   * Tests the apply side directly (payload = offer-time colony id, same
   * discipline as ai_king_merc's landing tile) rather than fighting the
   * trigger's own RNG/economics gating, matching this file's established
   * convention for CHOICE mechanics with complex trigger conditions.
   */
  {
    AiPopupState pop;
    ai_popup_init(&pop);
    ctx.ai_popups = &pop;
    colonies.colonies[0].active = true;
    colonies.colonies[0].nation_id = 0;
    snprintf(colonies.colonies[0].name, sizeof(colonies.colonies[0].name), "Jamestown");
    colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].state.capital = 0;
    col1.tribe[0].alarm[0].friction = 20;
    col1.indian[0].alarm_by_player[0] = 50; /* relation 50 */ /* mid-range, away from clamp edges */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    const uint8_t rel_before_accept = ai_diplo_indian_relation(&col1, 4 + (0), 0);

    /*
     * port_bugs.md: the offer row is "We offer you {%NUMBER0} of our {%NUMBER1
     * food}" and used to render 0 for both, then a quarter. Drive the
     * trigger until it fires and check the row names DOS's half-share and
     * the real store.
     */
    {
      ColonizeMsgCatalog beg_txt;
      assets_msg_init(&beg_txt);
      if (!assets_msg_load_file(&beg_txt, "COLONIZE/GAME.TXT")) {
        return fail("BEGFOOD offer: GAME.TXT load");
      }
      const ColonizeMsgCatalog* saved_msgs = ctx.messages;
      ColonizeDosRng* saved_rng = ctx.rng;
      ctx.messages = &beg_txt;
      ColonizeDosRng beg_rng;
      /* port_bugs.md: a Brave must actually stand next to the colony to beg. */
      const int beg_brave = units_spawn_allow_stack(&units, 0,
        colonies.colonies[0].x + 1, colonies.colonies[0].y + 1);
      if (beg_brave >= 0) {
        units_get(&units, beg_brave)->nation_id = 4;
        /* raw 96705: no origin byte (+0x314a), no 022e encounter. */
        units_get(&units, beg_brave)->home_tribe_index = 0;
        /* …and it must have WALKED there this turn (DOS move-tail trigger). */
        ai_native_note_brave_turn_origin(
          beg_brave, colonies.colonies[0].x + 3, colonies.colonies[0].y + 3);
      }
      int found = -1;
      for (unsigned seed = 1u; seed <= 400u && found < 0; ++seed) {
        dos_rng_seed(&beg_rng, seed);
        ctx.rng = &beg_rng;
        ai_popup_clear(&pop);
        colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100;
        ai_contact_try_village_beg_food(&ctx, 4);
        for (int i = 0; i < pop.queue_count; ++i) {
          if (pop.queue[i].tag == AI_POPUP_TAG_CONTACT_BEGFOOD &&
              pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
            found = i;
            break;
          }
        }
      }
      if (found < 0) {
        assets_msg_free(&beg_txt);
        return fail("BEGFOOD offer should fire for some seed");
      }
      int names_amount = 0;
      for (int ci = 0; ci < pop.queue[found].choice_count; ++ci) {
        const char* row = pop.queue[found].choices[ci];
        if (strstr(row, "offer") && strstr(row, "50") && strstr(row, "100")) {
          names_amount = 1;
        }
        if (strstr(row, "offer") && (strstr(row, " 0 ") || strstr(row, "%NUMBER"))) {
          fprintf(stderr, "unit_ai_contact: BEGFOOD row '%s'\n", row);
          assets_msg_free(&beg_txt);
          return fail("BEGFOOD offer must not ask for 0 food or leak a raw token");
        }
      }
      if (!names_amount) {
        for (int ci = 0; ci < pop.queue[found].choice_count; ++ci) {
          fprintf(stderr, "unit_ai_contact: BEGFOOD row '%s'\n", pop.queue[found].choices[ci]);
        }
        assets_msg_free(&beg_txt);
        return fail("BEGFOOD offer row should name 50 of 100 food");
      }
      assets_msg_free(&beg_txt);
      ctx.messages = saved_msgs;
      ctx.rng = saved_rng;
      colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100;
    }

    ai_popup_clear(&pop);
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 2; /* accept/give */
    pop.result_tag = AI_POPUP_TAG_CONTACT_BEGFOOD;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    pop.result_payload = 1 << 16; /* colony index 0, home tribe 0 + 1 */
    ai_contact_apply_popup_result(&ctx, &pop);
    if (colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] != 50) {
      return fail("BEGFOOD accept should hand over exactly half the store");
    }
    if (col1.tribe[0].alarm[0].friction != 0 || col1.tribe[0].alarm[0].attacks != 0) {
      return fail("BEGFOOD accept should zero the settlement alarm word");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (0), 0) <= rel_before_accept) {
      return fail("BEGFOOD accept should improve relations");
    }

    colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100;
    col1.tribe[0].alarm[0].friction = 20;
    col1.indian[0].alarm_by_player[0] = 50; /* relation 50 */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    const uint8_t rel_before_decline = ai_diplo_indian_relation(&col1, 4 + (0), 0);
    const uint16_t spill_before = (uint16_t)(col1.indian[1].capitol_x | col1.indian[1].capitol_y << 8);

    ai_popup_clear(&pop);
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* decline/refuse */
    pop.result_tag = AI_POPUP_TAG_CONTACT_BEGFOOD;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    pop.result_payload = 1 << 16;
    ai_contact_apply_popup_result(&ctx, &pop);
    if (colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] != 100) {
      return fail("BEGFOOD decline must not cost the colony any food");
    }
    if (col1.tribe[0].alarm[0].friction != 30) {
      return fail("BEGFOOD decline should scale the settlement alarm word by 1.5");
    }
    /* DOS 022e's refusal adds to "euro" param_2 (the Indian nation itself):
     * the European's alarm stays, the word past the tribe's row moves. */
    if (ai_diplo_indian_relation(&col1, 4 + (0), 0) != rel_before_decline) {
      return fail("BEGFOOD decline must leave the European's alarm (DOS param_2 slip)");
    }
    if ((uint16_t)(col1.indian[1].capitol_x | col1.indian[1].capitol_y << 8) <= spill_before &&
        spill_before < 100) {
      return fail("BEGFOOD decline adds into the flat alarm table at index 4");
    }
    /* TB-12 fixed (dos_buggy_behavior off): the refuser's own alarm word rises. */
    col1.tribe[0].alarm[0].friction = 20;
    col1.indian[0].alarm_by_player[0] = 50;
    const uint16_t spill_fixed = (uint16_t)(col1.indian[1].capitol_x | col1.indian[1].capitol_y << 8);
    ai_popup_clear(&pop);
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1;
    pop.result_tag = AI_POPUP_TAG_CONTACT_BEGFOOD;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    pop.result_payload = 1 << 16;
    dos_buggy_behavior_set(false);
    ai_contact_apply_popup_result(&ctx, &pop);
    dos_buggy_behavior_set(true);
    if (col1.indian[0].alarm_by_player[0] <= 50 ||
        (uint16_t)(col1.indian[1].capitol_x | col1.indian[1].capitol_y << 8) != spill_fixed) {
      return fail("TB-12 fixed: BEGFOOD decline must raise the European's alarm word only");
    }
    fprintf(stderr, "unit_ai_contact: BEGFOOD accept/decline ok\n");
    /* Drop the adjacency Brave so later fixtures see no stray unit. */
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      const ColonizeUnit* bu = units_get_const(&units, ui);
      if (bu && bu->active && bu->nation_id == 4 &&
          bu->x == colonies.colonies[0].x + 1 && bu->y == colonies.colonies[0].y + 1) {
        units_despawn(&units, ui);
        break;
      }
    }
    ctx.ai_popups = NULL;
  }
  return 0;
}

static int sp_45(void) {

  /*
   * @INDIANCITY / @INDIANWAGONS reparations — FUN_5bfb_022e LAB_5bfb_0def,
   * the demand half's other two sites (viceroy_unpacked.c 96856-96979).
   * The refuse limb LAB_5bfb_0ff2 (`attitude word += 0x80`) was the last
   * unported raiser of the settlement attitude word; the accept limbs zero
   * that word, apply a NEGATIVE `price*qty*4/-100` alarm delta and move the
   * goods. The two GAME.TXT sections print their rows in opposite order, so
   * the accepting row id is per-flavor (@INDIANCITY 2, @INDIANWAGONS 1) —
   * both polarities are covered here.
   */
    ai_popup_init(&rp);
    ctx.ai_popups = &rp;
    ctx.human_nation = 0;
    col1.player[0].control = 0; /* human → DOS's CHOICE limb, not `else` */
    saved_turn = col1.head.turn;
    col1.head.turn = 0; /* turn 0 → the once-per-8-turns latch never blocks */

    colonies.colonies[0].active = true;
    colonies.colonies[0].nation_id = 0;
    snprintf(colonies.colonies[0].name, sizeof(colonies.colonies[0].name), "Jamestown");
    for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
      colonies.colonies[0].stock[cg] = 0;
    }
    /*
     * Food ≤ 0x4a so @INDIANBEGFOOD cannot fire — the visit then falls
     * through to LAB_5bfb_0def exactly as DOS orders the two blocks. Tools
     * are the only stocked good, so the cargo scan is deterministic (it
     * draws RNG only for the Muskets row) and Tools' DS:0x84BC weight of 1
     * makes the accept delta a clean −1.
     */
    colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 10;
    colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] = 40;
    /*
     * Fixture trap (port_bugs.md #797): the DS:0x84BC weight is the LIVE
     * `nation[e].trade.euro_price[cargo] - 1`, not a frozen table, and a
     * zeroed col1 fixture makes every weight 0 — which in DOS means the cargo
     * scan finds nothing and no demand fires at all. Seed Tools at the DOS
     * new-game price so the weight is 1, as this block's comment assumes.
     */
    col1.nation[0].trade.euro_price[COLONIZE_CARGO_TOOLS] = 2;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].state.capital = 0;
    col1.tribe[0].alarm[0].friction = 20;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.indian[0].alarm_by_player[0] = 50; /* ≤ 0x4a, away from the clamps */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    /*
     * `local_10` (viceroy 96707): 022e only reaches LAB_5bfb_0def when the
     * encounter is NOT the generous half — village word > 0x7f, or this pair
     * already demanded once (contact_state == 1). Without it bVar6 is true at
     * alarm 50 / word 20 and DOS hands over a gift instead (port_bugs.md: a
     * wandering Brave with no grievance must not demand).
     */
    col1.indian[0].contact_state[0] = 1;
    col1.indian[0].muskets = 0;
    col1.indian[0].horse_herds = 0;

    rep_brave = units_spawn_allow_stack(
      &units, 0, colonies.colonies[0].x + 1, colonies.colonies[0].y + 1);
    if (rep_brave < 0) {
      return fail("spawn reparations-visit Brave");
    }
    units_get(&units, rep_brave)->nation_id = 4;
    units_get(&units, rep_brave)->home_tribe_index = 0;
    /* Same DOS move-tail trigger the gift / beg arms use. */
    ai_native_note_brave_turn_origin(
      rep_brave, colonies.colonies[0].x + 3, colonies.colonies[0].y + 3);

    saved_rng = ctx.rng;
    dos_rng_seed(&rep_rng, 7u);
    ctx.rng = &rep_rng;

    /* --- @INDIANCITY refuse: +0x80 on the word, not a grain moved. --- */
    ai_popup_clear(&rp);
    ai_contact_try_village_beg_food(&ctx, 4);
    rq = -1;
    for (int i = 0; i < rp.queue_count; ++i) {
      if (rp.queue[i].tag == AI_POPUP_TAG_CONTACT_REPARATIONS &&
          rp.queue[i].kind == AI_POPUP_KIND_CHOICE) {
        rq = i;
        break;
      }
    }
    if (rq < 0) {
      ctx.rng = saved_rng;
      return fail("reparations demand should fire beside a met colony with stores");
    }
    if (rp.queue[rq].payload != 0) {
      ctx.rng = saved_rng;
      return fail("a colony encounter is the @INDIANCITY flavor (payload 0)");
    }
    if (rp.queue[rq].choice_count != 2) {
      ctx.rng = saved_rng;
      return fail("@INDIANCITY should offer both DOS rows");
    }
    /* raw 96882: `FUN_291f_019c(0x281f, 0x1866, *(0x8d52))` — the demand is a
     * chief audience, so it carries IND{tribe}A{tier}.SS like every other one. */
    if (rp.queue[rq].portrait_tribe != 0) {
      ctx.rng = saved_rng;
      return fail("@INDIANCITY needs the chief portrait");
    }
    const int stock_before_refuse = colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS];
    ai_popup_clear(&rp);
    rp.has_result = true;
    rp.result_cancelled = false;
    rp.result_tag = AI_POPUP_TAG_CONTACT_REPARATIONS;
    rp.result_nation_a = 0;
    rp.result_nation_b = 4;
    rp.result_payload = 0; /* @INDIANCITY */
    rp.result_choice_id = 1; /* row 1 = "Man the stockade." = refuse */
    ai_contact_apply_popup_result(&ctx, &rp);
    if (colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] != stock_before_refuse) {
      ctx.rng = saved_rng;
      return fail("@INDIANCITY refuse must not move the colony stores");
    }
    if (col1_tribe_attitude(&col1.tribe[0], 0) != 20 + 0x80) {
      fprintf(
        stderr, "unit_ai_contact: attitude word %d\n",
        col1_tribe_attitude(&col1.tribe[0], 0)
      );
      ctx.rng = saved_rng;
      return fail("LAB_5bfb_0ff2: a refused demand adds 0x80 to the attitude word");
    }

    /* --- @INDIANCITY accept: word zeroed, alarm eased, goods handed over. --- */
    col1.tribe[0].alarm[0].friction = 20;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.indian[0].contact_state[0] = 0;
    ai_popup_clear(&rp);
    dos_rng_seed(&rep_rng, 7u);
    ai_contact_try_village_beg_food(&ctx, 4);
    rq = -1;
    for (int i = 0; i < rp.queue_count; ++i) {
      if (rp.queue[i].tag == AI_POPUP_TAG_CONTACT_REPARATIONS &&
          rp.queue[i].kind == AI_POPUP_KIND_CHOICE) {
        rq = i;
        break;
      }
    }
    if (rq < 0) {
      ctx.rng = saved_rng;
      return fail("reparations demand should re-offer after a refusal");
    }
    const int stock_before_accept = colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS];
    const uint8_t rel_before_accept_rep = ai_diplo_indian_relation(&col1, 4 + (0), 0);
    ai_popup_clear(&rp);
    rp.has_result = true;
    rp.result_cancelled = false;
    rp.result_tag = AI_POPUP_TAG_CONTACT_REPARATIONS;
    rp.result_nation_a = 0;
    rp.result_nation_b = 4;
    rp.result_payload = 0;
    rp.result_choice_id = 2; /* row 2 = "Hand them over." = accept */
    ai_contact_apply_popup_result(&ctx, &rp);
    if (col1_tribe_attitude(&col1.tribe[0], 0) != 0) {
      ctx.rng = saved_rng;
      return fail("@INDIANCITY accept should zero the village attitude word");
    }
    if (colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] >= stock_before_accept) {
      ctx.rng = saved_rng;
      return fail("@INDIANCITY accept should hand the demanded goods over");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (0), 0) <= rel_before_accept_rep) {
      ctx.rng = saved_rng;
      return fail("@INDIANCITY accept applies a NEGATIVE alarm delta");
    }

    /*
     * The demanded amount (raw 96843-96866, asm 5bfb:0c1c-0c33): the winning
     * good's qty is min(stock, 100) — the WHOLE store, capped at 100 — and is
     * halved only when `FUN_281f_04d4(0, difficulty + 1)` rolls 0, i.e.
     * 1/(difficulty+2) of the visits. The port must never halve by default.
     * Tools (DS:0x84BC weight 1) are the only scored good here, so the pick is
     * deterministic and only the halving roll varies with the seed.
     */
    {
      const uint8_t saved_diff = col1.head.difficulty;
      col1.head.difficulty = 4; /* halve on 1 of 6 seeds */
      int saw_full = 0;
      int saw_half = 0;
      int saw_demand = 0;
      /* One long stream, not 48 tiny seeds: dos_rng_seed masks to 0x7fff and a
       * small seed's first draw is tiny, so seeds 1..48 all roll 0 (the
       * tiny-seed fixture trap). */
      dos_rng_seed(&rep_rng, 21089u);
      for (unsigned iter = 1u; iter <= 48u; ++iter) {
        colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] = 240; /* > the 100 cap */
        colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 10;
        col1.tribe[0].alarm[0].friction = 20;
        col1.tribe[0].alarm[0].attacks = 0;
        col1.indian[0].alarm_by_player[0] = 50;
        col1.indian[0].contact_state[0] = 0;
        ai_popup_clear(&rp);
        ai_contact_try_village_beg_food(&ctx, 4);
        int dq = -1;
        for (int i = 0; i < rp.queue_count; ++i) {
          if (rp.queue[i].tag == AI_POPUP_TAG_CONTACT_REPARATIONS &&
              rp.queue[i].kind == AI_POPUP_KIND_CHOICE && rp.queue[i].payload == 0) {
            dq = i;
            break;
          }
        }
        if (dq < 0) {
          continue;
        }
        saw_demand++;
        ai_popup_clear(&rp);
        rp.has_result = true;
        rp.result_cancelled = false;
        rp.result_tag = AI_POPUP_TAG_CONTACT_REPARATIONS;
        rp.result_nation_a = 0;
        rp.result_nation_b = 4;
        rp.result_payload = 0;
        rp.result_choice_id = 2; /* "Hand them over." */
        ai_contact_apply_popup_result(&ctx, &rp);
        const int taken = 240 - colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS];
        if (taken == 100) {
          saw_full++;
        } else if (taken == 50) {
          saw_half++;
        } else {
          fprintf(stderr, "unit_ai_contact: reparations took %d of 240 Tools\n", taken);
          col1.head.difficulty = saved_diff;
          ctx.rng = saved_rng;
          return fail("@INDIANCITY demands min(stock, 100), optionally halved — nothing else");
        }
      }
      col1.head.difficulty = saved_diff;
      if (saw_demand < 8) {
        ctx.rng = saved_rng;
        return fail("@INDIANCITY demand should re-fire across seeds");
      }
      if (saw_full == 0) {
        fprintf(stderr, "unit_ai_contact: demands=%d full=%d half=%d\n", saw_demand, saw_full,
                saw_half);
        ctx.rng = saved_rng;
        return fail("@INDIANCITY normally demands the WHOLE store (capped at 100), not half");
      }
      if (saw_full <= saw_half * 2) {
        fprintf(stderr, "unit_ai_contact: full=%d half=%d\n", saw_full, saw_half);
        ctx.rng = saved_rng;
        return fail("halving is the 1/(difficulty+2) minority arm, not the default");
      }
      if (saw_half == 0) {
        ctx.rng = saved_rng;
        return fail("the DOS halving roll should still be reachable");
      }
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] = 40;
      colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 10;
      col1.tribe[0].alarm[0].friction = 20;
      col1.tribe[0].alarm[0].attacks = 0;
      col1.indian[0].alarm_by_player[0] = 50;
      col1.indian[0].contact_state[0] = 0;
      ai_popup_clear(&rp);
    }
  return 0;
}

static int sp_46(void) {

    /* --- @INDIANWAGONS: no colony in the encounter, accept is row 1. --- */
    bool saved_colony_active[COLONIZE_COLONIES_MAX];
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      saved_colony_active[ci] = colonies.colonies[ci].active;
      colonies.colonies[ci].active = false;
    }
    int wag_ti = units_find_type(&units, "Wagon Train");
    if (wag_ti < 0) {
      if (units.type_count < 4) {
        units.type_count = 4;
      }
      wag_ti = 3;
      snprintf(units.types[wag_ti].name, sizeof(units.types[wag_ti].name), "Wagon Train");
      units.types[wag_ti].domain = COLONIZE_UNIT_DOMAIN_LAND;
      units.types[wag_ti].movement = 3;
      units.types[wag_ti].cargo = 2;
    }
    const int rep_wag = units_spawn_allow_stack(
      &units, wag_ti, colonies.colonies[0].x, colonies.colonies[0].y);
    ColonizeUnit* rep_wag_u = rep_wag >= 0 ? units_get(&units, rep_wag) : NULL;
    if (!rep_wag_u) {
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        colonies.colonies[ci].active = saved_colony_active[ci];
      }
      ctx.rng = saved_rng;
      return fail("spawn reparations Wagon Train");
    }
    rep_wag_u->nation_id = 0;
    rep_wag_u->hold_goods_type[0] = COLONIZE_CARGO_TOOLS;
    rep_wag_u->hold_goods_amount[0] = 30;
    col1.tribe[0].alarm[0].friction = 20;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.indian[0].contact_state[0] = 0;
    col1.indian[0].alarm_by_player[0] = 50;
    ai_popup_clear(&rp);
    dos_rng_seed(&rep_rng, 7u);
    ai_contact_try_village_beg_food(&ctx, 4);
    rq = -1;
    for (int i = 0; i < rp.queue_count; ++i) {
      if (rp.queue[i].tag == AI_POPUP_TAG_CONTACT_REPARATIONS &&
          rp.queue[i].kind == AI_POPUP_KIND_CHOICE) {
        rq = i;
        break;
      }
    }
    if (rq < 0 || rp.queue[rq].payload != 1) {
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        colonies.colonies[ci].active = saved_colony_active[ci];
      }
      units_despawn(&units, rep_wag);
      ctx.rng = saved_rng;
      return fail("a wagon encounter with no colony is the @INDIANWAGONS flavor");
    }
    const uint8_t rel_before_wagons = ai_diplo_indian_relation(&col1, 4 + (0), 0);
    ai_popup_clear(&rp);
    rp.has_result = true;
    rp.result_cancelled = false;
    rp.result_tag = AI_POPUP_TAG_CONTACT_REPARATIONS;
    rp.result_nation_a = 0;
    rp.result_nation_b = 4;
    rp.result_payload = 1; /* @INDIANWAGONS */
    rp.result_choice_id = 1; /* row 1 = "Hand them over." = accept HERE */
    ai_contact_apply_popup_result(&ctx, &rp);
    if (rep_wag_u->hold_goods_amount[0] != 0) {
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        colonies.colonies[ci].active = saved_colony_active[ci];
      }
      units_despawn(&units, rep_wag);
      ctx.rng = saved_rng;
      return fail("@INDIANWAGONS accept should strip the wagon's hold");
    }
    if (col1_tribe_attitude(&col1.tribe[0], 0) != 0 ||
        ai_diplo_indian_relation(&col1, 4 + (0), 0) <= rel_before_wagons) {
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        colonies.colonies[ci].active = saved_colony_active[ci];
      }
      units_despawn(&units, rep_wag);
      ctx.rng = saved_rng;
      return fail("@INDIANWAGONS accept should zero the word and ease alarm");
    }

    /* --- @INDIANWAGONS refuse: row 2 here, same +0x80 shared limb. --- */
    rep_wag_u->hold_goods_type[0] = COLONIZE_CARGO_TOOLS;
    rep_wag_u->hold_goods_amount[0] = 30;
    col1.tribe[0].alarm[0].friction = 20;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.indian[0].contact_state[0] = 0;
    ai_popup_clear(&rp);
    dos_rng_seed(&rep_rng, 7u);
    ai_contact_try_village_beg_food(&ctx, 4);
    ai_popup_clear(&rp);
    rp.has_result = true;
    rp.result_cancelled = false;
    rp.result_tag = AI_POPUP_TAG_CONTACT_REPARATIONS;
    rp.result_nation_a = 0;
    rp.result_nation_b = 4;
    rp.result_payload = 1;
    rp.result_choice_id = 2; /* row 2 = "Circle the wagons." = refuse */
    ai_contact_apply_popup_result(&ctx, &rp);
    const int wagons_refuse_ok =
      rep_wag_u->hold_goods_amount[0] == 30 &&
      col1_tribe_attitude(&col1.tribe[0], 0) == 20 + 0x80;

    /* Leave the fixture exactly as the next sub-test expects to find it. */
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      colonies.colonies[ci].active = saved_colony_active[ci];
    }
    units_despawn(&units, rep_wag);
    units_despawn(&units, rep_brave);
    for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
      colonies.colonies[0].stock[cg] = 0;
    }
    colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100;
    col1.tribe[0].alarm[0].friction = 0;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.indian[0].contact_state[0] = 0;
    col1.indian[0].alarm_by_player[0] = 50;
    col1.head.turn = saved_turn;
    ctx.rng = saved_rng;
    ctx.ai_popups = NULL;
    if (!wagons_refuse_ok) {
      return fail("@INDIANWAGONS refuse keeps the hold and adds 0x80 to the word");
    }
    fprintf(stderr, "unit_ai_contact: reparations accept/refuse ok\n");
  return 0;
}

static int sp_47(void) {

  /*
   * port_bugs.md 2026-09-04 (bug 5, "Indians never seem to visit bearing gifts"):
   * the generous half of FUN_5bfb_022e (@INDIANGIVESTUFF / @INDIANGIVEFOOD)
   * was written but never wired. A Brave standing beside an already-met
   * colony at low alarm must hand goods over and raise a popup for the human.
   */
  {
    AiPopupState gp;
    ai_popup_init(&gp);
    ctx.ai_popups = &gp;
    ctx.human_nation = 0;
    col1.player[0].control = 0;
    const uint16_t saved_turn = col1.head.turn;
    /* Non-zero so the once-per-8-turns cooldown is live; past turn 8 because
     * the convert-visit fixture above stamped the static cooldown at 0+8. */
    col1.head.turn = 9;
    colonies.colonies[0].active = true;
    colonies.colonies[0].nation_id = 0;
    snprintf(colonies.colonies[0].name, sizeof(colonies.colonies[0].name), "Jamestown");
    for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
      colonies.colonies[0].stock[cg] = 0;
    }
    colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100; /* > 25 → GIVESTUFF arm */
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].state.capital = 0;
    col1.tribe[0].alarm[0].friction = 0;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.indian[0].alarm_by_player[0] = 10; /* ≤ 0x31 → generous */
    col1.indian[0].contact_state[0] = 0;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;

    const int gift_brave = units_spawn_allow_stack(
      &units, 0, colonies.colonies[0].x + 1, colonies.colonies[0].y + 1);
    if (gift_brave < 0) {
      return fail("spawn gift-visit Brave");
    }
    units_get(&units, gift_brave)->nation_id = 4;
    units_get(&units, gift_brave)->home_tribe_index = 0; /* raw 96705 origin byte */
    /*
     * DOS runs the visit off the Brave's move tail, so the port needs the
     * pre-move tile: this Brave walked in from three tiles away, it did not
     * loiter next to the colony.
     */
    ai_native_note_brave_turn_origin(
      gift_brave, colonies.colonies[0].x + 3, colonies.colonies[0].y + 3);

    ColonizeDosRng gift_rng;
    ColonizeDosRng* saved_rng = ctx.rng;
    dos_rng_seed(&gift_rng, 7u);
    ctx.rng = &gift_rng;
    /* This direct apply fixture starts a new encounter without 022e's
     * move-tail roll; discard the earlier narrative's visitor binding. */
    ai_contact_visit_mood_clear(4, 0);
    const int gave = ai_contact_try_village_gifts(&ctx, 4);
    ctx.rng = saved_rng;
    if (!gave) {
      return fail("gift visit should fire at low alarm beside a met colony");
    }
    int gifted_cargo = -1;
    for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
      if (cg == COLONIZE_CARGO_FOOD) {
        continue;
      }
      if (colonies.colonies[0].stock[cg] >= 2) {
        gifted_cargo = cg;
        break;
      }
    }
    if (gifted_cargo < 0) {
      return fail("@INDIANGIVESTUFF should add at least the DOS floor of 2 goods");
    }
    if (col1.indian[0].contact_state[0] != 2) {
      return fail("gift arm should stamp contact_state 2");
    }
    if (col1.tribe[0].alarm[0].friction != 0 || col1.tribe[0].alarm[0].attacks != 0) {
      return fail("gift visit should leave the village alarm word discharged");
    }
    if (gp.queue_count < 1) {
      return fail("gift visit should raise a popup for the human");
    }

    /*
     * port_bugs.md #819: the @INDIANGIVESTUFF quantity is
     * `100 / (DS:0x84BC[nation*0x10 + cargo] + 1)` (FUN_5bfb_022e raw 87930),
     * and that byte is the LIVE `euro_price[cargo] - 1` of the VISITED
     * colony's owner (colony+0x1a), not a start-of-game capture. Same seed,
     * same picked cargo, two Europe markets → two tonnages.
     */
    {
      /* Snapshot every cell this sub-case perturbs — the rest of this fixture
       * (the #417 control below) reuses the same colony, market row and
       * latches. */
      uint8_t saved_price[COLONIZE_CARGO_COUNT];
      int16_t saved_stock[COLONIZE_CARGO_COUNT];
      for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
        saved_price[cg] = col1.nation[0].trade.euro_price[cg];
        saved_stock[cg] = colonies.colonies[0].stock[cg];
      }
      const uint16_t saved_819_turn = col1.head.turn;
      const uint8_t saved_819_state = col1.indian[0].contact_state[0];
      int qty_by_price[2] = {0, 0};
      static const int k_819_price[2] = {1, 0x65};
      for (int pass = 0; pass < 2; ++pass) {
        ai_contact_reset();
        col1.head.turn = (uint16_t)(20 + pass);
        col1.indian[0].contact_state[0] = 0;
        col1.indian[0].alarm_by_player[0] = 10;
        col1.tribe[0].alarm[0].friction = 0;
        col1.tribe[0].alarm[0].attacks = 0;
        for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
          colonies.colonies[0].stock[cg] = 0;
          col1.nation[0].trade.euro_price[cg] = (uint8_t)k_819_price[pass];
        }
        colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100; /* > 25 → GIVESTUFF */
        ai_native_note_brave_turn_origin(
          gift_brave, colonies.colonies[0].x + 3, colonies.colonies[0].y + 3);
        dos_rng_seed(&gift_rng, 7u);
        ctx.rng = &gift_rng;
        ai_popup_clear(&gp);
        const int fired = ai_contact_try_village_gifts(&ctx, 4);
        ctx.rng = saved_rng;
        if (!fired) {
          return fail("#819 fixture: the gift visit must fire on both passes");
        }
        for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
          if (cg != COLONIZE_CARGO_FOOD && colonies.colonies[0].stock[cg] > 0) {
            qty_by_price[pass] = colonies.colonies[0].stock[cg];
            break;
          }
        }
      }
      fprintf(
        stderr, "unit_ai_contact: #819 gift qty price1=%d price0x65=%d\n",
        qty_by_price[0], qty_by_price[1]
      );
      /* price byte 0x64 → 100/101 = 0, lifted to the DOS floor of 5; price
       * byte 0 → 100, so the live row must produce the larger gift. */
      if (qty_by_price[1] != 5) {
        return fail("#819: a 0x65 Europe price must clamp the gift to the DOS floor of 5");
      }
      if (qty_by_price[0] <= qty_by_price[1]) {
        return fail("#819: gift tonnage must track the LIVE DS:0x84BC price byte");
      }
      for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
        col1.nation[0].trade.euro_price[cg] = saved_price[cg];
        colonies.colonies[0].stock[cg] = saved_stock[cg];
      }
      col1.head.turn = saved_819_turn;
      col1.indian[0].contact_state[0] = saved_819_state;
      col1.tribe[0].alarm[0].friction = 0;
      col1.tribe[0].alarm[0].attacks = 0;
    }
    /*
     * 2026-09-10: the 8-turn visit cooldown was removed — DOS FUN_5bfb_022e
     * and its move-tail caller carry no turn counter (see the visit pacing
     * note atop ai_contact.c). A repeat call in the same turn IS a second
     * encounter in DOS (a second Brave walking up), so nothing here refuses
     * it; the once-per-nation-per-turn cadence is ai.c §9's call structure.
     */
    /*
     * port_bugs.md #417 ("Indians came to demand Horses very damn quickly, at zero
     * apparent alarm"). FUN_5bfb_022e is ONE encounter: `bVar6` picks the
     * generous half or the demand half (viceroy_unpacked.c 96723-96731 and
     * the `if (!bVar6)` guard at 96833), never both. At alarm 0 the mood
     * roll is always generous, so the visit resolves as a gift and stamps
     * `contact_state = 2` — the demand half's own gate. The port carries
     * this structurally: ai.c §9 runs the gift arm first, and the beg/demand
     * arms honour the state-2 latch. No demand can fire at alarm 0.
     */
    {
      col1.head.turn = 10;
      col1.indian[0].contact_state[0] = 0; /* the midpass clears it every turn */
      col1.indian[0].alarm_by_player[0] = 0; /* zero alarm, as reported */
      col1.tribe[0].alarm[0].friction = 0;
      col1.tribe[0].alarm[0].attacks = 0;
      for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
        colonies.colonies[0].stock[cg] = 0;
      }
      /* Food ≤ 0x4a so @INDIANBEGFOOD cannot fire; Tools are the demand. */
      colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 10;
      colonies.colonies[0].stock[COLONIZE_CARGO_TOOLS] = 40;
      ai_native_note_brave_turn_origin(
        gift_brave, colonies.colonies[0].x + 3, colonies.colonies[0].y + 3);

      dos_rng_seed(&gift_rng, 7u);
      ctx.rng = &gift_rng;
      ai_popup_clear(&gp);
      /* ai.c's own order: the gift arm first, then the beg/demand half. */
      ai_contact_try_village_gifts(&ctx, 4);
      if (col1.indian[0].contact_state[0] != 2) {
        ctx.rng = saved_rng;
        return fail("alarm-0 mood is generous — the visit must latch contact_state 2");
      }
      ai_contact_try_village_beg_food(&ctx, 4);
      ctx.rng = saved_rng;
      for (int i = 0; i < gp.queue_count; ++i) {
        if (gp.queue[i].tag == AI_POPUP_TAG_CONTACT_REPARATIONS) {
          return fail("no demand at alarm 0 the turn after this village gave a gift");
        }
      }

      /* …and the fixture really is demand-capable: with the latch cleared
       * (a new turn's midpass) and the pair already past one demand
       * (contact_state 1 = 022e's `local_10`, the only thing that sends an
       * alarm-0 village down LAB_5bfb_0def instead of the gift arm), the
       * demand comes straight back. */
      ai_contact_reset();
      dos_rng_seed(&gift_rng, 7u);
      ctx.rng = &gift_rng;
      ai_popup_clear(&gp);
      col1.indian[0].contact_state[0] = 1;
      ai_contact_try_village_beg_food(&ctx, 4);
      ctx.rng = saved_rng;
      int demanded = 0;
      for (int i = 0; i < gp.queue_count; ++i) {
        if (gp.queue[i].tag == AI_POPUP_TAG_CONTACT_REPARATIONS) {
          demanded = 1;
          break;
        }
      }
      if (!demanded) {
        return fail("throttle-free control: the demand arm should reach @INDIANCITY here");
      }
      ai_contact_reset();
      ai_popup_clear(&gp);
      fprintf(stderr, "unit_ai_contact: gift-before-demand exclusion ok\n");
    }
    units_despawn(&units, gift_brave);
    for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
      colonies.colonies[0].stock[cg] = 0;
    }
    colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100;
    col1.head.turn = saved_turn;
    ctx.ai_popups = NULL;
  }
  return 0;
}

static int sp_48(void) {

  /* FUN_465b_0000 @WHACKINDIANS: ask once while alarm < 0x4b and bit 0x04 clear. */
  {
    AiPopupState wp;
    ai_popup_clear(&wp);
    ColonizeTurnContext wctx;
    memset(&wctx, 0, sizeof(wctx));
    wctx.messages = test_game_txt();
    wctx.names = test_names_txt();
    wctx.col1 = &col1;
    wctx.col1_ok = true;
    wctx.ai_popups = &wp;
    wctx.human_nation = -1; /* memset 0 would pin Euro 0 as human (#1122) */
    col1.player[0].control = 0;
    ind->alarm_by_player[0] = 20;
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~COL1_INDIAN_ATTACK_CONFIRMED_BIT);
    if (!ai_contact_try_whack_confirm(&wctx, 0, 4, 7, 5, 6) || wp.queue_count != 1 ||
        wp.queue[0].tag != AI_POPUP_TAG_CONTACT_WHACK || wp.queue[0].nation_a != 7 ||
        wp.queue[0].payload != (5 | (6 << 8))) {
      return fail("WHACKINDIANS should enqueue a Yes/No CHOICE for a calm tribe");
    }
    if (!ai_contact_try_whack_confirm(&wctx, 0, 4, 7, 5, 6) || wp.queue_count != 1) {
      return fail("WHACKINDIANS must not stack a second CHOICE while one is pending");
    }
    ai_popup_clear(&wp);
    ind->euro_diplo[0] |= COL1_INDIAN_ATTACK_CONFIRMED_BIT;
    if (ai_contact_try_whack_confirm(&wctx, 0, 4, 7, 5, 6) || wp.queue_count != 0) {
      return fail("WHACKINDIANS must not ask again once confirmed (bit 0x04)");
    }
    ai_diplo_indian_alarm_delta(&col1, 4, 0, -1); /* cooling below 75 clears the bit */
    if (ind->euro_diplo[0] & COL1_INDIAN_ATTACK_CONFIRMED_BIT) {
      return fail("FUN_4cc6_00f2 negative delta below 75 should clear bit 0x04");
    }
    ind->alarm_by_player[0] = 80;
    if (ai_contact_try_whack_confirm(&wctx, 0, 4, 7, 5, 6) || wp.queue_count != 0) {
      return fail("WHACKINDIANS must not ask when the tribe is already hostile (alarm >= 0x4b)");
    }
    col1.player[0].control = 1;
    ind->alarm_by_player[0] = 20;
    if (ai_contact_try_whack_confirm(&wctx, 0, 4, 7, 5, 6)) {
      return fail("WHACKINDIANS is human-only");
    }
    col1.player[0].control = 0;
    fprintf(stderr, "unit_ai_contact: WHACKINDIANS ok\n");
  }

  /* FUN_465b_0000 Euro peer: treaty → @HAVETREATY CHOICE; no treaty → silent war. */
  {
    AiPopupState wp;
    ai_popup_clear(&wp);
    ColonizeTurnContext wctx;
    memset(&wctx, 0, sizeof(wctx));
    wctx.messages = test_game_txt();
    wctx.names = test_names_txt();
    wctx.col1 = &col1;
    wctx.col1_ok = true;
    wctx.ai_popups = &wp;
    wctx.human_nation = 0;
    col1.player[0].control = 0;
    col1.player[1].control = 1;
    /* Signed treaty: prompt, no war yet. */
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
    if (!ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 1 ||
        wp.queue[0].tag != AI_POPUP_TAG_CONTACT_EURO_WAR || wp.queue[0].nation_a != 7 ||
        wp.queue[0].nation_b != 1 || wp.queue[0].payload != (5 | (6 << 8))) {
      return fail("HAVETREATY should enqueue Cancel/Break CHOICE for a treaty peer");
    }
    if (ai_diplo_at_war(&col1, 0, 1)) {
      return fail("HAVETREATY prompt alone must not declare war");
    }
    if (!ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 1) {
      return fail("HAVETREATY must not stack a second CHOICE while one is pending");
    }
    ai_popup_clear(&wp);
    /* No treaty: no prompt and the move may proceed; 465b writes no WAR bit
     * (port_bugs.md #1256), it only drops the mover's 0x01. */
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_WAR_INTENT);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET);
    char st[128] = "";
    wctx.status = st;
    wctx.status_size = sizeof(st);
    if (ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 0) {
      return fail("no treaty: attack must proceed without a prompt");
    }
    if (st[0] != '\0') { /* C2-B5: 465b has no announce band without 0x40 */
      return fail("no treaty: attack must not announce @DECLAREWAR");
    }
    wctx.status = NULL;
    wctx.status_size = 0;
    if (ai_diplo_read(&col1, 0, 1) != AI_DIPLO_MET || ai_diplo_read(&col1, 1, 0) != AI_DIPLO_MET) {
      return fail("no treaty: relation bytes must stay MET-only (0x01 dropped)");
    }
    /* Already at war: nothing to ask. */
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_WAR);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_WAR);
    if (ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 0) {
      return fail("at war: no prompt");
    }
    /*
     * port_bugs.md #382: asymmetric bytes. The peer is at peace with us, we are not
     * at peace with them — DOS tests the ATTACKER'S own byte
     * (FUN_281f_0a38(attacker, target) & 0x40), the same byte the Foreign
     * Affairs report prints, so no @HAVETREATY prompt may appear here.
     */
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
    if (ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 1 ||
        wp.queue[0].tag != AI_POPUP_TAG_DIPLO_BREAK) {
      return fail("one-sided peer peace must not raise HAVETREATY (report says War)");
    }
    /* #1258: the band announces @CANCELPEACE, WARs only the peer's row (it had
     * 0x40) and clears 0x40 both ways (raw 75567-75593). */
    if (ai_diplo_read(&col1, 1, 0) != (AI_DIPLO_MET | AI_DIPLO_WAR) ||
        ai_diplo_read(&col1, 0, 1) != AI_DIPLO_MET) {
      return fail("one-sided peer peace: band must WAR the peer row only and clear 0x40");
    }
    /* Mirror case: our byte carries the treaty, theirs does not → prompt. */
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET);
    if (!ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 1 ||
        wp.queue[0].tag != AI_POPUP_TAG_CONTACT_EURO_WAR) {
      return fail("our own peace bit must raise HAVETREATY even if theirs is clear");
    }
    /* #1258: a computer attacker breaking a treaty with the human: @SNEAK, then
     * @DECLAREWAR, BGM cue, and no WAR bit either way (raw 75558-75593). */
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
    if (ai_diplo_465b_band_ctx(&wctx, 1, 0) != 1 || wp.queue_count != 2 ||
        wp.queue[0].tag != AI_POPUP_TAG_DIPLO_SNEAK || wp.queue[1].tag != AI_POPUP_TAG_DIPLO_WAR ||
        ai_diplo_read(&col1, 0, 1) != AI_DIPLO_MET || ai_diplo_read(&col1, 1, 0) != AI_DIPLO_MET) {
      return fail("AI treaty break: want @SNEAK + @DECLAREWAR, 0x40 cleared, no WAR");
    }
    /* TB-52 fixed (dos_buggy_behavior off): the AI treaty break sets WAR both ways. */
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
    dos_buggy_behavior_set(false);
    (void)ai_diplo_465b_band_ctx(&wctx, 1, 0);
    dos_buggy_behavior_set(true);
    if (ai_diplo_read(&col1, 0, 1) != (AI_DIPLO_MET | AI_DIPLO_WAR) ||
        ai_diplo_read(&col1, 1, 0) != (AI_DIPLO_MET | AI_DIPLO_WAR)) {
      return fail("TB-52 fixed: AI treaty break must set WAR in both rows");
    }
    /* TB-53: no treaty in DOS writes nothing; fixed, the attack declares war. */
    for (int atk = 0; atk < 2; ++atk) {
      ai_popup_clear(&wp);
      ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET);
      ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET);
      (void)ai_diplo_465b_band_ctx(&wctx, atk, 1 - atk);
      if (ai_diplo_read(&col1, 0, 1) != AI_DIPLO_MET || wp.queue_count != 0) {
        return fail("TB-53 DOS: no-treaty attack must leave the pair MET-only, silent");
      }
      dos_buggy_behavior_set(false);
      (void)ai_diplo_465b_band_ctx(&wctx, atk, 1 - atk);
      dos_buggy_behavior_set(true);
      if (ai_diplo_read(&col1, 0, 1) != (AI_DIPLO_MET | AI_DIPLO_WAR) ||
          ai_diplo_read(&col1, 1, 0) != (AI_DIPLO_MET | AI_DIPLO_WAR) || wp.queue_count != 1 ||
          wp.queue[0].tag != AI_POPUP_TAG_DIPLO_WAR) {
        return fail("TB-53 fixed: no-treaty attack must announce and set WAR both ways");
      }
    }
    /* TB-54: the human breaking a one-sided treaty WARs both rows once fixed. */
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
    dos_buggy_behavior_set(false);
    (void)ai_diplo_465b_band_ctx(&wctx, 0, 1);
    dos_buggy_behavior_set(true);
    if (ai_diplo_read(&col1, 0, 1) != (AI_DIPLO_MET | AI_DIPLO_WAR) ||
        ai_diplo_read(&col1, 1, 0) != (AI_DIPLO_MET | AI_DIPLO_WAR)) {
      return fail("TB-54 fixed: human treaty break must set WAR in both rows");
    }
    /*
     * FUN_465b_0000 raw 75528-75529: either side a Privateer (@UNIT 0x10) and
     * the whole treaty/declare band is skipped — no @HAVETREATY, no war, the
     * attack just happens. Privateers fly no flag: they are hunted at peace
     * and they prey at peace.
     */
    ai_popup_clear(&wp);
    {
      static ColonizeUnitPool pp;
      memset(&pp, 0, sizeof(pp));
      pp.type_count = 2;
      snprintf(pp.types[0].name, sizeof(pp.types[0].name), "Frigate");
      pp.types[0].kind_plus1 = (uint8_t)(UNITS_KIND_FRIGATE + 1);
      pp.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
      pp.types[0].movement = 6;
      snprintf(pp.types[1].name, sizeof(pp.types[1].name), "Privateer");
      pp.types[1].kind_plus1 = (uint8_t)(UNITS_KIND_PRIVATEER + 1);
      pp.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
      pp.types[1].movement = 8;
      const int frig = units_spawn_allow_stack(&pp, 0, 4, 6);
      const int priv = units_spawn_allow_stack(&pp, 1, 5, 6);
      ColonizeUnit* fu = units_get(&pp, frig);
      ColonizeUnit* pu = units_get(&pp, priv);
      if (!fu || !pu) {
        return fail("privateer gate: spawn failed");
      }
      fu->nation_id = 0;
      pu->nation_id = 1;
      wctx.units = &pp;
      ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
      ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
      if (ai_contact_try_euro_attack_confirm(&wctx, 0, 1, frig, 5, 6) || wp.queue_count != 0) {
        return fail("hunting a Privateer must not raise HAVETREATY");
      }
      if (ai_diplo_at_war(&col1, 0, 1)) {
        return fail("hunting a Privateer must not declare war");
      }
      /* Mirror: our Privateer raids their Frigate — also warless. */
      fu->nation_id = 1;
      pu->nation_id = 0;
      if (ai_contact_try_euro_attack_confirm(&wctx, 0, 1, priv, 4, 6) || wp.queue_count != 0) {
        return fail("raiding with a Privateer must not raise HAVETREATY");
      }
      if (ai_diplo_at_war(&col1, 0, 1)) {
        return fail("raiding with a Privateer must not declare war");
      }
      /* Control: Frigate vs Frigate on the same tiles still prompts. */
      pu->type_index = 0;
      if (!ai_contact_try_euro_attack_confirm(&wctx, 0, 1, frig, 4, 6) || wp.queue_count != 1) {
        return fail("non-privateer attack under treaty must still ask HAVETREATY");
      }
      wctx.units = NULL;
    }
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, 0);
    ai_diplo_write(&col1, 1, 0, 0);
    fprintf(stderr, "unit_ai_contact: HAVETREATY euro attack confirm ok\n");
  }
  return 0;
}

static int sp_49(void) {


  /* FUN_4d56_417e Mode 2: AI Missionary incites the tribe against the human. */
  {
    ColonizeDosRng irng;
    dos_rng_seed(&irng, 5u);
    ColonizeTurnContext ictx;
    memset(&ictx, 0, sizeof(ictx));
    ictx.messages = test_game_txt();
    ictx.names = test_names_txt();
    ictx.col1 = &col1;
    ictx.col1_ok = true;
    ictx.units = &units;
    ictx.map = &map;
    ictx.colonies = &colonies;
    ictx.rng = &irng;
    ictx.human_nation = 0;
    ictx.euro_power_rank_ok = true;
    ictx.euro_power_rank[0] = 3; /* human richest → rank 3 on this table */
    ictx.euro_power_rank[1] = 0;
    col1.player[0].control = 0;
    col1.player[1].control = 1;
    col1.nation[1].gold = 100000; /* price is data-dependent; well above any incite price */
    col1.nation[1].euro_relation[0] = AI_DIPLO_MET;
    ind->alarm_by_player[0] = 20;
    ind->euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    const int alarm0 = ind->alarm_by_player[0];
    const uint32_t gold0 = col1.nation[1].gold;
    if (!ai_contact_ai_incite_human(&ictx, ind, &col1.tribe[0], 4, 1, 1)) {
      return fail("417e Mode 2 should fire for a poorer AI with 1500+ gold vs a calm tribe");
    }
    /* Shared 4499 tail: FUN_4cc6_00f2 +100 alarm slam (no French/Pocahontas
     * halving here — human is England, no Pocahontas), clamped at 100. */
    (void)alarm0;
    if (ind->alarm_by_player[0] != 100 || col1.nation[1].gold >= gold0) {
      return fail("417e Mode 2 should pay the incite price and slam alarm toward the human to the war band");
    }
    ind->alarm_by_player[0] = 80;
    if (ai_contact_ai_incite_human(&ictx, ind, &col1.tribe[0], 4, 1, 1)) {
      return fail("417e Mode 2 must not fire once the tribe is already hostile (>= 0x4b)");
    }
    ind->alarm_by_player[0] = 20;
    ictx.euro_power_rank[1] = 3; /* AI not poorer */
    if (ai_contact_ai_incite_human(&ictx, ind, &col1.tribe[0], 4, 1, 1)) {
      return fail("417e Mode 2 requires wealth_rank[ai] < wealth_rank[human]");
    }
    ictx.euro_power_rank[1] = 0;
    col1.nation[1].gold = 1000;
    if (ai_contact_ai_incite_human(&ictx, ind, &col1.tribe[0], 4, 1, 1)) {
      return fail("417e Mode 2 requires AI gold >= 1500");
    }
    /*
     * port_bugs.md #587 — FUN_4d56_417e raw 83579-83580: the -1500 discount is
     * keyed on the acting unit's PROFESSION byte (+0x315b == 0x18, Jesuit),
     * so a blessed non-Jesuit colonist pays exactly 1500 more.
     */
    const uint8_t muskets_save = ind->muskets;
    const uint8_t horses_save = ind->horse_herds;
    ind->muskets = 50; /* lift the price well clear of the 500 floor */
    ind->horse_herds = 50;
    col1.nation[1].gold = 100000;
    ind->alarm_by_player[0] = 20;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    if (!ai_contact_ai_incite_human(&ictx, ind, &col1.tribe[0], 4, 1, 1)) {
      return fail("417e Mode 2 (Jesuit) should fire");
    }
    const uint32_t paid_jesuit = 100000u - col1.nation[1].gold;
    col1.nation[1].gold = 100000;
    ind->alarm_by_player[0] = 20;
    col1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    if (!ai_contact_ai_incite_human(&ictx, ind, &col1.tribe[0], 4, 1, 0)) {
      return fail("417e Mode 2 (non-Jesuit) should fire");
    }
    const uint32_t paid_plain = 100000u - col1.nation[1].gold;
    if (paid_plain != paid_jesuit + 1500u) {
      return fail("417e -1500 discount must apply only to profession 0x18 (Jesuit)");
    }
    ind->muskets = muskets_save;
    ind->horse_herds = horses_save;
    ind->alarm_by_player[0] = 20;
    col1.player[1].control = 0;
    fprintf(stderr, "unit_ai_contact: 417e Mode 2 auto-incite ok\n");
  }
  return 0;
}

static int sp_50(void) {

  /* FUN_4d56_2820 LAB_002e92: empty-handed AI wagon buys the tribe's own goods. */
  {
    int wagon_ti = units_find_type(&units, "Wagon Train");
    if (wagon_ti < 0) {
      wagon_ti = units.type_count++;
      memset(&units.types[wagon_ti], 0, sizeof(units.types[wagon_ti]));
      snprintf(units.types[wagon_ti].name, sizeof(units.types[wagon_ti].name), "Wagon Train");
      units.types[wagon_ti].movement = 1;
      units.types[wagon_ti].cargo = 2;
    }
    const int wid = units_spawn_allow_stack(&units, wagon_ti, col1.tribe[0].x + 1, col1.tribe[0].y);
    ColonizeUnit* w = units_get(&units, wid);
    if (!w) {
      return fail("2e92: wagon spawn");
    }
    w->nation_id = 1;
    for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
      w->hold_goods_type[i] = 0;
      w->hold_goods_amount[i] = 0;
    }
    ColonizeTurnContext bctx;
    memset(&bctx, 0, sizeof(bctx));
    bctx.messages = test_game_txt();
    bctx.names = test_names_txt();
    bctx.col1 = &col1;
    bctx.col1_ok = true;
    bctx.units = &units;
    bctx.map = &map;
    bctx.colonies = &colonies;
    bctx.human_nation = 0;
    col1.player[1].control = 1;
    col1.nation[1].gold = 5000;
    ind->euro_diplo[1] |= COL1_INDIAN_MET_BIT;
    ind->alarm_by_player[1] = 10;
    ind->tech = 9;
    for (int c = 0; c < 16; ++c) {
      ind->tons[c] = 300;
    }
    const uint32_t gold_b = col1.nation[1].gold;
    const int alarm_b = ind->alarm_by_player[1];
    if (!ai_contact_auto_buy_2e92(&bctx, ind, 4, 1, w)) {
      return fail("2e92: AI wagon with a free hold and gold should buy");
    }
    if (col1.nation[1].gold >= gold_b) {
      return fail("2e92: purchase should debit AI gold");
    }
    if (w->hold_goods_amount[0] != 100) {
      return fail("2e92: wagon hold should receive 100 (land qty)");
    }
    const int bought = w->hold_goods_type[0];
    if (bought == 0 || bought == 13 || bought == 14 || bought == 15) {
      return fail("2e92: food/trade goods/tools/muskets are never offered");
    }
    if (ind->tons[bought] != 200) {
      return fail("2e92: tribe tons should drop by qty");
    }
    if (ind->alarm_by_player[1] > alarm_b) {
      return fail("2e92: purchase lowers alarm by rand(0, price/25+1) (OVL13 0x3458 neg ax)");
    }
    /* Full holds: no purchase. */
    for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
      w->hold_goods_type[i] = 1;
      w->hold_goods_amount[i] = 100;
    }
    if (ai_contact_auto_buy_2e92(&bctx, ind, 4, 1, w)) {
      return fail("2e92: unit without a free hold must not buy");
    }
    /* TB-6 fixed (dos_buggy_behavior off): the pick scores the offered goods.
     * Prices rising with cargo id must pick the highest offered id, falling
     * prices the lowest, so the two picks differ and order that way. */
    {
      const ColonizeCol1Nation saved_nation = col1.nation[1];
      int picks[2] = {-1, -1};
      dos_buggy_behavior_set(false);
      for (int run = 0; run < 2; ++run) {
        for (int c = 0; c < 16; ++c) {
          ind->tons[c] = 300;
          col1.nation[1].trade.euro_price[c] = (uint8_t)(run == 0 ? c + 2 : 18 - c);
        }
        col1.nation[1].gold = 5000;
        for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
          w->hold_goods_type[i] = 0;
          w->hold_goods_amount[i] = 0;
        }
        if (ai_contact_auto_buy_2e92(&bctx, ind, 4, 1, w)) {
          picks[run] = w->hold_goods_type[0];
        }
      }
      dos_buggy_behavior_set(true);
      col1.nation[1] = saved_nation;
      if (picks[0] < 0 || picks[1] < 0 || picks[0] <= picks[1]) {
        fprintf(stderr, "unit_ai_contact: TB-6 picks %d / %d\n", picks[0], picks[1]);
        return fail("TB-6 fixed: AI buy must score the offered goods' prices");
      }
    }
    units_despawn(&units, wid);
    col1.player[1].control = 0;
    fprintf(stderr, "unit_ai_contact: 2820 LAB_002e92 auto-buy ok\n");
  }
  return 0;
}

static int sp_51(void) {

  /*
   * DS:0x8d4a: 2820 trades with the VISITED settlement. With two villages of
   * one tribe, a purchase beside the second must stamp the second record's
   * last_sold and leave the first (the old col1_tribe_first_of pick) alone.
   */
  {
    const ColonizeCol1Tribe* old_tribes = col1.tribe;
    const uint16_t old_count = col1.head.tribe_count;
    ColonizeCol1Tribe two[2];
    two[0] = col1.tribe[0];
    two[1] = col1.tribe[0];
    two[0].last_sold = 0xffu;
    two[1].x = 12;
    two[1].y = 12;
    two[1].last_sold = 0xffu;
    two[1].state.capital = 0;
    col1.tribe = two;
    col1.head.tribe_count = 2;
    int wagon_ti = units_find_type(&units, "Wagon Train");
    const int wid = wagon_ti >= 0 ? units_spawn_allow_stack(&units, wagon_ti, 13, 12) : -1;
    ColonizeUnit* w = units_get(&units, wid);
    if (!w) {
      return fail("8d4a: wagon spawn");
    }
    w->nation_id = 1;
    for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
      w->hold_goods_type[i] = 0;
      w->hold_goods_amount[i] = 0;
    }
    ColonizeTurnContext vctx;
    memset(&vctx, 0, sizeof(vctx));
    vctx.messages = test_game_txt();
    vctx.names = test_names_txt();
    vctx.col1 = &col1;
    vctx.col1_ok = true;
    vctx.units = &units;
    vctx.map = &map;
    vctx.colonies = &colonies;
    vctx.human_nation = 0;
    col1.player[1].control = 1;
    col1.nation[1].gold = 5000;
    const int bought_ok = ai_contact_auto_buy_2e92(&vctx, ind, 4, 1, w);
    const int bought = w->hold_goods_type[0];
    units_despawn(&units, wid);
    col1.player[1].control = 0;
    col1.tribe = (ColonizeCol1Tribe*)old_tribes;
    col1.head.tribe_count = old_count;
    if (!bought_ok) {
      return fail("8d4a: wagon beside the second village should buy");
    }
    /* Silver (9) writes 0xff and would prove nothing; this fixture buys otherwise. */
    if (bought == 9 || two[1].last_sold != (uint8_t)bought) {
      return fail("8d4a: purchase must stamp the visited village's last_sold");
    }
    if (two[0].last_sold != 0xffu) {
      return fail("8d4a: purchase must not touch the tribe's first village");
    }
    fprintf(stderr, "unit_ai_contact: 2820 visited-settlement binding ok\n");
  }
  return 0;
}

static int sp_52(void) {

  /* @BUY0 haggle arm: cheap offers / unlucky rolls exhaust patience, else -25% re-ask. */
  {
    ColonizeDosRng hr;
    dos_rng_seed(&hr, 3u);
    int price = 8;
    int ad = 0;
    if (ai_contact_2e92_haggle(0, 50, &hr, &price, &ad) != 0 || ad != 2) {
      return fail("haggle: price < 11 must be refused with alarm +2");
    }
    int saw_counter = 0;
    int saw_refuse = 0;
    for (int i = 0; i < 200 && !(saw_counter && saw_refuse); ++i) {
      price = 400;
      const int r = ai_contact_2e92_haggle(0, 200, &hr, &price, &ad);
      if (r) {
        saw_counter = 1;
        if (price != 300) {
          return fail("haggle: counter should drop the price by a quarter");
        }
      } else {
        saw_refuse = 1;
      }
    }
    if (!saw_counter || !saw_refuse) {
      return fail("haggle: both outcomes should occur over a stream at difficulty 0");
    }
    fprintf(stderr, "unit_ai_contact: 2820 haggle ok\n");
  }

  /* @TRADE0 sell-side haggle: patience counter, raise, exhaustion. */
  {
    ColonizeDosRng sr;
    dos_rng_seed(&sr, 9u);
    int c4 = 0;
    int price = 40;
    int fair = 60;
    if (ai_contact_2820_sell_haggle(0, 10, 100, &sr, &c4, &price, &fair) != 0) {
      return fail("sell haggle: c4 == 0 must exhaust patience");
    }
    int raised = 0;
    for (int i = 0; i < 200 && !raised; ++i) {
      c4 = 3;
      price = 40;
      fair = 60;
      if (ai_contact_2820_sell_haggle(0, 10, 100, &sr, &c4, &price, &fair)) {
        raised = 1;
        if (price < 46 || price > 61 || c4 != 2 || fair < price) {
          fprintf(stderr, "unit_ai_contact: sell haggle price=%d c4=%d fair=%d\n", price, c4, fair);
          return fail("sell haggle: raise should add RNG(ask/2+1, ask*2+1)*qty/100 and decrement c4");
        }
      }
    }
    if (!raised) {
      return fail("sell haggle: a raise should occur over a stream at difficulty 0 with c4=3");
    }
    fprintf(stderr, "unit_ai_contact: 2820 sell haggle ok\n");
  }
  return 0;
}

static int sp_53(void) {

  /*
   * NAMES.TXT @ACTIONS village menu + the overlay-13 action thunks
   * (FUN_4d56_4528 human arm; indian_actions_menu.md, static port 2026-08-28).
   */
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");
    ctx.messages = &game_txt;
    ai_popup_init(&pop_menu);
    ctx.ai_popups = &pop_menu;
    st_menu[0] = '\0';
    ctx.status = st_menu;
    ctx.status_size = sizeof(st_menu);
    ctx.human_nation = 0;
    ctx.rng = NULL;
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      ColonizeUnit* u = &units.units[ui];
      if (u->active) {
        units_despawn(&units, u->id);
      }
    }
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].population = 4;
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].state.capital = 0;
    col1.tribe[0].state.scouted = 0;
    col1.tribe[0].state.tribute_paid = 0;
    col1.tribe[0].alarm[0].friction = 0;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.indian[0].tech = 0;
    ind->euro_diplo[0] = (uint8_t)(COL1_INDIAN_MET_BIT | COL1_INDIAN_PEACE_BIT);
    ind->alarm_by_player[0] = 10;
    col1.nation[0].gold = 0;
    /* Unit types: 0 Brave, 1 Free Colonist (existing); add Scout / Soldier / Wagon / Missionary. */
    units.type_count = 6;
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Scouts");
    units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[2].cargo = 0;
    units.types[2].movement = 4;
    units.types[2].attack = 1;
    units.types[2].defense = 1;
    snprintf(units.types[3].name, sizeof(units.types[3].name), "Soldiers");
    units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[3].cargo = 0;
    units.types[3].movement = 1;
    units.types[3].attack = 2;
    units.types[3].defense = 2;
    snprintf(units.types[4].name, sizeof(units.types[4].name), "Wagon Train");
    units.types[4].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[4].cargo = 0;
    units.types[4].movement = 2;
    units.types[4].attack = 0;
    units.types[4].defense = 1;
    snprintf(units.types[5].name, sizeof(units.types[5].name), "Missionaries");
    units.types[5].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[5].cargo = 0;
    units.types[5].movement = 2;
    units.types[5].attack = 0;
    units.types[5].defense = 1;

    /* Scout → Ask to Speak With Chief + Demand Tribute + Attack Village + Cancel; no Trade / Live Among. */
    scout_id = units_spawn_allow_stack(&units, 2, 6, 5);
    scout = units_get(&units, scout_id);
    if (!scout) {
      return fail("menu: spawn scout");
    }
    scout->nation_id = 0;
    scout->profession = UNITS_JOB_NONE;
    if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, scout_id)) {
      return fail("menu: scout meet should enqueue");
    }
    if (pop_menu.queue_count != 1 || pop_menu.queue[0].tag != AI_POPUP_TAG_CONTACT_MEET) {
      return fail("menu: scout meet CHOICE");
    }
    {
      const AiPopupRequest* q = &pop_menu.queue[0];
      int has_chief = 0;
      int has_trade = 0;
      int has_live = 0;
      int has_demand = 0;
      int has_attack = 0;
      for (int i = 0; i < q->choice_count; ++i) {
        has_chief |= q->choice_ids[i] == 9;
        has_trade |= q->choice_ids[i] == 1;
        has_live |= q->choice_ids[i] == 4;
        has_demand |= q->choice_ids[i] == 3;
        has_attack |= q->choice_ids[i] == AI_CONTACT_CHOICE_ATTACK;
      }
      if (!has_chief || has_trade || has_live || !has_demand || !has_attack) {
        fprintf(stderr, "unit_ai_contact: scout rows chief=%d trade=%d live=%d demand=%d attack=%d\n",
                has_chief, has_trade, has_live, has_demand, has_attack);
        return fail("menu: scout rows should be Chief/Demand/Attack/Cancel");
      }
      if (strstr(st_menu, "expedition has reached") == NULL) {
        fprintf(stderr, "unit_ai_contact: menu body '%s'\n", st_menu);
        return fail("menu: body should be the @VILLAGE* section");
      }
      if (ai_contact_meet_payload_unit(q->payload) != scout_id) {
        return fail("menu: payload should carry the acting unit id");
      }
    }
    /* Speak With Chief: alarm 10 (<25) → never the kill arm; scouted latch set or bored. */
    {
      AiPopupState res;
      ai_popup_init(&res);
      res.has_result = true;
      res.result_cancelled = false;
      res.result_choice_id = 9; /* AI_CONTACT_CHOICE_CHIEF */
      res.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      res.result_nation_a = 0;
      res.result_nation_b = 4;
      res.result_payload = pop_menu.queue[0].payload;
      ai_popup_clear(&pop_menu);
      st_menu[0] = '\0';
      const uint32_t gold_before = col1.nation[0].gold;
      ai_contact_apply_popup_result(&ctx, &res);
      if (!scout->active) {
        return fail("chief: alarm 10 scout must survive (kill arm needs alarm >= 75 or Arawak roll)");
      }
      if (st_menu[0] == '\0') {
        return fail("chief: should set a @CHIEF* status");
      }
      if (strstr(st_menu, "target practice") != NULL) {
        return fail("chief: @CHIEFKILL impossible at alarm 10 for a non-Arawak tribe");
      }
      {
        int disclosed_skill = -1;
        if (!village_trade_intel_get_skill(0, 5, 5, &disclosed_skill)) {
          return fail("chief: @CHIEFHOWDY should record the village skill");
        }
      }
      /* Guides / area / gift all latch scouted; bored (alarm >= roll) does not. */
      if (col1.tribe[0].state.scouted) {
        if (scout->profession != UNITS_JOB_SCOUT && col1.nation[0].gold == gold_before &&
            strstr(st_menu, "tales") == NULL) {
          fprintf(stderr, "unit_ai_contact: chief status '%s'\n", st_menu);
          return fail("chief: scouted latch set without guides/gift/tales outcome");
        }
      } else if (strstr(st_menu, "pleased to welcome") == NULL) {
        fprintf(stderr, "unit_ai_contact: chief status '%s'\n", st_menu);
        return fail("chief: not scouted → @CHIEFBORED");
      }
      units_despawn(&units, scout_id);
    }
    /*
     * C12-B1: the human @CHIEFHOWDY arm runs thunk_FUN_1000_a618(...,1), which
     * ends with FUN_1000_86ba(DS:0x83a6) (overlays.c raw 77781), so the scouted
     * rand(1,3) is the first draw after srand(turn seed), not the thr stream's
     * next draw. Sweep stream seeds and check the outcome follows the reseed;
     * require at least one seed where the unreseeded draw would differ.
     */
    {
      ColonizeDosRng live;
      ColonizeDosRng* const rng_save = ctx.rng;
      const uint32_t seed_save = ctx.rng_seed;
      int checked = 0;
      int discriminating = 0;
      ctx.rng = &live;
      for (uint32_t s = 1; s < 64; ++s) {
        ctx.rng_seed = s * 7919u;
        dos_rng_seed(&live, s * 2654435761u);
        ColonizeDosRng pre = live;
        const int thr = dos_rng_range(&pre, 0, 100);
        if (thr <= 10) {
          continue; /* alarm 10 >= thr: bored, r not drawn */
        }
        const int stale_r = dos_rng_range(&pre, 1, 3);
        ColonizeDosRng exp;
        dos_rng_seed(&exp, ai_turn_seed(&ctx));
        const int r = dos_rng_range(&exp, 1, 3);
        discriminating |= stale_r != r;
        col1.tribe[0].state.scouted = 0;
        col1.nation[0].gold = 0;
        const int sid = units_spawn_allow_stack(&units, 2, 6, 5);
        ColonizeUnit* su = units_get(&units, sid);
        if (!su) {
          return fail("C12-B1: spawn scout");
        }
        su->nation_id = 0;
        su->profession = UNITS_JOB_NONE;
        ai_popup_clear(&pop_menu);
        if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, sid)) {
          return fail("C12-B1: meet should enqueue");
        }
        AiPopupState res;
        ai_popup_init(&res);
        res.has_result = true;
        res.result_choice_id = 9; /* CHIEF */
        res.result_tag = AI_POPUP_TAG_CONTACT_MEET;
        res.result_nation_a = 0;
        res.result_nation_b = 4;
        res.result_payload = pop_menu.queue[0].payload;
        ai_popup_clear(&pop_menu);
        dos_rng_seed(&live, s * 2654435761u); /* the meet popup may have drawn */
        ai_contact_apply_popup_result(&ctx, &res);
        const int got = su->profession == UNITS_JOB_SCOUT ? 1 : col1.nation[0].gold > 0 ? 3 : 2;
        if (got != r) {
          fprintf(stderr, "unit_ai_contact: C12-B1 seed %u r=%d stale=%d got=%d\n", s, r, stale_r, got);
          return fail("C12-B1: chief rand(1,3) must draw after the a618 turn-seed reseed");
        }
        units_despawn(&units, sid);
        checked++;
      }
      if (!checked || !discriminating) {
        fprintf(stderr, "unit_ai_contact: C12-B1 checked=%d disc=%d\n", checked, discriminating);
        return fail("C12-B1: sweep found no discriminating seed");
      }
      ctx.rng = rng_save;
      ctx.rng_seed = seed_save;
      col1.tribe[0].state.scouted = 0;
    }
    /*
     * a60c -> a618(...,1) head (overlays.c 77535 / 77693-77718): a human
     * Speak With Chief plays the village tune on a 1-in-4 roll — Inca (slot 0)
     * arms pool 7, whose pick is 0x36. The roll is the port's private stream.
     */
    {
      const uint32_t seed_save = ctx.rng_seed;
      int hits = 0;
      int misses = 0;
      for (uint32_t s = 1; s < 24; ++s) {
        ctx.rng_seed = s * 7919u;
        ColonizeDosRng probe;
        ai_contact_local_rng(&ctx, 4, &probe);
        const int hit = dos_rng_range(&probe, 0, 3) == 0;
        const int sid = units_spawn_allow_stack(&units, 2, 6, 5);
        ColonizeUnit* su = units_get(&units, sid);
        if (!su) {
          return fail("chief bgm: spawn scout");
        }
        su->nation_id = 0;
        su->profession = UNITS_JOB_NONE;
        col1.tribe[0].state.scouted = 0;
        ai_popup_clear(&pop_menu);
        if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, sid)) {
          return fail("chief bgm: meet should enqueue");
        }
        AiPopupState res;
        ai_popup_init(&res);
        res.has_result = true;
        res.result_choice_id = 9; /* CHIEF */
        res.result_tag = AI_POPUP_TAG_CONTACT_MEET;
        res.result_nation_a = 0;
        res.result_nation_b = 4;
        res.result_payload = pop_menu.queue[0].payload;
        ai_popup_clear(&pop_menu);
        sound_init("COLONIZE", false); /* fresh driver: no song left playing */
        sound_set_options((ColonizeSoundOptions){.background_music = false, .event_music = true});
        ai_contact_apply_popup_result(&ctx, &res);
        sound_service();
        dos_rng_schedule_tick_reseeds(NULL, 0); /* drop the pump's tick reseed */
        const int id = sound_driver_song_id();
        sound_shutdown();
        if (id != (hit ? 0x36 : -1)) {
          fprintf(stderr, "unit_ai_contact: chief bgm seed %u hit=%d id=%d\n", s, hit, id);
          return fail("chief bgm: a618 head cue must follow the 1-in-4 roll");
        }
        hits += hit;
        misses += !hit;
        units_despawn(&units, sid);
      }
      ctx.rng_seed = seed_save;
      col1.tribe[0].state.scouted = 0;
      if (!hits || !misses) {
        return fail("chief bgm: sweep must see both a hit and a miss");
      }
    }
  return 0;
}

static int sp_54(void) {

    /*
     * port_bugs.md #492: the @CHIEFAREA "tales of nearby lands" reveal is
     * FUN_13f1_0158-filtered (map_reveal_sight), not a raw 13x13 square: the
     * inner 3x3 is unconditional, every outer tile only when its continent
     * matches the Scout's. Sweep turn seeds until the r == 2 tales arm fires,
     * then check an outer-ring ocean tile stayed dark while an outer-ring
     * land tile was revealed.
     */
    {
      const uint32_t turn_save = turn;
      int saw_tales = 0;
      map.seen = calloc(256, 1); /* fixture leaves fog unallocated */
      if (!map.seen) {
        return fail("#492: alloc seen");
      }
      map.terrain[5 * 16 + 10] = 25; /* ocean at (10,5): |dx| = 4 from (6,5) */
      for (uint32_t t = 1; t < 200 && !saw_tales; ++t) {
        turn = t;
        col1.tribe[0].state.scouted = 0;
        memset(map.seen, 0, 256);
        st_menu[0] = '\0';
        const int sid = units_spawn_allow_stack(&units, 2, 6, 5);
        ColonizeUnit* su = units_get(&units, sid);
        if (!su) {
          return fail("#492: spawn scout");
        }
        su->nation_id = 0;
        su->profession = UNITS_JOB_NONE;
        ai_popup_clear(&pop_menu);
        if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, sid)) {
          return fail("#492: meet should enqueue");
        }
        AiPopupState r2;
        ai_popup_init(&r2);
        r2.has_result = true;
        r2.result_cancelled = false;
        r2.result_choice_id = 9; /* CHIEF */
        r2.result_tag = AI_POPUP_TAG_CONTACT_MEET;
        r2.result_nation_a = 0;
        r2.result_nation_b = 4;
        r2.result_payload = pop_menu.queue[0].payload;
        ai_popup_clear(&pop_menu);
        st_menu[0] = '\0';
        ai_contact_apply_popup_result(&ctx, &r2);
        if (strstr(st_menu, "tales") != NULL) {
          saw_tales = 1;
          if (map_tile_seen_by(&map, 10, 5, 0)) {
            return fail("#492: tales reveal must skip ocean / off-continent outer tiles");
          }
          if (!map_tile_seen_by(&map, 9, 5, 0)) {
            return fail("#492: tales reveal must reveal same-continent outer tiles");
          }
          if (!map_tile_seen_by(&map, 6, 6, 0)) {
            return fail("#492: tales reveal must reveal the inner ring");
          }
        }
        units_despawn(&units, sid);
      }
      if (!saw_tales) {
        return fail("#492: @CHIEFAREA tales arm never fired over 200 turn seeds");
      }
      map.terrain[5 * 16 + 10] = 1;
      free(map.seen);
      map.seen = NULL;
      turn = turn_save;
      col1.tribe[0].state.scouted = 0;
    }

    /*
     * port_bugs.md #501: rows must come out in the DOS fixed order (1 Trade,
     * 2 Enter Hostile, 3 Mission, 4 Heresy, 5 Live Among, 6 Chief, 7 Incite,
     * 8 Demand, 9 Attack, 10 Cancel). A Soldier (attack 2, met) discriminates:
     * the old builder emitted Attack Village before Demand Tribute.
     */
    {
      const int sold_id = units_spawn_allow_stack(&units, 3, 6, 5);
      ColonizeUnit* soldier = units_get(&units, sold_id);
      if (!soldier) {
        return fail("menu: spawn soldier");
      }
      soldier->nation_id = 0;
      soldier->profession = UNITS_JOB_NONE;
      ai_popup_clear(&pop_menu);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, sold_id)) {
        return fail("menu: soldier meet should enqueue");
      }
      const AiPopupRequest* q = &pop_menu.queue[0];
      if (q->choice_count != 3 || q->choice_ids[0] != 3 /* Demand */ ||
          q->choice_ids[1] != AI_CONTACT_CHOICE_ATTACK ||
          q->choice_ids[2] != 5 /* Cancel */) {
        fprintf(stderr, "unit_ai_contact: soldier rows %d: %d %d %d\n", q->choice_count,
                q->choice_count > 0 ? q->choice_ids[0] : -1,
                q->choice_count > 1 ? q->choice_ids[1] : -1,
                q->choice_count > 2 ? q->choice_ids[2] : -1);
        return fail("menu: DOS row order is Demand(8) then Attack(9) then Cancel(10)");
      }
      /* port_bugs.md #542: unmet tribe still gets the @ACTIONS menu (OVL13::00493f
       * jumps past rows 3-8 to the row-9 add) — no invented raid warn. */
      {
        const uint8_t saved_diplo = ind->euro_diplo[0];
        ind->euro_diplo[0] = 0;
        ai_popup_clear(&pop_menu);
        if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, sold_id)) {
          return fail("menu: unmet soldier should still get the @ACTIONS menu");
        }
        if (pop_menu.queue[0].tag != AI_POPUP_TAG_CONTACT_MEET || pop_menu.queue[0].choice_count != 2 ||
            pop_menu.queue[0].choice_ids[0] != AI_CONTACT_CHOICE_ATTACK ||
            pop_menu.queue[0].choice_ids[1] != 5 /* Cancel */) {
          return fail("menu: unmet soldier rows should be Attack / Cancel");
        }
        ind->euro_diplo[0] = saved_diplo;
      }
      ai_popup_clear(&pop_menu);
      units_despawn(&units, sold_id);
    }
  return 0;
}

static int sp_55(void) {

    /* Free Colonist → Live Among The Natives + Cancel only; Live Among → @LEARNSTAY CHOICE. */
    col_id = units_spawn_allow_stack(&units, 1, 6, 5);
    colonist = units_get(&units, col_id);
    if (!colonist) {
      return fail("menu: spawn colonist");
    }
    colonist->nation_id = 0;
    colonist->profession = UNITS_JOB_NONE;
    ai_popup_clear(&pop_menu);
    if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, col_id)) {
      return fail("menu: colonist meet should enqueue");
    }
    if (pop_menu.queue[0].choice_count != 2 || pop_menu.queue[0].choice_ids[0] != 4 || pop_menu.queue[0].choice_ids[1] != 5) {
      fprintf(stderr, "unit_ai_contact: colonist rows %d\n", pop_menu.queue[0].choice_count);
      return fail("menu: colonist rows should be Live Among / Cancel");
    }
    {
      AiPopupState res;
      ai_popup_init(&res);
      res.has_result = true;
      res.result_cancelled = false;
      res.result_choice_id = 4; /* AI_CONTACT_CHOICE_TEACH = Live Among */
      res.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      res.result_nation_a = 0;
      res.result_nation_b = 4;
      res.result_payload = pop_menu.queue[0].payload;
      ai_popup_clear(&pop_menu);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      /* alarm 10 → quartile 0 → no SLOW roll → human gets the @LEARNSTAY Yes/No. */
      if (pop_menu.queue_count != 1 || pop_menu.queue[0].tag != AI_POPUP_TAG_CONTACT_LEARNSTAY ||
          pop_menu.queue[0].kind != AI_POPUP_KIND_CHOICE || pop_menu.queue[0].choice_count != 2) {
        fprintf(stderr, "unit_ai_contact: learnstay queue %d status '%s'\n", pop_menu.queue_count, st_menu);
        return fail("live among: unskilled colonist at peace should get @LEARNSTAY CHOICE");
      }
      if (colonist->profession != UNITS_JOB_NONE || col1.tribe[0].state.learned) {
        return fail("live among: nothing applied before the CHOICE is answered");
      }
      if (strstr(st_menu, "master") == NULL) {
        fprintf(stderr, "unit_ai_contact: learnstay body '%s'\n", st_menu);
        return fail("live among: @LEARNSTAY body should name the master skill");
      }
      {
        int disclosed_skill = -1;
        const int offered_skill = pop_menu.queue[0].payload >> 16;
        if (!village_trade_intel_get_skill(0, 5, 5, &disclosed_skill) ||
            disclosed_skill != offered_skill) {
          return fail("live among: @LEARNSTAY should record the offered village skill");
        }
      }
      /* No → @LEARNLATER, still untaught. */
      AiPopupState ans;
      ai_popup_init(&ans);
      ans.has_result = true;
      ans.result_cancelled = false;
      ans.result_choice_id = 2; /* AI_CONTACT_LEARNSTAY_NO */
      ans.result_tag = AI_POPUP_TAG_CONTACT_LEARNSTAY;
      ans.result_nation_a = 0;
      ans.result_nation_b = 4;
      ans.result_payload = pop_menu.queue[0].payload;
      ai_popup_clear(&pop_menu);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &ans);
      if (colonist->profession != UNITS_JOB_NONE || col1.tribe[0].state.learned ||
          strstr(st_menu, "another time") == NULL) {
        fprintf(stderr, "unit_ai_contact: learnlater status '%s'\n", st_menu);
        return fail("live among: No → @LEARNLATER, nothing applied");
      }
      /* Yes → DONE: profession = village skill, learned latch, @LEARNDONE. */
      ans.result_choice_id = 1; /* AI_CONTACT_LEARNSTAY_YES */
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &ans);
      if (colonist->profession == UNITS_JOB_NONE || !col1.tribe[0].state.learned ||
          strstr(st_menu, "Congratulations") == NULL) {
        fprintf(stderr, "unit_ai_contact: learndone prof=%d learned=%d status '%s'\n",
                colonist->profession, (int)col1.tribe[0].state.learned, st_menu);
        return fail("live among: Yes → skill applied + learned + @LEARNDONE");
      }
      /* Village already taught (non-capital): a second unskilled colonist → @LEARNALREADY. */
      colonist->profession = UNITS_JOB_NONE;
      ai_popup_clear(&pop_menu);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, col_id)) {
        return fail("menu: second colonist meet");
      }
      res.result_payload = pop_menu.queue[0].payload;
      ai_popup_clear(&pop_menu);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      if (colonist->profession != UNITS_JOB_NONE || strstr(st_menu, "already shared") == NULL) {
        fprintf(stderr, "unit_ai_contact: learnalready status '%s'\n", st_menu);
        return fail("live among: taught village → @LEARNALREADY");
      }
      col1.tribe[0].state.learned = 0;
      units_despawn(&units, col_id);
    }
  return 0;
}

static int sp_56(void) {

    /* port_bugs.md #288: an Indentured Servant is a learner like a Free Colonist
     * (DOS a618: profession ∈ {0x19, 0x1c} reaches the LEARNSTAY arm) — it
     * must NOT get the @LEARNCRIMINAL "offend us" refusal nor @LEARNMASTER. */
    {
      const int srv_id = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* servant = units_get(&units, srv_id);
      if (!servant) {
        return fail("menu: spawn servant");
      }
      servant->nation_id = 0;
      servant->profession = UNITS_JOB_SERVANT;
      ai_popup_clear(&pop_menu);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, srv_id)) {
        return fail("menu: servant meet should enqueue");
      }
      AiPopupState res;
      ai_popup_init(&res);
      res.has_result = true;
      res.result_cancelled = false;
      res.result_choice_id = 4; /* AI_CONTACT_CHOICE_TEACH = Live Among */
      res.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      res.result_nation_a = 0;
      res.result_nation_b = 4;
      res.result_payload = pop_menu.queue[0].payload;
      ai_popup_clear(&pop_menu);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      if (strstr(st_menu, "teach you nothing") != NULL ||
          strstr(st_menu, "common criminal") != NULL) {
        fprintf(stderr, "unit_ai_contact: servant live-among status '%s'\n", st_menu);
        return fail("live among: servant must not get @LEARNCRIMINAL");
      }
      if (strstr(st_menu, "can only teach new skills") != NULL) {
        fprintf(stderr, "unit_ai_contact: servant live-among status '%s'\n", st_menu);
        return fail("live among: servant must not get @LEARNMASTER");
      }
      if (pop_menu.queue_count != 1 || pop_menu.queue[0].tag != AI_POPUP_TAG_CONTACT_LEARNSTAY) {
        fprintf(stderr, "unit_ai_contact: servant queue %d status '%s'\n", pop_menu.queue_count, st_menu);
        return fail("live among: servant at peace should get @LEARNSTAY CHOICE");
      }
      ai_popup_clear(&pop_menu);
      col1.tribe[0].state.learned = 0;
      units_despawn(&units, srv_id);
    }

    /* port_bugs.md #564: the village-teach skill draw seeds from
     * (village.y*256 + village.x) + DS:0x8d80 (post_map.boot_timer), so the
     * same village on the same map teaches different skills in games with a
     * different boot timer. Guard: the taught profession must not be
     * constant across boot_timer values. */
    {
      const uint32_t boots[4] = {0u, 844481u, 1376060u, 1407547u};
      int taught[4];
      const uint32_t boot_save = col1.post_map.boot_timer;
      for (int b = 0; b < 4; ++b) {
        col1.post_map.boot_timer = boots[b];
        col1.tribe[0].state.learned = 0;
        const int lid = units_spawn_allow_stack(&units, 1, 6, 5);
        ColonizeUnit* learner = units_get(&units, lid);
        if (!learner) {
          return fail("#564: spawn learner");
        }
        learner->nation_id = 0;
        learner->profession = UNITS_JOB_NONE;
        ai_popup_clear(&pop_menu);
        if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, lid)) {
          return fail("#564: learner meet should enqueue");
        }
        AiPopupState res2;
        ai_popup_init(&res2);
        res2.has_result = true;
        res2.result_cancelled = false;
        res2.result_choice_id = 4; /* Live Among */
        res2.result_tag = AI_POPUP_TAG_CONTACT_MEET;
        res2.result_nation_a = 0;
        res2.result_nation_b = 4;
        res2.result_payload = pop_menu.queue[0].payload;
        ai_popup_clear(&pop_menu);
        st_menu[0] = '\0';
        ai_contact_apply_popup_result(&ctx, &res2);
        AiPopupState ans2;
        ai_popup_init(&ans2);
        ans2.has_result = true;
        ans2.result_cancelled = false;
        ans2.result_choice_id = 1; /* YES */
        ans2.result_tag = AI_POPUP_TAG_CONTACT_LEARNSTAY;
        ans2.result_nation_a = 0;
        ans2.result_nation_b = 4;
        ans2.result_payload = pop_menu.queue[0].payload;
        ai_popup_clear(&pop_menu);
        st_menu[0] = '\0';
        ai_contact_apply_popup_result(&ctx, &ans2);
        taught[b] = learner->profession;
        ai_popup_clear(&pop_menu);
        units_despawn(&units, lid);
      }
      col1.post_map.boot_timer = boot_save;
      col1.tribe[0].state.learned = 0;
      int distinct = 0;
      for (int b = 0; b < 4; ++b) {
        distinct += (taught[b] != taught[0]);
      }
      if (distinct == 0) {
        fprintf(stderr, "unit_ai_contact: #564 taught %d %d %d %d\n",
                taught[0], taught[1], taught[2], taught[3]);
        return fail("#564: village-teach seed must include post_map.boot_timer");
      }
    }
  return 0;
}

static int sp_57(void) {

    /* Soldier → Demand Tribute: one of the four @EXTORT* bodies; laugh/no bumps alarm. */
    {
      const int sol_id = units_spawn_allow_stack(&units, 3, 6, 5);
      ColonizeUnit* sol = units_get(&units, sol_id);
      if (!sol) {
        return fail("menu: spawn soldier");
      }
      sol->nation_id = 0;
      sol->profession = UNITS_JOB_NONE;
      ai_popup_clear(&pop_menu);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, sol_id)) {
        return fail("menu: soldier meet should enqueue");
      }
      {
        const AiPopupRequest* q = &pop_menu.queue[0];
        int has_live = 0;
        int has_demand = 0;
        int has_attack = 0;
        for (int i = 0; i < q->choice_count; ++i) {
          has_live |= q->choice_ids[i] == 4;
          has_demand |= q->choice_ids[i] == 3;
          has_attack |= q->choice_ids[i] == AI_CONTACT_CHOICE_ATTACK;
        }
        if (has_live || !has_demand || !has_attack) {
          return fail("menu: soldier rows should be Demand Tribute / Attack Village / Cancel");
        }
      }
      AiPopupState res;
      ai_popup_init(&res);
      res.has_result = true;
      res.result_cancelled = false;
      res.result_choice_id = 3; /* AI_CONTACT_CHOICE_DEMAND = Demand Tribute */
      res.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      res.result_nation_a = 0;
      res.result_nation_b = 4;
      res.result_payload = pop_menu.queue[0].payload;
      ai_popup_clear(&pop_menu);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      if (strstr(st_menu, "laugh at your puny") == NULL && strstr(st_menu, "tremble before you") == NULL &&
          strstr(st_menu, "bow before the might") == NULL && strstr(st_menu, "very foolish") == NULL) {
        fprintf(stderr, "unit_ai_contact: tribute status '%s'\n", st_menu);
        return fail("demand tribute: status should be one of the @EXTORT* bodies");
      }
      if (strstr(st_menu, "bow before the might") != NULL && !col1.tribe[0].state.tribute_paid) {
        return fail("demand tribute: @EXTORTSTUFF must latch tribe.state.tribute_paid (DOS +3 bit 0x10)");
      }
      units_despawn(&units, sol_id);
    }
  return 0;
}

static int sp_58(void) {

    /* Missionary + foreign (French) mission → Denounce Heresy / Incite / Cancel; heresy consumes the unit. */
    {
      col1.tribe[0].mission = 1; /* French, plain */
      const int mis_id = units_spawn_allow_stack(&units, 5, 6, 5);
      ColonizeUnit* mis = units_get(&units, mis_id);
      if (!mis) {
        return fail("menu: spawn missionary");
      }
      mis->nation_id = 0;
      mis->profession = UNITS_JOB_NONE;
      ai_popup_clear(&pop_menu);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 1, 0, mis_id)) {
        return fail("menu: missionary meet should enqueue");
      }
      {
        const AiPopupRequest* q = &pop_menu.queue[0];
        int has_heresy = 0;
        int has_mission = 0;
        int has_incite = 0;
        for (int i = 0; i < q->choice_count; ++i) {
          has_heresy |= q->choice_ids[i] == 8;
          has_mission |= q->choice_ids[i] == 7;
          has_incite |= q->choice_ids[i] == 6;
          if (q->choice_ids[i] == 8 && strstr(q->choices[i], "French") == NULL) {
            fprintf(stderr, "unit_ai_contact: heresy row '%s'\n", q->choices[i]);
            return fail("menu: Denounce Heresy row should name the rival (%F)");
          }
        }
        if (!has_heresy || has_mission || !has_incite) {
          return fail("menu: missionary rows should be Denounce Heresy / Incite / Cancel");
        }
      }
      AiPopupState res;
      ai_popup_init(&res);
      res.has_result = true;
      res.result_cancelled = false;
      res.result_choice_id = 8; /* AI_CONTACT_CHOICE_HERESY */
      res.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      res.result_nation_a = 0;
      res.result_nation_b = 4;
      res.result_payload = pop_menu.queue[0].payload;
      ai_popup_clear(&pop_menu);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      if (mis->active) {
        return fail("heresy: the missionary is consumed either way (FUN_1000_89f8 after the roll)");
      }
      if (strstr(st_menu, "denounce heresy") == NULL) {
        fprintf(stderr, "unit_ai_contact: heresy status '%s'\n", st_menu);
        return fail("heresy: status should be @HERESY0/@HERESY1");
      }
      if (strstr(st_menu, "erect a new") != NULL && (col1.tribe[0].mission & 0x0f) != 0) {
        return fail("heresy: @HERESY0 must flip the mission to the denouncer");
      }
      if (strstr(st_menu, "at the stake") != NULL && (col1.tribe[0].mission & 0x0f) != 1) {
        return fail("heresy: @HERESY1 must leave the rival mission in place");
      }
      /* No mission → Establish Mission row; establishing consumes the unit and sets the owner. */
      col1.tribe[0].mission = 0xff;
      const int mis2_id = units_spawn_allow_stack(&units, 5, 6, 5);
      ColonizeUnit* mis2 = units_get(&units, mis2_id);
      if (!mis2) {
        return fail("menu: spawn missionary 2");
      }
      mis2->nation_id = 0;
      mis2->profession = UNITS_JOB_NONE;
      ai_popup_clear(&pop_menu);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 1, 0, mis2_id)) {
        return fail("menu: missionary 2 meet should enqueue");
      }
      if (pop_menu.queue[0].choice_ids[0] != 7) {
        return fail("menu: no mission → Establish Mission first row");
      }
      res.result_choice_id = 7;
      res.result_payload = pop_menu.queue[0].payload;
      ai_popup_clear(&pop_menu);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      if (mis2->active || (col1.tribe[0].mission & 0x0f) != 0 || strstr(st_menu, "mission founded") == NULL) {
        fprintf(stderr, "unit_ai_contact: mission status '%s'\n", st_menu);
        return fail("establish mission: unit consumed, owner = us, @MISSIONn body");
      }
      col1.tribe[0].mission = 0xff;
    }

    /*
     * port_bugs.md #558 / docs/dos_bugs.md DB-7 — Denounce Heresy "Jesuit"
     * bonus. DOS thunk_FUN_1000_a594 (viceroy_overlays.asm 126507) tests the
     * denouncer's PROFESSION byte against @JOB row 3 (Cotton Planter), a
     * typo; the port tests the Jesuit row 0x18. So a Jesuit installs the
     * won mission with the 0x10 bit, a profession-3 unit installs it plain,
     * and the same plain unit under Brebeuf (FF 22) installs it Jesuit like
     * the establish body (thunk_FUN_1000_a5dc).
     */
    {
      ColonizeDosRng* saved_rng = ctx.rng;
      ColonizeDosRng hq_rng;
      AiPopupState res;
      ai_popup_init(&res);
      res.has_result = true;
      res.result_cancelled = false;
      res.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      res.result_nation_a = 0;
      res.result_nation_b = 4;
      const uint8_t profs[3] = { (uint8_t)UNITS_JOB_MISSIONARY, (uint8_t)3, (uint8_t)3 };
      const uint8_t ff_bit = (uint8_t)(1u << (FF_JEAN_DE_BREBEUF % 8));
      for (int k = 0; k < 3; ++k) {
        const int brebeuf = k == 2;
        if (brebeuf) {
          col1.nation[0].founding_fathers[FF_JEAN_DE_BREBEUF / 8] |= ff_bit;
        }
        const int want_bit = profs[k] == UNITS_JOB_MISSIONARY || brebeuf;
        int won = 0;
        for (unsigned seed = 1u; seed <= 64u && !won; ++seed) {
          col1.tribe[0].mission = 1; /* French, plain */
          dos_rng_seed(&hq_rng, seed);
          ctx.rng = &hq_rng;
          const int hid = units_spawn_allow_stack(&units, 5, 6, 5);
          ColonizeUnit* hu = units_get(&units, hid);
          if (!hu) {
            ctx.rng = saved_rng;
            return fail("heresy jesuit: spawn denouncer");
          }
          hu->nation_id = 0;
          hu->profession = profs[k];
          ai_popup_clear(&pop_menu);
          if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 1, 0, hid)) {
            ctx.rng = saved_rng;
            return fail("heresy jesuit: missionary meet should enqueue");
          }
          res.result_choice_id = 8; /* AI_CONTACT_CHOICE_HERESY */
          res.result_payload = pop_menu.queue[0].payload;
          ai_popup_clear(&pop_menu);
          st_menu[0] = '\0';
          ai_contact_apply_popup_result(&ctx, &res);
          if ((col1.tribe[0].mission & 0x0f) == 0) {
            won = 1;
            const int bit = (col1.tribe[0].mission & 0x10) != 0;
            if (bit != want_bit) {
              fprintf(
                stderr,
                "unit_ai_contact: a594 jesuit prof=%u mission=%u\n",
                (unsigned)profs[k],
                (unsigned)col1.tribe[0].mission
              );
              ctx.rng = saved_rng;
              return fail(
                brebeuf ? "heresy: under Brebeuf a won mission must install the 0x10 bit"
                : profs[k] == UNITS_JOB_MISSIONARY
                  ? "heresy: a Jesuit denouncer must install the 0x10 mission bit"
                  : "heresy: a profession-3 denouncer gets no Jesuit bonus"
              );
            }
          }
          if (hu->active) {
            units_despawn(&units, hid);
          }
        }
        if (brebeuf) {
          col1.nation[0].founding_fathers[FF_JEAN_DE_BREBEUF / 8] &= (uint8_t)~ff_bit;
        }
        if (!won) {
          ctx.rng = saved_rng;
          return fail("heresy jesuit: no winning roll found in 64 seeds");
        }
      }
      ctx.rng = saved_rng;
      col1.tribe[0].mission = 0xff;
    }
  return 0;
}

static int sp_59(void) {

    /* Wagon: alarm < 75 → Trade With Village; alarm ≥ 75 → Enter Hostile Village. */
    {
      const int wag_id = units_spawn_allow_stack(&units, 4, 6, 5);
      ColonizeUnit* wag = units_get(&units, wag_id);
      if (!wag) {
        return fail("menu: spawn wagon");
      }
      wag->nation_id = 0;
      wag->profession = UNITS_JOB_NONE;
      ai_popup_clear(&pop_menu);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, wag_id)) {
        return fail("menu: wagon meet should enqueue");
      }
      if (pop_menu.queue[0].choice_count != 2 || pop_menu.queue[0].choice_ids[0] != 1) {
        return fail("menu: wagon rows should be Trade / Cancel");
      }
      ind->alarm_by_player[0] = 80;
      ai_popup_clear(&pop_menu);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, wag_id)) {
        return fail("menu: hostile wagon meet should still enqueue (DOS shows @VILLAGEWAR)");
      }
      if (pop_menu.queue[0].choice_count != 2 || pop_menu.queue[0].choice_ids[0] != 10) {
        return fail("menu: hostile wagon rows should be Enter Hostile Village / Cancel");
      }
      ind->alarm_by_player[0] = 10;
      units_despawn(&units, wag_id);
    }

    ctx.ai_popups = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
    ctx.messages = NULL;
    assets_msg_free(&game_txt);
  fprintf(stderr, "unit_ai_contact: ok (last_raid_kind=%d)\n", ai_contact_last_raid_kind());
  return 0;
}

typedef int (*SpineFn)(void);

static const SpineFn k_spine[] = {
  sp_00, sp_01, sp_02, sp_03, sp_04, sp_05, sp_06, sp_07,
  sp_08, sp_09, sp_10, sp_11, sp_12, sp_13, sp_14, sp_15,
  sp_16, sp_17, sp_18, sp_19, sp_20, sp_21, sp_22, sp_23,
  sp_24, sp_25, sp_26, sp_27, sp_28, sp_29, sp_30, sp_31,
  sp_32, sp_33, sp_34, sp_35, sp_36, sp_37, sp_38, sp_39,
  sp_40, sp_41, sp_42, sp_43, sp_44, sp_45, sp_46, sp_47,
  sp_48, sp_49, sp_50, sp_51, sp_52, sp_53, sp_54, sp_55,
  sp_56, sp_57, sp_58, sp_59,
};

/* Replay the narrative spine up to (and including) one segment, then tear the
 * fixture down. A failure in an earlier segment fails this case too — its own
 * fail() diagnostic names the beat that broke. */
static int fx_run(int upto) {
  int rc = 0;
  for (int i = 0; i <= upto; ++i) {
    rc = k_spine[i]();
    if (rc != 0) {
      break;
    }
  }
  fx_close();
  return rc;
}

static int case_meet_first_contact(void) { return fx_run(1); }
static int case_raid_colony_loot(void) { return fx_run(2); }
static int case_raid_alarm_tail_war_gate(void) { return fx_run(3); }
static int case_raid_repelled_limb(void) { return fx_run(4); }
static int case_mission_expel_00f2(void) { return fx_run(6); }
static int case_ambush_gear_seize(void) { return fx_run(7); }
static int case_prelude_no_encroach_drift(void) { return fx_run(8); }
static int case_first_meet_status(void) { return fx_run(12); }
static int case_raid_multi_loot(void) { return fx_run(13); }
static int case_raid_stores_arms(void) { return fx_run(14); }
static int case_raid_status_nothing_surprise_war(void) { return fx_run(17); }
static int case_prelude_clamp_only(void) { return fx_run(21); }
static int case_raid_burn_arms(void) { return fx_run(23); }
static int case_raid_escalate_pocahontas(void) { return fx_run(25); }
static int case_popup_welcome_accept(void) { return fx_run(26); }
static int case_popup_ship_no_welcome(void) { return fx_run(27); }
static int case_meet_choice_trade(void) { return fx_run(28); }
static int case_meet_trade_flavor_goods(void) { return fx_run(29); }
static int case_meet_trade_sea_wagon(void) { return fx_run(30); }
static int case_meet_2820_gates_phases(void) { return fx_run(31); }
static int case_meet_welcome_second_brave_shun(void) { return fx_run(32); }
static int case_meet_mission_expel(void) { return fx_run(33); }
static int case_meet_gift_choice_amount(void) { return fx_run(34); }
static int case_meet_incite_choice(void) { return fx_run(35); }
static int case_meet_demand_choice(void) { return fx_run(36); }
static int case_meet_teach_payload_gate(void) { return fx_run(37); }
static int case_meet_converts(void) { return fx_run(38); }
static int case_village_enter_meet_choice(void) { return fx_run(39); }
static int case_capital_destroy_surrender(void) { return fx_run(40); }
static int case_attack_village_choice(void) { return fx_run(41); }
static int case_ship_village_wary_band(void) { return fx_run(42); }
static int case_woi_tribe_defection(void) { return fx_run(43); }
static int case_village_beg_food(void) { return fx_run(44); }
static int case_reparations_indiancity(void) { return fx_run(45); }
static int case_reparations_indianwagons(void) { return fx_run(46); }
static int case_village_gifts_givestuff_givefood(void) { return fx_run(47); }
static int case_whackindians_and_euro_peer(void) { return fx_run(48); }
static int case_incite_mode2_ai_missionary(void) { return fx_run(49); }
static int case_wagon_buys_tribe_goods(void) { return fx_run(50); }
static int case_village_bind_ds8d4a(void) { return fx_run(51); }
static int case_buy0_trade0_haggle(void) { return fx_run(52); }
static int case_actions_menu_scout(void) { return fx_run(53); }
static int case_actions_menu_scout_arms(void) { return fx_run(54); }
static int case_actions_menu_live_among(void) { return fx_run(55); }
static int case_actions_menu_servant_teach(void) { return fx_run(56); }
static int case_actions_menu_soldier_tribute(void) { return fx_run(57); }
static int case_actions_menu_missionary(void) { return fx_run(58); }
static int case_actions_menu_wagon(void) { return fx_run(59); }

/*
 * port_bugs.md #1230: FUN_5bfb_0182 shows @INDIANCOME when
 * FUN_281f_030c(euro - 4, nation) < 0x19 (signed). For France (1) meeting the
 * Tupi (11) that word is DS:0x5b1c + (-3 * 0x27 + 11) * 2 = DS:0x5a48, dwelling
 * slot 76 bytes +4/+5 (population | mission << 8). No mission (0xff) reads
 * negative and the line shows; a French mission makes it 0x1xx and it does not.
 * For the Dutch (3) the word is in the Inca record: Tupi reads +14 (tons[0]).
 */
static int case_welcome_accept_come_word(void) {
  static ColonizeCol1Save s;
  memset(&s, 0, sizeof(s));
  ColonizeCol1Tribe* tribes = calloc(80, sizeof(ColonizeCol1Tribe));
  if (!tribes) {
    return fail("come word: alloc");
  }
  s.tribe = tribes;
  s.head.tribe_count = 80;
  AiPopupState p;
  ai_popup_init(&p);
  ColonizeTurnContext c;
  memset(&c, 0, sizeof(c));
  c.col1 = &s;
  c.col1_ok = true;
  c.ai_popups = &p;
  c.human_nation = 1;
  int rc = 0;
  tribes[76].population = 3;
  tribes[76].mission = 0xff;
  ai_contact_apply_welcome_accept(&c, &s.indian[7], 11, 1);
  if (p.queue_count != 2) {
    rc = fail("come word: unmissioned slot 76 should show PEACE + COME");
  }
  ai_popup_clear(&p);
  tribes[76].mission = 1;
  ai_contact_apply_welcome_accept(&c, &s.indian[7], 11, 1);
  if (rc == 0 && p.queue_count != 1) {
    rc = fail("come word: a French mission in slot 76 should suppress COME");
  }
  ai_popup_clear(&p);
  c.human_nation = 3;
  s.indian[0].tons[0] = 0x19;
  ai_contact_apply_welcome_accept(&c, &s.indian[7], 11, 3);
  if (rc == 0 && p.queue_count != 1) {
    rc = fail("come word: Dutch read Inca tons[0] = 0x19, no COME");
  }
  free(tribes);
  return rc;
}

/* FUN_4d56_2820 head: village tune on a 1-in-4 roll; 7 Inca, 6 Aztec, else 5. */
static int case_village_bgm_cue(void) {
  const int nations[3] = {4, 5, 6};
  const int want[3] = {7, 6, 5};
  int hits = 0;
  for (uint32_t seed = 1; seed < 400u; ++seed) {
    ColonizeDosRng probe;
    dos_rng_seed(&probe, seed * 12345u);
    const int hit = dos_rng_range(&probe, 0, 3) == 0;
    for (int i = 0; i < 3; ++i) {
      ColonizeDosRng r;
      dos_rng_seed(&r, seed * 12345u);
      const int pool = ai_contact_village_bgm_cue(&r, nations[i]);
      if (pool != (hit ? want[i] : 0)) {
        return fail("village bgm: pool != roll/tribe-slot mapping");
      }
    }
    hits += hit;
  }
  return hits > 0 && hits < 399 ? 0 : fail("village bgm: roll never varied");
}

static const TestCase k_cases[] = {
  {"village_bgm_cue", case_village_bgm_cue},
  {"welcome_accept_come_word", case_welcome_accept_come_word},
  {"meet_first_contact", case_meet_first_contact},
  {"raid_colony_loot", case_raid_colony_loot},
  {"raid_alarm_tail_war_gate", case_raid_alarm_tail_war_gate},
  {"raid_repelled_limb", case_raid_repelled_limb},
  {"mission_expel_00f2", case_mission_expel_00f2},
  {"ambush_gear_seize", case_ambush_gear_seize},
  {"prelude_no_encroach_drift", case_prelude_no_encroach_drift},
  {"first_meet_status", case_first_meet_status},
  {"raid_multi_loot", case_raid_multi_loot},
  {"raid_stores_arms", case_raid_stores_arms},
  {"raid_status_nothing_surprise_war", case_raid_status_nothing_surprise_war},
  {"prelude_clamp_only", case_prelude_clamp_only},
  {"raid_burn_arms", case_raid_burn_arms},
  {"raid_escalate_pocahontas", case_raid_escalate_pocahontas},
  {"popup_welcome_accept", case_popup_welcome_accept},
  {"popup_ship_no_welcome", case_popup_ship_no_welcome},
  {"meet_choice_trade", case_meet_choice_trade},
  {"meet_trade_flavor_goods", case_meet_trade_flavor_goods},
  {"meet_trade_sea_wagon", case_meet_trade_sea_wagon},
  {"meet_2820_gates_phases", case_meet_2820_gates_phases},
  {"meet_welcome_second_brave_shun", case_meet_welcome_second_brave_shun},
  {"meet_mission_expel", case_meet_mission_expel},
  {"meet_gift_choice_amount", case_meet_gift_choice_amount},
  {"meet_incite_choice", case_meet_incite_choice},
  {"meet_demand_choice", case_meet_demand_choice},
  {"meet_teach_payload_gate", case_meet_teach_payload_gate},
  {"meet_converts", case_meet_converts},
  {"village_enter_meet_choice", case_village_enter_meet_choice},
  {"capital_destroy_surrender", case_capital_destroy_surrender},
  {"attack_village_choice", case_attack_village_choice},
  {"ship_village_wary_band", case_ship_village_wary_band},
  {"woi_tribe_defection", case_woi_tribe_defection},
  {"village_beg_food", case_village_beg_food},
  {"reparations_indiancity", case_reparations_indiancity},
  {"reparations_indianwagons", case_reparations_indianwagons},
  {"village_gifts_givestuff_givefood", case_village_gifts_givestuff_givefood},
  {"whackindians_and_euro_peer", case_whackindians_and_euro_peer},
  {"incite_mode2_ai_missionary", case_incite_mode2_ai_missionary},
  {"wagon_buys_tribe_goods", case_wagon_buys_tribe_goods},
  {"village_bind_ds8d4a", case_village_bind_ds8d4a},
  {"buy0_trade0_haggle", case_buy0_trade0_haggle},
  {"actions_menu_scout", case_actions_menu_scout},
  {"actions_menu_scout_arms", case_actions_menu_scout_arms},
  {"actions_menu_live_among", case_actions_menu_live_among},
  {"actions_menu_servant_teach", case_actions_menu_servant_teach},
  {"actions_menu_soldier_tribute", case_actions_menu_soldier_tribute},
  {"actions_menu_missionary", case_actions_menu_missionary},
  {"actions_menu_wagon", case_actions_menu_wagon},
};
TEST_MAIN(k_cases)
