/*
 * Stage-seam smoke: the 2026-09-16 splits made ai_euro_act_*, game_update_*,
 * game_render_*, game_move_*, ai_contact_raid_*, ai_king_0982_*, ai_021a_*,
 * turn_step_* and turn_year_end_* file-local (static). core/internal.h's
 * COLONIZE_INTERNAL macro (COLONIZE_TESTING flips it to external linkage on
 * colonize_core, see CMakeLists.txt) plus each module's <module>_internal.h
 * now let a test call one stage directly instead of only through its
 * dispatcher. This file exercises one stage per header to prove the seam
 * actually works, not to re-cover behaviour the module's own tests already
 * pin.
 */
#include "core/ai_contact_internal.h"
#include "core/ai_euro_internal.h"
#include "core/ai_internal.h"
#include "core/col1_save.h"
#include "core/game_loop_internal.h"
#include "core/turn_internal.h"

#include "../common/ai_fixture.h"
#include "../common/test_runner.h"

#include <stdio.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_stage_seams: FAIL %s\n", msg);
  return 1;
}

/* turn_year_end_rival_rebels: pure function of (rebel_sentiment, census_pop_proxy). */
static int test_turn_year_end_rival_rebels(void) {
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.nation[1].rebel_sentiment = 50;
  col1.stuff.census_pop_proxy[1] = 20;
  const int v = turn_year_end_rival_rebels(&col1, 1);
  if (v != 10) { /* 50 * 20 / 100 */
    fprintf(stderr, "  got %d want 10\n", v);
    return fail("turn_year_end_rival_rebels formula");
  }
  /* Out-of-range rival is a no-op 0, not a crash. */
  if (turn_year_end_rival_rebels(&col1, 99) != 0) {
    return fail("turn_year_end_rival_rebels range guard");
  }
  return 0;
}

/* ai_contact_raid_alarm_delta: pure per-kind table, DOS-literal deltas. */
static int test_ai_contact_raid_alarm_delta(void) {
  if (ai_contact_raid_alarm_delta(AI_RAID_NOTHING) != 0) {
    return fail("raid_alarm_delta NOTHING");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_STORES) != -4) {
    return fail("raid_alarm_delta STORES");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_SHIP) != -16) {
    return fail("raid_alarm_delta SHIP");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_BURN) != -12) {
    return fail("raid_alarm_delta BURN");
  }
  if (ai_contact_raid_alarm_delta(AI_RAID_GOLD) != -8) {
    return fail("raid_alarm_delta GOLD");
  }
  return 0;
}

/* FUN_5fef_0f14's five human-victim raid outcome cue arms. */
static int test_ai_contact_raid_sound_rows(void) {
  const AiRaidChrome* row = ai_contact_raid_chrome_row(AI_RAID_NOTHING, 0);
  if (!row || row->sound != 0x5b || row->sound2 != -1) {
    return fail("raid sound NOTHING must be 0x5b");
  }
  row = ai_contact_raid_chrome_row(AI_RAID_STORES, 0);
  if (!row || row->sound != 0x4f || row->sound2 != -1) {
    return fail("raid sound STORES must be 0x4f");
  }
  row = ai_contact_raid_chrome_row(AI_RAID_BURN, 1);
  if (!row || row->sound != 0x53 || row->sound2 != -1) {
    return fail("raid sound BURN must be 0x53");
  }
  row = ai_contact_raid_chrome_row(AI_RAID_SHIP, 0);
  if (!row || row->sound != 0x4b || row->sound2 != 0x4d) {
    return fail("raid sound SHIP must be 0x4b then 0x4d");
  }
  row = ai_contact_raid_chrome_row(AI_RAID_GOLD, 0);
  if (!row || row->sound != 0x4e || row->sound2 != -1) {
    return fail("raid sound GOLD must be 0x4e");
  }
  return 0;
}

/* ai_021a_dir_tile on a hand-built tile ring: plain terrain, no owner, no
 * village — dest tile should score as unowned/self and never SKIP. */
static int test_ai_021a_dir_tile(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);

  struct ai_021a_ctx c;
  memset(&c, 0, sizeof(c));
  c.map = &map;
  c.units = &units;
  c.col1 = NULL;
  c.colonies = NULL;
  c.col = NULL;
  c.village = NULL;
  c.nation_id = 0;
  c.x = 4;
  c.y = 4;
  c.d = 0; /* east: MAP_DIR8 order used elsewhere; any in-range dir works */
  c.threat_nation = -1;
  c.vx = 0;
  c.vy = 0;

  const Ai021aDirStatus st = ai_021a_dir_tile(&c);
  fx_map_free(&map);

  if (st != AI_021A_DIR_OK) {
    return fail("ai_021a_dir_tile unexpectedly skipped a plain land tile");
  }
  /* Fresh layer3 is zero everywhere, so the destination reads as owned by
   * nation 0 == c.nation_id -> "not foreign", no grudge/hostile. */
  if (c.owner != c.nation_id) {
    return fail("ai_021a_dir_tile owner mismatch on zeroed layer3");
  }
  if (c.hostile != 0 || c.grudge != 0) {
    return fail("ai_021a_dir_tile hostile/grudge should be clear for own tile");
  }
  return 0;
}

/* bugs.md #553: FUN_465b_0000 local_4 — the settlement owner of a Euro
 * colony tile, overridden by the stack head's nation. ai_native_brave_step
 * uses it to keep a Brave from walking INTO a foreign colony (DOS attacks or
 * exhausts instead; it never co-locates). */
static int test_ai_465b_dest_owner(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  const size_t ci = (size_t)4 * map.width + 5;
  for (size_t i = 0; i < (size_t)map.width * map.height; ++i) {
    map.layer3[i] = 0xf0; /* unowned */
  }
  map.layer2[ci] |= 0x02u;  /* settlement bit */
  map.layer3[ci] = 0x21;    /* owner nibble 2 (Spain) */
  int rc = 0;
  if (ai_465b_dest_owner(&map, &units, 5, 4) != 2) {
    rc = fail("empty Spanish colony tile must read owner 2");
  }
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 3, 3) != -1) {
    rc = fail("empty unowned land must read -1");
  }
  ColonizeUnit* g = &units.units[0];
  g->active = true;
  g->x = 5;
  g->y = 4;
  g->aboard_ship_id = -1;
  g->nation_id = 2;
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 5, 4) != 2) {
    rc = fail("garrisoned colony must read the garrison's nation");
  }
  /* A lone Euro unit on open ground is foreign to a Brave too. */
  g->x = 3;
  g->y = 3;
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 3, 3) != 2) {
    rc = fail("unit-held open tile must read the unit's nation");
  }
  ColonizeUnit* later = &units.units[1];
  later->active = true;
  later->id = 1;
  later->x = 3;
  later->y = 3;
  later->aboard_ship_id = -1;
  later->nation_id = 3;
  units_tile_stack_arrive(&units, g->id);
  units_tile_stack_arrive(&units, later->id);
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 3, 3) != 3) {
    rc = fail("mixed stack must use latest arrival's nation");
  }
  units_tile_stack_arrive(&units, g->id);
  if (rc == 0 && ai_465b_dest_owner(&map, &units, 3, 3) != 2) {
    rc = fail("lower-slot returning unit must become stack head");
  }
  fx_map_free(&map);
  return rc;
}

/* game_render_select_palette with no display dependency: an in_menu-only
 * game state with none of the other screen flags set must pick game->palette
 * verbatim (the first arm of the cascade). */
static int test_game_render_select_palette(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  game.in_menu = true;
  game.palette.rgb[1][0] = 11;
  game.palette.rgb[1][1] = 22;
  game.palette.rgb[1][2] = 33;

  ColonizeFramebuffer8 fb;
  memset(&fb, 0, sizeof(fb));

  ColonizePalette out;
  memset(&out, 0, sizeof(out));

  game_render_select_palette(&game, &fb, &out, /*render_log_counter=*/1);

  if (out.rgb[1][0] != 11 || out.rgb[1][1] != 22 || out.rgb[1][2] != 33) {
    return fail("game_render_select_palette did not pick game->palette for in_menu");
  }
  return 0;
}

/* bugs.md #948: amount 0 is the normal whole-hold unload request. */
static int test_game_colony_unload_whole_hold(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  colonies_init(&game.colonies);
  game.colony_view_id = 0;
  ColonizeColony* colony = &game.colonies.colonies[0];
  colony->id = 0;
  colony->active = true;
  colony->nation_id = 0;
  game.colonies.colony_count = 1;

  fx_units_init(&game.units);
  game.units.type_count = 1;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  game.units.types[0].movement = 4;
  game.units.types[0].cargo = 2;
  snprintf(game.units.types[0].name, sizeof(game.units.types[0].name), "Test Ship");
  const int uid = units_spawn(&game.units, 0, 5, 5);
  if (uid < 0 || units_load_goods(&game.units, uid, COLONIZE_CARGO_SUGAR, 60) != 60) {
    return fail("whole-hold unload fixture");
  }

  const int moved = game_colony_unload_hold_commit(&game, uid, 0, 0, NULL);
  if (moved != 60 || colony->stock[COLONIZE_CARGO_SUGAR] != 60) {
    return fail("zero amount must unload the whole hold");
  }
  return 0;
}

/* campaign4 FoodToIsabella: DOS stores the trade-route cursor in +0x315b,
 * but the runtime keeps its unpacked stop index in col1_counter16 (+0x315a).
 * A wagon's ordinary colony-arrival reset must not erase that runtime cursor
 * before game_trade_route_retarget services the stop. */
static int test_trade_route_wagon_services_arrival_stop(void) {
  ColonizeGameState game;
  memset(&game, 0, sizeof(game));
  if (!fx_map_alloc(&game.world_map, 8, 8, /*terrain_fill=*/2, /*with_seen=*/true)) {
    return fail("trade-route map alloc");
  }
  game.world_map_ok = true;
  game.human_nation = 0;
  game.col1_ok = true;

  fx_units_init(&game.units);
  game.units_ok = true;
  game.units.type_count = 1;
  game.units.types[0].kind_plus1 = UNITS_KIND_WAGON + 1;
  game.units.types[0].movement = 2;
  game.units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  game.units.types[0].cargo = 2;

  fx_colonies_init(&game.colonies);
  game.colonies_ok = true;
  ColonizeColony* load = fx_colony_add(&game.colonies, 0, 5, 4, 1);
  ColonizeColony* other = fx_colony_add(&game.colonies, 0, 6, 4, 1);
  load->stock[COLONIZE_CARGO_SUGAR] = 60;
  units_set_occupancy_map(&game.world_map);
  colonies_set_occupancy_map(&game.world_map);

  ColonizeCol1TradeRoute* route = &game.col1.trade_route[0];
  route->dest_count = 2;
  route->stop[0].colony_index = (uint16_t)other->id;
  route->stop[1].colony_index = (uint16_t)load->id;
  route->stop[1].load_count = 1;
  col1_trade_nibble_set(
    route->stop[1].load_cargo_nibbles, 0, COLONIZE_CARGO_SUGAR
  );

  const int uid = units_spawn(&game.units, 0, 4, 4);
  ColonizeUnit* wagon = units_get(&game.units, uid);
  if (!wagon) {
    colonies_set_occupancy_map(NULL);
    units_set_occupancy_map(NULL);
    fx_map_free(&game.world_map);
    return fail("trade-route wagon spawn");
  }
  wagon->nation_id = 0;
  wagon->orders = UNITS_ORDER_TRADE_ROUTE;
  wagon->follow_unit_id = 0;
  wagon->col1_counter16 = 1;
  wagon->goto_x = load->x;
  wagon->goto_y = load->y;

  const ColonizeWorld w = fx_world(
    &game.units, &game.colonies, &game.world_map, &game.col1, NULL, NULL
  );
  int rc = 0;
  if (!units_try_move_w(&w, uid, load->x, load->y)) {
    rc = fail("trade-route wagon could not enter load colony");
  } else {
    game_trade_route_retarget(&game, wagon);
    int sugar = 0;
    for (int h = 0; h < units_goods_hold_count(&game.units, uid); ++h) {
      if (wagon->hold_goods_type[h] == COLONIZE_CARGO_SUGAR) {
        sugar += wagon->hold_goods_amount[h];
      }
    }
    if (sugar != 60 || load->stock[COLONIZE_CARGO_SUGAR] != 0) {
      rc = fail("trade-route arrival serviced the wrong stop");
    } else if (wagon->col1_counter16 != 0 ||
               wagon->goto_x != other->x || wagon->goto_y != other->y) {
      rc = fail("trade-route arrival did not advance to the next stop");
    }
  }

  colonies_set_occupancy_map(NULL);
  units_set_occupancy_map(NULL);
  fx_map_free(&game.world_map);
  return rc;
}


/*
 * ai_euro_5952_absorb_equip — FUN_5952_035e's absorption arm, Colonist case
 * (raw 94271-94274, an unconditional take-in) and its shared outer gate
 * `+0x1b & 0x10` (NEEDS_COLONISTS, raw 94231). Hosted in the colony tick as
 * a tile re-scan since 2026-09-18, so the seam is colony-side, not unit-side.
 */
static int test_ai_euro_5952_absorb_colonist(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("absorb map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, /*nation=*/1, 4, 4, /*pop=*/2);

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;

  const int uid = units_spawn_allow_stack(&units, 0, 4, 4);
  ColonizeUnit* u = units_get(&units, uid);
  if (!u) {
    fx_map_free(&map);
    return fail("absorb spawn colonist");
  }
  u->nation_id = 1;
  u->moves = UNITS_MP_PER_TILE;

  int labor = 0;
  /* Gate closed: the colony does not want colonists -> nothing happens. */
  c->ai_flags = 0;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 2 || !units_get(&units, uid)) {
    fx_map_free(&map);
    return fail("absorb changed state with the outer gate closed");
  }

  /* Gate open: the Colonist case has no test of its own. */
  c->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  const int pop_after = (int)colonies.colonies[0].population;
  const ColonizeUnit* gone = units_get(&units, uid);
  if (pop_after != 3) {
    fx_map_free(&map);
    return fail("absorption must add the colonist to the colony");
  }
  if (gone && gone->active) {
    fx_map_free(&map);
    return fail("the absorbed unit must leave the map");
  }
  if (labor != 0) {
    fx_map_free(&map);
    return fail("only the Soldier case touches iStack_76");
  }

  /* DOS re-scans the tile stack after every absorption (iStack_32), so two
   * colonists standing on the tile are BOTH taken in by one tick. */
  const int a1 = units_spawn_allow_stack(&units, 0, 4, 4);
  const int a2 = units_spawn_allow_stack(&units, 0, 4, 4);
  for (int i = 0; i < 2; ++i) {
    ColonizeUnit* w = units_get(&units, i == 0 ? a1 : a2);
    if (!w) {
      fx_map_free(&map);
      return fail("absorb spawn pair");
    }
    w->nation_id = 1;
    w->moves = UNITS_MP_PER_TILE;
  }
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  const int pop2 = (int)colonies.colonies[0].population;
  fx_map_free(&map);
  if (pop2 != 5) {
    return fail("the tile re-scan must absorb every eligible unit in one tick");
  }
  return 0;
}

/*
 * ai_euro_5952_absorb_equip — the Soldier/Dragoon case (raw 94239-94256).
 * Gate disjuncts: (b) the arriving unit is a specialist other than a Veteran
 * Soldier AND one of the two census cells aiStack_68[0x13]/[0x15] is non-zero,
 * (c) +0x1b bit 2 (MILITARY_SURPLUS). Absorbing clears bit 2, refunds the
 * muskets to the colony stock, keeps the profession, consumes one census cell
 * ([0x15] first, else [0x13]) and increments the tick-local iStack_76.
 */
static int test_ai_euro_5952_absorb_soldier(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("absorb-soldier map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldiers");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, /*nation=*/1, 4, 4, /*pop=*/2);
  c->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  c->stock[COLONIZE_CARGO_MUSKETS] = 0;

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;

  /* Census: one ordinary colonist to spare, no Veteran Soldier. */
  ai_euro_5952_set_absorb_census(c->id, /*nonexpert=*/1, /*vet_soldier=*/0);

  /* A Veteran Soldier (profession 0x15) is excluded from disjunct (b). */
  const int vid = units_spawn_allow_stack(&units, 0, 4, 4);
  ColonizeUnit* v = units_get(&units, vid);
  if (!v) {
    fx_map_free(&map);
    return fail("absorb-soldier spawn veteran");
  }
  v->nation_id = 1;
  v->profession = UNITS_JOB_SOLDIER;
  v->muskets = 50;
  int labor = 0;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 2) {
    fx_map_free(&map);
    return fail("a Veteran Soldier must not be absorbed through disjunct (b)");
  }

  /* MILITARY_SURPLUS (disjunct c) takes the same unit in, clears the bit,
   * refunds the muskets and keeps the profession. */
  c->ai_flags |= COLONIZE_COLONY_AI_MILITARY_SURPLUS;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 3 ||
      (c->ai_flags & COLONIZE_COLONY_AI_MILITARY_SURPLUS) != 0 ||
      c->stock[COLONIZE_CARGO_MUSKETS] != 50 ||
      (int)c->colonists[2].profession != UNITS_JOB_SOLDIER) {
    fx_map_free(&map);
    return fail("surplus absorption must clear bit 2, refund muskets, keep profession");
  }
  if (labor != 1) {
    fx_map_free(&map);
    return fail("the Soldier case must increment the tick-local iStack_76");
  }

  /* That absorption consumed a census cell too (raw 94248-94255), so re-seed
   * one before exercising disjunct (b). */
  ai_euro_5952_set_absorb_census(c->id, /*nonexpert=*/1, /*vet_soldier=*/0);

  /* Disjunct (b): a Hardy Pioneer carrying muskets, with census[0x13] = 1.
   * The absorption consumes that cell, so the second one on the tile stays
   * out even though DOS's re-scan sees it in the same tick. */
  for (int round = 0; round < 2; ++round) {
    const int pid = units_spawn_allow_stack(&units, 0, 4, 4);
    ColonizeUnit* p = units_get(&units, pid);
    if (!p) {
      fx_map_free(&map);
      return fail("absorb-soldier spawn specialist");
    }
    p->nation_id = 1;
    p->profession = UNITS_JOB_PIONEER; /* expert, != 0x15 */
    p->muskets = 50;
  }
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 4) {
    fx_map_free(&map);
    return fail("exactly one specialist must be absorbed per free census cell");
  }
  if (labor != 2) {
    fx_map_free(&map);
    return fail("the specialist absorption must also bump iStack_76");
  }
  fx_map_free(&map);
  return 0;
}

/* fx_colony_add sets population/colonist_count only; the equip arm ejects a
 * real colonist, so the slots have to be live. */
static void fx_colony_fill_colonists(ColonizeColony* c) {
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    c->tiles[i] = -1;
  }
  for (int i = 0; i < (int)c->colonist_count && i < COLONIZE_COLONY_POP_MAX; ++i) {
    c->colonists[i].active = true;
    c->colonists[i].profession = COLONIZE_PROF_FREE_COLONIST;
    c->colonists[i].field_job = -1;
  }
}

/*
 * ai_euro_5952_absorb_equip — the equip arm's SCOUT target (raw 94277-94285).
 * DOS: `if (0x65 < stock[horses]) { if (pop < 10 && pop < wanted_size) skip;
 * if (!(+0x1b & 0x10)) { local_8e = 0x16; local_136 = 1; } }`. With muskets
 * at 0 the Soldier/Dragoon arm cannot override, and with pop <= 10 the
 * local_90 conjunct short-circuits before its RNG draw, so this case is
 * fully deterministic and needs no rng.
 */
static int test_ai_euro_5952_equip_scout(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return fail("equip-scout map alloc");
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Scouts");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  /* pop 8 = ai_euro_colony_wanted_size's live answer, so `pop < wanted` is
   * false and the DOS `goto LAB_0de5` skip does NOT fire. */
  ColonizeColony* c = fx_colony_add(&colonies, /*nation=*/1, 4, 4, /*pop=*/8);
  c->ai_flags = 0;
  c->stock[COLONIZE_CARGO_MUSKETS] = 0;
  c->stock[COLONIZE_CARGO_HORSES] = 0x65; /* exactly the threshold: no arm */
  fx_colony_fill_colonists(c);

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;

  int labor = 0;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 8) {
    fx_map_free(&map);
    return fail("horses == 0x65 must not arm the Scout target (DOS tests 0x65 <)");
  }

  c->stock[COLONIZE_CARGO_HORSES] = 0x66;
  c->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS; /* +0x1b & 0x10 blocks it */
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 8) {
    fx_map_free(&map);
    return fail("NEEDS_COLONISTS must veto the Scout target");
  }

  c->ai_flags = 0;
  ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
  if ((int)c->population != 7) {
    fx_map_free(&map);
    return fail("the Scout target must re-type one colonist out of the colony");
  }
  if (c->stock[COLONIZE_CARGO_HORSES] != 0x66 - UNITS_EQUIP_HORSES) {
    fx_map_free(&map);
    return fail("FUN_15eb_1068 must charge the Scout's horses to the stock");
  }
  int scouts = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (u->active && u->x == 4 && u->y == 4) {
      scouts++;
    }
  }
  fx_map_free(&map);
  if (scouts != 1) {
    return fail("exactly one Scout must be produced per tick (local_136 is a flag)");
  }
  return 0;
}

/*
 * ai_euro_5952_absorb_equip — the equip arm's PIONEER target (raw
 * 94300-94303): `local_90 && unit_type_counts[nation][2] == 0 && local_10 &&
 * 0x13 < stock[tools]`. local_90 carries DOS's RNG(0,3) conjunct, so the
 * arm fires on roughly a quarter of the ticks; the test drives a fixed seed
 * for a fixed number of ticks and asserts the tools gate flips the outcome
 * from "some Pioneers" to "none", which is the gate this piece adds.
 */
static int equip_pioneer_run(int tools, int ticks) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, /*terrain_fill=*/0, /*with_seen=*/true)) {
    return -1;
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Pioneers");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* c = fx_colony_add(&colonies, /*nation=*/1, 4, 4, /*pop=*/20);
  c->ai_flags = 0;
  c->stock[COLONIZE_CARGO_MUSKETS] = 0;  /* Soldier/Dragoon arm stays shut */
  c->stock[COLONIZE_CARGO_HORSES] = 0;   /* Scout arm stays shut */
  c->stock[COLONIZE_CARGO_TOOLS] = tools;
  fx_colony_fill_colonists(c);

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.rng = &rng;

  int made = 0;
  int labor = 0;
  for (int t = 0; t < ticks; ++t) {
    const int before = (int)c->population;
    c->stock[COLONIZE_CARGO_TOOLS] = tools; /* one Pioneer's worth per tick */
    ai_euro_5952_absorb_equip(&ctx, 1, c, &labor);
    if ((int)c->population < before) {
      made++;
    }
  }
  fx_map_free(&map);
  return made;
}

static int test_ai_euro_5952_equip_pioneer(void) {
  const int open = equip_pioneer_run(/*tools=*/100, /*ticks=*/40);
  if (open < 0) {
    return fail("equip-pioneer map alloc");
  }
  if (open <= 0) {
    return fail("the Pioneer target must fire when tools > 0x13 and local_90 rolls");
  }
  const int shut = equip_pioneer_run(/*tools=*/0x13, /*ticks=*/40);
  if (shut != 0) {
    return fail("tools == 0x13 must veto the Pioneer target (DOS tests 0x13 <)");
  }
  return 0;
}


/*
 * bugs.md #822: FUN_465b_0000 routes a Brave step onto a foreign stack-head
 * tile with >= 3 thirds left into FUN_5fef_1b0e (raw 75467-75479,
 * 75631-75634, 75692) — the attack resolves and the Brave exhausts in place.
 * The port only exhausted, so a Brave beside a lone Euro unit stood forever.
 * Surround a full-MP Brave with foreign Soldiers: whichever direction 021a
 * picks, a committed step is an attack, so somebody must die.
 */
static int test_ai_brave_field_attack(void) {
  int rc = 0;
  int saw_move = 0;
  for (int k = 0; k < 24 && rc == 0 && !saw_move; ++k) {
    ColonizeWorldMap map;
    if (!fx_map_alloc(&map, 12, 12, /*terrain_fill=*/0, /*with_seen=*/true)) {
      return fail("brave-attack map alloc");
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.layer3[i] = 0xf0; /* unowned, continent 0 */
    }
    ColonizeUnitPool units;
    fx_units_init(&units);
    units.type_count = UNITS_KIND_MTD_BRAVE + 2;
    for (int t = 0; t < units.type_count; ++t) {
      units.types[t].movement = 1;
      units.types[t].attack = 2;
      units.types[t].defense = 2;
      units.types[t].domain = COLONIZE_UNIT_DOMAIN_LAND;
      snprintf(units.types[t].name, sizeof(units.types[t].name), "t%d", t);
    }
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    ColonizeCol1Tribe tribe;
    memset(&tribe, 0, sizeof(tribe));
    tribe.x = 5;
    tribe.y = 5;
    tribe.nation_id = 4;
    tribe.population = 3;
    col1.tribe = &tribe;
    col1.head.tribe_count = 1;
    col1.indian[0].euro_diplo[1] = 1; /* met — no first-contact ceremony */

    ColonizeUnit* b = &units.units[0];
    b->id = 1;
    b->active = true;
    b->nation_id = 4;
    b->type_index = UNITS_KIND_BRAVE;
    b->x = 5;
    b->y = 5;
    b->aboard_ship_id = -1;
    b->home_tribe_id = 0;
    b->moves = 0; /* Braves store thirds SPENT (conventions.md) */
    b->last_dir = 8;
    units.unit_count = 1;
    for (int d = 0; d < 8; ++d) {
      ColonizeUnit* f = &units.units[1 + d];
      f->id = 2 + d;
      f->active = true;
      f->nation_id = 1;
      f->type_index = 1;
      f->x = 5 + MAP_DIR8_DX[d];
      f->y = 5 + MAP_DIR8_DY[d];
      f->aboard_ship_id = -1;
      f->home_tribe_id = -1;
      f->moves = 1;
      units.unit_count++;
    }
    int before = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      before += units.units[i].active ? 1 : 0;
    }

    ColonizeDosRng rng;
    dos_rng_seed(&rng, (uint32_t)(k * 12345 + 7)); /* spread — tiny-seed trap */
    int steps = 0;
    (void)ai_native_brave_step(
      &units, &map, &col1, &rng, /*nation_id=*/4, /*seed100_init_burns=*/false, b,
      /*hx=*/5, /*hy=*/5, /*tech=*/0, /*max_mp=*/3, /*brave_index=*/0, &steps
    );
    int after = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      after += units.units[i].active ? 1 : 0;
    }
    if (steps > 0) {
      saw_move = 1;
      if (after != before - 1) {
        fprintf(stderr, "  seed %d: %d -> %d active units\n", k, before, after);
        rc = fail("a committed Brave step onto a foreign tile must resolve combat");
      } else if (b->active && (b->x != 5 || b->y != 5)) {
        rc = fail("a surviving Brave attacker must stay on its own tile");
      } else if (b->active && b->moves != 3) {
        rc = fail("an attacking Brave must exhaust its full allotment (0934)");
      }
    }
    fx_map_free(&map);
  }
  if (rc == 0 && !saw_move) {
    return fail("no seed made the Brave commit a step — test proved nothing");
  }
  return rc;
}

static const TestCase k_cases[] = {
    {"test_turn_year_end_rival_rebels", test_turn_year_end_rival_rebels},
    {"test_ai_contact_raid_alarm_delta", test_ai_contact_raid_alarm_delta},
    {"test_ai_contact_raid_sound_rows", test_ai_contact_raid_sound_rows},
    {"test_ai_021a_dir_tile", test_ai_021a_dir_tile},
    {"test_ai_465b_dest_owner", test_ai_465b_dest_owner},
    {"test_ai_brave_field_attack", test_ai_brave_field_attack},
    {"test_game_render_select_palette", test_game_render_select_palette},
    {"test_game_colony_unload_whole_hold", test_game_colony_unload_whole_hold},
    {"test_trade_route_wagon_services_arrival_stop", test_trade_route_wagon_services_arrival_stop},
    {"test_ai_euro_5952_absorb_colonist", test_ai_euro_5952_absorb_colonist},
    {"test_ai_euro_5952_absorb_soldier", test_ai_euro_5952_absorb_soldier},
    {"test_ai_euro_5952_equip_scout", test_ai_euro_5952_equip_scout},
    {"test_ai_euro_5952_equip_pioneer", test_ai_euro_5952_equip_pioneer},
};

TEST_MAIN(k_cases)
