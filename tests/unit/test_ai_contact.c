/* Smoke: Indian meet + friction raid loot (@RAID* kinds) + prelude encroachment. */
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_contact: FAIL %s\n", msg);
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
  ind->euro_diplo[0] = 0;
  ind->alarm_by_player[0] = 0;
  /* Zero-init founding_father[] == 0 would falsely grant FF to Euro 0. */
  for (int ffi = 0; ffi < (int)COLONIZE_COL1_FF_COUNT; ++ffi) {
    col1.head.founding_father[ffi] = -1;
  }

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

  /* Meet: adjacent Euro → euro_diplo + relation bump; peaceful friction decay. */
  const uint8_t rel0 = col1.nation[0].relation_by_indian[0];
  ai_contact_indian_meet_trade(&ctx, 4);
  if (!ind->euro_diplo[0]) {
    return fail("meet should set euro_diplo[0]");
  }
  if (col1.nation[0].relation_by_indian[0] <= rel0) {
    return fail("meet should bump relation_by_indian");
  }
  /* Peaceful meet-trade needs relation ≥40 (very-low refuse gate). */
  col1.nation[0].relation_by_indian[0] = 80;
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

  /*
   * Spain conquest bias: raid gate opens at friction/alarm ≥35 (others ≥40).
   * EN at 30 must not gate; SP at 35 must. Cite: fandom nation bias.
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
    brave->moves_left = 3;
    const int food_sp = csp->stock[COLONIZE_CARGO_FOOD];
    ai_contact_indian_raids(&ctx, 4);
    if (ai_contact_last_raid_kind() == AI_RAID_NOTHING &&
        csp->stock[COLONIZE_CARGO_FOOD] == food_sp &&
        col1.tribe[0].alarm[2].attacks == 0) {
      return fail("Spain gate ≥35 should allow raid when EN below 40");
    }
    /* Cleanup SP colony so later arms stay on EN fixtures. */
    csp->active = false;
    ind->alarm_by_player[2] = 0;
    col1.tribe[0].alarm[2].friction = 0;
    brave->x = 5;
    brave->y = 5;
  }

  /* Prelude mission burn/clear on high alarm (≥80); human status. */
  {
    char status_burn[128];
    status_burn[0] = '\0';
    ctx.status = status_burn;
    ctx.status_size = sizeof(status_burn);
    ctx.human_nation = 0;
    col1.tribe[0].mission = 0;
    ind->alarm_by_player[0] = 80; /* ≥80 burn threshold (FUN_4cc6_0000) */
    col1.tribe[0].alarm[0].friction = 10;
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].mission != 0xff) {
      return fail("prelude should clear mission on alarm >= 80");
    }
    if (strstr(status_burn, "burn") == NULL || strstr(status_burn, "mission") == NULL) {
      fprintf(stderr, "unit_ai_contact: mission-burn status '%s'\n", status_burn);
      return fail("prelude mission burn should set status");
    }
    if (strstr(status_burn, "Inca") == NULL) {
      fprintf(stderr, "unit_ai_contact: mission-burn status '%s'\n", status_burn);
      return fail("mission burn should name tribe (@INDIANBURN)");
    }
    /* Friction ≥80 burn path (alarm alone low): clear mission + status. */
    col1.tribe[0].mission = 0;
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 80;
    status_burn[0] = '\0';
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].mission != 0xff) {
      return fail("prelude should clear mission on friction >= 80");
    }
    if (strstr(status_burn, "burn") == NULL || strstr(status_burn, "mission") == NULL) {
      fprintf(stderr, "unit_ai_contact: friction-burn status '%s'\n", status_burn);
      return fail("prelude friction>=80 mission burn should set status");
    }
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Ambush gear seize (@INDIANWIN1): Brave win vs musketed foe → Brave gains
   * muskets + WIN1 status. Cite: GAME.TXT @INDIANWIN1; indian_raid_outcomes.md.
   */
  {
    char st_amb[128];
    st_amb[0] = '\0';
    ctx.status = st_amb;
    ctx.status_size = sizeof(st_amb);
    ctx.human_nation = 0;
    units.type_count = 3;
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
    units.types[2].movement = 1;
    units.types[2].attack = 0;
    units.types[2].defense = 0; /* total=Brave.attack → always Brave win */
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
    brave->moves_left = 3;
    brave->muskets = 0;
    brave->horses = 0;
    c->active = false; /* avoid colony loot pre-empt — combat is adjacent */
    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    euro->x = 10;
    euro->y = 10;
    ai_contact_indian_raids(&ctx, 4);
    if (!brave->active) {
      return fail("ambush win should keep Brave alive");
    }
    if (brave->muskets < 50) {
      return fail("ambush WIN1 should transfer foe muskets onto Brave");
    }
    if (strstr(st_amb, "Muskets") == NULL ||
        (strstr(st_amb, "ambush") == NULL && strstr(st_amb, "Ambush") == NULL) ||
        strstr(st_amb, "Soldier") == NULL) {
      fprintf(stderr, "unit_ai_contact: ambush-WIN1 status '%s'\n", st_amb);
      return fail("ambush WIN1 should set Muskets seized status with unit name");
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
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->muskets = 0;
    brave->horses = 0;
    ai_contact_indian_raids(&ctx, 4);
    if (brave->horses < 50) {
      return fail("ambush WIN2 should transfer foe horses onto Brave");
    }
    if (strstr(st_amb, "Horses") == NULL) {
      fprintf(stderr, "unit_ai_contact: ambush-WIN2 status '%s'\n", st_amb);
      return fail("ambush WIN2 should set Horses seized status");
    }
    units_despawn(&units, sol2);
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

  /*
   * Prelude encroachment: Dragoon also counts (mounted military presence).
   * Cite: fandom Alarm — military presence; indian_contact.md encroachment.
   */
  {
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags | 0x20);
    /* Park colony so only unit encroachment fires. */
    c->x = 14;
    c->y = 14;
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Dragoon");
    const int drag_id = units_spawn_allow_stack(&units, 2, 7, 5);
    ColonizeUnit* drag = units_get(&units, drag_id);
    if (!drag) {
      return fail("spawn Dragoon encroacher");
    }
    drag->nation_id = 0;
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].alarm[0].friction != 12) {
      return fail("Dragoon encroachment should bump tribe friction by 2");
    }
    units_despawn(&units, drag_id);
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Artillery");
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    const int art_id = units_spawn_allow_stack(&units, 2, 7, 5);
    ColonizeUnit* art = units_get(&units, art_id);
    if (!art) {
      return fail("spawn Artillery encroacher");
    }
    art->nation_id = 0;
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].alarm[0].friction != 12) {
      return fail("Artillery encroachment should bump tribe friction by 2");
    }
    units_despawn(&units, art_id);
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    c->x = 5;
    c->y = 5;
  }

  /*
   * Colony encroachment: Euro colony Chebyshev ≤2 of unmissioned tribe → +2.
   * Cite: fandom Alarm; GAME.TXT @INDIANFOREST2; indian_contact.md.
   */
  {
    char st_col[128];
    st_col[0] = '\0';
    ctx.status = st_col;
    ctx.status_size = sizeof(st_col);
    ctx.human_nation = 0;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 39;
    ind->alarm_by_player[0] = 39;
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags | 0x20);
    c->active = true;
    c->nation_id = 0;
    c->x = 6;
    c->y = 5; /* Chebyshev 1 from tribe (5,5) */
    snprintf(c->name, sizeof(c->name), "Roanoke");
    /* Park military encroachers away. */
    euro->x = 10;
    euro->y = 10;
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].alarm[0].friction < 40) {
      return fail("colony encroachment mid-cross should bump into ≥40");
    }
    if (strstr(st_col, "Roanoke") == NULL && strstr(st_col, "concerned") == NULL) {
      fprintf(stderr, "unit_ai_contact: colony-encroach status '%s'\n", st_col);
      return fail("colony encroachment mid-cross should set concern status");
    }
    c->x = 5;
    c->y = 5;
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    ctx.status = NULL;
    ctx.status_size = 0;
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
  c->x = 14;
  c->y = 14; /* park colony — only Soldier encroachment under test */
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].alarm[0].friction = 10;
  ind->alarm_by_player[0] = 10;
  ind->unknown31_flags = (uint8_t)(ind->unknown31_flags | 0x20); /* skip flag-body escalate */
  ai_contact_indian_prelude(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 12) {
    return fail("prelude encroachment should bump tribe friction by 2");
  }
  if (ind->alarm_by_player[0] != 12) {
    return fail("prelude encroachment should bump alarm_by_player by 2");
  }

  /*
   * Unit encroachment still bumps friction; @INDIANCOMMENT chrome only fires
   * when a colony encroaches (GAME.TXT talks about colonies overusing land).
   */
  {
    char st_cmt[128];
    st_cmt[0] = '\0';
    ctx.status = st_cmt;
    ctx.status_size = sizeof(st_cmt);
    ctx.human_nation = 0;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 39;
    ind->alarm_by_player[0] = 39;
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags | 0x20);
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].alarm[0].friction < 40) {
      return fail("comment-cross should bump friction into mid band");
    }
    if (st_cmt[0] != '\0' &&
        (strstr(st_cmt, "concerned") != NULL || strstr(st_cmt, "land use") != NULL)) {
      fprintf(stderr, "unit_ai_contact: unit-only comment status '%s'\n", st_cmt);
      return fail("unit encroachment must not fire @INDIANCOMMENT chrome");
    }
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Pocahontas half-rate alarm (wiki/fandom): same encroachment with FF →
   * bumps halved (+2 → +1). Cite: docs/fandom_col1994.md Pocahontas.
   */
  {
    col1.head.founding_father[FF_POCAHONTAS] = 0; /* Euro 0 owns Pocahontas */
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags | 0x20);
    /* Reuse existing soldier at Chebyshev 2; do not double-bump. */
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].alarm[0].friction != 11) {
      return fail("Pocahontas should halve encroachment friction bump to +1");
    }
    if (ind->alarm_by_player[0] != 11) {
      return fail("Pocahontas should halve encroachment alarm bump to +1");
    }
    col1.head.founding_father[FF_POCAHONTAS] = -1; /* clear for later tests */
  }

  /*
   * French national bonus (wiki/fandom Alarm): hostility growth half as fast
   * for nation 1. Encroachment +2 → +1. Cite: docs/fandom_col1994.md Indians.
   */
  {
    ind->alarm_by_player[1] = 10;
    col1.tribe[0].alarm[1].friction = 10;
    col1.tribe[0].mission = 0xff;
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags | 0x20);
    units.type_count = 3;
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
    units.types[2].movement = 1;
    units.types[2].attack = 2;
    units.types[2].defense = 2;
    const int fr_sol = units_spawn_allow_stack(&units, 2, 7, 5);
    ColonizeUnit* frs = units_get(&units, fr_sol);
    if (!frs) {
      return fail("French encroachment spawn");
    }
    frs->nation_id = 1;
    /* EN soldier still near tribe from Pocahontas block — despawn so only FR bumps. */
    units_despawn(&units, soldier_id);
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].alarm[1].friction != 11) {
      fprintf(
        stderr,
        "unit_ai_contact: FR friction=%u want 11\n",
        (unsigned)col1.tribe[0].alarm[1].friction
      );
      return fail("French should halve encroachment friction bump to +1");
    }
    if (ind->alarm_by_player[1] != 11) {
      return fail("French should halve encroachment alarm bump to +1");
    }
    units_despawn(&units, fr_sol);
  }

  /*
   * French + Pocahontas stack: quarter-rate (+2 → +0 via successive /2).
   * Cite: ai_contact_alarm_bump_amount; docs/fandom_col1994.md.
   */
  {
    col1.head.founding_father[FF_POCAHONTAS] = 1; /* France owns Pocahontas */
    ind->alarm_by_player[1] = 20;
    col1.tribe[0].alarm[1].friction = 20;
    col1.tribe[0].mission = 0xff;
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags | 0x20);
    units.type_count = 3;
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Soldier");
    units.types[2].movement = 1;
    const int fr_sol2 = units_spawn_allow_stack(&units, 2, 7, 5);
    ColonizeUnit* frs2 = units_get(&units, fr_sol2);
    if (!frs2) {
      return fail("FR+Poca encroachment spawn");
    }
    frs2->nation_id = 1;
    ai_contact_indian_prelude(&ctx, 4);
    if (col1.tribe[0].alarm[1].friction != 20) {
      fprintf(
        stderr,
        "unit_ai_contact: FR+Poca friction=%u want 20\n",
        (unsigned)col1.tribe[0].alarm[1].friction
      );
      return fail("French+Pocahontas should quarter encroachment bump to +0");
    }
    if (ind->alarm_by_player[1] != 20) {
      return fail("French+Pocahontas should quarter alarm bump to +0");
    }
    units_despawn(&units, fr_sol2);
    col1.head.founding_father[FF_POCAHONTAS] = (int8_t)-1; /* clear FF ownership */
    c->x = 5;
    c->y = 5;
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

  /* Relation tick: met peaceful → tribe friction −1; hot alarm → +1. */
  ind->euro_diplo[0] = 1;
  col1.tribe[0].alarm[0].friction = 12;
  ind->alarm_by_player[0] = 12;
  ai_contact_indian_relation_tick(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 11) {
    return fail("peaceful relation tick should decay tribe friction by 1");
  }
  col1.tribe[0].alarm[0].friction = 50;
  ind->alarm_by_player[0] = 40; /* mid-band floor */
  ai_contact_indian_relation_tick(&ctx, 4);
  if (col1.tribe[0].alarm[0].friction != 51) {
    return fail("mid/hot relation tick should bump tribe friction by 1");
  }
  col1.tribe[0].alarm[0].friction = 10;
  ind->alarm_by_player[0] = 10;

  /*
   * Missionary convert pulse: adjacent Missionary + non-hostile →
   * tribe.mission = euro id and nation current_crosses++.
   * Human success status (teach already had success chrome).
   */
  units.type_count = 3;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
  units.types[2].movement = 1;
  units.types[2].attack = 0;
  units.types[2].defense = 1;
  int miss_id = units_spawn_allow_stack(&units, 2, 6, 5);
  ColonizeUnit* miss = units_get(&units, miss_id);
  if (!miss) {
    return fail("spawn missionary");
  }
  miss->nation_id = 0;
  col1.tribe[0].mission = 0xff;
  col1.tribe[0].alarm[0].friction = 10;
  ind->alarm_by_player[0] = 10;
  col1.nation[0].relation_by_indian[0] = 80; /* above very-low refuse */
  {
    char status_ok[128];
    status_ok[0] = '\0';
    ctx.status = status_ok;
    ctx.status_size = sizeof(status_ok);
    ctx.human_nation = 0;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    snprintf(c->name, sizeof(c->name), "Jamestown");
    const uint16_t crosses0 = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].mission != 0) {
      return fail("missionary convert should set tribe.mission to euro nation");
    }
    if (col1.nation[0].current_crosses != (uint16_t)(crosses0 + 1)) {
      return fail("missionary convert should bump nation current_crosses");
    }
    if (strstr(status_ok, "accept") == NULL || strstr(status_ok, "conversion") == NULL ||
        strstr(status_ok, "The ") == NULL) {
      fprintf(stderr, "unit_ai_contact: convert-ok status '%s'\n", status_ok);
      return fail("convert success should set tribe-named accept-conversion status");
    }
    if (strstr(status_ok, "Jamestown") == NULL) {
      fprintf(stderr, "unit_ai_contact: convert-ok status '%s'\n", status_ok);
      return fail("convert success should name nearest colony (@INDIANSCONVERT)");
    }
    /*
     * Convert once: mission already set → skip pulse (no re-crosses / no
     * accept status). Cite: indian_contact.md convert once.
     */
    {
      const uint16_t crosses1 = col1.nation[0].current_crosses;
      const uint8_t fr1 = col1.tribe[0].alarm[0].friction;
      const uint16_t al1 = ind->alarm_by_player[0];
      status_ok[0] = '\0';
      ai_contact_indian_meet_trade(&ctx, 4);
      if (col1.tribe[0].mission != 0) {
        return fail("convert-once should keep own mission");
      }
      if (col1.nation[0].current_crosses != crosses1) {
        return fail("convert-once should not bump crosses again");
      }
      if (col1.tribe[0].alarm[0].friction != fr1 || ind->alarm_by_player[0] != al1) {
        return fail("convert-once should not re-decay alarm/friction");
      }
      if (strstr(status_ok, "accept") != NULL || strstr(status_ok, "conversion") != NULL) {
        fprintf(stderr, "unit_ai_contact: convert-once status '%s'\n", status_ok);
        return fail("convert-once should skip accept-conversion status");
      }
    }
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Foreign mission → heresy denounce 50/50 (wiki/HandWiki; fandom Missionaries).
   * Seed 1 → roll 0 success (replace mission); seed 5006 → roll 50 fail (burn).
   * Own-mission convert-once still skips above. Cite: indian_contact.md heresy.
   */
  {
    char status_cv[128];
    status_cv[0] = '\0';
    ctx.status = status_cv;
    ctx.status_size = sizeof(status_cv);
    ctx.human_nation = 0;
    ColonizeDosRng heresy_rng;
    dos_rng_seed(&heresy_rng, 1u); /* first roll 0 → success */
    ctx.rng = &heresy_rng;
    col1.tribe[0].mission = 1; /* foreign Euro owns mission */
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    miss->active = true;
    miss->x = 6;
    miss->y = 5;
    miss->nation_id = 0;
    const uint16_t crosses_f = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if ((col1.tribe[0].mission & COL1_TRIBE_MISSION_NATION_MASK) != 0) {
      return fail("heresy success should replace foreign mission with denouncer");
    }
    if (col1.tribe[0].mission & COL1_TRIBE_MISSION_JESUIT_BIT) {
      return fail("heresy success should install regular (non-Jesuit) mission");
    }
    if (col1.nation[0].current_crosses != (uint16_t)(crosses_f + 1)) {
      return fail("heresy success should bump crosses");
    }
    if (strstr(status_cv, "Heresy") == NULL && strstr(status_cv, "foreign") == NULL) {
      fprintf(stderr, "unit_ai_contact: heresy-ok status '%s'\n", status_cv);
      return fail("heresy success should set denounce status");
    }

    /* Foreign owner (human) learns mission burned when AI denounces. */
    {
      char status_own[128];
      status_own[0] = '\0';
      ctx.status = status_own;
      ctx.human_nation = 0;
      dos_rng_seed(&heresy_rng, 1u);
      col1.tribe[0].mission = 0; /* human owns mission */
      /* Move human missionary away so French denouncer is the adjacent actor. */
      miss->x = 9;
      miss->y = 9;
      units.type_count = 4;
      snprintf(units.types[3].name, sizeof(units.types[3].name), "Missionary");
      units.types[3].movement = 1;
      const int fr_m = units_spawn_allow_stack(&units, 3, 6, 5);
      ColonizeUnit* frm = units_get(&units, fr_m);
      if (!frm) {
        return fail("spawn French denouncer missionary");
      }
      frm->nation_id = 1;
      ind->alarm_by_player[1] = 10;
      col1.tribe[0].alarm[1].friction = 10;
      ind->euro_diplo[1] = 1;
      ai_contact_indian_meet_trade(&ctx, 4);
      if ((col1.tribe[0].mission & COL1_TRIBE_MISSION_NATION_MASK) != 1) {
        fprintf(
          stderr,
          "unit_ai_contact: AI heresy mission=%u\n",
          (unsigned)col1.tribe[0].mission
        );
        return fail("AI heresy success should install French mission");
      }
      if (strstr(status_own, "burn your mission") == NULL) {
        fprintf(stderr, "unit_ai_contact: heresy-owner status '%s'\n", status_own);
        return fail("heresy success should notify human foreign mission owner");
      }
      units_despawn(&units, fr_m);
      miss->x = 6;
      miss->y = 5;
      ctx.status = status_cv;
    }

    /* Fail arm: burn denouncer at the stake. */
    dos_rng_seed(&heresy_rng, 5006u); /* first roll 50 → fail */
    col1.tribe[0].mission = 1;
    status_cv[0] = '\0';
    const uint16_t crosses_b = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].mission != 1) {
      return fail("heresy fail should keep foreign mission");
    }
    if (col1.nation[0].current_crosses != crosses_b) {
      return fail("heresy fail should not bump crosses");
    }
    if (miss->active) {
      return fail("heresy fail should despawn denouncer missionary");
    }
    if (strstr(status_cv, "stake") == NULL && strstr(status_cv, "burn") == NULL) {
      fprintf(stderr, "unit_ai_contact: heresy-fail status '%s'\n", status_cv);
      return fail("heresy fail should set burn-at-stake status");
    }
    ctx.rng = NULL;

    /* Respawn missionary for alarmed refuse arm. */
    {
      const int mid2 = units_spawn_allow_stack(&units, 2, 6, 5);
      miss = units_get(&units, mid2);
      if (!miss) {
        return fail("respawn missionary after heresy");
      }
      miss->nation_id = 0;
      miss_id = mid2; /* teach later despawns via miss_id */
    }

    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 60;
    ind->alarm_by_player[0] = 60;
    status_cv[0] = '\0';
    const uint16_t crosses_a = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].mission != 0xff) {
      return fail("alarmed convert refuse should not set mission");
    }
    if (col1.nation[0].current_crosses != crosses_a) {
      return fail("alarmed convert refuse should not bump crosses");
    }
    if (strstr(status_cv, "refuse") == NULL || strstr(status_cv, "conversion") == NULL) {
      fprintf(stderr, "unit_ai_contact: convert-refuse status '%s'\n", status_cv);
      return fail("alarmed convert should set refuse-conversion status");
    }
    /* Restore peaceful band for later tests. */
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Mid-range Jesuit convert (40..54): Jesuit-grade establish + −2 decay.
   * Plain Missionary mid → refuse (PEDIA Jesuit effectiveness; no Brebeuf).
   * Cite: COLONIZE/PEDIA.TXT @JOB24; indian_contact.md convert gate.
   */
  {
    char status_mid[128];
    status_mid[0] = '\0';
    ctx.status = status_mid;
    ctx.status_size = sizeof(status_mid);
    ctx.human_nation = 0;
    /* Plain Missionary mid-alarm → refuse (not Jesuit-grade). */
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
    miss->x = 6;
    miss->y = 5;
    miss->active = true;
    miss->profession = UNITS_JOB_NONE;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 40;
    ind->alarm_by_player[0] = 40;
    col1.nation[0].relation_by_indian[0] = 80;
    const uint16_t crosses_plain = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].mission != 0xff) {
      return fail("plain Missionary mid-alarm should not establish mission");
    }
    if (col1.nation[0].current_crosses != crosses_plain) {
      return fail("plain Missionary mid-alarm should not bump crosses");
    }
    if (strstr(status_mid, "refuse") == NULL || strstr(status_mid, "conversion") == NULL) {
      fprintf(stderr, "unit_ai_contact: mid-plain status '%s'\n", status_mid);
      return fail("plain Missionary mid should set refuse-conversion status");
    }

    /* Jesuit Missionary mid → convert with −2. */
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Jesuit Missionary");
    status_mid[0] = '\0';
    miss->x = 6;
    miss->y = 5;
    miss->active = true;
    col1.tribe[0].mission = 0xff;
    /* Floor of mid band: convert −2 → 38; pacify meet skips (<40). */
    col1.tribe[0].alarm[0].friction = 40;
    ind->alarm_by_player[0] = 40;
    col1.nation[0].relation_by_indian[0] = 80;
    const uint16_t crosses_m = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if ((col1.tribe[0].mission & COL1_TRIBE_MISSION_NATION_MASK) != 0 ||
        (col1.tribe[0].mission & COL1_TRIBE_MISSION_JESUIT_BIT) == 0) {
      return fail("mid-range Jesuit convert should establish mission");
    }
    if (col1.nation[0].current_crosses != (uint16_t)(crosses_m + 1)) {
      return fail("mid-range Jesuit convert should bump crosses");
    }
    if (col1.tribe[0].alarm[0].friction != 38 || ind->alarm_by_player[0] != 38) {
      return fail("mid-range Jesuit convert should decay friction/alarm by 2");
    }
    if (strstr(status_mid, "accept") == NULL) {
      fprintf(stderr, "unit_ai_contact: mid-jesuit status '%s'\n", status_mid);
      return fail("mid-range Jesuit convert should set accept status");
    }
    /* Restore type name for later Missionary flees. */
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Brebeuf unlock: plain Missionary mid-band convert as Jesuit-grade (−2).
   * Cite: docs/fandom_col1994.md Father Jean de Brebeuf — all missionaries
   * function as experts; PEDIA @JOB24; indian_contact.md convert gate.
   * No invent elect crosses — convert +1 only on establish.
   */
  {
    char status_br[128];
    status_br[0] = '\0';
    ctx.status = status_br;
    ctx.status_size = sizeof(status_br);
    ctx.human_nation = 0;
    col1.head.founding_father[FF_JEAN_DE_BREBEUF] = 0;
    col1.nation[0].founding_fathers[FF_JEAN_DE_BREBEUF / 8] |=
      (uint8_t)(1u << (FF_JEAN_DE_BREBEUF % 8));
    if (!founding_fathers_brebeuf_missionaries_are_experts(&col1, 0)) {
      return fail("Brebeuf ownership gate should be true");
    }
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
    miss->x = 6;
    miss->y = 5;
    miss->active = true;
    miss->profession = UNITS_JOB_NONE;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 40;
    ind->alarm_by_player[0] = 40;
    col1.nation[0].relation_by_indian[0] = 80;
    const uint16_t crosses_br = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if ((col1.tribe[0].mission & COL1_TRIBE_MISSION_NATION_MASK) != 0 ||
        (col1.tribe[0].mission & COL1_TRIBE_MISSION_JESUIT_BIT) == 0) {
      return fail("Brebeuf plain Missionary mid should establish mission");
    }
    if (col1.nation[0].current_crosses != (uint16_t)(crosses_br + 1)) {
      return fail("Brebeuf mid convert should bump crosses by 1 only");
    }
    if (col1.tribe[0].alarm[0].friction != 38 || ind->alarm_by_player[0] != 38) {
      return fail("Brebeuf mid convert should decay friction/alarm by 2");
    }
    if (strstr(status_br, "accept") == NULL) {
      fprintf(stderr, "unit_ai_contact: Brebeuf mid status '%s'\n", status_br);
      return fail("Brebeuf mid convert should set accept status");
    }
    /* Clear Brebeuf so later tests stay plain-Missionary gated. */
    col1.head.founding_father[FF_JEAN_DE_BREBEUF] = -1;
    col1.nation[0].founding_fathers[FF_JEAN_DE_BREBEUF / 8] &=
      (uint8_t)~(1u << (FF_JEAN_DE_BREBEUF % 8));
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    ctx.status = NULL;
    ctx.status_size = 0;
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
  col1.nation[0].relation_by_indian[0] = 80;
  ai_contact_indian_meet_trade(&ctx, 4);
  if (!col1.tribe[0].state.learned) {
    return fail("teach-skill should set tribe.state.learned");
  }
  if (euro->profession != COLONIZE_JOB_FUR_TRAPPER) {
    return fail("teach-skill last_sold furs → Expert Fur Trapper");
  }

  /*
   * Already learned (Col1 one-shot): state.learned set → skip teach and do not
   * write teach status (preserves other chrome). Cite: indian_contact.md.
   */
  {
    char status_al[128];
    status_al[0] = '\0';
    ctx.status = status_al;
    ctx.status_size = sizeof(status_al);
    ctx.human_nation = 0;
    euro->x = 6;
    euro->y = 5;
    euro->profession = UNITS_JOB_NONE;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].state.learned = 1;
    col1.tribe[0].last_sold = 0;
    col1.tribe[0].alarm[0].friction = 5;
    ind->alarm_by_player[0] = 5;
    ind->euro_diplo[0] = 1;
    col1.nation[0].gold = 0; /* no gift overwrite */
    col1.nation[0].relation_by_indian[0] = 80;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (euro->profession != UNITS_JOB_NONE) {
      return fail("already-learned should not re-teach profession");
    }
    if (strstr(status_al, "teach") != NULL) {
      fprintf(stderr, "unit_ai_contact: already-learned status '%s'\n", status_al);
      return fail("already-learned should skip teach status");
    }
    ctx.status = NULL;
    ctx.status_size = 0;
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
  col1.nation[0].relation_by_indian[3] = 80; /* Iroquois idx 3 */
  ai_contact_indian_meet_trade(&ctx, 7);
  if (!col1.tribe[0].state.learned) {
    return fail("teach-skill nation map should set tribe.state.learned");
  }
  if (euro->profession != COLONIZE_JOB_FUR_TRAPPER) {
    return fail("teach-skill Iroquois nation → Expert Fur Trapper");
  }

  /*
   * Nation map deepen smoke: Arawak (6) → Fisherman (no cargo id; nation only).
   * Cite: indian_contact.md teach-skill profession map.
   */
  {
    euro->profession = UNITS_JOB_NONE;
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].last_sold = 0;
    col1.tribe[0].nation_id = 6;
    ColonizeCol1Indian* arawak = &col1.indian[2];
    memset(arawak, 0, sizeof(*arawak));
    arawak->alarm_by_player[0] = 5;
    col1.tribe[0].alarm[0].friction = 5;
    col1.nation[0].relation_by_indian[2] = 80; /* Arawak idx 2 */
    ai_contact_indian_meet_trade(&ctx, 6);
    if (!col1.tribe[0].state.learned) {
      return fail("teach-skill Arawak should set tribe.state.learned");
    }
    if (euro->profession != COLONIZE_JOB_FISHERMAN) {
      return fail("teach-skill Arawak nation → Expert Fisherman");
    }
  }

  /*
   * Nation map deepen: Cherokee (8) → Tobacco Planter (unused table entry).
   * Also assert teach success status chrome. Cite: indian_contact.md map.
   */
  {
    char status_tch[128];
    status_tch[0] = '\0';
    ctx.status = status_tch;
    ctx.status_size = sizeof(status_tch);
    ctx.human_nation = 0;
    euro->profession = UNITS_JOB_NONE;
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].last_sold = 0;
    col1.tribe[0].nation_id = 8;
    ColonizeCol1Indian* cherokee = &col1.indian[4];
    memset(cherokee, 0, sizeof(*cherokee));
    cherokee->alarm_by_player[0] = 5;
    col1.tribe[0].alarm[0].friction = 5;
    col1.nation[0].relation_by_indian[4] = 80; /* Cherokee idx 4 */
    col1.nation[0].gold = 0; /* no gift overwrite of teach status */
    ai_contact_indian_meet_trade(&ctx, 8);
    if (!col1.tribe[0].state.learned) {
      return fail("teach-skill Cherokee should set tribe.state.learned");
    }
    if (euro->profession != COLONIZE_JOB_TOBACCO_PLANTER) {
      return fail("teach-skill Cherokee nation → Expert Tobacco Planter");
    }
    if (strstr(status_tch, "teach") == NULL || strstr(status_tch, "Cherokee") == NULL) {
      fprintf(stderr, "unit_ai_contact: teach-ok status '%s'\n", status_tch);
      return fail("teach success should set Natives-teach-Cherokee status");
    }
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Skill-map deepen: Apache (9) → Cotton Planter; Scout → Seasoned Scout.
   * Cite: indian_contact.md teach-skill profession map; FUN_5bfb_022e.
   */
  {
    euro->profession = UNITS_JOB_NONE;
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].last_sold = 0;
    col1.tribe[0].nation_id = 9;
    ColonizeCol1Indian* apache = &col1.indian[5];
    memset(apache, 0, sizeof(*apache));
    apache->alarm_by_player[0] = 5;
    col1.tribe[0].alarm[0].friction = 5;
    col1.nation[0].relation_by_indian[5] = 80; /* Apache idx 5 */
    ai_contact_indian_meet_trade(&ctx, 9);
    if (!col1.tribe[0].state.learned) {
      return fail("teach-skill Apache should set tribe.state.learned");
    }
    if (euro->profession != COLONIZE_JOB_COTTON_PLANTER) {
      return fail("teach-skill Apache nation → Expert Cotton Planter");
    }

    units.type_count = 4;
    snprintf(units.types[3].name, sizeof(units.types[3].name), "Scout");
    units.types[3].movement = 4;
    units.types[3].attack = 0;
    units.types[3].defense = 1;
    const int scout_teach_id = units_spawn_allow_stack(&units, 3, 6, 5);
    ColonizeUnit* scout_t = units_get(&units, scout_teach_id);
    if (!scout_t) {
      return fail("spawn Scout for teach");
    }
    scout_t->nation_id = 0;
    scout_t->profession = UNITS_JOB_NONE;
    scout_t->horses = 50; /* display name → Scout / Seasoned after teach */
    euro->x = 12;
    euro->y = 12; /* clear Free Colonist from tribe adjacency */
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].last_sold = 0;
    col1.tribe[0].nation_id = 4;
    ind->alarm_by_player[0] = 5;
    col1.tribe[0].alarm[0].friction = 5;
    col1.nation[0].relation_by_indian[0] = 80;
    char status_sc[128];
    status_sc[0] = '\0';
    ctx.status = status_sc;
    ctx.status_size = sizeof(status_sc);
    ctx.human_nation = 0;
    col1.nation[0].gold = 0;
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");
    ctx.messages = &game_txt;
    AiPopupState pops;
    ai_popup_init(&pops);
    ctx.ai_popups = &pops;
    ai_contact_indian_meet_trade(&ctx, 4);
    scout_t = units_get(&units, scout_teach_id);
    if (!col1.tribe[0].state.learned) {
      assets_msg_free(&game_txt);
      return fail("teach-skill Scout should set tribe.state.learned");
    }
    if (!scout_t || scout_t->profession != UNITS_JOB_SCOUT) {
      assets_msg_free(&game_txt);
      return fail("teach-skill Scout → Seasoned Scout profession");
    }
    if (strstr(status_sc, "Seasoned") == NULL && strstr(status_sc, "Scouts") == NULL) {
      fprintf(stderr, "unit_ai_contact: scout-teach status '%s'\n", status_sc);
      assets_msg_free(&game_txt);
      return fail("Scout teach should set WELLSEASONED status");
    }
    if (pops.queue_count < 1 ||
        (strstr(pops.queue[0].body, "Seasoned") == NULL &&
         strstr(pops.queue[0].body, "Scouts") == NULL)) {
      fprintf(
        stderr,
        "unit_ai_contact: WELLSEASONED popup weak q=%d body='%s'\n",
        pops.queue_count,
        pops.queue_count > 0 ? pops.queue[0].body : ""
      );
      assets_msg_free(&game_txt);
      return fail("Scout teach should enqueue WELLSEASONED popup");
    }
    ctx.messages = NULL;
    ctx.ai_popups = NULL;
    assets_msg_free(&game_txt);
    units_despawn(&units, scout_teach_id);
    euro->x = 6;
    euro->y = 5;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Mid-alarm teach refuse (40..54): Free Colonist at tribe → no learned;
   * status @LEARNMAD ("ill manners infuriate us … learn anything from us").
   * Cite: indian_contact.md mid refuse; GAME.TXT @LEARNMAD.
   */
  {
    char status_mt[128];
    status_mt[0] = '\0';
    ctx.status = status_mt;
    ctx.status_size = sizeof(status_mt);
    ctx.human_nation = 0;
    euro->x = 6;
    euro->y = 5;
    euro->active = true;
    euro->profession = UNITS_JOB_NONE;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 45;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 20; /* mid via tribe friction */
    col1.nation[0].gold = 0;
    col1.nation[0].relation_by_indian[0] = 80;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].state.learned) {
      return fail("mid-alarm teach refuse should not set learned");
    }
    if (strstr(status_mt, "infuriate") == NULL || strstr(status_mt, "learn") == NULL) {
      fprintf(stderr, "unit_ai_contact: mid-teach status '%s'\n", status_mt);
      return fail("mid-alarm teach should set @LEARNMAD refuse status");
    }
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * @LEARNMASTER: an already-expert colonist (peaceful band, would otherwise
   * teach) is refused without consuming the village's one-shot teach — a
   * later unskilled colonist may still learn. Cite: GAME.TXT @LEARNMASTER.
   */
  {
    char status_lm[128];
    status_lm[0] = '\0';
    ctx.status = status_lm;
    ctx.status_size = sizeof(status_lm);
    ctx.human_nation = 0;
    euro->x = 6;
    euro->y = 5;
    euro->active = true;
    euro->profession = COLONIZE_JOB_FARMER; /* already a master */
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 5;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 5;
    col1.nation[0].gold = 0;
    col1.nation[0].relation_by_indian[0] = 80;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].state.learned) {
      return fail("LEARNMASTER refuse should not consume the village one-shot");
    }
    if (euro->profession != COLONIZE_JOB_FARMER) {
      return fail("LEARNMASTER refuse should not touch the learner's profession");
    }
    if (strstr(status_lm, "teach new skills") == NULL) {
      fprintf(stderr, "unit_ai_contact: LEARNMASTER status '%s'\n", status_lm);
      return fail("already-expert learner should set @LEARNMASTER refuse status");
    }
    euro->profession = UNITS_JOB_NONE;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Auto gift (2154 tables + 5bfb gates): gold≥20 Large; Generous needs
   * ask[0]-bid[0]≥1, gold≥0x4b, and delta≥RNG. Cite: indian_meet_scoring_2154.md.
   */
  col1.tribe[0].nation_id = 4;
  col1.tribe[0].population = 4;
  ind->euro_diplo[0] = 1;
  ind->alarm_by_player[0] = 10;
  ind->tech = 0;
  col1.tribe[0].alarm[0].friction = 10;
  col1.tribe[0].state.learned = 1;
  col1.tribe[0].state.capital = 0;
  col1.nation[0].gold = 50; /* ≥20 Large; <0x4b no Generous */
  col1.nation[0].relation_by_indian[0] = 80;
  euro->x = 6;
  euro->y = 5;
  brave->x = 5;
  brave->y = 5;
  brave->nation_id = 4;
  ctx.human_nation = 1; /* Euro 0 as AI — silent gift stand-in */
  char status[128];
  status[0] = '\0';
  ctx.status = status;
  ctx.status_size = sizeof(status);
  ai_contact_indian_meet_trade(&ctx, 4);
  if (col1.nation[0].gold != 40u) {
    fprintf(stderr, "unit_ai_contact: gift gold50→%u (want Large 40)\n",
            (unsigned)col1.nation[0].gold);
    return fail("AI gift gold 20..74 should cost Euro 10 gold (Large)");
  }
  if (col1.tribe[0].alarm[0].friction != 8) {
    return fail("Large AI gift should reduce tribe friction by 2");
  }
  if (status[0] != '\0') {
    return fail("AI gift stand-in should not set human chrome status");
  }
  /* Mid purse Large (−10/−2). */
  col1.nation[0].gold = 25;
  ind->alarm_by_player[0] = 10;
  col1.tribe[0].alarm[0].friction = 10;
  ai_contact_indian_meet_trade(&ctx, 4);
  if (col1.nation[0].gold != 15u) {
    return fail("AI gift gold 20..39 should cost Euro 10 gold (Large)");
  }
  if (col1.tribe[0].alarm[0].friction != 8) {
    return fail("Large AI gift should reduce tribe friction by 2");
  }
  /*
   * 2154 Generous: sparse neighborhood (arctic pad → high ask−bid) + gold≥75.
   * Dense forest raises bid and can suppress delta. Cite: FUN_4d56_2154.
   */
  {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      colonies.colonies[ci].active = false;
    }
    colonies.colony_count = 0;
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        const int nx = 5 + dx;
        const int ny = 5 + dy;
        if (nx >= 0 && ny >= 0 && nx < 16 && ny < 16) {
          map.terrain[ny * 16 + nx] = 24; /* arctic unscored */
        }
      }
    }
    /* Capital doubles ask after clamp → delta≥100 beats RNG 1..100. */
    col1.tribe[0].state.capital = 1;
    col1.nation[0].gold = 80;
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    ind->euro_diplo[0] = 1;
    ctx.human_nation = 1;
    ctx.rng_seed = 1;
    euro->x = 6;
    euro->y = 5;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 60u) {
      fprintf(stderr, "unit_ai_contact: sparse gift gold=%u (want 60 Generous)\n",
              (unsigned)col1.nation[0].gold);
      return fail("sparse capital + gold≥75 should Generous (−20)");
    }
    if (col1.tribe[0].alarm[0].friction != 7) {
      return fail("Generous AI gift should reduce tribe friction by 3");
    }
    /* Gold 40 alone must not Generous (needs ≥0x4b). */
    col1.tribe[0].state.capital = 1;
    col1.nation[0].gold = 40;
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 30u) {
      fprintf(stderr, "unit_ai_contact: gold40 gift=%u (want Large 30)\n",
              (unsigned)col1.nation[0].gold);
      return fail("gold <0x4b must Large even with positive delta");
    }
    /*
     * Dense forest + high tons[0] → ask crushed, delta≤0 → Large at gold 80.
     */
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        const int nx = 5 + dx;
        const int ny = 5 + dy;
        if (nx >= 0 && ny >= 0 && nx < 16 && ny < 16) {
          map.terrain[ny * 16 + nx] = 8; /* forest class */
        }
      }
    }
    ind->tons[0] = 2500; /* phase-5 tons mix suppresses ask[0] */
    col1.tribe[0].state.capital = 0;
    col1.nation[0].gold = 80;
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 70u) {
      fprintf(stderr, "unit_ai_contact: forest gift gold=%u (want Large 70)\n",
              (unsigned)col1.nation[0].gold);
      return fail("dense forest + tons should suppress Generous (Large −10)");
    }
    ind->tons[0] = 0;
    /*
     * Cover clears forest buckets + capital → Generous at gold 80.
     * 281f_0ce0 work-slot gate: only the colony's own tile plus *actively
     * worked* immediate (N/NE/E/SE/S/SW/W/NW) tiles are "covered" — the
     * outer distance-2 ring is never worker-assignable (col1_bridge.c
     * tiles[8..19] stay -1) and so is never covered. Shrink the forest
     * patch to the 8 immediate ring cells (the second ring reverts to
     * plains) and assign a worker to each so the whole patch is actually
     * covered, matching the real DOS gate instead of the old "full ring"
     * stand-in. Cite: indian_meet_scoring_2154.md Phase 1.
     */
    {
      for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
          if (abs(dx) == 2 || abs(dy) == 2) {
            const int nx = 5 + dx;
            const int ny = 5 + dy;
            if (nx >= 0 && ny >= 0 && nx < 16 && ny < 16) {
              map.terrain[ny * 16 + nx] = 1; /* plains: outside worked ring */
            }
          }
        }
      }
      ColonizeColony* cov = &colonies.colonies[0];
      cov->id = 0;
      cov->active = true;
      cov->nation_id = 0;
      cov->x = 5;
      cov->y = 5;
      cov->population = 2;
      cov->colonist_count = 2;
      for (int dir = 0; dir < 8; ++dir) {
        cov->tiles[dir] = 0; /* every immediate ring tile worked */
      }
      colonies.colony_count = 1;
      col1.tribe[0].state.capital = 1;
      col1.nation[0].gold = 80;
      ind->alarm_by_player[0] = 10;
      col1.tribe[0].alarm[0].friction = 10;
      ai_contact_indian_meet_trade(&ctx, 4);
      if (col1.nation[0].gold != 60u) {
        fprintf(stderr, "unit_ai_contact: cover gift gold=%u (want Generous 60)\n",
                (unsigned)col1.nation[0].gold);
        return fail("colony cover + capital should Generous");
      }
      cov->active = false;
      colonies.colony_count = 0;
      col1.tribe[0].state.capital = 0;
    }
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
  }

  /*
   * Capital mix doubles ask[0..7] → Generous at gold 80 where non-capital
   * + tons crush stays Large. Cite: 2154 capital phase-5.
   */
  {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      colonies.colonies[ci].active = false;
    }
    colonies.colony_count = 0;
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        const int nx = 5 + dx;
        const int ny = 5 + dy;
        if (nx >= 0 && ny >= 0 && nx < 16 && ny < 16) {
          map.terrain[ny * 16 + nx] = 24; /* arctic sparse */
        }
      }
    }
    col1.tribe[0].state.capital = 0;
    ind->tons[0] = 2500;
    col1.nation[0].gold = 80;
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    ind->euro_diplo[0] = 1;
    ctx.human_nation = 1;
    ctx.rng_seed = 1;
    euro->x = 6;
    euro->y = 5;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 70u) {
      fprintf(stderr, "unit_ai_contact: non-capital sparse+tons gift=%u (want Large 70)\n",
              (unsigned)col1.nation[0].gold);
      return fail("non-capital + tons should Large at gold 80");
    }
    col1.tribe[0].state.capital = 1;
    ind->tons[0] = 0;
    col1.nation[0].gold = 80;
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 60u) {
      fprintf(stderr, "unit_ai_contact: capital sparse gift=%u (want Generous 60)\n",
              (unsigned)col1.nation[0].gold);
      return fail("capital mix should Generous at gold 80 on sparse land");
    }
    col1.tribe[0].state.capital = 0;
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
  }

  /*
   * Gift refuse when Euro gold < 10: no gold change. Human adjacency: no
   * spontaneous chrome (village dialog PARKED).
   */
  {
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    col1.tribe[0].state.learned = 1;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].gold = 5; /* < 10 */
    col1.nation[0].relation_by_indian[0] = 80;
    euro->x = 6;
    euro->y = 5;
    brave->x = 5;
    brave->y = 5;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 5u) {
      return fail("gift gold<10 refuse should not drain gold");
    }
    if (status[0] != '\0') {
      return fail("human Brave adjacency must not chrome gift refuse");
    }
  }

  /*
   * First-meet status (gold in 10..19 → gift skips silent; learned → teach skips):
   * FUN_5bfb_022e WELCOME → "The … offer peace."
   */
  ind->euro_diplo[0] = 0;
  ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
  col1.nation[0].gold = 15; /* ≥10 no refuse; <20 no gift drain */
  col1.nation[0].relation_by_indian[0] = 0;
  status[0] = '\0';
  ai_contact_indian_meet_trade(&ctx, 4);
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
    /*
     * Secondary −5 muskets; STORES primary takes half stock clamp 1..10
     * (FUN_5fef_0f14). Cite: indian_raid_loot.md; indian_raid_outcomes.md.
     */
    int musk_expect = muskets_ml - 5;
    if (kind_ml == AI_RAID_STORES) {
      int half = muskets_ml >> 1;
      if (half > 10) {
        half = 10;
      }
      if (half < 1) {
        half = 1;
      }
      musk_expect = muskets_ml - half - 5;
    }
    if (c->stock[COLONIZE_CARGO_MUSKETS] != musk_expect) {
      return fail("multi-loot should steal muskets stock (secondary ± STORES)");
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
    brave->moves_left = 3;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 50;
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].gold = 0; /* no GOLD arm */
    col1.nation[0].relation_by_indian[0] = 40;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->population = 1; /* no SCALP fallback */
    c->colonist_count = 1;
    c->building_in_production = -1;
    snprintf(c->name, sizeof(c->name), "Roanoke");
    memset(c->stock, 0, sizeof(c->stock));
    c->stock[COLONIZE_CARGO_MUSKETS] = 3; /* STORES prefs hit muskets; <5 → no −5 secondary */
    const int musk_st = c->stock[COLONIZE_CARGO_MUSKETS];
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    ai_contact_indian_raids(&ctx, 4);
    if (ai_contact_last_raid_kind() != AI_RAID_STORES) {
      return fail("muskets-only warehouse should pick AI_RAID_STORES");
    }
    if (c->stock[COLONIZE_CARGO_MUSKETS] != musk_st - 1) {
      return fail("STORES primary should drain 1 muskets stock");
    }
    if (strstr(status, "stores") == NULL || strstr(status, "Roanoke") == NULL) {
      fprintf(stderr, "unit_ai_contact: STORES status '%s'\n", status);
      return fail("STORES raid should set @RAIDSTORES-shaped status");
    }
  }

  /*
   * STORES goods-value pick (FUN_5fef_016c stand-in): food+silver warehouse
   * at mid alarm → drain silver (higher value), not food. Cite:
   * indian_raid_outcomes.md @RAIDSTORES; peel FUN_5fef_016c.
   */
  {
    euro->x = 10;
    euro->y = 10;
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 50;
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].gold = 0;
    col1.nation[0].relation_by_indian[0] = 40;
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
    ai_contact_indian_raids(&ctx, 4);
    if (ai_contact_last_raid_kind() != AI_RAID_STORES) {
      return fail("food+silver warehouse should pick AI_RAID_STORES");
    }
    if (c->stock[COLONIZE_CARGO_SILVER] != sil_vs - 1) {
      return fail("STORES value-sort should drain silver before food");
    }
    if (c->stock[COLONIZE_CARGO_FOOD] != food_vs) {
      return fail("STORES value-sort should leave food when silver present");
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
  const int scout_spawn = units_spawn_allow_stack(&units, 3, 6, 5);
  int scout_id = scout_spawn;
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
  if (strstr(status, "Scout warned") == NULL && strstr(status, "warn your Scout") == NULL &&
      strstr(status, "village") == NULL) {
    return fail("359c displace should set status warn line");
  }

  /*
   * Thin 359c RNG kill-with-flee (alarm ≥95): even with free land, ~1/4 kill.
   * Sweep turn seeds until kill fires (deterministic local RNG). Cite:
   * indian_raid_outcomes.md §9; FUN_4d56_359c.
   */
  {
    int killed = 0;
    for (uint32_t tseed = 1; tseed < 80 && !killed; ++tseed) {
      turn = tseed;
      /* Respawn scout next to brave with free land around. */
      for (int i = 0; i < 256; ++i) {
        map.terrain[i] = 1;
      }
      scout = units_get(&units, scout_id);
      if (!scout || !scout->active) {
        const int sid = units_spawn_allow_stack(&units, 3, 6, 5);
        scout = units_get(&units, sid);
        if (!scout) {
          return fail("359c RNG kill respawn");
        }
      }
      scout->active = true;
      scout->x = 6;
      scout->y = 5;
      scout->nation_id = 0;
      scout->horses = 50;
      brave->x = 5;
      brave->y = 5;
      brave->moves_left = 0;
      euro->x = 10;
      euro->y = 10;
      ind->alarm_by_player[0] = 95;
      col1.tribe[0].alarm[0].friction = 95;
      status[0] = '\0';
      const int sid = scout->id;
      ai_contact_indian_raids(&ctx, 4);
      scout = units_get(&units, sid);
      if (!scout || !scout->active) {
        if (strstr(status, "kill") == NULL) {
          return fail("359c RNG kill should set kill status");
        }
        killed = 1;
      }
    }
    if (!killed) {
      return fail("359c RNG kill-with-flee should fire for some turn seed");
    }
    /* Restore alarm band used by later blocked-despawn arm. */
    ind->alarm_by_player[0] = 90;
    col1.tribe[0].alarm[0].friction = 90;
  }

  /*
   * Thin alarmed refuse-talk: human Brave adjacency must not spam refuse chrome
   * (village dialog PARKED). No gold drain either.
   */
  {
    col1.nation[0].gold = 100;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 60;
    col1.tribe[0].alarm[0].friction = 60;
    col1.tribe[0].state.learned = 1; /* skip teach overwrite */
    col1.tribe[0].mission = 0xff;
    euro->x = 6;
    euro->y = 5;
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 1;
    status[0] = '\0';
    ctx.human_nation = 0;
    const uint32_t gold0 = col1.nation[0].gold;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != gold0) {
      return fail("alarmed refuse-talk should not gift/trade gold");
    }
    if (strstr(status, "refuse") != NULL) {
      return fail("human Brave adjacency must not chrome refuse-talk");
    }
  }

  /*
   * Alarmed gift refuse (≥55): human adjacency — no gold drain, no chrome.
   */
  {
    col1.nation[0].gold = 100;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 60;
    col1.tribe[0].alarm[0].friction = 10; /* gift-band */
    col1.tribe[0].state.learned = 1;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].relation_by_indian[0] = 80;
    euro->x = 6;
    euro->y = 5;
    brave->x = 5;
    brave->y = 5;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 100u) {
      return fail("alarmed gift refuse should not drain gold");
    }
    if (status[0] != '\0') {
      return fail("human Brave adjacency must not chrome gift refuse");
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
  scout = units_get(&units, scout_id);
  if (!scout || !scout->active) {
    scout_id = units_spawn_allow_stack(&units, 3, 6, 5);
    scout = units_get(&units, scout_id);
    if (!scout) {
      return fail("359c blocked respawn");
    }
    scout->nation_id = 0;
    scout->horses = 50;
  }
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

  /*
   * Alarmed teach refuse (≥55 refuse-talk gate): Free Colonist at tribe + high
   * alarm → no state.learned; human status @LEARNMAD.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 6;
    euro->y = 5;
    euro->active = true;
    euro->profession = UNITS_JOB_NONE;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 60;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 60;
    status[0] = '\0';
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].state.learned) {
      return fail("alarmed teach refuse should not set tribe.state.learned");
    }
    if (strstr(status, "infuriate") == NULL || strstr(status, "learn") == NULL) {
      fprintf(stderr, "unit_ai_contact: teach-refuse status '%s'\n", status);
      return fail("alarmed teach should set @LEARNMAD refuse status");
    }
  }

  /*
   * Mission pacify deepen (meet pulse): mission owner + mid friction (40..80)
   * → −2 tribe friction / alarm (prelude low-band −1 unchanged).
   */
  {
    col1.tribe[0].mission = 0;
    col1.tribe[0].alarm[0].friction = 50;
    ind->alarm_by_player[0] = 50;
    col1.tribe[0].state.learned = 1; /* skip teach status overwrite */
    euro->x = 10;
    euro->y = 10; /* no adjacent meet/gift side effects */
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].alarm[0].friction != 48) {
      return fail("meet mission pacify should decay mid friction by 2");
    }
    if (ind->alarm_by_player[0] != 48) {
      return fail("meet mission pacify should decay mid alarm_by_player by 2");
    }
  }

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
    brave->moves_left = 3;
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
    ai_contact_indian_raids(&ctx, 4);
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
   * @INDIANSURPRISE / @INDIANWAR thin: successful loot while not at war →
   * surprise status; with peace bit + high friction → clear peace + war status.
   * Cite: GAME.TXT @INDIANSURPRISE / @INDIANWAR; indian_raid_outcomes.md.
   */
  {
    char st_sur[128];
    st_sur[0] = '\0';
    ctx.status = st_sur;
    ctx.status_size = sizeof(st_sur);
    ctx.human_nation = 0;
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    col1.nation[0].relation_by_indian[0] = 80; /* not at-war */
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
    ai_contact_indian_raids(&ctx, 4);
    if (ai_contact_last_raid_kind() == AI_RAID_NOTHING) {
      /* retry with more food/alarm already set — accept if attacks recorded */
      if (col1.tribe[0].alarm[0].attacks == 0) {
        return fail("surprise raid should loot or record attacks");
      }
    } else if ((strstr(st_sur, "surprise") == NULL && strstr(st_sur, "denies") == NULL) ||
               strstr(st_sur, "Roanoke") == NULL) {
      fprintf(stderr, "unit_ai_contact: surprise status '%s'\n", st_sur);
      return fail("non-war successful raid should set @INDIANSURPRISE near colony");
    }

    st_sur[0] = '\0';
    brave->moves_left = 3;
    brave->x = 5;
    brave->y = 5;
    ind->euro_diplo[0] |= COL1_INDIAN_PEACE_BIT;
    col1.nation[0].relation_by_indian[0] = 80;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    c->stock[COLONIZE_CARGO_FOOD] = 20;
    ai_contact_indian_raids(&ctx, 4);
    if (ai_contact_indian_has_peace(&col1, 4, 0)) {
      return fail("high-friction raid should clear peace bit (@INDIANWAR)");
    }
    if (ai_contact_last_raid_kind() != AI_RAID_NOTHING &&
        strstr(st_sur, "WAR") == NULL && strstr(st_sur, "war") == NULL) {
      fprintf(stderr, "unit_ai_contact: war-raid status '%s'\n", st_sur);
      return fail("peace-breaking raid should set @INDIANWAR status");
    }
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Alarmed demand refuse: human Brave adjacency — no tools/gold taken, no chrome.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 6;
    euro->y = 5;
    euro->active = true;
    euro->tools = 20;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].state.learned = 1;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 60;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 50;
    col1.nation[0].gold = 100;
    col1.nation[0].relation_by_indian[0] = 80;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->stock[COLONIZE_CARGO_TOOLS] = 30;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    const uint32_t gold_d = col1.nation[0].gold;
    const int tools_c = c->stock[COLONIZE_CARGO_TOOLS];
    const int tools_u = euro->tools;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != gold_d) {
      return fail("alarmed demand refuse should not take gold");
    }
    if (c->stock[COLONIZE_CARGO_TOOLS] != tools_c || euro->tools != tools_u) {
      return fail("alarmed demand refuse should not take tools");
    }
    if (status[0] != '\0') {
      return fail("human Brave adjacency must not chrome demand refuse");
    }
  }

  /*
   * Demand succeed (AI Euro silent): mid friction, colony tools ≥20 → −10 tools,
   * friction −3. Cite: indian_contact.md mid demand.
   */
  {
    euro->x = 6;
    euro->y = 5;
    euro->active = true;
    euro->tools = 5;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].state.learned = 1;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 45;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 20;
    col1.nation[0].gold = 5;
    col1.nation[0].relation_by_indian[0] = 80;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->stock[COLONIZE_CARGO_TOOLS] = 25;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 1; /* AI Euro 0 */
    const int tools_ok = c->stock[COLONIZE_CARGO_TOOLS];
    const uint8_t fr_ok = col1.tribe[0].alarm[0].friction;
    const uint16_t al_ok = ind->alarm_by_player[0];
    ai_contact_indian_meet_trade(&ctx, 4);
    if (c->stock[COLONIZE_CARGO_TOOLS] != tools_ok - 10) {
      return fail("demand succeed should take 10 tools from stock ≥20");
    }
    if (col1.tribe[0].alarm[0].friction != (uint8_t)(fr_ok - 3) ||
        ind->alarm_by_player[0] != (uint16_t)(al_ok - 3)) {
      return fail("demand succeed should decay friction by 3");
    }
    /* Stock <20 + gold <50 → no drain (AI silent, no refuse chrome). */
    c->stock[COLONIZE_CARGO_TOOLS] = 15;
    col1.tribe[0].alarm[0].friction = 45;
    ind->alarm_by_player[0] = 20;
    col1.nation[0].gold = 5;
    status[0] = '\0';
    const int tools_short = c->stock[COLONIZE_CARGO_TOOLS];
    ai_contact_indian_meet_trade(&ctx, 4);
    if (c->stock[COLONIZE_CARGO_TOOLS] != tools_short) {
      return fail("demand must not take tools when warehouse stock < 20");
    }
    if (col1.nation[0].gold != 5u) {
      return fail("demand gold stand-in needs treasury >= 50 when tools short");
    }
  }

  /*
   * Demand tools from Wagon Train hold (@INDIANWAGONS thin): colony tools short,
   * wagon TOOLS ≥20 within reach → −10 hold. Cite: GAME.TXT @INDIANWAGONS.
   */
  {
    if (units.type_count < 4) {
      units.type_count = 4;
    }
    snprintf(units.types[3].name, sizeof(units.types[3].name), "Wagon Train");
    units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
    units.types[3].movement = 3;
    units.types[3].cargo = 4;
    const int wag_id = units_spawn_allow_stack(&units, 3, 6, 6);
    ColonizeUnit* wag = units_get(&units, wag_id);
    if (!wag) {
      return fail("demand-wagon spawn");
    }
    wag->nation_id = 0;
    wag->hold_goods_type[0] = COLONIZE_CARGO_TOOLS;
    wag->hold_goods_amount[0] = 25;
    euro->x = 6;
    euro->y = 5;
    euro->tools = 5;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    col1.tribe[0].alarm[0].friction = 45;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 20;
    col1.nation[0].gold = 5;
    c->stock[COLONIZE_CARGO_TOOLS] = 5; /* warehouse short */
    ctx.human_nation = 1;
    status[0] = '\0';
    ai_contact_indian_meet_trade(&ctx, 4);
    if (wag->hold_goods_amount[0] != 15) {
      return fail("demand should take 10 tools from wagon hold when colony short");
    }
    if (c->stock[COLONIZE_CARGO_TOOLS] != 5) {
      return fail("demand wagon path should not touch colony tools");
    }
    units_despawn(&units, wag_id);
  }

  /*
   * Demand gold path (AI Euro silent): mid friction, tools short, gold ≥50
   * → −15 gold, friction −3.
   */
  {
    euro->x = 6;
    euro->y = 5;
    euro->active = true;
    euro->tools = 5;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].state.learned = 1;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 45;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 20;
    col1.nation[0].gold = 50;
    col1.nation[0].relation_by_indian[0] = 80;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->stock[COLONIZE_CARGO_TOOLS] = 10;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 1;
    const uint8_t fr_g = col1.tribe[0].alarm[0].friction;
    const uint16_t al_g = ind->alarm_by_player[0];
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 35u) {
      return fail("demand gold>=50 stand-in should take 15 gold");
    }
    if (c->stock[COLONIZE_CARGO_TOOLS] != 10 || euro->tools != 5) {
      return fail("demand gold path should not touch tools when stock < 20");
    }
    if (col1.tribe[0].alarm[0].friction != (uint8_t)(fr_g - 3) ||
        ind->alarm_by_player[0] != (uint16_t)(al_g - 3)) {
      return fail("demand gold path should decay friction by 3");
    }
  }

  /*
   * 2154 demand preference: ask[0] < bid[0] → gold-first when both payable.
   * High tons[0] crushes ask; forest raises bid. Cite: LAB_5bfb_096c.
   */
  {
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        const int nx = 5 + dx;
        const int ny = 5 + dy;
        if (nx >= 0 && ny >= 0 && nx < 16 && ny < 16) {
          map.terrain[ny * 16 + nx] = 8;
        }
      }
    }
    ind->tons[0] = 2500;
    col1.tribe[0].state.capital = 0;
    euro->x = 6;
    euro->y = 5;
    euro->active = true;
    euro->tools = 5;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    col1.tribe[0].alarm[0].friction = 45;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 20;
    col1.nation[0].gold = 60;
    c->active = true;
    c->nation_id = 0;
    c->x = 5;
    c->y = 5;
    c->stock[COLONIZE_CARGO_TOOLS] = 40; /* tools also payable */
    ctx.human_nation = 1;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != 45u) {
      fprintf(stderr, "unit_ai_contact: ask<bid demand gold=%u (want 45)\n",
              (unsigned)col1.nation[0].gold);
      return fail("ask[0]<bid[0] should prefer gold demand when both payable");
    }
    if (c->stock[COLONIZE_CARGO_TOOLS] != 40) {
      return fail("gold-first demand must not take tools when gold pays");
    }
    ind->tons[0] = 0;
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
  }

  /*
   * Very-low relation: human Brave adjacency — no gift, no refuse chrome.
   */
  {
    euro->x = 6;
    euro->y = 5;
    brave->x = 5;
    brave->y = 5;
    brave->nation_id = 4;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].state.learned = 1;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 10;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 10;
    col1.nation[0].relation_by_indian[0] = 30;
    col1.nation[0].gold = 100;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    const uint32_t gold_r = col1.nation[0].gold;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.nation[0].gold != gold_r) {
      return fail("very-low relation refuse should not gift gold");
    }
    if (status[0] != '\0') {
      return fail("human Brave adjacency must not chrome very-low refuse");
    }
  }

  /*
   * Missionary flee: adjacent to alarmed tribe (≥55), not converting →
   * move 1 free land tile away + AI_MOVE. Cite: fandom Alarm.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    units.type_count = 3;
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
    units.types[2].movement = 1;
    units.types[2].attack = 0;
    units.types[2].defense = 1;
    const int flee_id = units_spawn_allow_stack(&units, 2, 6, 5);
    ColonizeUnit* flee_m = units_get(&units, flee_id);
    if (!flee_m) {
      return fail("spawn missionary for flee");
    }
    flee_m->nation_id = 0;
    flee_m->orders = 0;
    euro->x = 10;
    euro->y = 10;
    brave->x = 5;
    brave->y = 5;
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0; /* own mission — flee chrome (not convert refuse) */
    col1.tribe[0].alarm[0].friction = 60;
    ind->alarm_by_player[0] = 60;
    col1.nation[0].relation_by_indian[0] = 80;
    const int mx0 = flee_m->x;
    const int my0 = flee_m->y;
    char status_flee[128];
    status_flee[0] = '\0';
    ctx.status = status_flee;
    ctx.status_size = sizeof(status_flee);
    ctx.human_nation = 0;
    ai_contact_indian_meet_trade(&ctx, 4);
    flee_m = units_get(&units, flee_id);
    if (!flee_m || !flee_m->active) {
      return fail("missionary flee should keep unit active");
    }
    if (flee_m->x == mx0 && flee_m->y == my0) {
      return fail("missionary flee should move 1 tile away from alarmed tribe");
    }
    if (flee_m->orders != UNITS_ORDER_AI_MOVE) {
      return fail("missionary flee should set AI_MOVE orders");
    }
    if (strstr(status_flee, "flees") == NULL) {
      fprintf(stderr, "unit_ai_contact: flee status '%s'\n", status_flee);
      return fail("missionary flee should set flee status");
    }
    /* Chebyshev distance from tribe should be > 1 (was adjacent). */
    {
      const int dx = flee_m->x - 5;
      const int dy = flee_m->y - 5;
      const int adx = dx < 0 ? -dx : dx;
      const int ady = dy < 0 ? -dy : dy;
      const int cheb = adx > ady ? adx : ady;
      if (cheb < 2) {
        return fail("missionary flee should increase distance from tribe");
      }
    }
    units_despawn(&units, flee_id);
  }

  /*
   * Raid prefer high-friction Euro among candidates: equal alarm band, lower
   * relation wins target (colony of Euro 1 looted, Euro 0 untouched).
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    brave->x = 8;
    brave->y = 8;
    brave->moves_left = 3;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 50;
    ind->alarm_by_player[1] = 50; /* equal friction band */
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].alarm[1].friction = 50;
    col1.nation[0].relation_by_indian[0] = 90; /* less hostile */
    col1.nation[1].relation_by_indian[0] = 20; /* prefer this target */
    ColonizeColony* c0 = &colonies.colonies[0];
    ColonizeColony* c1 = &colonies.colonies[1];
    c0->id = 0;
    c0->active = true;
    c0->nation_id = 0;
    c0->x = 8;
    c0->y = 8; /* same tile as brave — would loot if targeted */
    c0->population = 3;
    c0->colonist_count = 3;
    c0->building_in_production = -1;
    memset(c0->stock, 0, sizeof(c0->stock));
    c0->stock[COLONIZE_CARGO_FOOD] = 20;
    c1->id = 1;
    c1->active = true;
    c1->nation_id = 1;
    c1->x = 8;
    c1->y = 8; /* co-located; only target_euro's colony is considered */
    c1->population = 3;
    c1->colonist_count = 3;
    c1->building_in_production = -1;
    memset(c1->stock, 0, sizeof(c1->stock));
    c1->stock[COLONIZE_CARGO_FOOD] = 20;
    colonies.colony_count = 2;
    const int food0 = c0->stock[COLONIZE_CARGO_FOOD];
    const int food1 = c1->stock[COLONIZE_CARGO_FOOD];
    ai_contact_indian_raids(&ctx, 4);
    if (c1->stock[COLONIZE_CARGO_FOOD] >= food1 && c1->population >= 3 &&
        col1.tribe[0].alarm[1].attacks == 0) {
      return fail("raid should prefer lower-relation Euro among equal friction");
    }
    if (c0->stock[COLONIZE_CARGO_FOOD] != food0) {
      return fail("raid should not loot higher-relation Euro when friction tied");
    }
    /* Restore single-colony smoke baseline. */
    c1->active = false;
    colonies.colony_count = 1;
    ind->alarm_by_player[1] = 0;
    col1.tribe[0].alarm[1].friction = 0;
  }

  /*
   * Raid prefer at-war Euro even when friction is lower: Euro0 alarm 60 /
   * relation 80 (not at war); Euro1 alarm 45 / relation 30 (at war <50).
   * Cite: ai_diplo_indian_at_war; indian_raid_outcomes.md gate.
   */
  {
    euro->x = 12;
    euro->y = 12;
    brave->x = 8;
    brave->y = 8;
    brave->moves_left = 3;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 60;
    ind->alarm_by_player[1] = 45;
    col1.tribe[0].alarm[0].friction = 60;
    col1.tribe[0].alarm[1].friction = 45;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.tribe[0].alarm[1].attacks = 0;
    col1.nation[0].relation_by_indian[0] = 80; /* not at war */
    col1.nation[1].relation_by_indian[0] = 30; /* at war */
    ColonizeColony* c0 = &colonies.colonies[0];
    ColonizeColony* c1 = &colonies.colonies[1];
    c0->active = true;
    c0->nation_id = 0;
    c0->x = 8;
    c0->y = 8;
    c0->population = 3;
    c0->colonist_count = 3;
    memset(c0->stock, 0, sizeof(c0->stock));
    c0->stock[COLONIZE_CARGO_FOOD] = 20;
    c1->active = true;
    c1->nation_id = 1;
    c1->x = 8;
    c1->y = 8;
    c1->population = 3;
    c1->colonist_count = 3;
    memset(c1->stock, 0, sizeof(c1->stock));
    c1->stock[COLONIZE_CARGO_FOOD] = 20;
    colonies.colony_count = 2;
    const int food0w = c0->stock[COLONIZE_CARGO_FOOD];
    const int food1w = c1->stock[COLONIZE_CARGO_FOOD];
    ai_contact_indian_raids(&ctx, 4);
    if (c1->stock[COLONIZE_CARGO_FOOD] >= food1w && c1->population >= 3 &&
        col1.tribe[0].alarm[1].attacks == 0) {
      return fail("raid should prefer at-war Euro over higher-friction peer");
    }
    if (c0->stock[COLONIZE_CARGO_FOOD] != food0w) {
      return fail("raid should not loot non-war Euro when at-war peer exists");
    }
    c1->active = false;
    colonies.colony_count = 1;
    ind->alarm_by_player[1] = 0;
    col1.tribe[0].alarm[1].friction = 0;
  }

  /*
   * Raid prefer colony with muskets/horses at equal distance: secondary military
   * loot thresholds (muskets≥5 / horses≥1). Cite: indian_raid_outcomes.md.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    brave->x = 8;
    brave->y = 8;
    brave->moves_left = 3;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.nation[0].relation_by_indian[0] = 40; /* at-war band */
    ColonizeColony* c_food = &colonies.colonies[0];
    ColonizeColony* c_mil = &colonies.colonies[1];
    c_food->id = 0;
    c_food->active = true;
    c_food->nation_id = 0;
    c_food->x = 8;
    c_food->y = 8; /* same tile / equal dist */
    c_food->population = 3;
    c_food->colonist_count = 3;
    c_food->building_in_production = -1;
    memset(c_food->stock, 0, sizeof(c_food->stock));
    c_food->stock[COLONIZE_CARGO_FOOD] = 20;
    c_mil->id = 1;
    c_mil->active = true;
    c_mil->nation_id = 0; /* same Euro; prefer military stock */
    c_mil->x = 8;
    c_mil->y = 8;
    c_mil->population = 3;
    c_mil->colonist_count = 3;
    c_mil->building_in_production = -1;
    memset(c_mil->stock, 0, sizeof(c_mil->stock));
    c_mil->stock[COLONIZE_CARGO_FOOD] = 20;
    c_mil->stock[COLONIZE_CARGO_MUSKETS] = 15;
    c_mil->stock[COLONIZE_CARGO_HORSES] = 3;
    colonies.colony_count = 2;
    const int food_plain = c_food->stock[COLONIZE_CARGO_FOOD];
    const int musk_mil = c_mil->stock[COLONIZE_CARGO_MUSKETS];
    ai_contact_indian_raids(&ctx, 4);
    if (c_mil->stock[COLONIZE_CARGO_MUSKETS] >= musk_mil &&
        c_mil->stock[COLONIZE_CARGO_FOOD] >= 20 &&
        col1.tribe[0].alarm[0].attacks == 0) {
      return fail("raid should prefer equal-distance colony with muskets/horses");
    }
    if (c_food->stock[COLONIZE_CARGO_FOOD] != food_plain) {
      return fail("raid should not loot non-military colony when mil peer tied");
    }
    c_mil->active = false;
    colonies.colony_count = 1;
  }

  /*
   * Mid-friction raid gate prefers non-mission villages: tribe friction 55 with
   * mission set must not raise the gate when alarm_by_player is low (<40).
   * Cite: fandom Alarm — missions slow hostility; indian_raid_outcomes.md gate.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 12;
    euro->y = 12;
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->nation_id = 4;
    ind->alarm_by_player[0] = 10; /* below raid gate alone */
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0; /* mission present */
    col1.tribe[0].alarm[0].friction = 55; /* mid — should be ignored */
    col1.tribe[0].alarm[0].attacks = 0;
    col1.nation[0].relation_by_indian[0] = 40;
    ColonizeColony* c_ms = &colonies.colonies[0];
    c_ms->active = true;
    c_ms->nation_id = 0;
    c_ms->x = 5;
    c_ms->y = 5;
    c_ms->population = 3;
    c_ms->colonist_count = 3;
    c_ms->building_in_production = -1;
    memset(c_ms->stock, 0, sizeof(c_ms->stock));
    c_ms->stock[COLONIZE_CARGO_FOOD] = 20;
    colonies.colony_count = 1;
    const int food_ms = c_ms->stock[COLONIZE_CARGO_FOOD];
    ai_contact_indian_raids(&ctx, 4);
    if (c_ms->stock[COLONIZE_CARGO_FOOD] != food_ms ||
        col1.tribe[0].alarm[0].attacks != 0) {
      return fail("mid-friction mission tribe should not raise raid gate");
    }
    /* Burn band (≥80) from mission tribe still raises the gate. */
    col1.tribe[0].alarm[0].friction = 85;
    brave->moves_left = 3;
    ai_contact_indian_raids(&ctx, 4);
    if (c_ms->stock[COLONIZE_CARGO_FOOD] >= food_ms &&
        col1.tribe[0].alarm[0].attacks == 0) {
      return fail("burn-band mission tribe friction should still allow raid");
    }
    col1.tribe[0].mission = 0xff;
  }

  /*
   * Prelude escalate + Pocahontas: flag body (unknown31_flags bit 0x20 clear) with
   * met + alarm<30 → difficulty bump; Pocahontas halves (wiki/fandom).
   * Seed/turn: rng_seed=42 + turn=1 fires escalate at diff=2 (bump 7 → 3).
   * Cite: docs/fandom_col1994.md Pocahontas; indian_contact.md prelude.
   */
  {
    /* Despawn any encroacher so only escalate arm bumps alarm. */
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
    c->x = 14;
    c->y = 14; /* park — escalate arm alone */
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 0;
    ind->euro_diplo[0] = 1;
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags & (uint8_t)~0x20);
    ind->alarm_by_player[0] = 10;
    col1.head.founding_father[FF_POCAHONTAS] = -1;
    col1.head.difficulty = 2;
    turn = 1;
    ctx.rng_seed = 42;
    ai_contact_indian_prelude(&ctx, 4);
    if ((ind->unknown31_flags & 0x20) == 0) {
      return fail("prelude escalate should sticky-set flag bit 0x20");
    }
    if (ind->alarm_by_player[0] != 17) { /* 10 + 7 */
      fprintf(
        stderr,
        "unit_ai_contact: escalate alarm=%u (want 17)\n",
        (unsigned)ind->alarm_by_player[0]
      );
      return fail("prelude escalate should bump alarm by 7 at difficulty 2");
    }
    if (col1.tribe[0].alarm[0].friction != 7) {
      return fail("prelude escalate should bump tribe friction by 7");
    }
    /* colony stays parked for Pocahontas escalate arm below */

    /* Same seed path with Pocahontas → half bump (+7 → +3). */
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags & (uint8_t)~0x20);
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 0;
    col1.head.founding_father[FF_POCAHONTAS] = 0;
    ai_contact_indian_prelude(&ctx, 4);
    if (ind->alarm_by_player[0] != 13) { /* 10 + 3 */
      fprintf(
        stderr,
        "unit_ai_contact: poca escalate alarm=%u (want 13)\n",
        (unsigned)ind->alarm_by_player[0]
      );
      return fail("Pocahontas should halve prelude escalate bump to +3");
    }
    col1.head.founding_father[FF_POCAHONTAS] = -1;
    c->x = 5;
    c->y = 5;
  }

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
    brave->moves_left = 3;
    brave->nation_id = 4;
    brave->active = true;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].relation_by_indian[0] = 40;
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
    const int food_plain = c_plain->stock[COLONIZE_CARGO_FOOD];
    const int tools_pref = c_tools->stock[COLONIZE_CARGO_TOOLS];
    ai_contact_indian_raids(&ctx, 4);
    if (c_tools->stock[COLONIZE_CARGO_TOOLS] >= tools_pref &&
        c_tools->stock[COLONIZE_CARGO_FOOD] >= 20 &&
        col1.tribe[0].alarm[0].attacks == 0) {
      return fail("raid should prefer equal-distance colony with tools>=10");
    }
    if (c_plain->stock[COLONIZE_CARGO_FOOD] != food_plain) {
      return fail("raid should not loot non-tools colony when tools peer tied");
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
    brave->moves_left = 3;
    brave->nation_id = 4;
    brave->active = true;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].alarm[0].friction = 65;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].relation_by_indian[0] = 40;
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
    const int food_plain = c_plain->stock[COLONIZE_CARGO_FOOD];
    const int silver_pref = c_silver->stock[COLONIZE_CARGO_SILVER];
    ai_contact_indian_raids(&ctx, 4);
    if (c_silver->stock[COLONIZE_CARGO_SILVER] >= silver_pref &&
        c_silver->stock[COLONIZE_CARGO_FOOD] >= 20 &&
        col1.tribe[0].alarm[0].attacks == 0) {
      return fail("raid should prefer equal-distance colony with silver wealth");
    }
    if (c_plain->stock[COLONIZE_CARGO_FOOD] != food_plain) {
      return fail("raid should not loot plain colony when silver peer tied");
    }
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
    brave->moves_left = 3;
    brave->nation_id = 4;
    brave->active = true;
    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.tribe[0].mission = 0xff;
    col1.nation[0].relation_by_indian[0] = 40;
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
    const int tools_pref = c_tools->stock[COLONIZE_CARGO_TOOLS];
    const int silver_pref = c_silver->stock[COLONIZE_CARGO_SILVER];
    ai_contact_indian_raids(&ctx, 4);
    if (c_tools->stock[COLONIZE_CARGO_TOOLS] >= tools_pref &&
        col1.tribe[0].alarm[0].attacks == 0) {
      return fail("alarm 55 should prefer tools colony over silver peer");
    }
    if (c_silver->stock[COLONIZE_CARGO_SILVER] != silver_pref) {
      return fail("alarm 55 should not loot silver when tools peer tied");
    }
    /* Reset and hot alarm≥80 → silver before tools. */
    c_tools->stock[COLONIZE_CARGO_TOOLS] = 12;
    c_tools->stock[COLONIZE_CARGO_FOOD] = 20;
    c_silver->stock[COLONIZE_CARGO_SILVER] = 8;
    c_silver->stock[COLONIZE_CARGO_FOOD] = 20;
    col1.tribe[0].alarm[0].attacks = 0;
    brave->moves_left = 3;
    brave->orders = UNITS_ORDER_NONE;
    ind->alarm_by_player[0] = 80;
    col1.tribe[0].alarm[0].friction = 80;
    ai_contact_indian_raids(&ctx, 4);
    if (c_silver->stock[COLONIZE_CARGO_SILVER] >= silver_pref &&
        col1.tribe[0].alarm[0].attacks == 0) {
      return fail("alarm≥80 should prefer silver wealth over tools peer");
    }
    if (c_tools->stock[COLONIZE_CARGO_TOOLS] != 12) {
      return fail("alarm≥80 should not loot tools when silver peer tied");
    }
    c_silver->active = false;
    colonies.colony_count = 1;
    ind->alarm_by_player[0] = 0;
    col1.tribe[0].alarm[0].friction = 0;
  }

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
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->nation_id = 4;
    brave->active = true;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 65;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.nation[0].relation_by_indian[0] = 40;
    col1.nation[0].gold = 0; /* no GOLD */
    col1.head.founding_father[FF_POCAHONTAS] = -1;
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
    colonies.colony_count = 1;
    const int lumber0 = c_burn->stock[COLONIZE_CARGO_LUMBER];
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    ai_contact_indian_raids(&ctx, 4);
    if (ai_contact_last_raid_kind() != AI_RAID_BURN) {
      fprintf(
        stderr,
        "unit_ai_contact: burn-lumber kind=%d\n",
        ai_contact_last_raid_kind()
      );
      return fail("lumber-only colony at alarm≥60 should pick AI_RAID_BURN");
    }
    if (c_burn->stock[COLONIZE_CARGO_LUMBER] >= lumber0) {
      return fail("BURN should drain lumber stock when no construction");
    }
    if (strstr(status, "burns buildings") == NULL || strstr(status, "Roanoke") == NULL) {
      fprintf(stderr, "unit_ai_contact: BURN-lumber status '%s'\n", status);
      return fail("BURN lumber should set @RAIDBURN-shaped buildings status");
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
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->nation_id = 4;
    brave->active = true;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    for (int e = 0; e < 4; ++e) {
      ind->alarm_by_player[e] = 0;
      col1.tribe[0].alarm[e].friction = 0;
      col1.tribe[0].alarm[e].attacks = 0;
      col1.nation[e].relation_by_indian[0] = 100; /* peaceful baseline */
      col1.nation[e].gold = 0;
    }
    ind->alarm_by_player[1] = 75; /* ≥70 abandon gate targets nation 1 */
    col1.tribe[0].alarm[1].friction = 75;
    col1.nation[1].relation_by_indian[0] = 40;
    col1.head.founding_father[FF_POCAHONTAS] = -1;
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
    c_fbrn->stock[COLONIZE_CARGO_LUMBER] = 6; /* forces BURN kind */
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
    ai_contact_indian_raids(&ctx, 4);
    units_set_combat_popups(NULL, NULL);
    units_set_combat_human_nation(-1);
    ctx.ai_popups = NULL;
    assets_msg_free(&game_txt_fbrn);
    if (ai_contact_last_raid_kind() != AI_RAID_BURN) {
      fprintf(stderr, "unit_ai_contact: bystander burn kind=%d\n",
              ai_contact_last_raid_kind());
      return fail("bystander lumber-only colony at alarm≥70 should pick AI_RAID_BURN");
    }
    if (status[0] != '\0') {
      return fail("bystander raid must not write human status (not a party)");
    }
    int found_burned3 = 0;
    for (int qi = 0; qi < pop_fbrn.queue_count; ++qi) {
      if (pop_fbrn.queue[qi].kind == AI_POPUP_KIND_OK &&
          strstr(pop_fbrn.queue[qi].body, "Spies report") != NULL &&
          strstr(pop_fbrn.queue[qi].body, "Jamestown") != NULL) {
        found_burned3 = 1;
      }
    }
    if (!found_burned3) {
      fprintf(stderr, "unit_ai_contact: bystander popup queue_count=%d\n",
              pop_fbrn.queue_count);
      for (int qi = 0; qi < pop_fbrn.queue_count; ++qi) {
        fprintf(stderr, "  [%d] '%s'\n", qi, pop_fbrn.queue[qi].body);
      }
      return fail("bystander colony burn should enqueue @BURNED3 spy-report OK");
    }
    /* Restore neutral baseline for nation 1 so later blocks (which only
     * touch nation 0) are not hijacked by this block's higher alarm. */
    ind->alarm_by_player[1] = 0;
    col1.tribe[0].alarm[1].friction = 0;
    col1.nation[1].relation_by_indian[0] = 0;
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
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->nation_id = 4;
    brave->active = true;
    ind->alarm_by_player[0] = 65;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 65;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.nation[0].relation_by_indian[0] = 40;
    col1.nation[0].gold = 0;
    col1.head.founding_father[FF_POCAHONTAS] = -1;
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
    ai_contact_indian_raids(&ctx, 4);
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
      brave->moves_left = 3;
      brave->x = 5;
      brave->y = 5;
      ai_contact_indian_raids(&ctx, 4);
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

  /*
   * Thin Brave escort (14fe): idle Brave units_follow_unit a same-nation
   * Brave already on AI_MOVE. Cite: units_follow_unit; ai_contact raids escort.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    /* Park euro away so raid gate does not yank escort. */
    euro->x = 14;
    euro->y = 14;
    euro->active = true;
    ind->alarm_by_player[0] = 0;
    col1.tribe[0].alarm[0].friction = 0;
    col1.nation[0].relation_by_indian[0] = 50;
    colonies.colonies[0].active = false;
    const int lead_id = units_spawn_allow_stack(&units, 0, 9, 5);
    ColonizeUnit* lead = units_get(&units, lead_id);
    if (!lead) {
      return fail("escort lead spawn");
    }
    lead->nation_id = 4;
    lead->moves_left = 0; /* lead does not consume escort turn */
    lead->orders = UNITS_ORDER_AI_MOVE;
    lead->goto_x = 12;
    lead->goto_y = 5;
    brave->x = 6;
    brave->y = 5;
    brave->nation_id = 4;
    brave->active = true;
    brave->moves_left = 3;
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
    const int bx0 = brave->x;
    const int by0 = brave->y;
    ai_contact_indian_raids(&ctx, 4);
    brave = units_get(&units, brave_id);
    if (!brave || !brave->active) {
      return fail("escort follower must remain active");
    }
    if (brave->orders != UNITS_ORDER_FOLLOW || brave->follow_unit_id != lead_id) {
      fprintf(stderr, "unit_ai_contact: escort orders=%d follow=%d\n", brave->orders,
              brave->follow_unit_id);
      return fail("idle Brave should FOLLOW lead with AI_MOVE");
    }
    /* MD was 3 — advance_follow should step closer (not hold-adjacent). */
    if (brave->x == bx0 && brave->y == by0) {
      return fail("FOLLOW escort should advance one step toward lead");
    }
    /* Cleanup lead for later probes. */
    units_despawn(&units, lead_id);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
  }

  /*
   * Escort deepen: when raid gate Euro known, prefer lead whose goto is closer
   * to that Euro's colony over nearer lead heading away. Cite: units_follow_unit;
   * indian_raid_outcomes.md §1.
   */
  {
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 1;
    }
    euro->x = 14;
    euro->y = 14;
    euro->active = true;
    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    col1.nation[0].relation_by_indian[0] = 40;
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
    brave->moves_left = 3;
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
    near_lead->moves_left = 0;
    near_lead->orders = UNITS_ORDER_AI_MOVE;
    near_lead->goto_x = 4;
    near_lead->goto_y = 5; /* away from colony at (12,5) */
    far_lead->nation_id = 4;
    far_lead->moves_left = 0;
    far_lead->orders = UNITS_ORDER_AI_MOVE;
    far_lead->goto_x = 12;
    far_lead->goto_y = 5; /* toward raid-gate Euro colony */
    ai_contact_indian_raids(&ctx, 4);
    brave = units_get(&units, brave_id);
    if (!brave || brave->orders != UNITS_ORDER_FOLLOW ||
        brave->follow_unit_id != far_id) {
      fprintf(stderr, "unit_ai_contact: escort deepen follow=%d (want %d)\n",
              brave ? brave->follow_unit_id : -1, far_id);
      return fail("escort should prefer goto toward raid-gate Euro colony");
    }
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
    const int near2 = units_spawn_allow_stack(&units, 0, 6, 5);
    const int far2 = units_spawn_allow_stack(&units, 0, 5, 7);
    near_lead = units_get(&units, near2);
    far_lead = units_get(&units, far2);
    if (!near_lead || !far_lead) {
      return fail("escort fallback lead spawn");
    }
    near_lead->nation_id = 4;
    near_lead->moves_left = 0;
    near_lead->orders = UNITS_ORDER_AI_MOVE;
    near_lead->goto_x = 4;
    near_lead->goto_y = 5;
    far_lead->nation_id = 4;
    far_lead->moves_left = 0;
    far_lead->orders = UNITS_ORDER_AI_MOVE;
    far_lead->goto_x = 12;
    far_lead->goto_y = 5;
    ai_contact_indian_raids(&ctx, 4);
    brave = units_get(&units, brave_id);
    if (!brave || brave->orders != UNITS_ORDER_FOLLOW ||
        brave->follow_unit_id != near2) {
      fprintf(stderr, "unit_ai_contact: escort fallback follow=%d (want %d)\n",
              brave ? brave->follow_unit_id : -1, near2);
      return fail("escort without gate should pick nearest lead");
    }
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
    col1.nation[0].relation_by_indian[0] = 40;
    c_tgt->active = true;
    c_tgt->nation_id = 0;
    c_tgt->x = 12;
    c_tgt->y = 5;
    colonies.colony_count = 1;
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->orders = UNITS_ORDER_NONE;
    const int md4 = units_spawn_allow_stack(&units, 0, 9, 5); /* MD=4 from (5,5) */
    ColonizeUnit* md4_lead = units_get(&units, md4);
    if (!md4_lead) {
      return fail("alarmed escort MD4 lead spawn");
    }
    md4_lead->nation_id = 4;
    md4_lead->moves_left = 0;
    md4_lead->orders = UNITS_ORDER_AI_MOVE;
    md4_lead->goto_x = 12;
    md4_lead->goto_y = 5;
    ai_contact_indian_raids(&ctx, 4);
    brave = units_get(&units, brave_id);
    if (!brave || brave->orders != UNITS_ORDER_FOLLOW || brave->follow_unit_id != md4) {
      fprintf(stderr, "unit_ai_contact: alarmed MD4 follow=%d\n",
              brave ? brave->follow_unit_id : -1);
      return fail("alarm≥55 escort should reach lead at MD=4");
    }
    units_despawn(&units, md4);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;

    /*
     * Series N: alarm≥80 escort MD≤5 (alarm 55 still caps at 4).
     * Cite: ai_contact_escort_pick_lead hot deepen.
     */
    ind->alarm_by_player[0] = 55;
    col1.tribe[0].alarm[0].friction = 55;
    col1.nation[0].relation_by_indian[0] = 40;
    c_tgt->active = true;
    c_tgt->nation_id = 0;
    c_tgt->x = 12;
    c_tgt->y = 5;
    colonies.colony_count = 1;
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->orders = UNITS_ORDER_NONE;
    const int md5 = units_spawn_allow_stack(&units, 0, 10, 5); /* MD=5 from (5,5) */
    ColonizeUnit* md5_lead = units_get(&units, md5);
    if (!md5_lead) {
      return fail("hot escort MD5 lead spawn");
    }
    md5_lead->nation_id = 4;
    md5_lead->moves_left = 0;
    md5_lead->orders = UNITS_ORDER_AI_MOVE;
    md5_lead->goto_x = 12;
    md5_lead->goto_y = 5;
    ai_contact_indian_raids(&ctx, 4);
    brave = units_get(&units, brave_id);
    if (brave && brave->orders == UNITS_ORDER_FOLLOW && brave->follow_unit_id == md5) {
      return fail("alarm 55 escort must not reach lead at MD=5");
    }
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
    ind->alarm_by_player[0] = 80;
    col1.tribe[0].alarm[0].friction = 80;
    ai_contact_indian_raids(&ctx, 4);
    brave = units_get(&units, brave_id);
    if (!brave || brave->orders != UNITS_ORDER_FOLLOW || brave->follow_unit_id != md5) {
      fprintf(stderr, "unit_ai_contact: hot MD5 follow=%d\n",
              brave ? brave->follow_unit_id : -1);
      return fail("alarm≥80 escort should reach lead at MD=5");
    }
    units_despawn(&units, md5);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
    c_tgt->active = false;
    colonies.colony_count = 0;
    ind->alarm_by_player[0] = 0;
    col1.tribe[0].alarm[0].friction = 0;
  }

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
    brave->x = 5;
    brave->y = 5;
    brave->moves_left = 3;
    brave->nation_id = 4;
    brave->active = true;
    ind->alarm_by_player[0] = 50;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.nation[0].relation_by_indian[0] = 40;
    col1.head.founding_father[FF_POCAHONTAS] = -1;
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
    ai_contact_indian_raids(&ctx, 4);
    const int raid_kind = ai_contact_last_raid_kind();
    if (raid_kind == AI_RAID_NOTHING) {
      return fail("raid friction escalate needs successful loot kind");
    }
    int want_bump = 4;
    if (raid_kind == AI_RAID_BURN || raid_kind == AI_RAID_WREAK) {
      want_bump = 12;
    } else if (raid_kind == AI_RAID_SCALP) {
      want_bump = 16;
    } else if (raid_kind == AI_RAID_GOLD || raid_kind == AI_RAID_SHIP) {
      want_bump = 8;
    } else if (raid_kind == AI_RAID_STORES) {
      want_bump = 4;
    }
    const unsigned want_fr = (unsigned)(50 + want_bump);
    if (col1.tribe[0].alarm[0].friction != want_fr) {
      fprintf(
        stderr,
        "unit_ai_contact: raid friction=%u (want %u kind=%d)\n",
        (unsigned)col1.tribe[0].alarm[0].friction,
        want_fr,
        raid_kind
      );
      return fail("successful raid should bump tribe friction by kind delta");
    }
    if (ind->alarm_by_player[0] != want_fr) {
      fprintf(
        stderr,
        "unit_ai_contact: raid alarm=%u (want %u kind=%d)\n",
        (unsigned)ind->alarm_by_player[0],
        want_fr,
        raid_kind
      );
      return fail("successful raid should bump alarm_by_player by kind delta");
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
      /* Force many rolls; accept demote when WREAK would have fired. */
      int saw_demote = 0;
      for (int attempt = 0; attempt < 40; ++attempt) {
        c_fr->stock[COLONIZE_CARGO_FOOD] = 20;
        c_fr->stock[COLONIZE_CARGO_TOOLS] = 10;
        c_fr->population = 3;
        ind->alarm_by_player[0] = 90;
        col1.tribe[0].alarm[0].friction = 90;
        ai_contact_indian_raids(&ctx, 4);
        const int k = ai_contact_last_raid_kind();
        if (k == AI_RAID_WREAK) {
          return fail("year<1520 should demote WREAK away");
        }
        if (k == AI_RAID_STORES || k == AI_RAID_NOTHING) {
          saw_demote = 1;
        }
      }
      if (!saw_demote) {
        return fail("early-year demote path should yield STORES/NOTHING in samples");
      }
      col1.head.year = 1492;
      col1.head.difficulty = 2;
    }

    /* Same path with Pocahontas → half kind bump. */
    col1.head.founding_father[FF_POCAHONTAS] = 0;
    ind->alarm_by_player[0] = 50;
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].alarm[0].attacks = 0;
    c_fr->stock[COLONIZE_CARGO_FOOD] = 20;
    c_fr->population = 3;
    c_fr->colonist_count = 3;
    brave->moves_left = 3;
    brave->x = 5;
    brave->y = 5;
    ai_contact_indian_raids(&ctx, 4);
    const int poca_kind = ai_contact_last_raid_kind();
    if (poca_kind == AI_RAID_NOTHING) {
      return fail("Pocahontas raid escalate needs successful loot kind");
    }
    int poca_full = 4;
    if (poca_kind == AI_RAID_BURN || poca_kind == AI_RAID_WREAK) {
      poca_full = 12;
    } else if (poca_kind == AI_RAID_SCALP) {
      poca_full = 16;
    } else if (poca_kind == AI_RAID_GOLD || poca_kind == AI_RAID_SHIP) {
      poca_full = 8;
    } else if (poca_kind == AI_RAID_STORES) {
      poca_full = 4;
    }
    const unsigned want_poca = (unsigned)(50 + poca_full / 2);
    if (col1.tribe[0].alarm[0].friction != want_poca) {
      fprintf(
        stderr,
        "unit_ai_contact: poca raid friction=%u (want %u kind=%d)\n",
        (unsigned)col1.tribe[0].alarm[0].friction,
        want_poca,
        poca_kind
      );
      return fail("Pocahontas should halve raid friction kind bump");
    }
    if (ind->alarm_by_player[0] != want_poca) {
      fprintf(
        stderr,
        "unit_ai_contact: poca raid alarm=%u (want %u kind=%d)\n",
        (unsigned)ind->alarm_by_player[0],
        want_poca,
        poca_kind
      );
      return fail("Pocahontas should halve raid alarm kind bump");
    }
    col1.head.founding_father[FF_POCAHONTAS] = -1;
  }

  /*
   * AI popup unpark: first meet enqueues CONTACT_WELCOME Yes/No; Accept →
   * peace + PEACE/COME OKs only (no Meet CHOICE). Cite: FUN_5bfb_022e / 0182.
   */
  {
    AiPopupState pop;
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
    ColonizeUnit* brave2 = units_get(&units, b2);
    ColonizeUnit* euro2 = units_get(&units, e2);
    if (!brave2 || !euro2) {
      return fail("popup meet spawn");
    }
    brave2->nation_id = 4;
    brave2->moves_left = 1;
    euro2->nation_id = 0;
    euro2->profession = UNITS_JOB_NONE;
    ind->euro_diplo[0] = 0;
    ind->alarm_by_player[0] = 25; /* Accept should clear */
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].alarm[0].friction = 18; /* Accept should clear */
    col1.tribe[0].state.learned = 1; /* skip teach side-queue */
    col1.tribe[0].mission = 0xff;
    col1.nation[0].relation_by_indian[0] = 0;
    col1.nation[0].gold = 30;
    ColonizeColony* c_pop = &colonies.colonies[0];
    c_pop->active = true;
    c_pop->nation_id = 0;
    c_pop->x = 5;
    c_pop->y = 5;
    c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 5;
    const int goods0 = c_pop->stock[COLONIZE_CARGO_TRADE_GOODS];

    char st_pop[128];
    st_pop[0] = '\0';
    ctx.status = st_pop;
    ctx.status_size = sizeof(st_pop);
    ctx.human_nation = 0;
    ctx.ai_popups = &pop;
    ctx.units = &units;
    ctx.colonies = &colonies;
    ctx.col1 = &col1;
    ctx.col1_ok = true;

    ai_contact_indian_meet_trade(&ctx, 4);
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
    if (c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] != goods0) {
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
    if (ind->alarm_by_player[0] != 0 || col1.tribe[0].alarm[0].friction != 0) {
      return fail("WELCOME Yes should clear alarm/friction toward Euro");
    }
    /* Land grant: occupied tile stamped purchased + euro owner nibble. */
    {
      euro2 = units_get(&units, e2);
      if (!euro2) {
        return fail("WELCOME Yes euro gone");
      }
      const size_t gidx = (size_t)euro2->y * (size_t)map.width + (size_t)euro2->x;
      if ((map.layer2[gidx] & MAP_LAYER2_PURCHASED) == 0) {
        return fail("WELCOME Yes should mark occupied tile purchased");
      }
      if (((map.layer3[gidx] >> 4) & 0x0fu) != 0u) {
        return fail("WELCOME Yes should set euro owner nibble on grant tile");
      }
      if (colonies_indian_land_purchase_gold(&col1, &map, euro2->x, euro2->y, 0) != 0) {
        return fail("WELCOME grant tile must be free to found (purchase gold 0)");
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
      ai_contact_indian_meet_trade(&ctx, 4);
      if (ind->euro_diplo[0]) {
        return fail("ship adjacency must not set euro_diplo");
      }
      if (pop.queue_count > 0) {
        return fail("ship adjacency must not enqueue WELCOME");
      }
      /* Restore land Free Colonist for later arms. */
      units_despawn(&units, ship_id);
      const int el = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* elu = units_get(&units, el);
      if (elu) {
        elu->nation_id = 0;
      }
    }

    /* Synthetic Meet CHOICE Trade still works when player initiates (apply). */
    ai_popup_clear(&pop);
    ind->euro_diplo[0] = 1;
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] | 0x40);
    col1.nation[0].relation_by_indian[0] = 100;
    ind->alarm_by_player[0] = 10;
    c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = goods0;
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* AI_CONTACT_CHOICE_TRADE */
    pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    pop.result_payload = 0;
    st_pop[0] = '\0';
    ai_contact_apply_popup_result(&ctx, &pop);
    if (c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] != goods0 - 1) {
      return fail("Trade CHOICE should run thin auto-trade");
    }
    if (pop.queue_count < 1) {
      return fail("Trade apply should enqueue Trade accepted OK");
    }
    if (strstr(st_pop, "Trade") == NULL) {
      fprintf(stderr, "unit_ai_contact: trade status '%s'\n", st_pop);
      return fail("Trade apply should set Trade accepted status");
    }
    if (strstr(st_pop, "Jewelled Relics") == NULL) {
      fprintf(stderr, "unit_ai_contact: trade flavor status '%s'\n", st_pop);
      return fail("Trade apply should name @TRIBES flavor good (Inca Jewelled Relics)");
    }
    if (col1.tribe[0].last_bought != (uint8_t)COLONIZE_CARGO_TRADE_GOODS) {
      return fail("Trade apply should set tribe.last_bought to trade goods");
    }
    if (col1.tribe[0].last_sold != (uint8_t)COLONIZE_CARGO_SILVER) {
      return fail("Trade apply should set Inca last_sold to silver (teach map)");
    }

    /*
     * Hard-bargain thin (alarm 45..54): trade succeeds, no relation bump.
     * Cite: indian_contact.md Meet CHOICE Trade; FUN_4d56_2820 stand-in.
     */
    {
      const uint8_t rel_hb = col1.nation[0].relation_by_indian[0];
      ind->alarm_by_player[0] = 47;
      col1.tribe[0].alarm[0].friction = 20;
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 3;
      const int goods_hb = c_pop->stock[COLONIZE_CARGO_TRADE_GOODS];
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* TRADE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      /* Inca silver + hard-bargain → 2bbc peel drains 2 trade goods. */
      if (c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] != goods_hb - 2) {
        fprintf(stderr, "unit_ai_contact: hard-bargain goods %d→%d\n", goods_hb,
                c_pop->stock[COLONIZE_CARGO_TRADE_GOODS]);
        return fail("hard-bargain silver peel should drain 2 trade goods");
      }
      if (col1.nation[0].relation_by_indian[0] != rel_hb) {
        return fail("hard-bargain should skip relation bump");
      }
      if (col1.tribe[0].alarm[0].friction != 20) {
        return fail("hard-bargain should skip tribe friction decay (tension)");
      }
      if (ind->alarm_by_player[0] != 46) {
        return fail("hard-bargain should still decay alarm_by_player by 1");
      }
      if (strstr(st_pop, "hard bargain") == NULL) {
        fprintf(stderr, "unit_ai_contact: hard-bargain status '%s'\n", st_pop);
        return fail("hard-bargain should set hard-bargain status");
      }
      ind->alarm_by_player[0] = 10; /* restore peaceful for later arms */
    }

    /*
     * Hard-bargain ore peel (Aztec primary): same 2bbc-shaped extra goods drain.
     * Cite: indian_trade_2820.md; Series G2.
     */
    {
      ColonizeCol1Indian* az = &col1.indian[1];
      az->euro_diplo[0] = 1;
      az->alarm_by_player[0] = 47;
      col1.nation[0].relation_by_indian[1] = 80; /* Aztec×Euro meet floor */
      col1.tribe[0].nation_id = 5; /* Aztec ore */
      col1.tribe[0].alarm[0].friction = 20;
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 3;
      const int goods_ore = c_pop->stock[COLONIZE_CARGO_TRADE_GOODS];
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 5;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] != goods_ore - 2) {
        fprintf(stderr, "unit_ai_contact: ore hard-bargain goods %d→%d\n", goods_ore,
                c_pop->stock[COLONIZE_CARGO_TRADE_GOODS]);
        return fail("hard-bargain ore peel should drain 2 trade goods");
      }
      col1.tribe[0].nation_id = 4; /* restore Inca */
      ind->alarm_by_player[0] = 10;
      az->alarm_by_player[0] = 10;
      pop.result_nation_b = 4;
    }

    /*
     * Series M: hard-bargain primary extras for tobacco/cotton (+ one furs).
     * Non-0xff teach primaries drain 2 TG under alarm 45..54. Arawak 0xff stays 1.
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
        ind_m->alarm_by_player[0] = 47;
        col1.nation[0].relation_by_indian[cases[ci].tribe_nation - 4] = 80;
        col1.tribe[0].nation_id = (uint8_t)cases[ci].tribe_nation;
        col1.tribe[0].alarm[0].friction = 20;
        c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 3;
        const int goods_m = c_pop->stock[COLONIZE_CARGO_TRADE_GOODS];
        ai_popup_clear(&pop);
        pop.has_result = true;
        pop.result_cancelled = false;
        pop.result_choice_id = 1;
        pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
        pop.result_nation_a = 0;
        pop.result_nation_b = cases[ci].nation_b;
        st_pop[0] = '\0';
        ai_contact_apply_popup_result(&ctx, &pop);
        if (c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] != goods_m - 2) {
          fprintf(stderr, "unit_ai_contact: %s hard-bargain goods %d→%d\n",
                  cases[ci].label, goods_m, c_pop->stock[COLONIZE_CARGO_TRADE_GOODS]);
          return fail("hard-bargain primary peel should drain 2 trade goods");
        }
        ind_m->alarm_by_player[0] = 10;
      }
      col1.tribe[0].nation_id = 4;
      ind->alarm_by_player[0] = 10;
      pop.result_nation_b = 4;
    }

    /*
     * Sea trade thin (fandom sea/land): no colony goods, ship hold TRADE_GOODS
     * within reach → drain hold. Cite: docs/fandom_col1994.md Teach/trade.
     */
    {
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
      const int goods_land = c_pop->stock[COLONIZE_CARGO_TRADE_GOODS];
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 0;
      col1.tribe[0].x = 5;
      col1.tribe[0].y = 5;
      ind->alarm_by_player[0] = 10;
      col1.nation[0].relation_by_indian[0] = 80;
      const uint8_t rel_sea = col1.nation[0].relation_by_indian[0];
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* TRADE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (ship->hold_goods_amount[0] != 2) {
        return fail("sea-trade should drain 1 TRADE_GOODS from ship hold");
      }
      if (c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] != 0) {
        return fail("sea-trade should not invent colony warehouse goods");
      }
      if (col1.nation[0].relation_by_indian[0] != (uint8_t)(rel_sea + 2)) {
        return fail("sea-trade should bump relation like land trade");
      }
      if (strstr(st_pop, "Trade") == NULL) {
        fprintf(stderr, "unit_ai_contact: sea-trade status '%s'\n", st_pop);
        return fail("sea-trade should set Trade accepted status");
      }
      units_despawn(&units, ship_id);
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = goods_land;
    }

    /*
     * Wagon land-trade thin: TRADE_GOODS on Wagon Train hold within reach.
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
      const int goods_land2 = c_pop->stock[COLONIZE_CARGO_TRADE_GOODS];
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 0;
      ind->alarm_by_player[0] = 12;
      col1.nation[0].relation_by_indian[0] = 70;
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1;
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (wag->hold_goods_amount[0] != 1) {
        return fail("wagon-trade should drain 1 TRADE_GOODS from wagon hold");
      }
      if (col1.nation[0].relation_by_indian[0] != 72) {
        return fail("wagon-trade should bump relation +2");
      }
      units_despawn(&units, wag_id);
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = goods_land2;
    }

    /* Leave dismisses with thin Farewell OK; no trade side effects. */
    ai_popup_clear(&pop);
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 5; /* LEAVE */
    pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    const int goods_leave = c_pop->stock[COLONIZE_CARGO_TRADE_GOODS];
    st_pop[0] = '\0';
    ai_contact_apply_popup_result(&ctx, &pop);
    if (c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] != goods_leave) {
      return fail("Leave CHOICE should not trade");
    }
    if (pop.queue_count < 1 ||
        pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
        pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_MEET) {
      return fail("Leave CHOICE should enqueue Farewell OK");
    }
    if (strstr(st_pop, "Farewell") == NULL) {
      fprintf(stderr, "unit_ai_contact: leave status '%s'\n", st_pop);
      return fail("Leave CHOICE should set Farewell status");
    }

    /*
     * Trade CHOICE with no goods → haggle stub OK "Trade concluded."
     * Cite: FUN_5bfb_022e / 2aac…311e thin; deep 2820 PARKED.
     */
    {
      ai_popup_clear(&pop);
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 0;
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* TRADE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (pop.queue_count < 1) {
        return fail("Trade fail should enqueue Trade concluded OK");
      }
      if (pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_MEET) {
        return fail("Trade fail follow-up should be CONTACT_MEET OK");
      }
      if (strstr(st_pop, "Trade concluded") == NULL) {
        fprintf(stderr, "unit_ai_contact: trade-stub status '%s'\n", st_pop);
        return fail("Trade fail should set Trade concluded status");
      }
    }

    /*
     * Trade CHOICE haggle refuse (alarm≥50 gate): OK "The %s refuse to trade."
     * Cite: FUN_4d56_2aac refuse; fandom Alarm; deep 2820 PARKED.
     */
    {
      ai_popup_clear(&pop);
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 5;
      ind->alarm_by_player[0] = 55;
      col1.nation[0].relation_by_indian[0] = 80;
      col1.tribe[0].last_bought = (uint8_t)COLONIZE_CARGO_TRADE_GOODS;
      col1.tribe[0].last_sold = (uint8_t)COLONIZE_CARGO_FURS;
      const int goods_ref = c_pop->stock[COLONIZE_CARGO_TRADE_GOODS];
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* TRADE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] != goods_ref) {
        return fail("Trade refuse should not drain trade goods");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_REFUSE) {
        return fail("Trade refuse should enqueue CONTACT_REFUSE OK");
      }
      if (strstr(st_pop, "refuse") == NULL || strstr(st_pop, "trade") == NULL ||
          strstr(st_pop, "The ") == NULL) {
        fprintf(stderr, "unit_ai_contact: trade-refuse status '%s'\n", st_pop);
        return fail("Trade refuse should set tribe-named refuse-to-trade status");
      }
      /* FUN_4d56_2af6: abort clears last-goods flags. */
      if (col1.tribe[0].last_bought != 0xffu || col1.tribe[0].last_sold != 0xffu) {
        return fail("Trade refuse should clear tribe last_bought/last_sold");
      }
      ind->alarm_by_player[0] = 0; /* restore peaceful for later popup arms */
    }

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
      col1.nation[0].relation_by_indian[0] = 0;
      st_pop[0] = '\0';
      ai_contact_indian_meet_trade(&ctx, 4);
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
      col1.nation[0].relation_by_indian[0] = 10;
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
      ai_contact_indian_meet_trade(&ctx, 4);
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
      if (ind->alarm_by_player[0] < 80 || col1.tribe[0].alarm[0].friction < 80) {
        return fail("WELCOME No should raise alarm/friction to burn band");
      }
      if (col1.tribe[0].alarm[0].attacks < 1) {
        return fail("WELCOME No should increment tribe attacks");
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
      col1.nation[0].relation_by_indian[0] = 100;
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
      ai_contact_indian_meet_trade(&ctx, 4);
      if (strstr(st_pop, "refuse to talk") != NULL) {
        return fail("post-peace adjacency must not refuse-talk");
      }
    }

    /*
     * Mission burn (≥80): status + CONTACT_RAID OK enqueue.
     * Cite: FUN_4cc6_0000 / FUN_4d56_1816 prelude.
     */
    {
      ai_popup_clear(&pop);
      col1.tribe[0].mission = 0;
      ind->alarm_by_player[0] = 80;
      col1.tribe[0].alarm[0].friction = 10;
      st_pop[0] = '\0';
      ai_contact_indian_prelude(&ctx, 4);
      if (col1.tribe[0].mission != 0xff) {
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
    }

    /*
     * Gift CHOICE → amount CHOICE (Small/Large); Large apply −10 gold.
     * Cite: FUN_5bfb_102a / 1092; indian_contact.md gift amount widget.
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
        return fail("gift CHOICE spawn");
      }
      braveg->nation_id = 4;
      eurog->nation_id = 0;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 10;
      col1.tribe[0].alarm[0].friction = 10;
      col1.tribe[0].state.learned = 1;
      col1.tribe[0].mission = 0xff;
      col1.nation[0].gold = 50;
      col1.nation[0].relation_by_indian[0] = 80;
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 2; /* GIFT */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != 50u) {
        return fail("Meet Gift should defer drain until amount CHOICE");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_GIFT) {
        return fail("Gift CHOICE should enqueue CONTACT_GIFT amount CHOICE");
      }
      if (pop.queue[pop.queue_count - 1].choice_count < 3) {
        return fail("amount CHOICE should offer Small, Large, and Generous when gold≥20");
      }
      /* Apply Large (−10). */
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 2; /* AI_CONTACT_GIFT_LARGE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_GIFT;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != 40u) {
        return fail("Large gift should drain 10 gold");
      }
      if (col1.tribe[0].alarm[0].friction != 8) {
        return fail("Large gift should reduce friction by 2");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_GIFT ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK) {
        return fail("Large gift should enqueue Gift OK follow-up");
      }
      if (strstr(st_pop, "Gift") == NULL) {
        return fail("Large gift should set Gift status");
      }

      /* Small gift (−5 / friction −1). */
      ai_popup_clear(&pop);
      col1.nation[0].gold = 30;
      ind->alarm_by_player[0] = 10;
      col1.tribe[0].alarm[0].friction = 10;
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* AI_CONTACT_GIFT_SMALL */
      pop.result_tag = AI_POPUP_TAG_CONTACT_GIFT;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != 25u) {
        return fail("Small gift should drain 5 gold");
      }
      if (col1.tribe[0].alarm[0].friction != 9) {
        return fail("Small gift should reduce friction by 1");
      }

      /* Generous gift (−20 / friction −3); deep amount arm thin. */
      ai_popup_clear(&pop);
      col1.nation[0].gold = 40;
      ind->alarm_by_player[0] = 12;
      col1.tribe[0].alarm[0].friction = 12;
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 3; /* AI_CONTACT_GIFT_GENEROUS */
      pop.result_tag = AI_POPUP_TAG_CONTACT_GIFT;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != 20u) {
        return fail("Generous gift should drain 20 gold");
      }
      if (col1.tribe[0].alarm[0].friction != 9) {
        return fail("Generous gift should reduce friction by 3");
      }

      /*
       * Pocahontas: gift friction decay stays full (−2); half-rate is for
       * positive alarm bumps only. Cite: docs/fandom_col1994.md Pocahontas.
       */
      ai_popup_clear(&pop);
      col1.head.founding_father[FF_POCAHONTAS] = 0;
      col1.nation[0].gold = 30;
      ind->alarm_by_player[0] = 12;
      col1.tribe[0].alarm[0].friction = 12;
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 2; /* AI_CONTACT_GIFT_LARGE */
      pop.result_tag = AI_POPUP_TAG_CONTACT_GIFT;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != 20u) {
        return fail("Pocahontas Large gift should still drain 10 gold");
      }
      if (col1.tribe[0].alarm[0].friction != 10) {
        return fail("Pocahontas should not halve gift friction decay");
      }
      if (ind->alarm_by_player[0] != 10) {
        return fail("Pocahontas should not halve gift alarm decay");
      }
      col1.head.founding_father[FF_POCAHONTAS] = -1;
    }

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
      col1.nation[0].relation_by_indian[0] = 128;
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
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = target;
      pop.result_tag = AI_POPUP_TAG_CONTACT_INCITE;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold >= 5000u || col1.nation[0].gold > 4500u) {
        return fail("Incite should drain at least 500 gold from the inciter");
      }
      if (col1.nation[target].gold != target_gold_before) {
        return fail("Incite target's own gold should be untouched");
      }
      if (ind->alarm_by_player[target] != 10) {
        return fail("Incite should push the target's alarm with this tribe");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_INCITE ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK) {
        return fail("Incite should enqueue an Incite OK follow-up");
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
      if (pop.queue_count >= 1 &&
          pop.queue[pop.queue_count - 1].kind == AI_POPUP_KIND_CHOICE &&
          pop.queue[pop.queue_count - 1].tag == AI_POPUP_TAG_CONTACT_INCITE) {
        return fail("broke inciter should not see an incite target CHOICE");
      }
    }

    /*
     * Demand CHOICE → amount CHOICE (tools vs gold); gold apply −15.
     * Cite: FUN_5bfb_102a / 1092; indian_contact.md demand amount widget.
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
      ind->alarm_by_player[0] = 20;
      col1.tribe[0].alarm[0].friction = 45; /* mid demand band */
      col1.tribe[0].state.learned = 1;
      col1.tribe[0].mission = 0xff;
      col1.nation[0].gold = 80;
      col1.nation[0].relation_by_indian[0] = 80;
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
      if (col1.nation[0].gold != 80u || c->stock[COLONIZE_CARGO_TOOLS] != 25) {
        return fail("Meet Demand should defer drain until amount CHOICE");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_CHOICE ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_DEMAND) {
        return fail("Demand CHOICE should enqueue CONTACT_DEMAND amount CHOICE");
      }
      if (pop.queue[pop.queue_count - 1].choice_count < 2) {
        return fail("demand amount CHOICE should offer tools and gold");
      }
      /* Apply gold (−15). */
      ai_popup_clear(&pop);
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 2; /* AI_CONTACT_DEMAND_GOLD */
      pop.result_tag = AI_POPUP_TAG_CONTACT_DEMAND;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      const uint8_t fr_d = col1.tribe[0].alarm[0].friction;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (col1.nation[0].gold != 65u) {
        return fail("Demand gold CHOICE should drain 15 gold");
      }
      if (c->stock[COLONIZE_CARGO_TOOLS] != 25) {
        return fail("Demand gold CHOICE should not touch tools");
      }
      if (col1.tribe[0].alarm[0].friction != (uint8_t)(fr_d - 3)) {
        return fail("Demand gold CHOICE should decay friction by 3");
      }
      if (strstr(st_pop, "Tribute") == NULL) {
        return fail("Demand gold CHOICE should set Tribute status");
      }
      /* Tools path from amount CHOICE. */
      ai_popup_clear(&pop);
      col1.nation[0].gold = 80;
      ind->alarm_by_player[0] = 20;
      col1.tribe[0].alarm[0].friction = 45;
      c->stock[COLONIZE_CARGO_TOOLS] = 25;
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* AI_CONTACT_DEMAND_TOOLS */
      pop.result_tag = AI_POPUP_TAG_CONTACT_DEMAND;
      pop.result_nation_a = 0;
      pop.result_nation_b = 4;
      st_pop[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &pop);
      if (c->stock[COLONIZE_CARGO_TOOLS] != 15) {
        return fail("Demand tools CHOICE should drain 10 tools");
      }
      if (col1.nation[0].gold != 80u) {
        return fail("Demand tools CHOICE should not touch gold");
      }
    }

    /*
     * Demand CHOICE refuse when alarmed (≥55) → CONTACT_DEMAND OK
     * "Natives refuse demands."; no tools/gold drain (no amount CHOICE).
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
      col1.nation[0].relation_by_indian[0] = 80;
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
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_DEMAND) {
        return fail("Demand refuse should enqueue CONTACT_DEMAND OK");
      }
      if (strstr(st_pop, "refuse") == NULL || strstr(st_pop, "demand") == NULL) {
        fprintf(stderr, "unit_ai_contact: demand-refuse OK status '%s'\n", st_pop);
        return fail("Demand refuse should set refuse-demands status");
      }
    }

    /*
     * Teach CHOICE refuse (≥55) → follow-up CONTACT_TEACH OK.
     * Cite: FUN_5bfb_022e teach arm; fandom Alarm refuse-talk gate.
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
        return fail("teach refuse CHOICE spawn");
      }
      bravet->nation_id = 4;
      eurot->nation_id = 0;
      eurot->profession = UNITS_JOB_NONE;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 60;
      col1.tribe[0].nation_id = 4;
      col1.tribe[0].x = 5;
      col1.tribe[0].y = 5;
      col1.tribe[0].alarm[0].friction = 60;
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
        return fail("Teach refuse CHOICE should not set learned");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_TEACH) {
        return fail("Teach refuse should enqueue CONTACT_TEACH OK");
      }
      if (strstr(st_pop, "infuriate") == NULL || strstr(st_pop, "learn") == NULL) {
        fprintf(stderr, "unit_ai_contact: teach-refuse OK status '%s'\n", st_pop);
        return fail("Teach refuse should set @LEARNMAD refuse status");
      }
    }

    /*
     * Teach CHOICE success: Aztec (5) → Ore Miner (unused nation-map entry).
     * Cite: indian_contact.md teach-skill profession map; FUN_5bfb_022e.
     */
    {
      ai_popup_clear(&pop);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active &&
            (u->nation_id == 4 || u->nation_id == 5 || u->nation_id == 0)) {
          units_despawn(&units, u->id);
        }
      }
      const int bt = units_spawn_allow_stack(&units, 0, 5, 5);
      const int et = units_spawn_allow_stack(&units, 1, 6, 5);
      ColonizeUnit* bravet = units_get(&units, bt);
      ColonizeUnit* eurot = units_get(&units, et);
      if (!bravet || !eurot) {
        return fail("teach Aztec CHOICE spawn");
      }
      bravet->nation_id = 5;
      eurot->nation_id = 0;
      eurot->profession = UNITS_JOB_NONE;
      ColonizeCol1Indian* aztec = &col1.indian[1];
      memset(aztec, 0, sizeof(*aztec));
      aztec->euro_diplo[0] = 1;
      aztec->alarm_by_player[0] = 5;
      col1.tribe[0].nation_id = 5;
      col1.tribe[0].x = 5;
      col1.tribe[0].y = 5;
      col1.tribe[0].alarm[0].friction = 5;
      col1.tribe[0].state.learned = 0;
      col1.tribe[0].last_sold = 0;
      col1.tribe[0].mission = 0xff;
      col1.nation[0].relation_by_indian[1] = 80; /* Aztec idx 1 */
      col1.nation[0].gold = 0;
      st_pop[0] = '\0';
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 4; /* TEACH */
      pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
      pop.result_nation_a = 0;
      pop.result_nation_b = 5;
      ai_contact_apply_popup_result(&ctx, &pop);
      if (!col1.tribe[0].state.learned) {
        return fail("Teach Aztec CHOICE should set learned");
      }
      if (eurot->profession != COLONIZE_JOB_ORE_MINER) {
        return fail("Teach Aztec CHOICE → Expert Ore Miner");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_TEACH) {
        return fail("Teach Aztec success should enqueue CONTACT_TEACH OK");
      }
      if (strstr(st_pop, "teach") == NULL || strstr(st_pop, "Aztec") == NULL) {
        fprintf(stderr, "unit_ai_contact: teach-aztec status '%s'\n", st_pop);
        return fail("Teach Aztec should set Natives-teach-Aztec status");
      }
      /* Restore tribe nation for later arms. */
      col1.tribe[0].nation_id = 4;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 10;
    }

    /*
     * Convert success (mission establish): status + CONTACT_CONVERT OK.
     * Cite: FUN_5bfb_022e convert pulse; human chrome already enqueues.
     */
    {
      ai_popup_clear(&pop);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &units.units[ui];
        if (u->active && (u->nation_id == 4 || u->nation_id == 0)) {
          units_despawn(&units, u->id);
        }
      }
      units.type_count = 3;
      snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
      units.types[2].movement = 1;
      units.types[2].attack = 0;
      units.types[2].defense = 1;
      const int bm = units_spawn_allow_stack(&units, 0, 5, 5);
      const int mm = units_spawn_allow_stack(&units, 2, 6, 5);
      ColonizeUnit* bravem = units_get(&units, bm);
      ColonizeUnit* missm = units_get(&units, mm);
      if (!bravem || !missm) {
        return fail("convert OK spawn");
      }
      bravem->nation_id = 4;
      missm->nation_id = 0;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 10;
      col1.tribe[0].nation_id = 4;
      col1.tribe[0].x = 5;
      col1.tribe[0].y = 5;
      col1.tribe[0].alarm[0].friction = 10;
      col1.tribe[0].mission = 0xff;
      col1.tribe[0].state.learned = 1;
      col1.nation[0].relation_by_indian[0] = 80;
      const uint16_t crosses_m = col1.nation[0].current_crosses;
      st_pop[0] = '\0';
      ai_contact_indian_meet_trade(&ctx, 4);
      if (col1.tribe[0].mission != 0) {
        return fail("convert OK should establish tribe.mission");
      }
      if (col1.nation[0].current_crosses != (uint16_t)(crosses_m + 1)) {
        return fail("convert OK should bump crosses");
      }
      if (pop.queue_count < 1 ||
          pop.queue[pop.queue_count - 1].kind != AI_POPUP_KIND_OK ||
          pop.queue[pop.queue_count - 1].tag != AI_POPUP_TAG_CONTACT_CONVERT) {
        return fail("convert success should enqueue CONTACT_CONVERT OK");
      }
      if (strstr(st_pop, "accept") == NULL || strstr(st_pop, "conversion") == NULL) {
        fprintf(stderr, "unit_ai_contact: convert OK status '%s'\n", st_pop);
        return fail("convert success should set accept-conversion status");
      }
    }

    /* Village-enter Meet CHOICE: already-met human on tribe → Trade…Leave. */
    {
      ai_popup_clear(&pop);
      ind->euro_diplo[0] = 1;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] | 0x40);
      col1.nation[0].relation_by_indian[0] = 100;
      ind->alarm_by_player[0] = 10;
      st_pop[0] = '\0';
      if (!ai_contact_try_village_meet(&ctx, 0, 4)) {
        return fail("village meet should enqueue for already-met human");
      }
      if (pop.queue_count < 1 || pop.queue[0].tag != AI_POPUP_TAG_CONTACT_MEET ||
          pop.queue[0].kind != AI_POPUP_KIND_CHOICE) {
        return fail("village meet should enqueue CONTACT_MEET CHOICE");
      }
      if (pop.queue[0].choice_count < 5) {
        return fail("village Meet CHOICE should offer Trade/Gift/Demand/Teach/Leave");
      }
      if (strstr(st_pop, "welcomes") == NULL || strstr(st_pop, "worthy") == NULL) {
        fprintf(stderr, "unit_ai_contact: village status '%s'\n", st_pop);
        return fail("cool village meet should set @INDIANHELLO1 worthy status");
      }
      /* Hot mid alarm → ruthless HELLO2. */
      ai_popup_clear(&pop);
      ind->alarm_by_player[0] = 45;
      st_pop[0] = '\0';
      if (!ai_contact_try_village_meet(&ctx, 0, 4)) {
        return fail("hot village meet should still enqueue");
      }
      if (strstr(st_pop, "ruthless") == NULL) {
        fprintf(stderr, "unit_ai_contact: hot village status '%s'\n", st_pop);
        return fail("hot village meet should set @INDIANHELLO2 ruthless status");
      }
      ind->alarm_by_player[0] = 10;
      /* Unmet must not use village meet (WELCOME path). */
      ai_popup_clear(&pop);
      ind->euro_diplo[0] = 0;
      if (ai_contact_try_village_meet(&ctx, 0, 4)) {
        return fail("unmet village must not enqueue Meet CHOICE");
      }
      /* Restore met for leftover arms. */
      ind->euro_diplo[0] = 1;
      ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] | 0x40);
    }

    ctx.ai_popups = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Capital-destroy surrender (fandom): reset alarm/friction + peace toward
   * attacker. Cite: docs/fandom_col1994.md Capital destroy; ai_contact_indian_capital_surrender.
   */
  {
    ind->alarm_by_player[0] = 90;
    col1.tribe[0].alarm[0].friction = 80;
    col1.tribe[0].alarm[0].attacks = 5;
    col1.tribe[0].nation_id = 4;
    ind->euro_diplo[0] = (uint8_t)(ind->euro_diplo[0] & ~0x40u);
    col1.nation[0].relation_by_indian[0] = 20;
    ai_contact_indian_capital_surrender(&col1, 4, 0);
    if (ind->alarm_by_player[0] != 0 || col1.tribe[0].alarm[0].friction != 0) {
      return fail("capital surrender should clear alarm/friction");
    }
    if (col1.tribe[0].alarm[0].attacks != 0) {
      return fail("capital surrender should clear attack counter");
    }
    if (!ai_contact_indian_has_peace(&col1, 4, 0)) {
      return fail("capital surrender should set peace bit");
    }
    if (ai_diplo_indian_relation(&col1, 4, 0) < 96) {
      return fail("capital surrender should floor relation to peaceful");
    }
    /* Remaining villages must not become / stay capital (fandom). */
    col1.tribe[0].state.capital = 1;
    ai_contact_indian_capital_surrender(&col1, 4, 0);
    if (col1.tribe[0].state.capital != 0) {
      return fail("capital surrender should clear capital bit on remaining tribes");
    }
  }

  /*
   * Series U: 4528 human village raid warn CHOICE (Attack/Leave) + open
   * hostilities. Cite: indian_settlement_4528.md; ai_contact_try_village_raid_warn.
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
    ind->euro_diplo[0] = (uint8_t)(COL1_INDIAN_MET_BIT | 0x40u);
    col1.nation[0].relation_by_indian[0] = 60;
    ind->alarm_by_player[0] = 10;
    col1.tribe[0].alarm[0].friction = 10;
    ai_popup_clear(&pop);
    if (!ai_contact_try_village_raid_warn(&ctx, 0, 4, 7 /* unit id */, 5, 5)) {
      return fail("village raid warn should enqueue Attack/Leave");
    }
    if (pop.queue_count < 1 || pop.queue[0].tag != AI_POPUP_TAG_CONTACT_VILLAGE_WARN) {
      return fail("village warn tag missing");
    }
    if (pop.queue[0].choice_count < 2) {
      return fail("village warn should offer Leave/Attack");
    }
    if (pop.queue[0].nation_a != 7 || pop.queue[0].nation_b != 4) {
      return fail("village warn should store unit_id / indian nation");
    }
    if ((pop.queue[0].payload & 0xff) != 5 || ((pop.queue[0].payload >> 8) & 0xff) != 5) {
      return fail("village warn should pack dest xy");
    }
    /* Leave path: no hostilities. */
    ai_contact_village_open_hostilities(&ctx, 4, 0);
    if (ai_contact_indian_has_peace(&col1, 4, 0)) {
      return fail("Attack hostilities should clear peace");
    }
    if (!ai_diplo_indian_at_war(&col1, 0, 0)) {
      return fail("Attack hostilities should put Indian at war");
    }
    if (ind->alarm_by_player[0] < 80u) {
      return fail("Attack hostilities should raise alarm to burn band");
    }
    fprintf(stderr, "unit_ai_contact: village 4528 raid warn ok\n");
    ctx.ai_popups = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Series T: 4528 ship-village mid-relation wary band.
   * rel 60 → wary INFO + Meet CHOICE; rel 80 → MADAT (no wary); friction≥64 mad.
   * Cite: indian_settlement_4528.md; ai_contact_try_ship_village.
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
    col1.nation[0].relation_by_indian[0] = 60; /* mid band */
    ind->alarm_by_player[0] = 10;
    st_ship[0] = '\0';
    ai_popup_clear(&pop);
    if (!ai_contact_try_ship_village(&ctx, 0, 5, 5)) {
      return fail("ship mid-band should handle village tile");
    }
    int wary_ok = 0;
    int meet_ok = 0;
    for (int qi = 0; qi < pop.queue_count; ++qi) {
      if (pop.queue[qi].tag == AI_POPUP_TAG_INFO &&
          pop.queue[qi].body[0] &&
          strstr(pop.queue[qi].body, "wary of ships") != NULL) {
        wary_ok = 1;
      }
      if (pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_MEET &&
          pop.queue[qi].kind == AI_POPUP_KIND_CHOICE) {
        meet_ok = 1;
      }
    }
    if (!wary_ok) {
      return fail("ship mid-band should enqueue wary INFO");
    }
    if (!meet_ok) {
      return fail("ship mid-band should still enqueue Meet CHOICE");
    }

    /* Relation 80 (≥75) → MADAT; no wary. */
    ai_popup_clear(&pop);
    st_ship[0] = '\0';
    col1.nation[0].relation_by_indian[0] = 80;
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
    col1.nation[0].relation_by_indian[0] = 60;
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
    col1.head.unknown46[0] = 1; /* WoI declared (AI_KING_WOI_BYTE stand-in) */
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
    for (int i = 0; i < 60 && !(saw_defect && saw_no_defect); ++i) {
      memset(ind, 0, sizeof(*ind));
      ind->tech = 15;
      ind->muskets = 10;
      ind->horse_herds = 8;
      /* relation_by_indian is indexed [euro_nation].[indian_idx]; human=0,
       * crown fold=1 for human=0, tribe nation_id=4 → indian_idx=0. */
      col1.nation[0].relation_by_indian[0] = 80;
      col1.nation[1].relation_by_indian[0] = 50; /* crown fold for human=0 */
      status_woi[0] = '\0';
      ai_contact_indian_woi_defect(&ctx, 4);
      if (ind->woi_defect_resolved) {
        if (col1.nation[0].relation_by_indian[0] != 180) {
          return fail("WoI defect hit should add +100 relation vs human");
        }
        if (col1.nation[1].relation_by_indian[0] != 0) {
          fprintf(
            stderr,
            "unit_ai_contact: WoI crown relation=%u want 0\n",
            (unsigned)col1.nation[1].relation_by_indian[0]
          );
          return fail("WoI defect hit should floor relation vs crown at 0");
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
        if (col1.nation[0].relation_by_indian[0] != 80 ||
            col1.nation[1].relation_by_indian[0] != 50) {
          return fail("WoI defect miss should leave relation untouched");
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
    col1.nation[0].relation_by_indian[0] = 80;
    ai_contact_indian_woi_defect(&ctx, 4);
    if (col1.nation[0].relation_by_indian[0] != 80 || ind->muskets != 10) {
      return fail("WoI defect should skip an already-resolved tribe");
    }

    /* No WoI: must not touch state even on a hit-shaped seed. */
    col1.head.unknown46[0] = 0;
    dos_rng_seed(&woi_rng, 1u);
    memset(ind, 0, sizeof(*ind));
    ind->tech = 15;
    ind->muskets = 10;
    ind->horse_herds = 8;
    col1.nation[0].relation_by_indian[0] = 80;
    ai_contact_indian_woi_defect(&ctx, 4);
    if (ind->woi_defect_resolved || col1.nation[0].relation_by_indian[0] != 80) {
      return fail("WoI defect should no-op before independence is declared");
    }
    col1.head.unknown46[0] = 0;
    ctx.rng = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  col1_save_free(&col1);
  fprintf(stderr, "unit_ai_contact: ok (last_raid_kind=%d)\n", ai_contact_last_raid_kind());
  return 0;
}
