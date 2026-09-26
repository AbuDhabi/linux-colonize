/*
 * Regression guards for FIXED bugs.md rows that had no test (gap audit
 * 2026-09-26). Slice: save.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_king.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/combat_strength.h"
#include "core/europe.h"
#include "core/reports.h"
#include "core/units.h"
#include "core/world.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

/*
 * #865 (docs/archive/bugs_closed.md): DS:0x5382 bit 0x02 means "foreign
 * intervention ANNOUNCED" (FUN_43f7_1528 raw 74493, the bit's only writer,
 * never cleared) — not "REF present". The port stores that latch on
 * head.game_options.ref_present (AI_KING_INTERVENE_ANNOUNCED_BYTE,
 * ai_king.h) and keeps the separate "crown force on the map" port-only
 * latch in the human nation's unknown23_pad[0] bit 0x08
 * (AI_KING_REF_PRESENT_BYTE, set/cleared at ai_king_war.c:328/352/955/1052).
 * This guard: (a) the announce bit round-trips a save write/read, (b) the
 * port-only pad latch does not alias it (setting/clearing REF_PRESENT_BYTE
 * must not touch game_options.ref_present), and (c) the two DOS-literal
 * readers cited in the row — combat_strength.c's colony Bombard +50%
 * (raw 100504) and reports_score.c's SoL/score bells_pts arm
 * (raw 71327 `bit2 && bells>99`) — see the round-tripped bit.
 */
static int case_865_intervention_announce_latch(void) {
  diag_init(0, NULL);
  ColonizeCol1Save save;
  char err[256];
  col1_save_init(&save);
  col1_save_stamp_head(&save.head);
  save.head.map_size_x = COLONIZE_COL1_MAP_W_STD;
  save.head.map_size_y = COLONIZE_COL1_MAP_H_STD;
  save.head.difficulty = 2;
  save.head.human_player = 0;
  save.head.colony_count = 1;
  save.head.unit_count = 0;
  save.head.tribe_count = 0;
  if (!col1_save_alloc_sections(&save, err, sizeof(err))) {
    fprintf(stderr, "case_865: alloc failed: %s\n", err);
    return 1;
  }
  save.player[0].control = 0;
  save.colony[0].x = 10;
  save.colony[0].y = 10;
  save.colony[0].nation_id = 0;
  save.colony[0].population = 1;
  save.nation[0].liberty_bells_pool = 150;

  /* Set the real DOS latch (announce), and the port-only pad latch, then
   * clear the pad latch the way ai_king_war.c does on wave wipe — the
   * announce bit must survive that untouched. */
  ai_king_latch_set(&save, AI_KING_INTERVENE_ANNOUNCED_BYTE, 1);
  ai_king_latch_set(&save, AI_KING_REF_PRESENT_BYTE, 1);
  ai_king_latch_set(&save, AI_KING_REF_PRESENT_BYTE, 0);
  if (!save.head.game_options.ref_present) {
    fprintf(stderr, "case_865: pad latch clear must not clear the announce bit\n");
    return 1;
  }

  uint8_t* blob = NULL;
  size_t blob_size = 0;
  if (!col1_save_write_memory(&save, &blob, &blob_size, err, sizeof(err))) {
    fprintf(stderr, "case_865: write_memory failed: %s\n", err);
    col1_save_free(&save);
    return 1;
  }
  ColonizeCol1Save loaded;
  col1_save_init(&loaded);
  if (!col1_save_read_memory(blob, blob_size, &loaded, err, sizeof(err))) {
    fprintf(stderr, "case_865: read_memory failed: %s\n", err);
    free(blob);
    col1_save_free(&save);
    return 1;
  }
  free(blob);
  col1_save_free(&save);

  int ok = 1;
  if (ai_king_latch_get(&loaded, AI_KING_INTERVENE_ANNOUNCED_BYTE) != 1) {
    fprintf(stderr, "case_865: announce latch did not survive round-trip\n");
    ok = 0;
  }
  if (ai_king_latch_get(&loaded, AI_KING_REF_PRESENT_BYTE) != 0) {
    fprintf(stderr, "case_865: port-only pad latch should not have round-tripped set\n");
    ok = 0;
  }

  /* reports_score.c raw 71327: `game_options.ref_present && bells>=100`. */
  if (ok) {
    ColonizeWorld w;
    memset(&w, 0, sizeof(w));
    w.col1 = &loaded;
    ColonizeScoreBreakdown score;
    reports_compute_score_w(&w, &score, 0);
    if (score.bells_pts <= 0) {
      fprintf(stderr, "case_865: SoL/score bells_pts reader did not see ref_present\n");
      ok = 0;
    }
  }

  /* combat_strength.c raw 100504: colony Bombard +50% (COMBAT_FLAG_REF). */
  if (ok) {
    ColonizeUnitPool units;
    memset(&units, 0, sizeof(units));
    units_reset(&units);
    units.type_count = 1;
    units.types[0].attack = 2;
    units.types[0].defense = 2;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    ColonizeColonyPool cols;
    colonies_init(&cols);
    cols.colonies[0].id = 0;
    cols.colonies[0].active = true;
    cols.colonies[0].nation_id = 0;
    cols.colonies[0].x = 10;
    cols.colonies[0].y = 10;
    cols.colony_count = 1;
    loaded.head.game_options.woi = 1; /* combat_woi_active gate */
    const int aid = units_spawn_allow_stack(&units, 0, 10, 10);
    const int did = units_spawn_allow_stack(&units, 0, 10, 10);
    ColonizeUnit* a = units_get(&units, aid);
    ColonizeUnit* d = units_get(&units, did);
    if (!a || !d) {
      fprintf(stderr, "case_865: combat spawn failed\n");
      ok = 0;
    } else {
      a->nation_id = 0; /* rebel attacker */
      d->nation_id = 1;
      ColonizeCombatStrengthCtx sctx;
      memset(&sctx, 0, sizeof(sctx));
      sctx.units = &units;
      sctx.colonies = &cols;
      sctx.col1 = &loaded;
      ColonizeCombatEngageResult er;
      combat_land_engage(&sctx, aid, did, &er);
      if ((er.atk_flags.flags & COMBAT_FLAG_REF) == 0) {
        fprintf(stderr, "case_865: Bombard +50%% reader did not see round-tripped ref_present\n");
        ok = 0;
      }
    }
  }

  col1_save_free(&loaded);
  return ok ? 0 : 1;
}

/*
 * #901 (docs/archive/bugs_closed.md): DOS FUN_15eb_1068 (raw 11244) writes a
 * @JOB id 0..18 only; the port's col1 occupation import used to alias raw
 * bytes 27/28/29 to COLONIES_CHAIN_RUM (a legacy salvage for a pre-fix
 * exporter). The fix drops those aliases in col1_bridge.c so 27/28/29 fall
 * through to the generic Town-Hall salvage arm instead of being misread as
 * the Rum production chain. Guard: import a colonist whose col1 occupation
 * byte is 28 and confirm it lands as a Town-Hall salvage job, not
 * COLONIES_CHAIN_RUM.
 */
static int case_901_occupation_2729_not_rum(void) {
  ColonizeCol1Save save;
  char err[256];
  col1_save_init(&save);
  save.head.map_size_x = 8;
  save.head.map_size_y = 8;
  save.head.colony_count = 1;
  save.head.difficulty = 2;
  save.head.human_player = 0;
  col1_save_stamp_head(&save.head);
  if (!col1_save_alloc_sections(&save, err, sizeof(err))) {
    fprintf(stderr, "case_901: alloc failed: %s\n", err);
    return 1;
  }
  save.player[0].control = 0;
  save.colony[0].x = 3;
  save.colony[0].y = 3;
  save.colony[0].nation_id = 0;
  save.colony[0].population = 1;
  /* occupation[0] is colonist 0's raw @JOB byte (col1_bridge.c ~1116). Byte
   * 28 used to alias to the Rum chain; it must now fall through instead of
   * resolving to the colony's own Rum Distillers House. */
  save.colony[0].occupation[0] = 28;
  /* Give the colony a real Rum Distiller's House so the pre-fix alias and
   * the fixed fall-through are actually distinguishable (both would give
   * building_type==-1 if the colony owned no rum building at all). */
  save.colony[0].buildings.rum_distillers_house = 1;

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units.type_count = 23;
  for (int t = 0; t < units.type_count; ++t) {
    units.types[t].movement = 1;
    units.types[t].domain =
      (t >= 13 && t <= 18) ? COLONIZE_UNIT_DOMAIN_SEA : COLONIZE_UNIT_DOMAIN_LAND;
    units.types[t].cargo = (t >= 13 && t <= 18) ? 6 : 0;
  }
  units_set_occupancy_map(NULL);
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies.building_type_count = 1;
  memset(&colonies.building_types[0], 0, sizeof(colonies.building_types[0]));
  snprintf(
    colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Rum Distillers House"
  );
  colonies.building_types[0].row_plus1 = (int)COLONY_BUILDING_RUM_DISTILLERS_HOUSE + 1;
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult result;
  ColonizeWorld w;
  memset(&w, 0, sizeof(w));
  w.units = &units;
  w.colonies = &colonies;
  w.map = &map;
  w.col1 = &save;
  w.col1_ok = true;
  w.europe = &europe;
  if (!col1_bridge_apply_w(&w, &result, err, sizeof(err))) {
    fprintf(stderr, "case_901: bridge apply failed: %s\n", err);
    col1_save_free(&save);
    map_free(&map);
    return 1;
  }

  const int cid = colonies_id_at(&colonies, 3, 3);
  const ColonizeColony* col = colonies_get(&colonies, cid);
  const int rum_row = colonies_building_row(&colonies, COLONY_BUILDING_RUM_DISTILLERS_HOUSE);
  int ok = 1;
  if (!col || col->colonist_count < 1) {
    fprintf(stderr, "case_901: imported colony/colonist missing\n");
    ok = 0;
  } else if (rum_row >= 0 && col->colonists[0].building_type == rum_row) {
    fprintf(
      stderr,
      "case_901: occupation byte 28 still aliased to the colony's Rum Distillers House\n"
    );
    ok = 0;
  }

  col1_save_free(&save);
  map_free(&map);
  return ok ? 0 : 1;
}

static const TestCase k_cases[] = {
  {"865_intervention_announce_latch", case_865_intervention_announce_latch},
  {"901_occupation_2729_not_rum", case_901_occupation_2729_not_rum},
};
TEST_MAIN(k_cases)
