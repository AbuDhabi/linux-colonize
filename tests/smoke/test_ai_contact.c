/* Smoke: Indian meet + friction raid loot (@RAID* kinds) + prelude encroachment. */
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

  /*
   * Prelude encroachment: Soldier within Chebyshev ≤2 of tribe, no mission →
   * friction/alarm +2 (cap 100). Flag body sticky so RNG arm does not also bump.
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
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].alarm[0].friction = 10;
  ind->alarm_by_player[0] = 10;
  ind->unknown31[3] = (uint8_t)(ind->unknown31[3] | 0x20); /* skip flag-body escalate */
  ai_contact_indian_prelude(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 12) {
    return fail("prelude encroachment should bump tribe friction by 2");
  }
  if (ind->alarm_by_player[0] != 12) {
    return fail("prelude encroachment should bump alarm_by_player by 2");
  }

  /* Mission pacifies: mission present + low friction → extra −1. */
  col1.tribe[0].mission = 0;
  col1.tribe[0].alarm[0].friction = 12;
  ind->alarm_by_player[0] = 12;
  units_despawn(&units, soldier_id);
  ai_contact_indian_prelude(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 11) {
    return fail("prelude mission pacify should decay friction by 1");
  }
  if (ind->alarm_by_player[0] != 11) {
    return fail("prelude mission pacify should decay alarm_by_player by 1");
  }

  /* Relation tick should not crash. */
  ai_contact_indian_relation_tick(&ctx, 4);

  /*
   * Missionary convert pulse: adjacent Missionary + non-hostile →
   * tribe.mission = euro id and nation current_crosses++.
   */
  units.type_count = 3;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
  units.types[2].movement = 1;
  units.types[2].attack = 0;
  units.types[2].defense = 1;
  const int miss_id = units_spawn_allow_stack(&units, 2, 6, 5);
  ColonizeUnit* miss = units_get(&units, miss_id);
  if (!miss) {
    return fail("spawn missionary");
  }
  miss->nation_id = 0;
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].alarm[0].friction = 10;
  ind->alarm_by_player[0] = 10;
  const uint16_t crosses0 = col1.nation[0].current_crosses;
  ai_contact_indian_meet_trade(&ctx, 4);
  if (col1.tribe[0].mission != 0) {
    return fail("missionary convert should set tribe.mission to euro nation");
  }
  if (col1.nation[0].current_crosses != (uint16_t)(crosses0 + 1)) {
    return fail("missionary convert should bump nation current_crosses");
  }

  /*
   * Teach-skill pulse: peaceful Free Colonist adjacent to tribe →
   * tribe.state.learned and tribe-appropriate profession.
   * last_sold cargo (furs) drives Expert Fur Trapper over nation default.
   */
  units_despawn(&units, miss_id);
  euro->x = 6;
  euro->y = 5;
  euro->profession = UNITS_JOB_NONE;
  col1.tribe[0].state.learned = 0;
  col1.tribe[0].last_sold = (uint8_t)COLONIZE_CARGO_FURS;
  col1.tribe[0].alarm[0].friction = 5;
  ind->alarm_by_player[0] = 5;
  ai_contact_indian_meet_trade(&ctx, 4);
  if (!col1.tribe[0].state.learned) {
    return fail("teach-skill should set tribe.state.learned");
  }
  if (euro->profession != COLONIZE_JOB_FUR_TRAPPER) {
    return fail("teach-skill last_sold furs → Expert Fur Trapper");
  }

  /* Nation map: clear cargo override; Iroquois (7) → Fur Trapper. */
  euro->profession = UNITS_JOB_NONE;
  col1.tribe[0].state.learned = 0;
  col1.tribe[0].last_sold = 0;
  col1.tribe[0].nation_id = 7;
  ColonizeCol1Indian* iroq = &col1.indian[3];
  memset(iroq, 0, sizeof(*iroq));
  iroq->alarm_by_player[0] = 5;
  col1.tribe[0].alarm[0].friction = 5;
  ai_contact_indian_meet_trade(&ctx, 7);
  if (!col1.tribe[0].state.learned) {
    return fail("teach-skill nation map should set tribe.state.learned");
  }
  if (euro->profession != COLONIZE_JOB_FUR_TRAPPER) {
    return fail("teach-skill Iroquois nation → Expert Fur Trapper");
  }

  /*
   * Gift stand-in (5bfb_102a/1092 widgets OPEN; status chrome thinned):
   * low friction + gold >= 20 → Euro −10 gold, friction −2, status line.
   */
  col1.tribe[0].nation_id = 4;
  ind->met_by_player[0] = 1;
  ind->alarm_by_player[0] = 10;
  col1.tribe[0].alarm[0].friction = 10;
  col1.tribe[0].state.learned = 1; /* skip teach overwrite of status */
  col1.nation[0].gold = 50;
  euro->x = 6;
  euro->y = 5;
  brave->x = 5;
  brave->y = 5;
  brave->nation_id = 4;
  ctx.human_nation = 0;
  char status[128];
  status[0] = '\0';
  ctx.status = status;
  ctx.status_size = sizeof(status);
  ai_contact_indian_meet_trade(&ctx, 4);
  if (col1.nation[0].gold != 40u) {
    return fail("gift should cost Euro 10 gold");
  }
  if (col1.tribe[0].alarm[0].friction != 8) {
    return fail("gift should reduce tribe friction by 2");
  }
  if (ind->alarm_by_player[0] != 8) {
    return fail("gift should reduce alarm_by_player by 2");
  }
  if (strstr(status, "Gift") == NULL) {
    return fail("gift should set human-facing status line");
  }

  /*
   * First-meet status (no gold → gift skips; learned set → teach skips):
   * "You meet the …"
   */
  ind->met_by_player[0] = 0;
  col1.nation[0].gold = 0;
  status[0] = '\0';
  ai_contact_indian_meet_trade(&ctx, 4);
  if (!ind->met_by_player[0]) {
    return fail("meet should set met_by_player for status path");
  }
  if (strstr(status, "meet") == NULL) {
    return fail("meet should set human-facing status line");
  }

  /*
   * Multi-loot: high friction (≥80) successful colony raid → primary @RAID*
   * plus secondary drain (−5 muskets stock and −1 tools).
   */
  euro->x = 10;
  euro->y = 10;
  brave->x = 5;
  brave->y = 5;
  brave->moves_left = 3;
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
  ai_contact_indian_raids(&ctx, 4);
  {
    const int kind_ml = ai_contact_last_raid_kind();
    if (kind_ml == AI_RAID_NOTHING) {
      return fail("high-friction multi-loot raid should not be NOTHING");
    }
    if (c->stock[COLONIZE_CARGO_MUSKETS] != muskets_ml - 5) {
      return fail("multi-loot should steal 5 muskets stock");
    }
    /* Secondary tools −1; WREAK primary also takes tools → −2 total. */
    const int tools_expect = (kind_ml == AI_RAID_WREAK) ? (tools_ml - 2) : (tools_ml - 1);
    if (c->stock[COLONIZE_CARGO_TOOLS] != tools_expect) {
      return fail("high-friction multi-loot should steal tools as secondary cargo");
    }
    const int primary_hit = (c->stock[COLONIZE_CARGO_FOOD] < food_ml) ||
                            (c->population < pop_ml) || (col1.nation[0].gold < gold_ml) ||
                            (kind_ml == AI_RAID_BURN) || (kind_ml == AI_RAID_SHIP) ||
                            (kind_ml == AI_RAID_SCALP) || (kind_ml == AI_RAID_STORES) ||
                            (kind_ml == AI_RAID_WREAK) || (kind_ml == AI_RAID_GOLD);
    if (!primary_hit) {
      return fail("multi-loot should apply a primary @RAID* outcome");
    }
  }

  /*
   * FUN_4d56_359c: high alarm + Scout adjacent → prefer displace (still active,
   * moved 1–2 tiles). Brave moves_left=0 so combat arm skips before 359c.
   */
  units.type_count = 4;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Scout");
  units.types[3].movement = 4;
  units.types[3].attack = 0;
  units.types[3].defense = 1;
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
  brave->moves_left = 0;
  ind->alarm_by_player[0] = 90;
  col1.tribe[0].alarm[0].friction = 90;
  status[0] = '\0';
  const int sx0 = scout->x;
  const int sy0 = scout->y;
  ai_contact_indian_raids(&ctx, 4);
  scout = units_get(&units, scout_id);
  if (!scout || !scout->active) {
    return fail("359c should displace Scout when free land exists");
  }
  if (scout->x == sx0 && scout->y == sy0) {
    return fail("359c should move Scout 1–2 tiles away");
  }
  if (strstr(status, "warn") == NULL) {
    return fail("359c displace should set status warn line");
  }

  /*
   * Thin alarmed refuse-talk (2154 deep PARKED): met + alarm>=55 → skip trade/gift,
   * human status "Natives refuse to talk."
   */
  {
    col1.nation[0].gold = 100;
    ind->met_by_player[0] = 1;
    ind->alarm_by_player[0] = 60;
    col1.tribe[0].alarm[0].friction = 60;
    euro->x = 6;
    euro->y = 5;
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 1;
    status[0] = '\0';
    const uint32_t gold0 = col1.nation[0].gold;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != gold0) {
      return fail("alarmed refuse-talk should not gift/trade gold");
    }
    if (strstr(status, "refuse") == NULL) {
      fprintf(stderr, "smoke_ai_contact: alarmed status '%s'\n", status);
      return fail("alarmed meet should set refuse-to-talk status");
    }
  }

  /*
   * Blocked displace → despawn: isolate Scout on a land islet (ocean around).
   */
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 25; /* ocean */
  }
  map.terrain[5 * 16 + 5] = 1; /* brave */
  map.terrain[6 * 16 + 5] = 1; /* scout */
  scout->x = 6;
  scout->y = 5;
  scout->active = true;
  brave->x = 5;
  brave->y = 5;
  brave->moves_left = 0;
  euro->x = 10; /* clear scout tile */
  euro->y = 10;
  ind->alarm_by_player[0] = 90; /* 359c gate */
  col1.tribe[0].alarm[0].friction = 90;
  status[0] = '\0';
  ai_contact_indian_raids(&ctx, 4);
  scout = units_get(&units, scout_id);
  if (scout && scout->active) {
    return fail("359c should despawn Scout when displace is blocked");
  }
  if (strstr(status, "kill") == NULL) {
    return fail("359c despawn should set status kill line");
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  col1_save_free(&col1);
  fprintf(stderr, "smoke_ai_contact: ok (last_raid_kind=%d)\n", ai_contact_last_raid_kind());
  return 0;
}
