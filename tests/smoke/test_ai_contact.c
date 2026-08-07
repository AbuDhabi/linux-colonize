/* Smoke: Indian meet + friction raid loot (@RAID* kinds). */
#include "core/ai_contact.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_contact: FAIL %s\n", msg);
  return 1;
}

int main(void) {
  ColonizeCol1Save col1;
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

  ColonizeCol1Indian* ind = &col1.indian[0];
  memset(ind, 0, sizeof(*ind));
  ind->met_by_player[0] = 0;
  ind->alarm_by_player[0] = 0;

  ColonizeWorldMap map;
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

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 2;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Brave");
  units.types[0].movement = 3;
  units.types[0].attack = 2;
  units.types[0].defense = 1;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Free Colonist");
  units.types[1].movement = 1;
  units.types[1].attack = 0;
  units.types[1].defense = 1;

  const int brave_id = units_spawn_allow_stack(&units, 0, 5, 5);
  const int euro_id = units_spawn_allow_stack(&units, 1, 8, 5);
  ColonizeUnit* brave = units_get(&units, brave_id);
  ColonizeUnit* euro = units_get(&units, euro_id);
  if (!brave || !euro) {
    return fail("spawn");
  }
  brave->nation_id = 4;
  brave->moves_left = 3;
  euro->nation_id = 0;

  /* Meet needs adjacency — temporarily place euro next to brave. */
  euro->x = 6;
  euro->y = 5;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);

  uint32_t turn = 1;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng_seed = 42;

  /* Meet: adjacent Euro → met_by_player + relation bump; peaceful friction decay. */
  const uint8_t rel0 = col1.nation[0].relation_by_indian[0];
  ai_contact_indian_meet_trade(&ctx, 4);
  if (!ind->met_by_player[0]) {
    return fail("meet should set met_by_player[0]");
  }
  if (col1.nation[0].relation_by_indian[0] <= rel0) {
    return fail("meet should bump relation_by_indian");
  }
  /* Move Euro away so colony raid is not pre-empted by adjacent combat. */
  euro->x = 10;
  euro->y = 10;

  /* Raid: high friction, brave on colony tile → @RAID* loot path. */
  ind->alarm_by_player[0] = 55;
  col1.tribe[0].alarm[0].friction = 55;

  ColonizeColony* c = &colonies.colonies[0];
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
  brave->moves_left = 3;

  const int food0 = c->stock[COLONIZE_CARGO_FOOD];
  const uint8_t rel_pre_raid = col1.nation[0].relation_by_indian[0];
  ai_contact_indian_raids(&ctx, 4);
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
    brave->moves_left = 3;
    ai_contact_indian_raids(&ctx, 4);
  }
  if (ai_contact_last_raid_kind() == AI_RAID_NOTHING && c->stock[COLONIZE_CARGO_FOOD] == food0) {
    /* Still nothing: apply path must have run — accept if attacks bumped. */
    if (col1.tribe[0].alarm[0].attacks == 0) {
      return fail("raid should mutate colony or record attacks");
    }
  } else if (c->stock[COLONIZE_CARGO_FOOD] >= food0 && c->population >= 3 &&
             ai_contact_last_raid_kind() != AI_RAID_GOLD &&
             ai_contact_last_raid_kind() != AI_RAID_SHIP &&
             ai_contact_last_raid_kind() != AI_RAID_BURN &&
             ai_contact_last_raid_kind() != AI_RAID_NOTHING) {
    return fail("expected colony stock/pop change for raid kind");
  }
  /* High-friction successful raid → Indian×Euro hostility via relation_delta. */
  if (ai_contact_last_raid_kind() != AI_RAID_NOTHING &&
      col1.nation[0].relation_by_indian[0] >= rel_pre_raid) {
    return fail("high-friction raid should escalate Indian×Euro hostility");
  }

  /* Prelude mission clear on high alarm. */
  col1.tribe[0].mission = 0;
  ind->alarm_by_player[0] = 90;
  ai_contact_indian_prelude(&ctx, 4);
  if (col1.tribe[0].mission != 0xff) {
    return fail("prelude should clear mission on high alarm");
  }

  /* Relation tick should not crash. */
  ai_contact_indian_relation_tick(&ctx, 4);

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  col1_save_free(&col1);
  fprintf(stderr, "smoke_ai_contact: ok (last_raid_kind=%d)\n", ai_contact_last_raid_kind());
  return 0;
}
