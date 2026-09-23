/* Slice of the former tests/unit/test_ai_euro_war.c (split by feature 2026-09-23):
 * land hunts, adjacent-combat chain and defender preference ladder. */
#include "test_ai_euro_war_common.h"

/*
 * Indian×Euro war: Soldier hunts toward capital tribe over nearer non-capital.
 * Cite: ai_diplo_indian_at_war; tribe.state.capital; Cortes rich_capital path.
 */
static int unit_indian_war_capital_hunt(void) {
  const int nation = 1;
  const int indian = 4; /* Arawak */

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("indian-hunt alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  memset(units.types, 0, sizeof(units.types));
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 2;
  units.types[0].defense = 2;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);

  const int sid = units_spawn(&units, 0, 2, 2);
  ColonizeUnit* soldier = units_get(&units, sid);
  if (!soldier) {
    fx_map_free(&map);
    return fail("indian-hunt spawn");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 3 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  col1.player[nation].control = 0;
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 100;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  /* At war with Indian slot 0 (nation 4). */
  col1.indian[0].alarm_by_player[nation] = 80; /* relation 20 */
  col1.indian[0].euro_diplo[nation] |= COL1_INDIAN_MET_BIT;
  ai_diplo_indian_hostility_sync(&col1, nation);

  ColonizeCol1Tribe tribes[2];
  memset(tribes, 0, sizeof(tribes));
  tribes[0].x = 4;
  tribes[0].y = 2;
  tribes[0].nation_id = (uint8_t)indian;
  tribes[0].state.capital = 0; /* nearer non-capital */
  tribes[1].x = 10;
  tribes[1].y = 2;
  tribes[1].nation_id = (uint8_t)indian;
  tribes[1].state.capital = 1; /* farther capital — prefer */
  col1.tribe = tribes;
  col1.head.tribe_count = 2;

  ai_goals_reset();
  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;

  if (!ai_diplo_indian_any_at_war(&col1, nation)) {
    fx_map_free(&map);
    return fail("indian-hunt expected indian war");
  }

  ai_euro_dispatcher_turn(&ctx, nation);
  soldier = units_get(&units, sid);
  if (!soldier || !soldier->active) {
    fx_map_free(&map);
    return fail("indian-hunt soldier gone");
  }
  const int toward_cap =
    soldier->orders == UNITS_ORDER_AI_MOVE && soldier->goto_x == 10 && soldier->goto_y == 2;
  const int moved_east = soldier->x > 2;
  if (!toward_cap && !moved_east) {
    fprintf(
      stderr,
      "unit_ai_euro_war: indian-hunt orders=%d goto=(%d,%d) pos=(%d,%d)\n",
      soldier->orders,
      soldier->goto_x,
      soldier->goto_y,
      soldier->x,
      soldier->y
    );
    fx_map_free(&map);
    return fail("expected AI_MOVE toward capital tribe (10,2) or east move");
  }

  fx_map_free(&map);
  fprintf(
    stderr,
    "unit_ai_euro_war: indian capital hunt ok (goto_cap=%d east=%d)\n",
    toward_cap,
    moved_east
  );
  return 0;
}

/*
 * Sticky CONTACT re-hunt: fortified Soldier (hunter adjacent-attack skipped)
 * with moves left next to a war foe — sticky still try_attacks.
 */
static int unit_sticky_contact_rehunt(void) {
  const int nation = 1;
  const int foe = 2;
  const int own_x = 5;
  const int own_y = 5;
  const int foe_x = 6;
  const int foe_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("sticky alloc map");
  }
  /* layer3 is calloc'd (owner nibble 0 = nation 0); the 4d2e attack term
   * keys on the tile owner, so mark the field unclaimed like a real map. */
  memset(map.layer3, 0xf0, (size_t)(16 * 16));

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 3;
  own->y = 3;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("sticky spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = UNITS_ORDER_FORTIFIED; /* skip land_try_adjacent_attack */
  soldier->moves = 2 * UNITS_MP_PER_TILE;

  const int foe_id = units_spawn(&units, 0, foe_x, foe_y);
  ColonizeUnit* foe_u = units_get(&units, foe_id);
  if (!foe_u) {
    fx_map_free(&map);
    return fail("sticky spawn foe");
  }
  foe_u->nation_id = foe;
  foe_u->orders = 0;
  foe_u->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7);

  ai_goals_reset();

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  foe_u = units_get(&units, foe_id);
  const int combat_done =
    (soldier == NULL || !soldier->active) || (foe_u == NULL || !foe_u->active);

  if (!combat_done) {
    fprintf(
      stderr,
      "unit_ai_euro_war: sticky soldier_active=%d foe_active=%d moves=%d orders=%d\n",
      soldier && soldier->active,
      foe_u && foe_u->active,
      soldier ? soldier->moves : -1,
      soldier ? soldier->orders : -1
    );
    fx_map_free(&map);
    return fail("sticky CONTACT re-hunt should attempt combat vs adjacent war foe");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: sticky CONTACT re-hunt ok\n");
  return 0;
}

/*
 * Thin multi-step land adjacent combat: Soldier with MP>1 kills foe A then
 * continues onto adjacent foe B in the same act (drain moves). Cite:
 * euro_unit_act §2c multi-step combat; ai_euro_land_try_adjacent_attack chain.
 */
static int unit_land_adjacent_combat_chain(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("combat-chain alloc map");
  }
  /* layer3 is calloc'd (owner nibble 0 = nation 0); the 4d2e attack term
   * keys on the tile owner, so mark the field unclaimed like a real map. */
  memset(map.layer3, 0xf0, (size_t)(16 * 16));

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 2;
  own->y = 2;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  /* Soldier — foeA — foeB in a line (east). */
  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("combat-chain spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 3 * UNITS_MP_PER_TILE;

  const int foe_a = units_spawn(&units, 0, 6, 5);
  ColonizeUnit* fa = units_get(&units, foe_a);
  const int foe_b = units_spawn(&units, 0, 7, 5);
  ColonizeUnit* fb = units_get(&units, foe_b);
  if (!fa || !fb) {
    fx_map_free(&map);
    return fail("combat-chain spawn foes");
  }
  fa->nation_id = foe;
  fa->orders = 0;
  fa->moves = 0;
  fb->nation_id = foe;
  fb->orders = 0;
  fb->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset();

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL; /* deterministic attack>=defense wins */
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  fa = units_get(&units, foe_a);
  fb = units_get(&units, foe_b);
  const int a_dead = !fa || !fa->active;
  const int b_dead = !fb || !fb->active;
  /* bugs.md #243: a land attacker stays put after a win — foe A (adjacent)
   * dies; the AI may then STEP into the vacated tile as a normal move, but
   * the attack itself no longer carries it there, so foe B two tiles out
   * survives the act. */
  if (!a_dead || b_dead) {
    fprintf(
      stderr,
      "unit_ai_euro_war: chain soldier=%d,%d moves=%d a_dead=%d b_dead=%d\n",
      soldier ? soldier->x : -1,
      soldier ? soldier->y : -1,
      soldier ? soldier->moves : -1,
      a_dead,
      b_dead
    );
    fx_map_free(&map);
    return fail("adjacent foe dies, attacker stays put, far foe survives");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: land adjacent combat (stay-put) ok\n");
  return 0;
}

/*
 * FUN_521d_20e6 `0x46` gate: combat-capable land unit adjacent to an
 * *undefended* foreign Euro colony (no unit on the tile) walks in and
 * seizes it outright — no combat needed. Distinct from
 * unit_land_adjacent_combat_chain (defended foe) and from the goal-driven
 * MILITARY-goto capture path (this fires opportunistically regardless of
 * the unit's assigned goal).
 */
static int unit_land_adjacent_colony_seize(void) {
  const int nation = 1;
  const int foe = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("colony-seize alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 3;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 2;
  own->y = 2;
  own->population = 2;
  own->colonist_count = 2;
  ColonizeColony* target = &colonies.colonies[1];
  target->id = 1;
  target->active = true;
  target->nation_id = foe;
  target->x = 6;
  target->y = 5;
  target->population = 1;
  target->colonist_count = 1;
  target->stock[0] = 30; /* plunder should be reported, not required to move it */
  colonies.colony_count = 2;

  /* Soldier adjacent to the foe colony tile — no defender there. */
  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("colony-seize spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 3 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe);

  ai_goals_reset();

  uint32_t turn = 40;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  const ColonizeColony* seized = colonies_get(&colonies, 1);
  const int on_tile = soldier && soldier->active && soldier->x == 6 && soldier->y == 5;
  const int captured = seized && seized->active && seized->nation_id == nation;
  if (!on_tile || !captured) {
    fprintf(
      stderr,
      "unit_ai_euro_war: seize soldier=(%d,%d) active=%d colony_nation=%d\n",
      soldier ? soldier->x : -1,
      soldier ? soldier->y : -1,
      soldier ? soldier->active : -1,
      seized ? seized->nation_id : -1
    );
    fx_map_free(&map);
    return fail("land unit should walk into and seize an undefended adjacent foe colony");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: land adjacent undefended colony seize ok\n");
  return 0;
}

/*
 * Thin 20e6 land adjacent-foe pick: Soldier between fortified high-defense foe
 * (N) and weak Free Colonist (S). Prefer the weaker/non-fortified target.
 * Old first-dir scan would hit N first.
 */
static int unit_land_adjacent_foe_prefer_weak(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("adj-foe alloc map");
  }
  /* layer3 is calloc'd (owner nibble 0 = nation 0); the 4d2e attack term
   * keys on the tile owner, so mark the field unclaimed like a real map. */
  memset(map.layer3, 0xf0, (size_t)(16 * 16));

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 8;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Free Colonist");
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 0;
  units.types[1].defense = 1;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 2;
  own->y = 2;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("adj-foe spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 1 * UNITS_MP_PER_TILE; /* one adjacent fight only — no re-hunt onto fortified */

  /* Strong fortified foe to the north (first octant dir) — should NOT be preferred. */
  const int strong_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* strong = units_get(&units, strong_id);
  if (!strong) {
    fx_map_free(&map);
    return fail("adj-foe spawn strong");
  }
  strong->nation_id = foe_nat;
  strong->orders = UNITS_ORDER_FORTIFIED;
  strong->moves = 0;

  /* Weak colonist to the south — preferred target. */
  const int weak_id = units_spawn(&units, 1, own_x, own_y + 1);
  ColonizeUnit* weak = units_get(&units, weak_id);
  if (!weak) {
    fx_map_free(&map);
    return fail("adj-foe spawn weak");
  }
  weak->nation_id = foe_nat;
  weak->orders = 0;
  weak->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  /* No RNG → deterministic: attack 8 >= defense 1 → attacker wins vs weak. */
  ai_goals_reset();

  uint32_t turn = 41;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  strong = units_get(&units, strong_id);
  weak = units_get(&units, weak_id);

  const int weak_dead =
    weak == NULL || !weak->active || (weak->nation_id == nation);
  const int strong_alive = strong && strong->active && strong->nation_id == foe_nat;
  const int own_alive = soldier && soldier->active;

  if (!weak_dead || !strong_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-foe own=%d weak_dead=%d strong_alive=%d\n",
      own_alive,
      weak_dead,
      strong_alive
    );
    fx_map_free(&map);
    return fail("expected attack on weak colonist, fortified Soldier left alone");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer-weak ok\n");
  return 0;
}

/*
 * LAB_521d_4d2e adjacent foes: Scout (N) and Treasure (S), both at war with
 * the mover. The DOS scorer takes whichever direction scores higher (the
 * former "prefer Treasure loot" rule was a Linux invention, retired with the
 * adjacent-attack stand-in, bugs.md #521); the test pins that exactly one of
 * the two is attacked and killed.
 */
static int unit_land_adjacent_foe_prefer_treasure(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("adj-treasure alloc map");
  }
  /* layer3 is calloc'd (owner nibble 0 = nation 0); the 4d2e attack term
   * keys on the tile owner, so mark the field unclaimed like a real map. */
  memset(map.layer3, 0xf0, (size_t)(16 * 16));

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 8;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Scout");
  units.types[1].movement = 4;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].attack = 0;
  units.types[1].defense = 0;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Treasure");
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].attack = 0;
  units.types[2].defense = 0;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 2;
  own->y = 2;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("adj-treasure spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 1 * UNITS_MP_PER_TILE;

  const int scout_id = units_spawn(&units, 1, own_x, own_y - 1); /* N first */
  ColonizeUnit* scout = units_get(&units, scout_id);
  if (!scout) {
    fx_map_free(&map);
    return fail("adj-treasure spawn scout");
  }
  scout->nation_id = foe_nat;
  scout->orders = 0;
  scout->moves = 0;

  const int treasure_id = units_spawn(&units, 2, own_x, own_y + 1); /* S */
  ColonizeUnit* treasure = units_get(&units, treasure_id);
  if (!treasure) {
    fx_map_free(&map);
    return fail("adj-treasure spawn treasure");
  }
  treasure->nation_id = foe_nat;
  treasure->orders = 0;
  treasure->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();

  uint32_t turn = 41;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  scout = units_get(&units, scout_id);
  treasure = units_get(&units, treasure_id);

  const int treasure_dead = treasure == NULL || !treasure->active;
  const int scout_dead = !scout || !scout->active;
  const int own_alive = soldier && soldier->active;

  if (treasure_dead == scout_dead || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-treasure own=%d treasure_dead=%d scout_dead=%d\n",
      own_alive,
      treasure_dead,
      scout_dead
    );
    fx_map_free(&map);
    return fail("expected the 4d2e pick to attack exactly one adjacent foe");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe 4d2e pick ok\n");
  return 0;
}

/*
 * Thin 20e6 land adjacent-foe: same-type Soldiers — prefer open-field over
 * Stockade colony tile (+100% defense). Cite: colonies_fortification_defense_bonus_percent;
 * units_resolve_land_combat_ff Stockade replace fortified ×2.
 */
static int unit_land_adjacent_foe_prefer_open_over_stockade(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("adj-stockade alloc map");
  }
  /* layer3 is calloc'd (owner nibble 0 = nation 0); the 4d2e attack term
   * keys on the tile owner, so mark the field unclaimed like a real map. */
  memset(map.layer3, 0xf0, (size_t)(16 * 16));

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_type_count = 1;
  ColonizeColony* foe_col = &colonies.colonies[0];
  foe_col->id = 0;
  foe_col->active = true;
  foe_col->nation_id = foe_nat;
  foe_col->x = own_x;
  foe_col->y = own_y - 1; /* N: Stockade colony */
  foe_col->population = 2;
  foe_col->colonist_count = 2;
  foe_col->has_building[0] = true;
  ColonizeColony* own = &colonies.colonies[1];
  own->id = 1;
  own->active = true;
  own->nation_id = nation;
  own->x = 1;
  own->y = 1;
  own->population = 1;
  own->colonist_count = 1;
  colonies.colony_count = 2;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("adj-stockade spawn soldier");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 1 * UNITS_MP_PER_TILE;

  /* Stockade defender to the north (first dir) — tougher (def 4→8). */
  const int stock_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* stock = units_get(&units, stock_id);
  if (!stock) {
    fx_map_free(&map);
    return fail("adj-stockade spawn stockade foe");
  }
  stock->nation_id = foe_nat;
  stock->orders = 0;
  stock->moves = 0;

  /* Open-field same Soldier to the south — preferred (def 4). */
  const int open_id = units_spawn(&units, 0, own_x, own_y + 1);
  ColonizeUnit* open = units_get(&units, open_id);
  if (!open) {
    fx_map_free(&map);
    return fail("adj-stockade spawn open foe");
  }
  open->nation_id = foe_nat;
  open->orders = 0;
  open->moves = 0;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();

  uint32_t turn = 42;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 42;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  stock = units_get(&units, stock_id);
  open = units_get(&units, open_id);

  const int open_dead = open == NULL || !open->active;
  const int stock_alive = stock && stock->active;
  const int own_alive = soldier && soldier->active;

  if (!open_dead || !stock_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-stockade own=%d open_dead=%d stock_alive=%d\n",
      own_alive,
      open_dead,
      stock_alive
    );
    fx_map_free(&map);
    return fail("expected attack on open-field Soldier, Stockade left alone");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer open over Stockade ok\n");
  return 0;
}

/*
 * FUN_157e_004a vet peel: same-type Soldiers — prefer non-veteran (profession
 * none) over Veteran (UNITS_JOB_SOLDIER → +50% toughness). Cite: FUN_157e_004a
 * type Soldier/Dragoon + profession 0x15.
 */
static int unit_land_adjacent_foe_prefer_non_veteran(void) {
  const int nation = 1;
  const int foe_nat = 2;
  const int own_x = 5;
  const int own_y = 5;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    return fail("adj-vet alloc map");
  }
  /* layer3 is calloc'd (owner nibble 0 = nation 0); the 4d2e attack term
   * keys on the tile owner, so mark the field unclaimed like a real map. */
  memset(map.layer3, 0xf0, (size_t)(16 * 16));

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Soldier");
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 8;
  units.types[0].defense = 8;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  ColonizeColony* own = &colonies.colonies[0];
  own->id = 0;
  own->active = true;
  own->nation_id = nation;
  own->x = 2;
  own->y = 2;
  own->population = 2;
  own->colonist_count = 2;
  colonies.colony_count = 1;

  const int own_id = units_spawn(&units, 0, own_x, own_y);
  ColonizeUnit* soldier = units_get(&units, own_id);
  if (!soldier) {
    fx_map_free(&map);
    return fail("adj-vet spawn own");
  }
  soldier->nation_id = nation;
  soldier->orders = 0;
  soldier->moves = 1 * UNITS_MP_PER_TILE;
  soldier->profession = UNITS_JOB_NONE;

  /* Veteran to the north (first octant) — tougher via +50%. */
  const int vet_id = units_spawn(&units, 0, own_x, own_y - 1);
  ColonizeUnit* vet = units_get(&units, vet_id);
  if (!vet) {
    fx_map_free(&map);
    return fail("adj-vet spawn veteran");
  }
  vet->nation_id = foe_nat;
  vet->orders = 0;
  vet->moves = 0;
  vet->profession = UNITS_JOB_SOLDIER;

  /* Plain Soldier to the south — preferred. */
  const int plain_id = units_spawn(&units, 0, own_x, own_y + 1);
  ColonizeUnit* plain = units_get(&units, plain_id);
  if (!plain) {
    fx_map_free(&map);
    return fail("adj-vet spawn plain");
  }
  plain->nation_id = foe_nat;
  plain->orders = 0;
  plain->moves = 0;
  plain->profession = UNITS_JOB_NONE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
  }
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();

  uint32_t turn = 46;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;
  ctx.rng_seed = 47;

  ai_euro_dispatcher_turn(&ctx, nation);

  soldier = units_get(&units, own_id);
  vet = units_get(&units, vet_id);
  plain = units_get(&units, plain_id);

  const int plain_dead = plain == NULL || !plain->active;
  const int vet_alive = vet && vet->active;
  const int own_alive = soldier && soldier->active;

  if (!plain_dead || !vet_alive || !own_alive) {
    fprintf(
      stderr,
      "unit_ai_euro_war: adj-vet own=%d plain_dead=%d vet_alive=%d\n",
      own_alive,
      plain_dead,
      vet_alive
    );
    fx_map_free(&map);
    return fail("expected attack on non-veteran Soldier, veteran left alone");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: adjacent-foe prefer non-veteran ok\n");
  return 0;
}

/*
 * (Deleted 2026-09-23, bugs.md #759/#760.) Four artillery tests lived here,
 * pinning invented AI arms now removed from ai_euro.c: the "+16 siege
 * approach / +10 prefers a fortified port" direction-scorer adds
 * (unit_artillery_adjacent_prefer_stockade), the peace colony-defence wake
 * (unit_peace_artillery_border_wake) and the two Artillery-fortify arms
 * (unit_peace_artillery_fortify_colony, unit_artillery_fortify_colony).
 * FUN_521d_20e6 has no `+0x3146 == 0x0b` act arm and no additive colony or
 * fortification term (raw 88266-90445); artillery on its own colony is
 * handled by the type-agnostic LAB_5899 garrison arm (raw 88584-88612).
 */

/*
 * Dragoon land hunt: prefer open colony over farther Stockade (MD slack ≤3).
 * Cite: king_ref Dragoon open bias; leave fortified ports to Artillery.
 */
static int unit_dragoon_hunt_prefer_open(void) {
  const int nation = 1;
  const int foe_nat = 2;

  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 20, 20, 1, false)) {
    return fail("dragoon-hunt alloc map");
  }

  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Dragoon");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].attack = 6;
  units.types[0].defense = 4;

  ColonizeColonyPool colonies;
  fx_colonies_init(&colonies);
  snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
  colonies.building_type_count = 1;
  /* Stockade off the eastbound path (8,8); open at (10,5) — prefer open within slack. */
  ColonizeColony* stock_col = &colonies.colonies[0];
  stock_col->id = 0;
  stock_col->active = true;
  stock_col->nation_id = foe_nat;
  stock_col->x = 8;
  stock_col->y = 8;
  stock_col->population = 2;
  stock_col->colonist_count = 2;
  stock_col->has_building[0] = true;
  ColonizeColony* open_col = &colonies.colonies[1];
  open_col->id = 1;
  open_col->active = true;
  open_col->nation_id = foe_nat;
  open_col->x = 10;
  open_col->y = 5;
  open_col->population = 1;
  open_col->colonist_count = 1;
  colonies.colony_count = 2;

  const int own_id = units_spawn(&units, 0, 5, 5);
  ColonizeUnit* drag = units_get(&units, own_id);
  if (!drag) {
    fx_map_free(&map);
    return fail("dragoon-hunt spawn");
  }
  drag->nation_id = nation;
  drag->orders = 0;
  drag->moves = 4 * UNITS_MP_PER_TILE;

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }
  col1.head.difficulty = 0;
  col1.nation[nation].gold = 50;
  /* Quiet the live 5d04 no-ships gold floor; gold < 1000 keeps the 5c3c
   * ladder / recruit / Artillery buys naturally inert (blank census). */
  col1.stuff.ship_counts[nation] = 1;
  col1.nation[foe_nat].gold = 50;
  ai_diplo_declare_war(&col1, nation, foe_nat);

  ai_goals_reset();
  uint32_t turn = 45;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = NULL;

  ai_euro_dispatcher_turn(&ctx, nation);

  drag = units_get(&units, own_id);
  if (!drag || !drag->active) {
    fx_map_free(&map);
    return fail("dragoon-hunt despawned");
  }
  if (drag->goto_x != 10 || drag->goto_y != 5) {
    fprintf(
      stderr,
      "unit_ai_euro_war: dragoon-hunt goto=(%d,%d) want open (10,5)\n",
      drag->goto_x,
      drag->goto_y
    );
    fx_map_free(&map);
    return fail("Dragoon hunt should prefer open colony over Stockade");
  }

  fx_map_free(&map);
  fprintf(stderr, "unit_ai_euro_war: Dragoon hunt prefer open ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_indian_war_capital_hunt", unit_indian_war_capital_hunt},
    {"unit_sticky_contact_rehunt", unit_sticky_contact_rehunt},
    {"unit_land_adjacent_combat_chain", unit_land_adjacent_combat_chain},
    {"unit_land_adjacent_colony_seize", unit_land_adjacent_colony_seize},
    {"unit_land_adjacent_foe_prefer_weak", unit_land_adjacent_foe_prefer_weak},
    {"unit_land_adjacent_foe_prefer_treasure", unit_land_adjacent_foe_prefer_treasure},
    {"unit_land_adjacent_foe_prefer_open_over_stockade", unit_land_adjacent_foe_prefer_open_over_stockade},
    {"unit_land_adjacent_foe_prefer_non_veteran", unit_land_adjacent_foe_prefer_non_veteran},
    {"unit_dragoon_hunt_prefer_open", unit_dragoon_hunt_prefer_open},
};
TEST_MAIN(k_cases)
