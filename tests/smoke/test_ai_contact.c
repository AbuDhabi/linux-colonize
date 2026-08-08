/* Smoke: Indian meet + friction raid loot (@RAID* kinds) + prelude encroachment. */
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/founding_fathers.h"
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

  /* Meet: adjacent Euro → met_by_player + relation bump; peaceful friction decay. */
  const uint8_t rel0 = col1.nation[0].relation_by_indian[0];
  ai_contact_indian_meet_trade(&ctx, 4);
  if (!ind->met_by_player[0]) {
    return fail("meet should set met_by_player[0]");
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
      fprintf(stderr, "smoke_ai_contact: mission-burn status '%s'\n", status_burn);
      return fail("prelude mission burn should set status");
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
      fprintf(stderr, "smoke_ai_contact: friction-burn status '%s'\n", status_burn);
      return fail("prelude friction>=80 mission burn should set status");
    }
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
   * Human success status (teach already had success chrome).
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
  col1.nation[0].relation_by_indian[0] = 80; /* above very-low refuse */
  {
    char status_ok[128];
    status_ok[0] = '\0';
    ctx.status = status_ok;
    ctx.status_size = sizeof(status_ok);
    ctx.human_nation = 0;
    const uint16_t crosses0 = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].mission != 0) {
      return fail("missionary convert should set tribe.mission to euro nation");
    }
    if (col1.nation[0].current_crosses != (uint16_t)(crosses0 + 1)) {
      return fail("missionary convert should bump nation current_crosses");
    }
    if (strstr(status_ok, "accept") == NULL || strstr(status_ok, "conversion") == NULL) {
      fprintf(stderr, "smoke_ai_contact: convert-ok status '%s'\n", status_ok);
      return fail("convert success should set accept-conversion status");
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
        fprintf(stderr, "smoke_ai_contact: convert-once status '%s'\n", status_ok);
        return fail("convert-once should skip accept-conversion status");
      }
    }
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Convert pulse gate: foreign mission owner → no steal / no crosses.
   * Alarmed (≥55) → refuse conversion status; no crosses.
   */
  {
    char status_cv[128];
    status_cv[0] = '\0';
    ctx.status = status_cv;
    ctx.status_size = sizeof(status_cv);
    ctx.human_nation = 0;
    col1.tribe[0].mission = 1; /* foreign Euro owns mission */
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    const uint16_t crosses_f = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].mission != 1) {
      return fail("convert must not steal foreign mission");
    }
    if (col1.nation[0].current_crosses != crosses_f) {
      return fail("foreign mission convert should not bump crosses");
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
      fprintf(stderr, "smoke_ai_contact: convert-refuse status '%s'\n", status_cv);
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
      fprintf(stderr, "smoke_ai_contact: mid-plain status '%s'\n", status_mid);
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
    if (col1.tribe[0].mission != 0) {
      return fail("mid-range Jesuit convert should establish mission");
    }
    if (col1.nation[0].current_crosses != (uint16_t)(crosses_m + 1)) {
      return fail("mid-range Jesuit convert should bump crosses");
    }
    if (col1.tribe[0].alarm[0].friction != 38 || ind->alarm_by_player[0] != 38) {
      return fail("mid-range Jesuit convert should decay friction/alarm by 2");
    }
    if (strstr(status_mid, "accept") == NULL) {
      fprintf(stderr, "smoke_ai_contact: mid-jesuit status '%s'\n", status_mid);
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
    if (col1.tribe[0].mission != 0) {
      return fail("Brebeuf plain Missionary mid should establish mission");
    }
    if (col1.nation[0].current_crosses != (uint16_t)(crosses_br + 1)) {
      return fail("Brebeuf mid convert should bump crosses by 1 only");
    }
    if (col1.tribe[0].alarm[0].friction != 38 || ind->alarm_by_player[0] != 38) {
      return fail("Brebeuf mid convert should decay friction/alarm by 2");
    }
    if (strstr(status_br, "accept") == NULL) {
      fprintf(stderr, "smoke_ai_contact: Brebeuf mid status '%s'\n", status_br);
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
    ind->met_by_player[0] = 1;
    col1.nation[0].gold = 0; /* no gift overwrite */
    col1.nation[0].relation_by_indian[0] = 80;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (euro->profession != UNITS_JOB_NONE) {
      return fail("already-learned should not re-teach profession");
    }
    if (strstr(status_al, "teach") != NULL) {
      fprintf(stderr, "smoke_ai_contact: already-learned status '%s'\n", status_al);
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
      fprintf(stderr, "smoke_ai_contact: teach-ok status '%s'\n", status_tch);
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
    ai_contact_indian_meet_trade(&ctx, 4);
    scout_t = units_get(&units, scout_teach_id);
    if (!col1.tribe[0].state.learned) {
      return fail("teach-skill Scout should set tribe.state.learned");
    }
    if (!scout_t || scout_t->profession != UNITS_JOB_SCOUT) {
      return fail("teach-skill Scout → Seasoned Scout profession");
    }
    if (strstr(status_sc, "Seasoned Scout") == NULL) {
      fprintf(stderr, "smoke_ai_contact: scout-teach status '%s'\n", status_sc);
      return fail("Scout teach should set Seasoned Scout status");
    }
    units_despawn(&units, scout_teach_id);
    euro->x = 6;
    euro->y = 5;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Mid-alarm teach refuse (40..54): Free Colonist at tribe → no learned;
   * status "Natives refuse to teach." Cite: indian_contact.md mid refuse.
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
    ind->met_by_player[0] = 1;
    ind->alarm_by_player[0] = 20; /* mid via tribe friction */
    col1.nation[0].gold = 0;
    col1.nation[0].relation_by_indian[0] = 80;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].state.learned) {
      return fail("mid-alarm teach refuse should not set learned");
    }
    if (strstr(status_mt, "refuse") == NULL || strstr(status_mt, "teach") == NULL) {
      fprintf(stderr, "smoke_ai_contact: mid-teach status '%s'\n", status_mt);
      return fail("mid-alarm teach should set refuse-to-teach status");
    }
    col1.tribe[0].alarm[0].friction = 10;
    ind->alarm_by_player[0] = 10;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * Gift stand-in (AI Euro silent path via Brave adjacency; humans do not
   * auto-gift on meet pulse — village dialog PARKED):
   * low friction + gold >= 20 → Euro −10 gold, friction −2.
   */
  col1.tribe[0].nation_id = 4;
  ind->met_by_player[0] = 1;
  ind->alarm_by_player[0] = 10;
  col1.tribe[0].alarm[0].friction = 10;
  col1.tribe[0].state.learned = 1;
  col1.nation[0].gold = 50;
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
    return fail("gift should cost Euro 10 gold");
  }
  if (col1.tribe[0].alarm[0].friction != 8) {
    return fail("gift should reduce tribe friction by 2");
  }
  if (ind->alarm_by_player[0] != 8) {
    return fail("gift should reduce alarm_by_player by 2");
  }
  if (status[0] != '\0') {
    return fail("AI gift stand-in should not set human chrome status");
  }

  /*
   * Gift refuse when Euro gold < 10: no gold change. Human adjacency: no
   * spontaneous chrome (village dialog PARKED).
   */
  {
    ind->met_by_player[0] = 1;
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
  ind->met_by_player[0] = 0;
  ind->unknown33[0] = 0;
  col1.nation[0].gold = 15; /* ≥10 no refuse; <20 no gift drain */
  col1.nation[0].relation_by_indian[0] = 0;
  status[0] = '\0';
  ai_contact_indian_meet_trade(&ctx, 4);
  if (!ind->met_by_player[0]) {
    return fail("meet should set met_by_player for status path");
  }
  if (strstr(status, "peace") == NULL && strstr(status, "Peace") == NULL &&
      strstr(status, "visit") == NULL && strstr(status, "friendship") == NULL) {
    fprintf(stderr, "smoke_ai_contact: first-meet status '%s'\n", status);
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
     * Secondary −5 muskets; STORES value-sort may also primary-drain 1 musket
     * (FUN_5fef_016c) → −6 total. Cite: indian_raid_outcomes.md multi-loot.
     */
    const int musk_expect =
      (kind_ml == AI_RAID_STORES) ? (muskets_ml - 6) : (muskets_ml - 5);
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
   * → AI_RAID_STORES and muskets stock −1. Cite: indian_raid_outcomes.md
   * @RAIDSTORES; GAME.TXT tag.
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
    memset(c->stock, 0, sizeof(c->stock));
    c->stock[COLONIZE_CARGO_MUSKETS] = 3; /* STORES prefs hit muskets; <5 → no −5 secondary */
    const int musk_st = c->stock[COLONIZE_CARGO_MUSKETS];
    ai_contact_indian_raids(&ctx, 4);
    if (ai_contact_last_raid_kind() != AI_RAID_STORES) {
      return fail("muskets-only warehouse should pick AI_RAID_STORES");
    }
    if (c->stock[COLONIZE_CARGO_MUSKETS] != musk_st - 1) {
      return fail("STORES primary should drain 1 muskets stock");
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
  if (strstr(status, "Scout warned") == NULL && strstr(status, "village") == NULL) {
    return fail("359c displace should set status warn line");
  }

  /*
   * Thin alarmed refuse-talk: human Brave adjacency must not spam refuse chrome
   * (village dialog PARKED). No gold drain either.
   */
  {
    col1.nation[0].gold = 100;
    ind->met_by_player[0] = 1;
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
    ind->met_by_player[0] = 1;
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
   * alarm → no state.learned; human status "Natives refuse to teach."
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
    ind->met_by_player[0] = 1;
    ind->alarm_by_player[0] = 60;
    status[0] = '\0';
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].state.learned) {
      return fail("alarmed teach refuse should not set tribe.state.learned");
    }
    if (strstr(status, "refuse") == NULL || strstr(status, "teach") == NULL) {
      fprintf(stderr, "smoke_ai_contact: teach-refuse status '%s'\n", status);
      return fail("alarmed teach should set refuse-to-teach status");
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
   * ("Native raiding party wiped out."). Cite: GAME.TXT @RAIDNOTHING.
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
    if (strstr(status, "wiped") == NULL) {
      fprintf(stderr, "smoke_ai_contact: NOTHING status '%s'\n", status);
      return fail("empty raid NOTHING should set wiped-out status");
    }
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
    ind->met_by_player[0] = 1;
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
    ind->met_by_player[0] = 1;
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
    ind->met_by_player[0] = 1;
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
    ind->met_by_player[0] = 1;
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
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 60;
    ind->alarm_by_player[0] = 60;
    col1.nation[0].relation_by_indian[0] = 80;
    const int mx0 = flee_m->x;
    const int my0 = flee_m->y;
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
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 0;
    ind->met_by_player[0] = 1;
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
        "smoke_ai_contact: escalate alarm=%u (want 17)\n",
        (unsigned)ind->alarm_by_player[0]
      );
      return fail("prelude escalate should bump alarm by 7 at difficulty 2");
    }

    /* Same seed path with Pocahontas → half bump (+7 → +3). */
    ind->unknown31_flags = (uint8_t)(ind->unknown31_flags & (uint8_t)~0x20);
    ind->alarm_by_player[0] = 10;
    col1.head.founding_father[FF_POCAHONTAS] = 0;
    ai_contact_indian_prelude(&ctx, 4);
    if (ind->alarm_by_player[0] != 13) { /* 10 + 3 */
      fprintf(
        stderr,
        "smoke_ai_contact: poca escalate alarm=%u (want 13)\n",
        (unsigned)ind->alarm_by_player[0]
      );
      return fail("Pocahontas should halve prelude escalate bump to +3");
    }
    col1.head.founding_father[FF_POCAHONTAS] = -1;
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
    memset(c_burn->stock, 0, sizeof(c_burn->stock));
    c_burn->stock[COLONIZE_CARGO_LUMBER] = 6;
    colonies.colony_count = 1;
    const int lumber0 = c_burn->stock[COLONIZE_CARGO_LUMBER];
    ai_contact_indian_raids(&ctx, 4);
    if (ai_contact_last_raid_kind() != AI_RAID_BURN) {
      fprintf(
        stderr,
        "smoke_ai_contact: burn-lumber kind=%d\n",
        ai_contact_last_raid_kind()
      );
      return fail("lumber-only colony at alarm≥60 should pick AI_RAID_BURN");
    }
    if (c_burn->stock[COLONIZE_CARGO_LUMBER] >= lumber0) {
      return fail("BURN should drain lumber stock when no construction");
    }
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
      fprintf(stderr, "smoke_ai_contact: burn-building kind=%d\n",
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
      c_bd->has_building[1] = true;
      ai_contact_indian_raids(&ctx, 4);
      if (ai_contact_last_raid_kind() != AI_RAID_BURN || c_bd->has_building[1]) {
        return fail("BURN status probe needs building destroy");
      }
      if (strstr(status_burn_bd, "Warehouse") == NULL ||
          strstr(status_burn_bd, "burn") == NULL) {
        fprintf(stderr, "smoke_ai_contact: burn-building status '%s'\n",
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
      fprintf(stderr, "smoke_ai_contact: escort orders=%d follow=%d\n", brave->orders,
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
      fprintf(stderr, "smoke_ai_contact: escort deepen follow=%d (want %d)\n",
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
      fprintf(stderr, "smoke_ai_contact: escort fallback follow=%d (want %d)\n",
              brave ? brave->follow_unit_id : -1, near2);
      return fail("escort without gate should pick nearest lead");
    }
    units_despawn(&units, near2);
    units_despawn(&units, far2);
    brave->orders = UNITS_ORDER_NONE;
    brave->follow_unit_id = -1;
  }

  /*
   * Raid friction/alarm escalate: successful loot → tribe friction +
   * alarm_by_player +2; Pocahontas halves (+1). Cite: fandom Alarm /
   * Pocahontas; indian_raid_outcomes.md §7.
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
    if (ai_contact_last_raid_kind() == AI_RAID_NOTHING) {
      return fail("raid friction escalate needs successful loot kind");
    }
    if (col1.tribe[0].alarm[0].friction != 52) {
      fprintf(
        stderr,
        "smoke_ai_contact: raid friction=%u (want 52)\n",
        (unsigned)col1.tribe[0].alarm[0].friction
      );
      return fail("successful raid should bump tribe friction by 2");
    }
    if (ind->alarm_by_player[0] != 52) {
      fprintf(
        stderr,
        "smoke_ai_contact: raid alarm=%u (want 52)\n",
        (unsigned)ind->alarm_by_player[0]
      );
      return fail("successful raid should bump alarm_by_player by 2");
    }

    /* Same path with Pocahontas → half bump (+2 → +1). */
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
    if (ai_contact_last_raid_kind() == AI_RAID_NOTHING) {
      return fail("Pocahontas raid escalate needs successful loot kind");
    }
    if (col1.tribe[0].alarm[0].friction != 51) {
      fprintf(
        stderr,
        "smoke_ai_contact: poca raid friction=%u (want 51)\n",
        (unsigned)col1.tribe[0].alarm[0].friction
      );
      return fail("Pocahontas should halve raid friction bump to +1");
    }
    if (ind->alarm_by_player[0] != 51) {
      fprintf(
        stderr,
        "smoke_ai_contact: poca raid alarm=%u (want 51)\n",
        (unsigned)ind->alarm_by_player[0]
      );
      return fail("Pocahontas should halve raid alarm bump to +1");
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
    ind->met_by_player[0] = 0;
    ind->alarm_by_player[0] = 0;
    ind->unknown33[0] = 0;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].x = 5;
    col1.tribe[0].y = 5;
    col1.tribe[0].alarm[0].friction = 0;
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
    if (!ind->met_by_player[0]) {
      return fail("popup meet should set met_by_player");
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
      ind->met_by_player[0] = 0;
      ind->unknown33[0] = 0;
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
      if (ind->met_by_player[0]) {
        return fail("ship adjacency must not set met_by_player");
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
    ind->met_by_player[0] = 1;
    ind->unknown33[0] = 0x40;
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
      fprintf(stderr, "smoke_ai_contact: trade status '%s'\n", st_pop);
      return fail("Trade apply should set Trade accepted status");
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
      fprintf(stderr, "smoke_ai_contact: leave status '%s'\n", st_pop);
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
        fprintf(stderr, "smoke_ai_contact: trade-stub status '%s'\n", st_pop);
        return fail("Trade fail should set Trade concluded status");
      }
    }

    /*
     * Trade CHOICE haggle refuse (alarm≥50 gate): OK "Natives refuse to trade."
     * Cite: FUN_4d56_2aac refuse; fandom Alarm; deep 2820 PARKED.
     */
    {
      ai_popup_clear(&pop);
      c_pop->stock[COLONIZE_CARGO_TRADE_GOODS] = 5;
      ind->alarm_by_player[0] = 55;
      col1.nation[0].relation_by_indian[0] = 80;
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
      if (strstr(st_pop, "refuse") == NULL || strstr(st_pop, "trade") == NULL) {
        fprintf(stderr, "smoke_ai_contact: trade-refuse status '%s'\n", st_pop);
        return fail("Trade refuse should set refuse-to-trade status");
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
      ind->met_by_player[0] = 0;
      ind->alarm_by_player[0] = 0;
      ind->unknown33[0] = 0;
      col1.tribe[0].nation_id = 4;
      col1.tribe[0].alarm[0].friction = 0;
      col1.tribe[0].state.learned = 1;
      col1.tribe[0].mission = 0xff;
      col1.nation[0].relation_by_indian[0] = 0;
      st_pop[0] = '\0';
      ai_contact_indian_meet_trade(&ctx, 4);
      if (!ind->met_by_player[0]) {
        return fail("second-brave meet should set met_by_player");
      }
      int welcome_choices = 0;
      for (int qi = 0; qi < pop.queue_count; ++qi) {
        if (pop.queue[qi].kind == AI_POPUP_KIND_CHOICE &&
            pop.queue[qi].tag == AI_POPUP_TAG_CONTACT_WELCOME) {
          welcome_choices++;
        }
      }
      if (welcome_choices != 1) {
        fprintf(stderr, "smoke_ai_contact: WELCOME CHOICE count=%d\n", welcome_choices);
        return fail("second Brave same pulse must not re-offer WELCOME");
      }
    }

    /*
     * WELCOME No → @INDIANSHUN + at war. Cite: FUN_5bfb_022e reject.
     */
    {
      ai_popup_clear(&pop);
      ind->met_by_player[0] = 0;
      ind->unknown33[0] = 0;
      ind->alarm_by_player[0] = 0;
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
      if (strstr(st_pop, "WAR") == NULL && strstr(st_pop, "War") == NULL) {
        fprintf(stderr, "smoke_ai_contact: reject status '%s'\n", st_pop);
        return fail("WELCOME No should set Prepare for WAR status");
      }
    }

    /*
     * After Accept, second adjacency must not refuse solely due to relation==0.
     */
    {
      ai_popup_clear(&pop);
      ind->met_by_player[0] = 1;
      ind->unknown33[0] = 0x40; /* peace */
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
      ind->met_by_player[0] = 1;
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
      if (pop.queue[pop.queue_count - 1].choice_count < 2) {
        return fail("amount CHOICE should offer Small and Large");
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
      ind->met_by_player[0] = 1;
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
      ind->met_by_player[0] = 1;
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
        fprintf(stderr, "smoke_ai_contact: demand-refuse OK status '%s'\n", st_pop);
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
      ind->met_by_player[0] = 1;
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
      if (strstr(st_pop, "refuse") == NULL || strstr(st_pop, "teach") == NULL) {
        fprintf(stderr, "smoke_ai_contact: teach-refuse OK status '%s'\n", st_pop);
        return fail("Teach refuse should set refuse-to-teach status");
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
      aztec->met_by_player[0] = 1;
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
        fprintf(stderr, "smoke_ai_contact: teach-aztec status '%s'\n", st_pop);
        return fail("Teach Aztec should set Natives-teach-Aztec status");
      }
      /* Restore tribe nation for later arms. */
      col1.tribe[0].nation_id = 4;
      ind->met_by_player[0] = 1;
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
      ind->met_by_player[0] = 1;
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
        fprintf(stderr, "smoke_ai_contact: convert OK status '%s'\n", st_pop);
        return fail("convert success should set accept-conversion status");
      }
    }

    ctx.ai_popups = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  col1_save_free(&col1);
  fprintf(stderr, "smoke_ai_contact: ok (last_raid_kind=%d)\n", ai_contact_last_raid_kind());
  return 0;
}
