/* Slice of the former tests/unit/test_ai_contact.c (split by feature 2026-09-23):
 * first contact / encounter scan / popup chain ordering. */
#include "test_ai_contact_common.h"

/*
 * bugs.md #416 — first contact is strictly per Euro nation.
 *
 * DOS FUN_5bfb_3180 hands FUN_5bfb_022e the pair (euro, indian) it actually
 * found adjacent, and 022e only opens the @INDIANWELCOME dialog when THAT
 * euro's control byte (`*(char *)(param_1 * 0x34 + 0x543f)`) is 0; any other
 * nation takes `local_c = 1` and auto-accepts silently. So an AI nation's
 * meeting must leave the human with no popup and no met bit.
 */
static int test_ai_only_meet_is_silent_for_human(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 2;
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    return fail("ai-only meet: alloc tribe");
  }
  col1.tribe[0].x = 5;
  col1.tribe[0].y = 5;
  col1.tribe[0].nation_id = 5; /* Aztec */
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].population = 4;
  memset(&col1.indian[1], 0, sizeof(col1.indian[1]));

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("ai-only meet: alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
    map.layer3[i] = 0xf0u;
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Brave");
  units.types[0].movement = 1;
  units.types[0].attack = 2;
  units.types[0].defense = 1;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Free Colonist");
  units.types[1].movement = 1;
  units.types[1].attack = 0;
  units.types[1].defense = 1;

  const int brave_id = units_spawn_allow_stack(&units, 0, 5, 5);
  const int euro_id = units_spawn_allow_stack(&units, 1, 6, 5);
  ColonizeUnit* brave = units_get(&units, brave_id);
  ColonizeUnit* euro = units_get(&units, euro_id);
  if (!brave || !euro) {
    return fail("ai-only meet: spawn");
  }
  brave->nation_id = 5;
  brave->moves = 0;
  euro->nation_id = 0; /* English — an AI nation in this fixture */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);

  AiPopupState pop;
  ai_popup_init(&pop);

  uint32_t turn = 1;
  ColonizeTurnContext ctx;
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
  ctx.ai_popups = &pop;
  ctx.human_nation = 1; /* the player is France */

  ai_contact_indian_meet_trade(&ctx, 5);

  int rc = 0;
  if (col1.indian[1].euro_diplo[0] == 0) {
    rc = fail("ai-only meet: England (the nation that met) should carry the met bit");
  } else if (col1.indian[1].euro_diplo[1] != 0) {
    rc = fail("ai-only meet: France never met the Aztec — no met bit may be set");
  } else if (pop.queue_count != 0 || pop.open) {
    rc = fail("ai-only meet: an AI nation's first contact must raise no human popup");
  }
  col1_save_free(&col1);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  return rc;
}

/*
 * bugs.md #416 — the layer3 owner nibble is a stamp, not a settlement record.
 *
 * FUN_1427_02ca repaints the nibble of every tile a unit steps onto and
 * FUN_1427_023a leaves it behind when the unit leaves, so a Brave that crossed
 * a foreign village left that tile reading as its own nation. The
 * FUN_5bfb_3180 contact scan (ai_contact_encounter_scan) reads exactly that
 * nibble on `layer2 & 2` tiles, so the stale stamp opened first contact with a
 * tribe that has no settlement anywhere near.
 */
static int test_encounter_scan_ignores_stale_owner_stamp(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 2;
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }
  col1.head.tribe_count = 1;
  col1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    return fail("stale stamp: alloc tribe");
  }
  col1.tribe[0].x = 5;
  col1.tribe[0].y = 5;
  col1.tribe[0].nation_id = 8; /* Cherokee village really standing here */
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].population = 4;
  memset(&col1.indian[1], 0, sizeof(col1.indian[1])); /* Aztec */
  memset(&col1.indian[4], 0, sizeof(col1.indian[4])); /* Cherokee */

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("stale stamp: alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1;
    map.layer3[i] = 0xf0u;
  }
  const int vi = 5 * 16 + 5;
  map.layer2[vi] = 0x02u;  /* settlement bit */
  map.layer3[vi] = 0x50u;  /* stale stamp: an Aztec (5) Brave once walked here */

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);

  AiPopupState pop;
  ai_popup_init(&pop);

  uint32_t turn = 1;
  ColonizeTurnContext ctx;
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
  ctx.ai_popups = &pop;
  ctx.human_nation = 1;

  /* A French unit steps to (6,5) — beside the Cherokee village, nowhere near
   * any Aztec settlement. */
  (void)ai_contact_encounter_scan(&ctx, 1, 6, 5);

  int rc = 0;
  if (col1.indian[4].euro_diplo[1] == 0) {
    rc = fail("stale stamp: the Cherokee village on the tile must open contact");
  } else if (col1.indian[1].euro_diplo[1] != 0) {
    rc = fail("stale stamp: a passing Brave's nibble must not make France meet the Aztec");
  } else {
    for (int i = 0; i < pop.queue_count; ++i) {
      if (pop.queue[i].nation_b == 5) {
        rc = fail("stale stamp: no Aztec welcome popup may be queued");
        break;
      }
    }
  }
  col1_save_free(&col1);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  return rc;
}

/* Consume the answered popup the way game_loop does after applying it. */
static void take_popup_result(AiPopupState* pop) {
  ai_popup_cancel_current(pop);
  pop->has_result = false;
}

/*
 * bugs.md #427 — two contact chains must not interleave.
 *
 * FUN_5bfb_3180 calls FUN_5bfb_022e / FUN_5bfb_153e inline per neighbour, so a
 * Tupi exchange and a Spanish one are strictly sequential in DOS. The port
 * enqueues each follow-up only when the previous dialog is answered, which
 * lands it behind anything queued meanwhile — the chain key has to pull it
 * back to the front of the presentation order.
 */
static int test_contact_chains_do_not_interleave(void) {
  AiPopupState pop;
  ai_popup_init(&pop);

  static const char* yn[2] = {"Yes", "No"};
  static const int yn_ids[2] = {1, 2};

  /* One pulse queues the Tupi meet and the Spanish encounter opener. */
  if (!ai_popup_enqueue_choice_ctx(
        &pop, AI_POPUP_TAG_CONTACT_WELCOME, 1, 11, 0, NULL, "tupi-welcome", yn, yn_ids, 2
      )) {
    return fail("chain: enqueue tupi welcome");
  }
  if (!ai_popup_enqueue_ok_ctx(&pop, AI_POPUP_TAG_DIPLO_TALK, 1, 2, 0, NULL, "spain-hello")) {
    return fail("chain: enqueue spain hello");
  }
  if (pop.queue[0].chain == 0 || pop.queue[1].chain == 0 ||
      pop.queue[0].chain == pop.queue[1].chain) {
    return fail("chain: the two exchanges must carry distinct non-zero keys");
  }

  if (!ai_popup_try_present_next(&pop) || pop.current.tag != AI_POPUP_TAG_CONTACT_WELCOME) {
    return fail("chain: the head of the queue presents first");
  }
  take_popup_result(&pop);

  /* Answering the welcome enqueues the peace line — at the TAIL, behind Spain. */
  if (!ai_popup_enqueue_ok_ctx(&pop, AI_POPUP_TAG_CONTACT_MEET, 1, 11, 0, NULL, "tupi-peace")) {
    return fail("chain: enqueue tupi peace");
  }
  if (!ai_popup_try_present_next(&pop)) {
    return fail("chain: present after tupi welcome");
  }
  if (pop.current.tag != AI_POPUP_TAG_CONTACT_MEET || pop.current.nation_b != 11) {
    return fail("chain: the Tupi chain must finish before the Spanish one starts");
  }
  take_popup_result(&pop);

  /* Tupi chain exhausted — now Spain, and its own follow-up stays with it. */
  if (!ai_popup_try_present_next(&pop) || pop.current.tag != AI_POPUP_TAG_DIPLO_TALK ||
      pop.current.nation_b != 2) {
    return fail("chain: Spain presents once the Tupi chain is done");
  }
  take_popup_result(&pop);
  if (!ai_popup_enqueue_ok_ctx(&pop, AI_POPUP_TAG_DIPLO_TALK, 1, 2, 1, NULL, "spain-2")) {
    return fail("chain: enqueue spain stage 2");
  }
  if (!ai_popup_enqueue_ok_ctx(&pop, AI_POPUP_TAG_CONTACT_MEET, 1, 11, 0, NULL, "tupi-later")) {
    return fail("chain: enqueue a later tupi line");
  }
  if (!ai_popup_try_present_next(&pop) || pop.current.nation_b != 2) {
    return fail("chain: a new Tupi line must not cut into the live Spanish chain");
  }
  take_popup_result(&pop);
  if (!ai_popup_try_present_next(&pop) || pop.current.nation_b != 11) {
    return fail("chain: the Tupi line follows once Spain is done");
  }

  /* Ungrouped popups (colony chrome, king letters) keep key 0. */
  ai_popup_init(&pop);
  if (!ai_popup_enqueue_ok_ctx(&pop, AI_POPUP_TAG_KING_TAX, 1, 0, 0, NULL, "tax")) {
    return fail("chain: enqueue king tax");
  }
  if (pop.queue[0].chain != 0) {
    return fail("chain: non-contact popups must stay ungrouped");
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"test_ai_only_meet_is_silent_for_human", test_ai_only_meet_is_silent_for_human},
    {"test_encounter_scan_ignores_stale_owner_stamp", test_encounter_scan_ignores_stale_owner_stamp},
    {"test_contact_chains_do_not_interleave", test_contact_chains_do_not_interleave},
};
TEST_MAIN(k_cases)
