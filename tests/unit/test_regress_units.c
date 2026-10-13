/*
 * Regression guards for FIXED bugs.md rows that had no test (gap audit
 * 2026-09-26). Slice: units.
 *
 * Each case cites the bugs.md #id and the DOS FUN_/raw line the fix ported.
 * Skipped rows (with reason) are listed at the bottom of this comment:
 *
 *   #575 (labor report counts drafted Continental Army/Cavalry): the fix is
 *   `reports_labor_unit_is_person` (static, reports_congress.c), which reads
 *   the DOS-shaped ColonizeCol1Unit array and is consumed only by
 *   `reports_render_labor_grid`/`_detail` — full-pixel renderers needing a
 *   framebuffer+font+sprite sheet. There is no public entry that returns the
 *   filtered counts, and the task's src-edit exception (COLONIZE_INTERNAL)
 *   is scoped to units_internal.h/units_combat_resolve.c only, not
 *   reports_internal.h — so this row is skipped rather than faked.
 */
#include <stdio.h>
#include <string.h>

#include "../common/test_runner.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/font.h"
#include "core/map.h"
#include "core/map_menu.h"
#include "core/units.h"
#include "core/units_internal.h"
#include "core/units_move.h"
#include "core/world.h"
#include "platform/diagnostics.h"

/* ===== shared small fixtures ===== */

static int find_menu_section(const MapMenuBar* bar, const char* section) {
  for (int i = 0; i < bar->menu_count; ++i) {
    if (strcmp(bar->menus[i].section_name, section) == 0) {
      return i;
    }
  }
  return -1;
}

static bool build_join_visible(
  MapMenuBar* bar, const MapMenuOrdersContext* octx, int action
) {
  map_menu_refresh(bar, octx);
  const int oi = find_menu_section(bar, "ORDERS");
  if (oi < 0) {
    return false;
  }
  for (int i = 0; i < bar->menus[oi].item_count; ++i) {
    const MapMenuItem* it = &bar->menus[oi].items[i];
    if (it->action == action) {
      return it->visible;
    }
  }
  return false;
}

/*
 * bugs.md #667 — DOS-LITERAL FUN_2b5a_0b34 raw 42188-42196 (FUN_281f_0b78 =
 * FUN_15eb_0902, DS:0x30e[type] >= 0). map_menu.c's `can_found_unit` used to
 * be `land && !transport`, which let Regulars/Cavalry/Treasure/Artillery
 * (no profession slot) offer Build/Join Colony and would have hidden it for
 * Cont. Army/Cont. Cav. had they ever been transports. The fix is
 * `units_type_has_profession_slot(u->type_index)`.
 */
static int case_667_build_join_type_gate(void) {
  ColonizeMsgCatalog menu_txt;
  assets_msg_init(&menu_txt);
  if (!assets_msg_load_file(&menu_txt, "COLONIZE/MENU.TXT")) {
    fprintf(stderr, "case_667: MENU.TXT load failed\n");
    return 1;
  }
  MapMenuBar bar;
  map_menu_init(&bar);
  if (!map_menu_load(&bar, &menu_txt, true)) {
    fprintf(stderr, "case_667: map_menu_load failed\n");
    assets_msg_free(&menu_txt);
    return 1;
  }
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "case_667: NAMES.TXT load failed\n");
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "case_667: units_load_types failed\n");
    assets_msg_free(&names);
    map_menu_free(&bar);
    assets_msg_free(&menu_txt);
    return 1;
  }
  assets_msg_free(&names);

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  int rc = 0;
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "case_667: map_alloc failed: %s\n", err);
    rc = 1;
  } else {
    for (int i = 0; i < 8 * 8; ++i) {
      map.terrain[i] = 2; /* plains */
    }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    colonies_set_occupancy_map(NULL);

    struct { const char* name; bool want_build_visible; } cases[] = {
      {"Regulars", false},
      {"Cavalry", false},
      {"Treasure", false},
      {"Artillery", false},
      {"Cont. Army", true},
      {"Cont. Cav.", true},
    };
    for (size_t ci = 0; ci < sizeof(cases) / sizeof(cases[0]); ++ci) {
      const int ti = units_find_type(&pool, cases[ci].name);
      if (ti < 0) {
        fprintf(stderr, "case_667: type '%s' missing\n", cases[ci].name);
        rc = 1;
        continue;
      }
      const int uid = units_spawn(&pool, ti, 3, 3);
      ColonizeUnit* u = units_get(&pool, uid);
      u->nation_id = 0;
      MapMenuOrdersContext octx;
      memset(&octx, 0, sizeof(octx));
      octx.units = &pool;
      octx.map = &map;
      octx.colonies = &colonies;
      octx.selected_id = uid;
      octx.cursor_x = 3;
      octx.cursor_y = 3;
      octx.human_nation = 0;
      const bool vis = build_join_visible(&bar, &octx, MAP_MENU_ACTION_BUILD_COLONY);
      if (vis != cases[ci].want_build_visible) {
        fprintf(
          stderr, "case_667: %s BUILD visible=%d want=%d\n", cases[ci].name, (int)vis,
          (int)cases[ci].want_build_visible
        );
        rc = 1;
      }
      units_despawn(&pool, uid);
    }
    map_free(&map);
  }
  map_menu_free(&bar);
  assets_msg_free(&menu_txt);
  return rc;
}

/*
 * bugs.md #674 — DOS-LITERAL FUN_38fd_584a raw 68272 (`FUN_281f_0b78 >= 0`):
 * the human crosses-drain loop must skip a waiting dock immigrant whose
 * resolved @UNIT type carries no profession slot. europe_harbor.c's dock
 * type resolver only round-trips the six recruit kinds + Cont. Army/Cav
 * (all profession-slotted in the real @UNIT table), so this hand-builds a
 * 7-type pool where position 6 — the DS:0x30e slot Regulars/Artillery/
 * Treasure occupy (-1, no profession) — is tagged as the Cont. Army kind, to
 * exercise the guard's shape (`ti >= 0 && !has_slot -> skip`) directly.
 */
static int case_674_crosses_drain_profession_gate(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 7;
  for (int i = 0; i < 6; ++i) {
    snprintf(pool.types[i].name, sizeof(pool.types[i].name), "Filler%d", i);
    pool.types[i].domain = COLONIZE_UNIT_DOMAIN_LAND;
  }
  snprintf(pool.types[6].name, sizeof(pool.types[6].name), "Cont. Army");
  pool.types[6].domain = COLONIZE_UNIT_DOMAIN_LAND;
  pool.types[6].kind_plus1 = (int)UNITS_KIND_CONT_ARMY + 1;

  EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  eu.crosses_immigrant_seen = true;
  eu.current_crosses = 0;
  eu.dock_count = 1;
  eu.dock[0].present = true;
  eu.dock[0].dos_type = (int)UNITS_KIND_CONT_ARMY;

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  memset(col1.head.founding_father, 0xff, sizeof(col1.head.founding_father));
  col1.head.difficulty = 2;
  /* Nation 1 (not 0): europe_compute_immigration_score_w has an unconditional
   * extra `score = score*2/3` for nation_id == 0 that would otherwise pull
   * `need` low enough to trip the unrelated Phase 5 "dock immigrant, zero
   * current_crosses" branch and mask the drain-guard assertion below. */
  col1.player[1].control = 1; /* AI-controlled: diff-scaled score, need ~6 */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);

  ColonizeWorld w;
  memset(&w, 0, sizeof(w));
  w.units = &pool;
  w.colonies = &colonies;
  w.col1 = &col1;
  w.col1_ok = true;
  w.europe = &eu;

  const int before = eu.current_crosses;
  europe_tick_immigration_pressure_w(&w, 1);
  /* No drain: only the idle +2 tick applies; stays well under `need` so
   * Phase 5's dock-immigrant reset does not also zero it. */
  const int want = before + 2;
  if ((int)eu.current_crosses != want) {
    fprintf(
      stderr, "case_674: current_crosses=%d want=%d (profession-slot-less dock row drained)\n",
      eu.current_crosses, want
    );
    return 1;
  }
  return 0;
}

/*
 * bugs.md #639 — DOS-LITERAL FUN_112b_0060: the veteran-art test is
 * profession == 0x15 (UNITS_JOB_SOLDIER) exactly; 0x17 (UNITS_JOB_DRAGOON)
 * is never written to a unit by DOS and must not draw veteran art either.
 */
static int case_639_vet_prof_excludes_0x17(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "case_639: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "case_639: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);

  const int soldiers = units_find_type(&pool, "Soldiers");
  if (soldiers < 0) {
    fprintf(stderr, "case_639: Soldiers type missing\n");
    return 1;
  }
  const int uid = units_spawn(&pool, soldiers, 2, 2);
  ColonizeUnit* u = units_get(&pool, uid);
  u->muskets = 50;
  u->profession = UNITS_JOB_DRAGOON; /* 0x17: DOS never writes this to a unit */
  const int icon_0x17 = units_map_sprite(&pool, uid);

  u->profession = UNITS_JOB_SOLDIER; /* 0x15: the real veteran art gate */
  const int icon_0x15 = units_map_sprite(&pool, uid);

  int rc = 0;
  if (icon_0x17 != UNITS_ICON_SOLDIER) {
    fprintf(stderr, "case_639: profession 0x17 drew icon %d, want plain Soldier %d\n", icon_0x17,
            UNITS_ICON_SOLDIER);
    rc = 1;
  }
  if (icon_0x15 != UNITS_ICON_VETERAN_SOLDIER) {
    fprintf(stderr, "case_639: profession 0x15 drew icon %d, want Veteran Soldier %d\n", icon_0x15,
            UNITS_ICON_VETERAN_SOLDIER);
    rc = 1;
  }
  return rc;
}

/* Shared land-combat fixture for #646/#647/#678/#834: 8x8 all-plains map,
 * real NAMES.TXT @UNIT table, no colonies unless a case adds one. */
typedef struct LandFixture {
  ColonizeUnitPool pool;
  ColonizeWorldMap map;
  ColonizeColonyPool colonies;
  ColonizeCol1Save col1;
} LandFixture;

static int land_fixture_init(LandFixture* fx) {
  memset(fx, 0, sizeof(*fx));
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "land_fixture: NAMES.TXT load failed\n");
    return 1;
  }
  if (!units_load_types(&fx->pool, &names)) {
    fprintf(stderr, "land_fixture: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  char err[128];
  if (!map_alloc(&fx->map, 8, 8, err, sizeof(err))) {
    fprintf(stderr, "land_fixture: map_alloc failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < 8 * 8; ++i) {
    fx->map.terrain[i] = 2;   /* plains */
    fx->map.layer3[i] = 0xf1; /* no owner, continent 1: never reads as lake */
  }
  colonies_init(&fx->colonies);
  memset(fx->col1.head.founding_father, 0xff, sizeof(fx->col1.head.founding_father));
  fx->col1.head.difficulty = 2;
  fx->col1.player[0].control = 1;
  fx->col1.player[1].control = 1;
  units_set_occupancy_map(&fx->map);
  units_set_combat_colonies(NULL);
  units_set_ff_col1(&fx->col1);
  units_set_combat_human_nation(-1);
  return 0;
}

static void land_fixture_free(LandFixture* fx) {
  units_set_occupancy_map(NULL);
  units_set_combat_colonies(NULL);
  map_free(&fx->map);
}

/*
 * bugs.md #646 — DOS-LITERAL FUN_5fef_0352 raw 99433-99464: the demote arm
 * writes only the TYPE byte (+0x3146); no orders/MP write. A Fortified
 * Dragoon beaten to Soldiers must keep Fortified and its MP.
 */
static int case_646_demote_writes_type_only(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  int rc = 0;
  const int drag = units_find_type(&fx.pool, "Dragoons");
  const int sold = units_find_type(&fx.pool, "Soldiers");
  if (drag < 0 || sold < 0) {
    fprintf(stderr, "case_646: types missing\n");
    land_fixture_free(&fx);
    return 1;
  }
  const int lid = units_spawn_allow_stack(&fx.pool, drag, 3, 3);
  const int wid = units_spawn_allow_stack(&fx.pool, sold, 3, 3);
  ColonizeUnit* l = units_get(&fx.pool, lid);
  ColonizeUnit* w = units_get(&fx.pool, wid);
  l->nation_id = 0;
  w->nation_id = 1;
  l->orders = UNITS_ORDER_FORTIFY;
  l->moves = 7;
  l->muskets = 50;
  l->horses = 50;

  units_apply_land_loss_outcome(&fx.pool, lid, wid, &fx.col1, 0, NULL);

  l = units_get(&fx.pool, lid);
  if (!l || !l->active) {
    fprintf(stderr, "case_646: loser did not survive the demote\n");
    rc = 1;
  } else {
    if (l->type_index != sold) {
      fprintf(stderr, "case_646: loser type_index=%d want Soldiers=%d\n", l->type_index, sold);
      rc = 1;
    }
    if (l->orders != UNITS_ORDER_FORTIFY) {
      fprintf(stderr, "case_646: orders=%d want FORTIFY (%d) preserved\n", l->orders,
              UNITS_ORDER_FORTIFY);
      rc = 1;
    }
    if (l->moves != 7) {
      fprintf(stderr, "case_646: moves=%d want 7 preserved\n", l->moves);
      rc = 1;
    }
  }
  land_fixture_free(&fx);
  return rc;
}

/*
 * bugs.md #647 — DOS-LITERAL FUN_5fef_0352 raw 99355-99364 + 99433: the
 * demote ladder runs only when the winner is not a hull AND neither tile is
 * ocean/high seas. On water, or beaten by a ship, the loser is destroyed.
 */
static int case_647_demote_gated_off_water(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  int rc = 0;
  const int drag = units_find_type(&fx.pool, "Dragoons");
  const int sold = units_find_type(&fx.pool, "Soldiers");
  if (drag < 0 || sold < 0) {
    fprintf(stderr, "case_647: types missing\n");
    land_fixture_free(&fx);
    return 1;
  }
  /* Leg 1: ocean tile under the loser -> destroyed, not demoted. */
  fx.map.terrain[3 * fx.map.width + 3] = 25; /* ocean */
  {
    const int lid = units_spawn_allow_stack(&fx.pool, drag, 3, 3);
    const int wid = units_spawn_allow_stack(&fx.pool, sold, 3, 3);
    ColonizeUnit* l = units_get(&fx.pool, lid);
    ColonizeUnit* w = units_get(&fx.pool, wid);
    l->nation_id = 0;
    w->nation_id = 1;
    l->muskets = 50;
    l->horses = 50;
    units_apply_land_loss_outcome(&fx.pool, lid, wid, &fx.col1, 0, NULL);
    l = units_get(&fx.pool, lid);
    if (l && l->active) {
      fprintf(stderr, "case_647: loser demoted on a water tile, want destroyed\n");
      rc = 1;
    }
  }
  fx.map.terrain[3 * fx.map.width + 3] = 2; /* back to plains */

  /* Leg 2: same pair on land -> demoted (sanity: the gate does not
   * over-fire). */
  {
    const int lid = units_spawn_allow_stack(&fx.pool, drag, 4, 4);
    const int wid = units_spawn_allow_stack(&fx.pool, sold, 4, 4);
    ColonizeUnit* l = units_get(&fx.pool, lid);
    ColonizeUnit* w = units_get(&fx.pool, wid);
    l->nation_id = 0;
    w->nation_id = 1;
    l->muskets = 50;
    l->horses = 50;
    units_apply_land_loss_outcome(&fx.pool, lid, wid, &fx.col1, 0, NULL);
    l = units_get(&fx.pool, lid);
    if (!l || !l->active || l->type_index != sold) {
      fprintf(stderr, "case_647: loser on land did not demote to Soldiers\n");
      rc = 1;
    }
  }
  land_fixture_free(&fx);
  return rc;
}

/*
 * bugs.md #678 — DOS-LITERAL FUN_5fef_0352 raw 99343-99392: the capture-alive
 * test reads only the LOSER's @UNIT type (Colonists/Treasure/Wagon), never
 * its nation. A native-nation unit riding a Colonists-type body must still
 * change hands, not be destroyed, when it loses to a combat-capable Euro.
 */
static int case_678_capture_ignores_loser_nation(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  int rc = 0;
  const int colonist = units_find_type(&fx.pool, "Colonists");
  const int sold = units_find_type(&fx.pool, "Soldiers");
  if (colonist < 0 || sold < 0) {
    fprintf(stderr, "case_678: types missing\n");
    land_fixture_free(&fx);
    return 1;
  }
  const int lid = units_spawn_allow_stack(&fx.pool, colonist, 3, 3);
  const int wid = units_spawn_allow_stack(&fx.pool, sold, 3, 3);
  ColonizeUnit* l = units_get(&fx.pool, lid);
  ColonizeUnit* w = units_get(&fx.pool, wid);
  l->nation_id = 4; /* native nation riding a Colonists-type body */
  w->nation_id = 1; /* Euro, combat-capable (Soldiers, attack > 0) */
  w->muskets = 50;

  const int captured = units_apply_land_loss_outcome(&fx.pool, lid, wid, &fx.col1, 0, NULL);

  l = units_get(&fx.pool, lid);
  if (!captured || !l || !l->active) {
    fprintf(stderr, "case_678: loser was destroyed, want captured (nation flip)\n");
    rc = 1;
  } else if (l->nation_id != w->nation_id) {
    fprintf(stderr, "case_678: loser nation_id=%d want winner's %d (capture)\n", l->nation_id,
            w->nation_id);
    rc = 1;
  }
  land_fixture_free(&fx);
  return rc;
}

/*
 * FUN_5fef_1b0e raw 100568-100569: `0916(atk); 089e(atk)` park the attacker
 * off map and 0948 puts it back afterwards, so it ends as its tile's latest
 * arrival (DOSBox 1554: a Spanish Dragoon that attacked a village).
 */
static int case_1b0e_relinks_attacker(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  int rc = 0;
  const int sold = units_find_type(&fx.pool, "Soldiers");
  const int colonist = units_find_type(&fx.pool, "Colonists");
  const int aid = units_spawn_allow_stack(&fx.pool, sold, 3, 3);
  const int bid = units_spawn_allow_stack(&fx.pool, colonist, 3, 3); /* later arrival */
  const int did = units_spawn_allow_stack(&fx.pool, sold, 4, 3);
  ColonizeUnit* a = units_get(&fx.pool, aid);
  ColonizeUnit* b = units_get(&fx.pool, bid);
  ColonizeUnit* d = units_get(&fx.pool, did);
  if (!a || !b || !d) {
    land_fixture_free(&fx);
    return 1;
  }
  units_set_nation(a, 1);
  units_set_nation(b, 1);
  units_set_nation(d, 2);
  a->muskets = 50;
  d->muskets = 50;
  if (units_tile_head_id_at(&fx.pool, 3, 3) != bid) {
    fprintf(stderr, "1b0e relink: fixture head should be the later arrival\n");
    land_fixture_free(&fx);
    return 1;
  }
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 100);
  (void)units_resolve_land_combat(&fx.pool, aid, did, &rng);
  a = units_get(&fx.pool, aid);
  if (a && a->active && a->x == 3 && a->y == 3 && units_tile_head_id_at(&fx.pool, 3, 3) != aid) {
    fprintf(stderr, "1b0e relink: attacker must be the tile's latest arrival\n");
    rc = 1;
  }
  land_fixture_free(&fx);
  return rc;
}

/*
 * FUN_5fef_1b0e: the attacker sits off map during the fight and 0948 puts it
 * back after the outcome (raw 100750), so a colonist it captured is linked
 * on its tile first and the attacker ends ahead of it (DOSBox 1594: French
 * Dragoon 45 -> captured colonist 44 -> 47 -> 48).
 */
/* Fresh 1571 two-turn: a phantom Pioneer stamped 0xec into its record's
 * +0x15, was despawned, and a Soldier spawned later into the same port slot
 * with no raw tail of its own exported that 0xec. The raw bytes belong to the
 * record; clearing the slot must drop them. */
/* FUN_465b_0000 raw 75577-75578: bVar4 comes from the settlement owner too,
 * so an AI mover with under 3 thirds left that steps at an UNDEFENDED foreign
 * colony goes to LAB_465b_01ce: exhaust and stop, with no partial-MP roll
 * (fresh 1592 three-turn: a French Dragoon at 0 MP beside empty Curacao). */
static int case_465b_low_mp_foreign_colony_exhausts(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  const int sold = units_find_type(&fx.pool, "Soldiers");
  const int aid = units_spawn_allow_stack(&fx.pool, sold, 3, 3);
  ColonizeUnit* a = units_get(&fx.pool, aid);
  if (!a) {
    land_fixture_free(&fx);
    return 1;
  }
  units_set_nation(a, 1); /* control 1: AI */
  a->moves = 2;            /* under one tile */
  ColonizeColony* c = &fx.colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 4;
  c->y = 3;
  c->population = 1;
  fx.colonies.colony_count = 1;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 7);
  const ColonizeDosRng before = rng;
  ColonizeWorld w = world_make(&fx.pool, &fx.colonies, &fx.map, &fx.col1, true, &rng, NULL);
  const bool moved = units_try_move_w(&w, aid, 4, 3);
  a = units_get(&fx.pool, aid);
  int rc = 0;
  if (moved || !a || a->x != 3) {
    fprintf(stderr, "465b low MP: the mover must stay outside the colony\n");
    rc = 1;
  } else if (units_remaining_mp(&fx.pool, aid) != 0) {
    fprintf(stderr, "465b low MP: LAB_465b_01ce must exhaust the mover\n");
    rc = 1;
  } else if (memcmp(&before, &rng, sizeof rng) != 0) {
    fprintf(stderr, "465b low MP: no partial-MP roll may be drawn\n");
    rc = 1;
  }
  land_fixture_free(&fx);
  return rc;
}

static int case_despawn_drops_raw_record(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  const int sold = units_find_type(&fx.pool, "Soldiers");
  const int pid = units_spawn_allow_stack(&fx.pool, sold, 3, 3);
  ColonizeUnit* p = units_get(&fx.pool, pid);
  if (!p) {
    land_fixture_free(&fx);
    fprintf(stderr, "raw record: spawn\n");
    return 1;
  }
  p->col1_hold_raw[9] = 0xec;
  p->col1_hold_raw_valid = 1;
  const int slot = (int)(p - fx.pool.units);
  units_despawn(&fx.pool, pid);
  const ColonizeUnit* cleared = &fx.pool.units[slot];
  const int rc = cleared->col1_hold_raw_valid || cleared->col1_hold_raw[9] != 0;
  if (rc) {
    fprintf(stderr, "raw record: a cleared slot must not keep the old record's bytes\n");
  }
  land_fixture_free(&fx);
  return rc;
}

static int case_1b0e_attacker_back_after_capture(void) {
  for (int seed = 1; seed < 64; ++seed) {
    LandFixture fx;
    if (land_fixture_init(&fx) != 0) {
      return 1;
    }
    const int sold = units_find_type(&fx.pool, "Soldiers");
    const int colonist = units_find_type(&fx.pool, "Colonists");
    const int aid = units_spawn_allow_stack(&fx.pool, sold, 3, 3);
    const int did = units_spawn_allow_stack(&fx.pool, colonist, 4, 3);
    ColonizeUnit* a = units_get(&fx.pool, aid);
    ColonizeUnit* d = units_get(&fx.pool, did);
    if (!a || !d) {
      land_fixture_free(&fx);
      return 1;
    }
    units_set_nation(a, 1);
    units_set_nation(d, 3);
    a->muskets = 50;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, (uint32_t)seed);
    (void)units_resolve_land_combat(&fx.pool, aid, did, &rng);
    d = units_get(&fx.pool, did);
    if (d && d->active && d->nation_id == 1 && d->x == 3 && d->y == 3) {
      const int head = units_tile_head_id_at(&fx.pool, 3, 3);
      land_fixture_free(&fx);
      if (head != aid) {
        fprintf(stderr, "1b0e capture: head %d, want attacker %d ahead of captive %d\n",
                head, aid, did);
        return 1;
      }
      return 0;
    }
    land_fixture_free(&fx);
  }
  fprintf(stderr, "1b0e capture: no seed produced a capture\n");
  return 1;
}

/*
 * FUN_465b_0000 LAB_0673 (raw 75661-75664): a non-human mover ORs the dest
 * watch mask (0826) into itself and the units behind it on its tile (0c72
 * walks `prev`). DOSBox 1596: a Dutch hull leaving Vlissingen left France's
 * bit on the colonist behind it.
 */
static int case_465b_0673_vis_or_behind_mover(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  int rc = 0;
  const int colonist = units_find_type(&fx.pool, "Colonists");
  const int bid = units_spawn_allow_stack(&fx.pool, colonist, 3, 3); /* behind */
  const int aid = units_spawn_allow_stack(&fx.pool, colonist, 3, 3); /* next end */
  const int fid = units_spawn_allow_stack(&fx.pool, colonist, 5, 3);
  ColonizeUnit* a = units_get(&fx.pool, aid);
  ColonizeUnit* b = units_get(&fx.pool, bid);
  ColonizeUnit* f = units_get(&fx.pool, fid);
  if (!a || !b || !f) {
    land_fixture_free(&fx);
    return 1;
  }
  units_set_nation(a, 3);
  units_set_nation(b, 3);
  units_set_nation(f, 1);
  units_occupancy_refresh_tile(&fx.pool, 5, 3, -1);
  a->moves = units_max_mp(&fx.pool, aid);
  ColonizeWorld w = world_make(&fx.pool, &fx.colonies, &fx.map, &fx.col1, true, NULL, NULL);
  if (!units_try_move_w(&w, aid, 4, 3)) {
    fprintf(stderr, "0673: move failed\n");
    rc = 1;
  }
  b = units_get(&fx.pool, bid);
  if (!rc && (!b || (b->col1_vis_mask & 0x02) == 0)) {
    fprintf(stderr, "0673: unit behind the mover lacks France's bit (vis %d)\n",
            b ? b->col1_vis_mask : -1);
    rc = 1;
  }
  land_fixture_free(&fx);
  return rc;
}

/*
 * FUN_5fef_0352 capture arm, asm 5fef:0549-0569: 0812 unlinks the loser,
 * 0894 flips its flag, 0844 links it on the winner's tile. The flag change
 * never restamps the tile the loser left (DOSBox 1594/1595: it keeps the
 * Dutch owner nibble).
 */
static int case_capture_keeps_old_tile_owner(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  int rc = 0;
  const int colonist = units_find_type(&fx.pool, "Colonists");
  const int sold = units_find_type(&fx.pool, "Soldiers");
  const int lid = units_spawn_allow_stack(&fx.pool, colonist, 3, 3);
  const int wid = units_spawn_allow_stack(&fx.pool, sold, 4, 3);
  ColonizeUnit* l = units_get(&fx.pool, lid);
  ColonizeUnit* w = units_get(&fx.pool, wid);
  if (!l || !w) {
    land_fixture_free(&fx);
    return 1;
  }
  units_set_nation(l, 3);
  units_set_nation(w, 1);
  w->muskets = 50;
  map_set_owner_nibble(&fx.map, 3, 3, 3);
  (void)units_apply_land_loss_outcome(&fx.pool, lid, wid, &fx.col1, 0, NULL);
  l = units_get(&fx.pool, lid);
  const int owner = fx.map.layer3[3 * fx.map.width + 3] >> 4;
  if (!l || l->nation_id != 1 || l->x != 4 || owner != 3) {
    fprintf(stderr, "capture: nation %d at %d,%d, old tile owner %d (want 1 at 4,3, owner 3)\n",
            l ? l->nation_id : -1, l ? l->x : -1, l ? l->y : -1, owner);
    rc = 1;
  }
  land_fixture_free(&fx);
  return rc;
}

/*
 * bugs.md #834 — DOS-LITERAL FUN_5fef_1b0e raw 100641-100646: a native
 * winner attacking a tile that carries a colony latches
 * `attacker+0x3148 |= 0x10` (the guaranteed arm of the 0352 gear-return
 * gate), regardless of the rest of the outcome.
 */
static int case_834_native_winner_colony_latch(void) {
  LandFixture fx;
  if (land_fixture_init(&fx) != 0) {
    return 1;
  }
  int rc = 0;
  const int colonist = units_find_type(&fx.pool, "Colonists");
  const int brave = units_find_type(&fx.pool, "Braves");
  if (colonist < 0 || brave < 0) {
    fprintf(stderr, "case_834: types missing\n");
    land_fixture_free(&fx);
    return 1;
  }
  ColonizeColony* col = &fx.colonies.colonies[0];
  col->active = true;
  col->id = 0;
  col->nation_id = 0;
  col->x = 3;
  col->y = 3;
  snprintf(col->name, sizeof(col->name), "Jamestown");
  fx.colonies.colony_count = 1;
  units_set_combat_colonies(&fx.colonies);

  const int lid = units_spawn_allow_stack(&fx.pool, colonist, 3, 3);
  const int wid = units_spawn_allow_stack(&fx.pool, brave, 3, 3);
  ColonizeUnit* l = units_get(&fx.pool, lid);
  ColonizeUnit* w = units_get(&fx.pool, wid);
  l->nation_id = 0; /* Euro defender, loses */
  w->nation_id = 4; /* native winner, on a colony tile */

  units_apply_land_loss_outcome(&fx.pool, lid, wid, &fx.col1, 0, NULL);

  w = units_get(&fx.pool, wid);
  if (!w || (w->col1_flags15 & 0x10u) == 0) {
    fprintf(stderr, "case_834: native winner missing colony-capture latch (col1_flags15 & 0x10)\n");
    rc = 1;
  }
  units_set_combat_colonies(NULL);
  land_fixture_free(&fx);
  return rc;
}

/*
 * bugs.md #868 — DOS-LITERAL FUN_5bfb_312e raw 98448 (+0x3150): the naval
 * evade/ship-slow power term subtracts 4 per GOODS hold in use, never 4 per
 * passenger. A troop-laden Man-O-War must not be weakened by its passengers.
 */
static int case_868_naval_evade_goods_not_passengers(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "case_868: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "case_868: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);

  const int mow = units_find_type(&pool, "Man-O-War");
  if (mow < 0) {
    fprintf(stderr, "case_868: Man-O-War type missing\n");
    return 1;
  }
  const int sid = units_spawn(&pool, mow, 2, 2);
  ColonizeUnit* s = units_get(&pool, sid);
  /* Two GOODS holds in use (should count) plus three PASSENGERS (should
   * not) -- passenger ids need not be live units for this call: only
   * units_holds_used walks hold_goods_amount, cargo_count is untouched by
   * the fixed helper. */
  s->hold_goods_amount[0] = 50;
  s->hold_goods_amount[1] = 50;
  s->cargo_count = 3;

  const int power = units_naval_evade_power(&pool, sid);
  /* max_mp (movement 5 * 3 thirds = 15) + 3 = 18; Man-O-War is neither
   * Privateer nor Galleon; -4 per goods hold (2) = 10. */
  const int want = 10;
  int rc = 0;
  if (power != want) {
    fprintf(stderr, "case_868: units_naval_evade_power=%d want %d (goods holds only)\n", power,
            want);
    rc = 1;
  }
  return rc;
}

static const TestCase k_cases[] = {
    {"case_667_build_join_type_gate", case_667_build_join_type_gate},
    {"case_674_crosses_drain_profession_gate", case_674_crosses_drain_profession_gate},
    {"case_639_vet_prof_excludes_0x17", case_639_vet_prof_excludes_0x17},
    {"case_646_demote_writes_type_only", case_646_demote_writes_type_only},
    {"case_647_demote_gated_off_water", case_647_demote_gated_off_water},
    {"case_678_capture_ignores_loser_nation", case_678_capture_ignores_loser_nation},
    {"case_capture_keeps_old_tile_owner", case_capture_keeps_old_tile_owner},
    {"case_1b0e_attacker_back_after_capture", case_1b0e_attacker_back_after_capture},
    {"case_despawn_drops_raw_record", case_despawn_drops_raw_record},
    {"case_465b_low_mp_foreign_colony_exhausts", case_465b_low_mp_foreign_colony_exhausts},
    {"case_465b_0673_vis_or_behind_mover", case_465b_0673_vis_or_behind_mover},
    {"case_1b0e_relinks_attacker", case_1b0e_relinks_attacker},
    {"case_834_native_winner_colony_latch", case_834_native_winner_colony_latch},
    {"case_868_naval_evade_goods_not_passengers", case_868_naval_evade_goods_not_passengers},
};
TEST_MAIN(k_cases)
