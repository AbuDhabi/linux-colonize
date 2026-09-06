/* Smoke: Indian meet + friction raid loot (@RAID* kinds) + prelude encroachment. */
#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/colony_production.h"
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
  units.types[0].movement = 1; /* NAMES @UNIT; DOS max MP = 3 thirds */
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
  ind->alarm_by_player[0] = 33; /* first contact clamps alarm <= 20 (FUN_5bfb :96624) */
  ai_contact_indian_meet_trade(&ctx, 4);
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
  const uint8_t rel_pre_raid = ai_diplo_indian_relation(&col1, 4 + (0), 0);
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
      ai_diplo_indian_relation(&col1, 4 + (0), 0) >= rel_pre_raid) {
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
   * Retired 2026-09-03 (bugs.md "alarm rises incredibly fast"): the fandom
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

  /*
   * Missionary convert pulse: adjacent Missionary + non-hostile →
   * tribe.mission = euro id and nation current_crosses++.
   *
   * bugs.md: this pulse is the *AI's* stand-in for working a missionary. The
   * human establishes a mission through the @ACTIONS village menu, so these
   * blocks run with the human elsewhere (nation 2) and nation 0 as an AI, and
   * the human-only case is asserted inert at the end of the block.
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
  col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */ /* above very-low refuse */
  {
    char status_ok[128];
    status_ok[0] = '\0';
    ctx.status = status_ok;
    ctx.status_size = sizeof(status_ok);
    ctx.human_nation = 2;
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
    /*
     * bugs.md: with nation 0 as the human, the same adjacency must do nothing
     * at all — no mission, no crosses, no popup. The player's own missionary
     * only acts through the village @ACTIONS menu.
     */
    {
      col1.tribe[0].mission = 0xff;
      const uint16_t crosses_h = col1.nation[0].current_crosses;
      status_ok[0] = '\0';
      ctx.human_nation = 0;
      ai_contact_indian_meet_trade(&ctx, 4);
      if (col1.tribe[0].mission != 0xff || col1.nation[0].current_crosses != crosses_h ||
          status_ok[0] != '\0') {
        fprintf(stderr, "unit_ai_contact: human auto-convert status '%s'\n", status_ok);
        return fail("a human missionary must not auto-establish a mission by adjacency");
      }
      col1.tribe[0].mission = 0;
      ctx.human_nation = 2;
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
    ctx.human_nation = 2; /* nation 0 acts as an AI here — see the pulse note above */
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
      ctx.human_nation = 2;
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
    ctx.human_nation = 2;
    /* Plain Missionary mid-alarm → refuse (not Jesuit-grade). */
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Missionary");
    miss->x = 6;
    miss->y = 5;
    miss->active = true;
    miss->profession = UNITS_JOB_NONE;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].alarm[0].friction = 40;
    ind->alarm_by_player[0] = 40;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */
    const uint16_t crosses_plain = col1.nation[0].current_crosses;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].mission != 0xff) {
      return fail("plain Missionary mid-alarm should not establish mission");
    }
    if (col1.nation[0].current_crosses != crosses_plain) {
      return fail("plain Missionary mid-alarm should not bump crosses");
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */
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
    ctx.human_nation = 2;
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */
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
   * bugs.md 2026-09-04 (bugs 2 and 3): there is NO passive teach pulse.
   * DOS's Brave-side contact FUN_5bfb_022e (viceroy_unpacked_2.c 87455-88030)
   * has no teach arm at all - its arms are first contact, demand/beg-food,
   * gift (@INDIANGIVEFOOD/@INDIANGIVESTUFF), mission convert and raid.
   * Teaching is reached only through the deliberate "Live Among The Natives"
   * @ACTIONS row (FUN_4d56_4528 dispatch case 5 -> thunk_FUN_1000_a618).
   * So a Brave wandering past a colonist must never set tribe.state.learned,
   * never hand out a profession, and never emit a teach / @LEARN* line.
   */
  units_despawn(&units, miss_id);
  {
    char status_np[128];
    status_np[0] = '\0';
    ctx.status = status_np;
    ctx.status_size = sizeof(status_np);
    ctx.human_nation = 0;
    euro->x = 6;
    euro->y = 5;
    euro->active = true;
    euro->profession = UNITS_JOB_NONE;
    col1.tribe[0].nation_id = 4;
    col1.tribe[0].state.learned = 0;
    col1.tribe[0].state.capital = 0;
    col1.tribe[0].mission = 0xff;
    col1.tribe[0].last_sold = (uint8_t)COLONIZE_CARGO_FURS;
    col1.tribe[0].alarm[0].friction = 5;
    ind->euro_diplo[0] = 1;
    ind->alarm_by_player[0] = 5;
    col1.nation[0].gold = 0; /* no gift arm */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    ai_contact_indian_meet_trade(&ctx, 4);
    if (col1.tribe[0].state.learned) {
      return fail("Indian move pulse must not teach (5bfb_022e has no teach arm)");
    }
    if (euro->profession != UNITS_JOB_NONE) {
      return fail("Indian move pulse must not hand out a profession");
    }
    if (strstr(status_np, "teach") != NULL || strstr(status_np, "infuriate") != NULL) {
      fprintf(stderr, "unit_ai_contact: passive-teach status '%s'\n", status_np);
      return fail("Indian move pulse must not emit teach / @LEARNMAD chrome");
    }
    col1.tribe[0].last_sold = 0;
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
/* alarm pinned above (was relation write) */
  col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
  col1.indian[0].euro_diplo[0] = (uint8_t)(col1.indian[0].euro_diplo[0] & ~COL1_INDIAN_MET_BIT); /* unmet (was relation 0) */
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
    ind->alarm_by_player[0] = 76; /* at war (DOS band: alarm > 0x4a), below the 80 burn band */
    col1.tribe[0].alarm[0].friction = 50;
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
    brave->moves_left = 3;
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
    c->stock[COLONIZE_CARGO_MUSKETS] = 3;
    status[0] = '\0';
    ctx.status = status;
    ctx.status_size = sizeof(status);
    ctx.human_nation = 0;
    ColonizeMsgCatalog game_txt_raid;
    assets_msg_init(&game_txt_raid);
    (void)assets_msg_load_file(&game_txt_raid, "COLONIZE/GAME.TXT");
    ctx.messages = &game_txt_raid;
    ai_contact_indian_raids(&ctx, 4);
    ctx.messages = NULL;
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
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
   * Alarmed Brave next to a Free Colonist: still no teach, and (bugs.md
   * 2026-09-04 bug 2) no unprompted @LEARNMAD lecture either — the refusal
   * dialogs belong to the deliberate Live-Among-The-Natives action, not to
   * the Indian move pulse, which has no teach arm in DOS at all.
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
    if (strstr(status, "infuriate") != NULL) {
      fprintf(stderr, "unit_ai_contact: teach-refuse status '%s'\n", status);
      return fail("Indian move pulse must not lecture @LEARNMAD unprompted");
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
      brave->moves_left = 3;
      brave->x = 5;
      brave->y = 5;
      ai_contact_indian_raids(&ctx, 4);
      if (ai_contact_last_raid_kind() != AI_RAID_NOTHING) {
        break;
      }
      turn++; /* the pulse's local RNG is keyed on the turn — advance it */
    }
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 30; alarm pinned above) */
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 80; alarm pinned above) */
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
    /* Equal tribe friction; nation-level alarm breaks the tie (single store). */
    ind->alarm_by_player[0] = 10; /* less hostile (relation 90) */
    ind->alarm_by_player[1] = 80; /* prefer this target (relation 20) */
    col1.tribe[0].alarm[0].friction = 50;
    col1.tribe[0].alarm[1].friction = 50;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.indian[0].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
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
    /* Single store: at-war (thin) = nation alarm > 50; tribe friction is separate. */
    ind->alarm_by_player[0] = 45; /* not at war, higher friction */
    ind->alarm_by_player[1] = 60; /* at war, lower friction */
    col1.tribe[0].alarm[0].friction = 60;
    col1.tribe[0].alarm[1].friction = 45;
    col1.tribe[0].alarm[0].attacks = 0;
    col1.tribe[0].alarm[1].attacks = 0;
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
    col1.indian[0].euro_diplo[1] |= COL1_INDIAN_MET_BIT;
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 40; alarm pinned above) */ /* at-war band */
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met; alarm 10 pinned above */
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
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
      col1.indian[0].euro_diplo[e] |= COL1_INDIAN_MET_BIT; /* met (was relation 100; alarm pinned above) */ /* peaceful baseline */
      col1.nation[e].gold = 0;
    }
    ind->alarm_by_player[1] = 75; /* ≥70 abandon gate targets nation 1 */
    col1.tribe[0].alarm[1].friction = 75;
    col1.indian[0].euro_diplo[1] |= COL1_INDIAN_MET_BIT; /* met (was relation 40; alarm pinned above) */
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
    /* bugs.md 287: the raid pulse only LOOTS — a pop-1 colony survives a
     * BURN raid with its owner intact (destruction lives on the combat
     * path, and only when the last colonist falls there). */
    if (!c_fbrn->active || c_fbrn->nation_id != 1) {
      return fail("raid pulse must not destroy or capture a pop-1 colony (bugs.md 287)");
    }
    for (int qi = 0; qi < pop_fbrn.queue_count; ++qi) {
      if (strstr(pop_fbrn.queue[qi].body, "Spies report") != NULL ||
          strstr(pop_fbrn.queue[qi].body, "overrun") != NULL ||
          strstr(pop_fbrn.queue[qi].body, "march into") != NULL) {
        fprintf(stderr, "unit_ai_contact: raid abandon popup leaked: '%s'\n",
                pop_fbrn.queue[qi].body);
        return fail("raid pulse must not emit abandon/capture chrome (bugs.md 287)");
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
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 50; alarm pinned above) */
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
    brave->moves_left = 3; /* the deepen case spent this turn's 3 thirds */
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 40; alarm pinned above) */
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
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT; /* met (was relation 40; alarm pinned above) */
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
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
    /*
     * 2026-08-22: trade now keys on the contacting UNIT's own cargo hold
     * (LAB_002bbc's real shape), not an invented colony-warehouse search —
     * see ai_contact_auto_trade's header. euro2 is the unit adjacent to
     * brave2, so it carries the TRADE_GOODS this whole section trades.
     */
    euro2->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
    euro2->hold_goods_amount[0] = 5;
    const int goods0 = euro2->hold_goods_amount[0];
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
    if (ind->alarm_by_player[0] > 20 || col1.tribe[0].alarm[0].friction != 0) {
      return fail("WELCOME Yes should clamp alarm <= 20 (FUN_5bfb :96624) and clear friction");
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
        elu->hold_goods_type[0] = COLONIZE_CARGO_TRADE_GOODS;
        elu->hold_goods_amount[0] = goods0;
      }
    }

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
    ColonizeUnit* land0 = units_get(&units, units_id_at(&units, 6, 5));
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
     * ai_contact_auto_trade's own header comment.
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

    /*
     * 2026-08-22: the old "hard bargain" mid-alarm peel (extra trade-good
     * drained, no relation bump) is dropped -- LAB_002bbc's real body has
     * no such tension/resume-loop for the AI-controlled accept branch (a
     * single deterministic decision, see ai_contact_auto_trade's header).
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

    /*
     * Sea trade: a ship (not the land unit) is now the sole contacting
     * unit -- ai_contact_auto_trade keys on the ONE unit
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

    /* Leave dismisses with thin Farewell OK; no trade side effects. */
    ai_popup_clear(&pop);
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 5; /* LEAVE */
    pop.result_tag = AI_POPUP_TAG_CONTACT_MEET;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    st_pop[0] = '\0';
    ai_contact_apply_popup_result(&ctx, &pop);
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
     * Trade CHOICE with no contacting unit/cargo → haggle stub OK
     * "Trade concluded." Cite: FUN_5bfb_022e / 2aac…311e thin; deep 2820
     * PARKED.
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
/* alarm pinned above (was relation write) */
      col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
      col1.indian[0].alarm_by_player[0] = 20; /* relation 80 */
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
      col1.indian[1].alarm_by_player[0] = 20; /* relation 80 */ /* Aztec idx 1 */
      col1.indian[1].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
     * bugs.md / FUN_5bfb_022e @INDIANSCONVERT: the human gets Converts when a
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
      col1.tribe[0].alarm[0].friction = 10;
      col1.tribe[0].alarm[0].attacks = 3;
      col1.tribe[0].mission = 0; /* our mission, plain */
      col1.tribe[0].state.learned = 1;
      ind->euro_diplo[0] = 1;
      ind->alarm_by_player[0] = 10;
      ind->tech = 3;
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
      bravem->home_tribe_id = 0;

      ColonizeDosRng cv_rng;
      dos_rng_seed(&cv_rng, 7u);
      ctx.rng = &cv_rng;
      st_pop[0] = '\0';
      int converts = 0;
      for (int pulse = 0; pulse < 32 && converts == 0; ++pulse) {
        ai_contact_indian_meet_trade(&ctx, 4);
        for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
          const ColonizeUnit* u = &units.units[ui];
          if (u->active && u->nation_id == 0 && u->profession == COLONIZE_PROF_CONVERT &&
              u->x == c->x && u->y == c->y) {
            converts++;
          }
        }
      }
      ctx.rng = NULL;
      if (converts == 0) {
        return fail("mission settlement visit should spawn an Indian Convert in the colony");
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
      dos_rng_seed(&cv_rng, 7u);
      ctx.rng = &cv_rng;
      for (int pulse = 0; pulse < 32; ++pulse) {
        ai_contact_indian_meet_trade(&ctx, 4);
      }
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
/* alarm pinned above (was relation write) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
    col1.indian[0].alarm_by_player[0] = 40; /* relation 60 */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
    col1.indian[0].alarm_by_player[0] = 40; /* mid band (single store) */
    col1.indian[0].euro_diplo[0] |= COL1_INDIAN_MET_BIT;
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
    col1.head.unknown46[0] = 0;
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
    col1.head.unknown46[0] = 0;
    col1.head.game_options.woi = 0;
    ctx.rng = NULL;
    ctx.status = NULL;
    ctx.status_size = 0;
  }

  /*
   * @INDIANBEGFOOD accept/decline (FUN_5bfb_022e already-met adjacency —
   * ai_contact_try_village_beg_food / ai_contact_apply_beg_food). Sign
   * convention re-read off the decompile 2026-08-31 (bugs.md): DOS's
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
     * bugs.md: the offer row is "We offer you {%NUMBER0} of our {%NUMBER1
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
      /* bugs.md: a Brave must actually stand next to the colony to beg. */
      const int beg_brave = units_spawn_allow_stack(&units, 0,
        colonies.colonies[0].x + 1, colonies.colonies[0].y + 1);
      if (beg_brave >= 0) {
        units_get(&units, beg_brave)->nation_id = 4;
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
    pop.result_payload = 0; /* colony index */
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

    ai_popup_clear(&pop);
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* decline/refuse */
    pop.result_tag = AI_POPUP_TAG_CONTACT_BEGFOOD;
    pop.result_nation_a = 0;
    pop.result_nation_b = 4;
    pop.result_payload = 0;
    ai_contact_apply_popup_result(&ctx, &pop);
    if (colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] != 100) {
      return fail("BEGFOOD decline must not cost the colony any food");
    }
    if (col1.tribe[0].alarm[0].friction != 30) {
      return fail("BEGFOOD decline should scale the settlement alarm word by 1.5");
    }
    if (ai_diplo_indian_relation(&col1, 4 + (0), 0) >= rel_before_decline) {
      return fail("BEGFOOD decline should worsen relations");
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

  /*
   * bugs.md 2026-09-04 (bug 5, "Indians never seem to visit bearing gifts"):
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
    col1.head.turn = 5; /* non-zero so the once-per-8-turns cooldown is live */
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
    /* Second call in the same turn is refused (one visit per nation). */
    dos_rng_seed(&gift_rng, 7u);
    ctx.rng = &gift_rng;
    col1.indian[0].contact_state[0] = 0;
    const int again = ai_contact_try_village_gifts(&ctx, 4);
    ctx.rng = saved_rng;
    if (again) {
      return fail("gift visit should not repeat while its cooldown stands");
    }
    units_despawn(&units, gift_brave);
    for (int cg = 0; cg < COLONIZE_CARGO_COUNT; ++cg) {
      colonies.colonies[0].stock[cg] = 0;
    }
    colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100;
    col1.head.turn = saved_turn;
    ctx.ai_popups = NULL;
  }

  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  /* FUN_465b_0000 @WHACKINDIANS: ask once while alarm < 0x4b and bit 0x04 clear. */
  {
    AiPopupState wp;
    ai_popup_clear(&wp);
    ColonizeTurnContext wctx;
    memset(&wctx, 0, sizeof(wctx));
    wctx.col1 = &col1;
    wctx.col1_ok = true;
    wctx.ai_popups = &wp;
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
    /* No treaty: no prompt, war opens and the move may proceed. */
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET);
    if (ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 0) {
      return fail("no treaty: attack must proceed without a prompt");
    }
    if (!ai_diplo_at_war(&col1, 0, 1)) {
      return fail("no treaty: attack should open the war");
    }
    /* Already at war: nothing to ask. */
    if (ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 0) {
      return fail("at war: no prompt");
    }
    /*
     * bugs.md 388: asymmetric bytes. The peer is at peace with us, we are not
     * at peace with them — DOS tests the ATTACKER'S own byte
     * (FUN_281f_0a38(attacker, target) & 0x40), the same byte the Foreign
     * Affairs report prints, so no @HAVETREATY prompt may appear here.
     */
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET | AI_DIPLO_PEACE);
    if (ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 0) {
      return fail("one-sided peer peace must not raise HAVETREATY (report says War)");
    }
    /* Mirror case: our byte carries the treaty, theirs does not → prompt. */
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, AI_DIPLO_MET | AI_DIPLO_PEACE);
    ai_diplo_write(&col1, 1, 0, AI_DIPLO_MET);
    if (!ai_contact_try_euro_attack_confirm(&wctx, 0, 1, 7, 5, 6) || wp.queue_count != 1 ||
        wp.queue[0].tag != AI_POPUP_TAG_CONTACT_EURO_WAR) {
      return fail("our own peace bit must raise HAVETREATY even if theirs is clear");
    }
    ai_popup_clear(&wp);
    ai_diplo_write(&col1, 0, 1, 0);
    ai_diplo_write(&col1, 1, 0, 0);
    fprintf(stderr, "unit_ai_contact: HAVETREATY euro attack confirm ok\n");
  }


  /* FUN_4d56_417e Mode 2: AI Missionary incites the tribe against the human. */
  {
    ColonizeDosRng irng;
    dos_rng_seed(&irng, 5u);
    ColonizeTurnContext ictx;
    memset(&ictx, 0, sizeof(ictx));
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
    col1.player[1].control = 0;
    fprintf(stderr, "unit_ai_contact: 417e Mode 2 auto-incite ok\n");
  }

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
    if (ind->alarm_by_player[1] <= alarm_b) {
      return fail("2e92: purchase raises alarm by price/25+1 (FUN_4cc6_00f2 positive delta)");
    }
    /* Full holds: no purchase. */
    for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
      w->hold_goods_type[i] = 1;
      w->hold_goods_amount[i] = 100;
    }
    if (ai_contact_auto_buy_2e92(&bctx, ind, 4, 1, w)) {
      return fail("2e92: unit without a free hold must not buy");
    }
    units_despawn(&units, wid);
    col1.player[1].control = 0;
    fprintf(stderr, "unit_ai_contact: 2820 LAB_002e92 auto-buy ok\n");
  }

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

  /*
   * NAMES.TXT @ACTIONS village menu + the overlay-13 action thunks
   * (FUN_4d56_4528 human arm; indian_actions_menu.md, static port 2026-08-28).
   */
  {
    ColonizeMsgCatalog game_txt;
    assets_msg_init(&game_txt);
    (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");
    ctx.messages = &game_txt;
    AiPopupState pop;
    ai_popup_init(&pop);
    ctx.ai_popups = &pop;
    char st_menu[AI_POPUP_BODY_LEN];
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
    const int scout_id = units_spawn_allow_stack(&units, 2, 6, 5);
    ColonizeUnit* scout = units_get(&units, scout_id);
    if (!scout) {
      return fail("menu: spawn scout");
    }
    scout->nation_id = 0;
    scout->profession = UNITS_JOB_NONE;
    if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, scout_id)) {
      return fail("menu: scout meet should enqueue");
    }
    if (pop.queue_count != 1 || pop.queue[0].tag != AI_POPUP_TAG_CONTACT_MEET) {
      return fail("menu: scout meet CHOICE");
    }
    {
      const AiPopupRequest* q = &pop.queue[0];
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
      res.result_payload = pop.queue[0].payload;
      ai_popup_clear(&pop);
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

    /* Free Colonist → Live Among The Natives + Cancel only; Live Among → @LEARNSTAY CHOICE. */
    const int col_id = units_spawn_allow_stack(&units, 1, 6, 5);
    ColonizeUnit* colonist = units_get(&units, col_id);
    if (!colonist) {
      return fail("menu: spawn colonist");
    }
    colonist->nation_id = 0;
    colonist->profession = UNITS_JOB_NONE;
    ai_popup_clear(&pop);
    if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, col_id)) {
      return fail("menu: colonist meet should enqueue");
    }
    if (pop.queue[0].choice_count != 2 || pop.queue[0].choice_ids[0] != 4 || pop.queue[0].choice_ids[1] != 5) {
      fprintf(stderr, "unit_ai_contact: colonist rows %d\n", pop.queue[0].choice_count);
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
      res.result_payload = pop.queue[0].payload;
      ai_popup_clear(&pop);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      /* alarm 10 → quartile 0 → no SLOW roll → human gets the @LEARNSTAY Yes/No. */
      if (pop.queue_count != 1 || pop.queue[0].tag != AI_POPUP_TAG_CONTACT_LEARNSTAY ||
          pop.queue[0].kind != AI_POPUP_KIND_CHOICE || pop.queue[0].choice_count != 2) {
        fprintf(stderr, "unit_ai_contact: learnstay queue %d status '%s'\n", pop.queue_count, st_menu);
        return fail("live among: unskilled colonist at peace should get @LEARNSTAY CHOICE");
      }
      if (colonist->profession != UNITS_JOB_NONE || col1.tribe[0].state.learned) {
        return fail("live among: nothing applied before the CHOICE is answered");
      }
      if (strstr(st_menu, "master") == NULL) {
        fprintf(stderr, "unit_ai_contact: learnstay body '%s'\n", st_menu);
        return fail("live among: @LEARNSTAY body should name the master skill");
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
      ans.result_payload = pop.queue[0].payload;
      ai_popup_clear(&pop);
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
      ai_popup_clear(&pop);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, col_id)) {
        return fail("menu: second colonist meet");
      }
      res.result_payload = pop.queue[0].payload;
      ai_popup_clear(&pop);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      if (colonist->profession != UNITS_JOB_NONE || strstr(st_menu, "already shared") == NULL) {
        fprintf(stderr, "unit_ai_contact: learnalready status '%s'\n", st_menu);
        return fail("live among: taught village → @LEARNALREADY");
      }
      col1.tribe[0].state.learned = 0;
      units_despawn(&units, col_id);
    }

    /* bugs.md 294: an Indentured Servant is a learner like a Free Colonist
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
      ai_popup_clear(&pop);
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
      res.result_payload = pop.queue[0].payload;
      ai_popup_clear(&pop);
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
      if (pop.queue_count != 1 || pop.queue[0].tag != AI_POPUP_TAG_CONTACT_LEARNSTAY) {
        fprintf(stderr, "unit_ai_contact: servant queue %d status '%s'\n", pop.queue_count, st_menu);
        return fail("live among: servant at peace should get @LEARNSTAY CHOICE");
      }
      ai_popup_clear(&pop);
      col1.tribe[0].state.learned = 0;
      units_despawn(&units, srv_id);
    }

    /* Soldier → Demand Tribute: one of the four @EXTORT* bodies; laugh/no bumps alarm. */
    {
      const int sol_id = units_spawn_allow_stack(&units, 3, 6, 5);
      ColonizeUnit* sol = units_get(&units, sol_id);
      if (!sol) {
        return fail("menu: spawn soldier");
      }
      sol->nation_id = 0;
      sol->profession = UNITS_JOB_NONE;
      ai_popup_clear(&pop);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, sol_id)) {
        return fail("menu: soldier meet should enqueue");
      }
      {
        const AiPopupRequest* q = &pop.queue[0];
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
      res.result_payload = pop.queue[0].payload;
      ai_popup_clear(&pop);
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
      ai_popup_clear(&pop);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 1, 0, mis_id)) {
        return fail("menu: missionary meet should enqueue");
      }
      {
        const AiPopupRequest* q = &pop.queue[0];
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
      res.result_payload = pop.queue[0].payload;
      ai_popup_clear(&pop);
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
      ai_popup_clear(&pop);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 1, 0, mis2_id)) {
        return fail("menu: missionary 2 meet should enqueue");
      }
      if (pop.queue[0].choice_ids[0] != 7) {
        return fail("menu: no mission → Establish Mission first row");
      }
      res.result_choice_id = 7;
      res.result_payload = pop.queue[0].payload;
      ai_popup_clear(&pop);
      st_menu[0] = '\0';
      ai_contact_apply_popup_result(&ctx, &res);
      if (mis2->active || (col1.tribe[0].mission & 0x0f) != 0 || strstr(st_menu, "mission founded") == NULL) {
        fprintf(stderr, "unit_ai_contact: mission status '%s'\n", st_menu);
        return fail("establish mission: unit consumed, owner = us, @MISSIONn body");
      }
      col1.tribe[0].mission = 0xff;
    }

    /* Wagon: alarm < 75 → Trade With Village; alarm ≥ 75 → Enter Hostile Village. */
    {
      const int wag_id = units_spawn_allow_stack(&units, 4, 6, 5);
      ColonizeUnit* wag = units_get(&units, wag_id);
      if (!wag) {
        return fail("menu: spawn wagon");
      }
      wag->nation_id = 0;
      wag->profession = UNITS_JOB_NONE;
      ai_popup_clear(&pop);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, wag_id)) {
        return fail("menu: wagon meet should enqueue");
      }
      if (pop.queue[0].choice_count != 2 || pop.queue[0].choice_ids[0] != 1) {
        return fail("menu: wagon rows should be Trade / Cancel");
      }
      ind->alarm_by_player[0] = 80;
      ai_popup_clear(&pop);
      if (!ai_contact_try_village_meet_unit(&ctx, 0, 4, 0, 0, wag_id)) {
        return fail("menu: hostile wagon meet should still enqueue (DOS shows @VILLAGEWAR)");
      }
      if (pop.queue[0].choice_count != 2 || pop.queue[0].choice_ids[0] != 10) {
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
  }

  col1_save_free(&col1);
  fprintf(stderr, "unit_ai_contact: ok (last_raid_kind=%d)\n", ai_contact_last_raid_kind());

  return 0;
}
