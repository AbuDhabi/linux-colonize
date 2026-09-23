#include "test_units_common.h"

/*
 * bugs.md: equipping an existing unit keeps its tier. A Continental Army given
 * horses becomes Continental Cavalry (DOS DS:0x30e files type 9 under @JOB 21
 * Soldier and type 7 under @JOB 23 Dragoon — the same pair the WoI promotion
 * and the combat demote table use), never the plain Veteran Dragoons the flat
 * @JOB->@UNIT table name would give. Also covers the spawn kit: "Cont. Cav."
 * matched neither "Dragoon" nor "Cavalry", so it used to spawn with no
 * muskets and no horses at all.
 */
static int unit_continental_equip_tier(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "cont_equip: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "cont_equip: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int cont_army = units_find_type(&pool, "Cont. Army");
  const int cont_cav = units_find_type(&pool, "Cont. Cav.");
  const int colonists = units_find_type(&pool, "Colonists");
  const int regulars = units_find_type(&pool, "Regulars");
  if (cont_army < 0 || cont_cav < 0 || colonists < 0 || regulars < 0) {
    fprintf(stderr, "cont_equip: missing WoI @UNIT types\n");
    assets_msg_free(&names);
    return 1;
  }
  struct {
    int type;
    int role;
    const char* want;
  } cases[] = {
    /* bugs.md #648: DS:0x2f5 is flat — no tier is preserved by a gear change.
     * FUN_15eb_1068 case 1 raw 11268-11270 -> FUN_15eb_0916 raw 9949-9955. */
    {cont_army, COLONIZE_EJECT_DRAGOON, "Dragoons"},
    {cont_army, COLONIZE_EJECT_SOLDIER, "Soldiers"},
    {cont_army, COLONIZE_EJECT_COLONIST, "Colonists"},
    {cont_cav, COLONIZE_EJECT_SOLDIER, "Soldiers"},
    {cont_cav, COLONIZE_EJECT_DRAGOON, "Dragoons"},
    {colonists, COLONIZE_EJECT_DRAGOON, "Dragoons"},
    {colonists, COLONIZE_EJECT_SOLDIER, "Soldiers"},
    {regulars, COLONIZE_EJECT_DRAGOON, "Dragoons"},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    const char* got = units_equip_role_type_name(&pool, cases[i].type, cases[i].role);
    if (!got || strcmp(got, cases[i].want) != 0) {
      fprintf(stderr, "cont_equip: %s + role %d -> %s (want %s)\n",
              pool.types[cases[i].type].name, cases[i].role, got ? got : "(null)",
              cases[i].want);
      assets_msg_free(&names);
      return 1;
    }
  }
  const int cav_id = units_spawn_allow_stack(&pool, cont_cav, 3, 3);
  const ColonizeUnit* cav = units_get_const(&pool, cav_id);
  if (!cav || cav->muskets != UNITS_EQUIP_MUSKETS || cav->horses != UNITS_EQUIP_HORSES) {
    fprintf(stderr, "cont_equip: Cont. Cav. spawn kit muskets=%d horses=%d\n",
            cav ? cav->muskets : -1, cav ? cav->horses : -1);
    assets_msg_free(&names);
    return 1;
  }
  const int army_id = units_spawn_allow_stack(&pool, cont_army, 4, 3);
  const ColonizeUnit* army = units_get_const(&pool, army_id);
  if (!army || army->muskets != UNITS_EQUIP_MUSKETS || army->horses != 0) {
    fprintf(stderr, "cont_equip: Cont. Army spawn kit muskets=%d horses=%d\n",
            army ? army->muskets : -1, army ? army->horses : -1);
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  return 0;
}

/*
 * bugs.md #591: every DOS unit-name channel reads the @UNIT ROW string
 * (0x5230 + type*0xe; raw 14128, 42654, 99460-99461, 99511, 100625-100628),
 * never a @JOB-composed label. A bare colonist body reads the @UNIT row 0
 * name ("Colonists"); arming it reads the @UNIT row 1 name ("Soldiers"),
 * because DOS carries the equipment ladder in the type byte itself. The
 * rank words live in the separate profession line (units_profession_line).
 */
static int unit_display_name_free_colonist(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "display_name: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "display_name: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  const int colonist_ty = units_find_type(&pool, "Colonists");
  if (colonist_ty < 0) {
    fprintf(stderr, "display_name: no Colonists type\n");
    assets_msg_free(&names);
    return 1;
  }
  const int uid = units_spawn_allow_stack(&pool, colonist_ty, 1, 1);
  ColonizeUnit* u = units_get(&pool, uid);
  if (!u) {
    fprintf(stderr, "display_name: spawn failed\n");
    assets_msg_free(&names);
    return 1;
  }
  u->profession = UNITS_JOB_NONE;
  const ColonizeUnitType* colonist_row = units_type(&pool, colonist_ty);
  const char* name = units_display_name(&pool, u);
  if (!name || !name[0] || strcmp(name, colonist_row->name) != 0) {
    fprintf(stderr, "display_name: base Colonists got '%s' want @UNIT row 0 '%s'\n",
            name ? name : "(null)", colonist_row->name);
    assets_msg_free(&names);
    return 1;
  }
  /* An Expert Fisherman body is still @UNIT row 0: the profession never
   * touches the name channel. */
  u->profession = 6 /* @JOB Expert Fisherman */;
  name = units_display_name(&pool, u);
  if (!name || strcmp(name, colonist_row->name) != 0) {
    fprintf(stderr, "display_name: expert body got '%s' want '%s'\n",
            name ? name : "(null)", colonist_row->name);
    assets_msg_free(&names);
    return 1;
  }
  /* Armed: DOS rewrites the type byte to @UNIT row 1 ("Soldiers"). */
  const int soldier_ty = units_kind_type_index(&pool, UNITS_KIND_SOLDIER);
  u->muskets = UNITS_EQUIP_MUSKETS;
  name = units_display_name(&pool, u);
  if (soldier_ty < 0 || !name || strcmp(name, units_type(&pool, soldier_ty)->name) != 0) {
    fprintf(stderr, "display_name: armed expert got '%s' want @UNIT soldier row\n",
            name ? name : "(null)");
    assets_msg_free(&names);
    return 1;
  }
  u->muskets = 0;
  u->profession = UNITS_JOB_NONE;
  assets_msg_free(&names);
  fprintf(stderr, "unit_units: display_name = @UNIT row ok\n");
  return 0;
}

/*
 * bugs.md #581: production installs no name resolver (units_name_kind is a
 * test-only hook), so every class test must read the @UNIT ROW's kind_plus1.
 * Two gameplay sites used to read the English spelling and therefore went
 * dead in the shipped binary:
 *   (a) the FUN_5bfb_3180 adjacent-warship arm (raw 98519-98624) — a
 *       Privateer next to a foreign Frigate must still drain MP;
 *   (b) the royal-type demote guard (raw 99440-99465) — King's Regulars are
 *       DESTROYED on a loss, never stripped to a musket-less body.
 * Both run here with the resolver explicitly uninstalled.
 */
extern void test_name_kinds_reinstall(void);

static int unit_kind_without_name_resolver(void) {
  units_set_name_kind_resolver(NULL); /* production state */
  int rc = 0;

  /* (a) Privateer vs adjacent foreign Frigate: MP drains. */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    map.width = 16;
    map.height = 16;
    map.tile_count = 256;
    map.terrain = calloc(256, 1);
    map.layer2 = calloc(256, 1);
    map.layer3 = calloc(256, 1);
    if (!map.terrain || !map.layer2 || !map.layer3) {
      fprintf(stderr, "kind#581: map alloc\n");
      test_name_kinds_reinstall();
      return 1;
    }
    for (int i = 0; i < 256; ++i) {
      map.terrain[i] = 25; /* ocean */
      map.layer3[i] = 1;   /* continent 1: ocean, not lake */
    }

    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    units_reset(&pool);
    units_set_occupancy_map(NULL);
    pool.type_count = 2;
    /* No names at all — the row's kind_plus1 is the only identity. */
    pool.types[0].movement = 8;
    pool.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
    pool.types[0].kind_plus1 = (uint8_t)(UNITS_KIND_PRIVATEER + 1);
    pool.types[1].movement = 6;
    pool.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
    pool.types[1].kind_plus1 = (uint8_t)(UNITS_KIND_FRIGATE + 1);

    ColonizeColonyPool colonies;
    memset(&colonies, 0, sizeof(colonies));
    colonies_init(&colonies);
    units_set_occupancy_map(NULL);

    const int own_id = units_spawn_allow_stack(&pool, 0, 5, 5);
    const int foe_id = units_spawn_allow_stack(&pool, 1, 5, 4);
    ColonizeUnit* own = units_get(&pool, own_id);
    ColonizeUnit* foe = units_get(&pool, foe_id);
    if (!own || !foe) {
      fprintf(stderr, "kind#581: ship spawn failed\n");
      free(map.terrain); free(map.layer2); free(map.layer3);
      test_name_kinds_reinstall();
      return 1;
    }
    own->nation_id = 0;
    foe->nation_id = 2;
    units_occupancy_rebuild(&pool);

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    for (int i = 0; i < 4; ++i) {
      col1.player[i].control = 1; /* nobody human: no popups */
    }
    units_set_native_fallout_context(&col1, &map, -1);
    units_set_combat_popups(NULL, NULL);

    const int full = 8 * UNITS_MP_PER_TILE;
    int slowed = 0;
    for (uint32_t seed = 1000; seed < 60000 && !slowed; seed += 1777) {
      ColonizeDosRng rng;
      dos_rng_seed(&rng, seed);
      own->moves = full;
      ColonizeWorld w = {
        .units = &pool, .colonies = &colonies, .map = &map, .rng = &rng,
        .col1 = &col1, .col1_ok = true
      };
      units_ship_slow_scan_w(&w, own_id);
      if (own->moves < full) {
        slowed = 1;
      }
    }
    if (!slowed) {
      fprintf(stderr,
              "kind#581: Privateer next to a Frigate never slowed (@SHIPSLOW arm dead)\n");
      rc = 1;
    }
    units_set_native_fallout_context(NULL, NULL, -1);
    units_set_occupancy_map(NULL);
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
  }
  if (rc != 0) {
    test_name_kinds_reinstall();
    return rc;
  }

  /* (b) King's Regulars lose: destroyed, not demoted. */
  {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    units_reset(&pool);
    units_set_occupancy_map(NULL);
    pool.type_count = 2;
    pool.types[0].attack = 99;
    pool.types[0].defense = 99;
    pool.types[0].movement = 1;
    pool.types[0].kind_plus1 = (uint8_t)(UNITS_KIND_SOLDIER + 1);
    pool.types[1].attack = 0;
    pool.types[1].defense = 0;
    pool.types[1].movement = 1;
    pool.types[1].kind_plus1 = (uint8_t)(UNITS_KIND_REGULAR + 1);

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    for (int i = 0; i < 4; ++i) {
      col1.player[i].control = 1;
    }
    units_set_ff_col1(&col1);
    units_set_combat_popups(NULL, NULL);

    const int aid = units_spawn_allow_stack(&pool, 0, 5, 5);
    const int did = units_spawn_allow_stack(&pool, 1, 6, 5);
    ColonizeUnit* att = units_get(&pool, aid);
    ColonizeUnit* def = units_get(&pool, did);
    if (!att || !def) {
      fprintf(stderr, "kind#581: regular spawn failed\n");
      units_set_ff_col1(NULL);
      test_name_kinds_reinstall();
      return 1;
    }
    att->nation_id = 1;
    def->nation_id = 2;
    def->muskets = UNITS_EQUIP_MUSKETS;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 12345);
    ColonizeWorld w = {
      .units = &pool, .col1 = &col1, .col1_ok = true, .rng = &rng
    };
    units_resolve_land_combat_ff_w(&w, aid, did);
    const ColonizeUnit* after = units_get_const(&pool, did);
    if (after && after->active) {
      fprintf(stderr, "kind#581: Regulars survived a loss (muskets=%d) — royal guard dead\n",
              after->muskets);
      rc = 1;
    }
    units_set_ff_col1(NULL);
    units_set_occupancy_map(NULL);
  }

  test_name_kinds_reinstall(); /* other fixtures here are name-built */
  if (rc == 0) {
    fprintf(stderr, "unit_units: #581 kind_plus1 without name resolver ok\n");
  }
  return rc;
}

/*
 * Smell audit 2026-09-09, units/combat batch (#4, #6, #7, #8, #12, #13).
 * Everything here runs on synthetic rosters/pools on purpose — the point of
 * several of these fixes is that a Linux pool index is NOT a DOS @UNIT id.
 */
static void audit_type(
  ColonizeUnitType* t, const char* name, int mv, int atk, int def, ColonizeUnitDomain dom
) {
  memset(t, 0, sizeof(*t));
  snprintf(t->name, sizeof(t->name), "%s", name);
  t->movement = mv;
  t->attack = atk;
  t->defense = def;
  t->domain = dom;
}

static int unit_smell_audit_2026_09_09(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 8;
  map.height = 8;
  map.tile_count = 64;
  map.terrain = calloc(64, 1);
  map.layer2 = calloc(64, 1);
  map.layer3 = calloc(64, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    free(map.terrain);
    free(map.layer2);
    free(map.layer3);
    return 1;
  }
  for (int i = 0; i < 64; ++i) {
    map.terrain[i] = 2; /* plains — every tile is land */
  }
  units_set_occupancy_map(&map);

  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  memset(col1.head.founding_father, 0xff, sizeof(col1.head.founding_father));
  col1.head.difficulty = 2;
  col1.player[0].control = 0; /* human */
  col1.player[1].control = 1; /* AI */
  units_set_ff_col1(&col1);
  units_set_combat_human_nation(0);

  int rc = 0;

  /*
   * #8 — the Brave-vs-human-Artillery auto-loss must key on the @UNIT NAME
   * family, not on a raw pool index. Here Braves sits at 0 and Artillery at 1,
   * so the old `type_index == 0x13 && == 0x0b` test could never fire.
   */
  {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Braves", 1, 1, 1, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[1], "Artillery", 1, 7, 5, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[2], "Armed Braves", 1, 2, 2, COLONIZE_UNIT_DOMAIN_LAND);
    pool.type_count = 3;

    int armed_wins = 0;
    for (int seed = 1; seed <= 40 && rc == 0; ++seed) {
      const int aid = units_spawn_allow_stack(&pool, 0, 4, 5);
      const int did = units_spawn_allow_stack(&pool, 1, 5, 5);
      units_get(&pool, aid)->nation_id = 4;
      units_get(&pool, did)->nation_id = 0;
      ColonizeDosRng rng;
      /* Small seeds draw small first values (the "tiny-seed RNG fixture" trap
       * in bugs.md); spread them so the 14-vs-240 roll can actually land. */
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
      if (units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, aid, did)) {
        fprintf(stderr,
                "audit#8: seed %d — plain Brave beat HUMAN Artillery on a shuffled roster\n",
                seed);
        rc = 1;
      }
      units_despawn(&pool, aid);
      units_despawn(&pool, did);

      if (rc == 0) {
        /* Armed Braves (DOS 0x14) is outside the latch: the roll must decide. */
        const int aid2 = units_spawn_allow_stack(&pool, 2, 4, 6);
        const int did2 = units_spawn_allow_stack(&pool, 1, 5, 6);
        units_get(&pool, aid2)->nation_id = 4;
        units_get(&pool, did2)->nation_id = 0;
        ColonizeDosRng rng2;
        dos_rng_seed(&rng2, seed);
        if (units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng2)}, aid2, did2)) {
          ++armed_wins;
        }
        units_despawn(&pool, aid2);
        units_despawn(&pool, did2);
      }
    }
    if (rc == 0 && armed_wins == 0) {
      fprintf(stderr, "audit#8: Armed Braves never won in 40 rolls — latch is too wide\n");
      rc = 1;
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: audit#8 brave/artillery auto-loss is name-matched ok\n");
    }
  }

  /*
   * #13 — FUN_5fef_0000's domain gate reads the SCANNED TILE's
   * ocean_or_high_seas bit (its `param_2` is the first unit standing on the
   * target tile, raw 100353-100354), not the attacker's own ship-ness. A land
   * tile is defended by land units even against a warship attacking out of a
   * harbour berth; a water tile is defended by ships only.
   * #12 rides along: the armed tier ranks a musket-carrying colonist BODY.
   */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Privateer", 8, 8, 4, COLONIZE_UNIT_DOMAIN_SEA);
    audit_type(&pool.types[1], "Caravel", 4, 0, 2, COLONIZE_UNIT_DOMAIN_SEA);
    audit_type(&pool.types[2], "Colonists", 1, 0, 1, COLONIZE_UNIT_DOMAIN_LAND);
    pool.type_count = 3;

    const int atk = units_spawn_allow_stack(&pool, 0, 3, 4); /* berthed: land tile */
    const int hull = units_spawn_allow_stack(&pool, 1, 4, 4);
    const int body = units_spawn_allow_stack(&pool, 2, 4, 4);
    units_get(&pool, atk)->nation_id = 0;
    units_get(&pool, hull)->nation_id = 1;
    units_get(&pool, body)->nation_id = 1;
    units_get(&pool, body)->muskets = 50; /* colony-armed colonist body */

    /* Target tile is LAND: the armed body defends, the moored hull is out of
     * domain — even though the attacker is a ship. */
    const int pick = units_best_defender_at(&pool, &col1, 4, 4, atk, atk);
    if (pick != body) {
      fprintf(stderr,
              "audit#13: land target tile picked %d, want the armed body %d "
              "(hull %d must be out of domain)\n",
              pick, body, hull);
      rc = 1;
    }
    if (rc == 0) {
      /* Same stack on a WATER tile: now only the hull is in domain. */
      map.terrain[4 * 8 + 4] = 25; /* ocean class on the target tile */
      const int sea_pick = units_best_defender_at(&pool, &col1, 4, 4, atk, atk);
      map.terrain[4 * 8 + 4] = 2;
      if (sea_pick != hull) {
        fprintf(stderr, "audit#13: water target tile picked %d, want hull %d\n", sea_pick, hull);
        rc = 1;
      }
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: audit#13 defender domain gate reads the target tile ok\n");
    }
    units_despawn(&pool, atk);
    units_despawn(&pool, hull);
    units_despawn(&pool, body);
  }

  /*
   * #4 — entry seizure takes the non-combat LAND bystanders only. A berthed
   * foreign Caravel (attack 0, no capture/demote row) used to be despawned
   * outright while an armed hull was left alone.
   */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Soldiers", 1, 2, 2, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[1], "Colonists", 1, 0, 1, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[2], "Caravel", 4, 0, 2, COLONIZE_UNIT_DOMAIN_SEA);
    pool.type_count = 3;

    const int win = units_spawn_allow_stack(&pool, 0, 4, 4);
    const int civ = units_spawn_allow_stack(&pool, 1, 4, 4);
    const int hull = units_spawn_allow_stack(&pool, 2, 4, 4);
    units_set_nation(units_get(&pool, win), 0);
    units_set_nation(units_get(&pool, civ), 1);
    units_set_nation(units_get(&pool, hull), 1);

    units_seize_noncombat_at(&pool, win, 4, 4, &col1);

    const ColonizeUnit* h = units_get(&pool, hull);
    const ColonizeUnit* c = units_get(&pool, civ);
    if (!h || !h->active || h->nation_id != 1) {
      fprintf(stderr, "audit#4: berthed Caravel must survive a colony capture untouched\n");
      rc = 1;
    } else if (!c || !c->active || c->nation_id != 0) {
      fprintf(stderr, "audit#4: civilian bystander should have been seized (nation=%d)\n",
              c && c->active ? c->nation_id : -1);
      rc = 1;
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: audit#4 entry seizure spares hulls, takes civilians ok\n");
    }
    units_despawn(&pool, win);
    units_despawn(&pool, civ);
    units_despawn(&pool, hull);
  }

  /*
   * Parked lead (2026-09-09): FUN_5fef_0352's hull arm, raw 99518-99649.
   * FUN_5fef_0ec0 (raw 99719-99730) hands EVERY unit on the swept stack to
   * 0352 with no predicate, and 0352 tests the LOSER's type byte first —
   * `if ((0xc < type) && (type < 0x13))` — so a berthed hull caught by a LAND
   * loss takes the damage/repair-port arm, not a despawn and not a skip.
   * Because the roll at raw 99527 is guarded by `winner_type*0xe + 0x523b != 0`
   * (the @UNIT guns column, zero for every land type), a land winner never
   * draws: the hull is ALWAYS damaged.
   *
   * bugs.md #473: the swept stack is the DEFENDER's tile (raw 100722, taken
   * when the winner's attack byte is 0). An attacker that loses sweeps only
   * the off-map park it was lifted to at raw 100568 (FUN_281f_0916 →
   * FUN_1427_12f6 → 0362(unit, -2, -2)), so a hull berthed on the
   * ATTACKER's tile must come through untouched — no bogus @SHIPDAMAGE.
   */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Soldiers", 1, 1, 1, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[1], "Regulars", 1, 30, 30, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[2], "Caravel", 4, 0, 2, COLONIZE_UNIT_DOMAIN_SEA);
    pool.types[2].guns = 0;
    pool.types[2].hull = 4;
    pool.type_count = 3;

    /* Attacker sallies out of its own port and loses. */
    const int hull = units_spawn_allow_stack(&pool, 2, 4, 4);
    units_set_nation(units_get(&pool, hull), 0);
    units_get(&pool, hull)->hold_goods_type[0] = 3;
    units_get(&pool, hull)->hold_goods_amount[0] = 100;

    int lost = 0;
    for (int seed = 1; seed <= 40 && !lost && rc == 0; ++seed) {
      const int atk = units_spawn_allow_stack(&pool, 0, 4, 4);
      const int def = units_spawn_allow_stack(&pool, 1, 5, 4);
      if (atk < 0 || def < 0) {
        rc = 1;
        break;
      }
      units_set_nation(units_get(&pool, atk), 0);
      units_set_nation(units_get(&pool, def), 1);
      ColonizeDosRng rng;
      /* Small seeds draw small first values (the "tiny-seed RNG fixture" trap
       * in bugs.md); spread them so the 14-vs-240 roll can actually land. */
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
      if (!units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, atk, def)) {
        lost = 1;
      }
      if (units_get(&pool, atk) && units_get(&pool, atk)->active) {
        units_despawn(&pool, atk);
      }
      if (units_get(&pool, def) && units_get(&pool, def)->active) {
        units_despawn(&pool, def);
      }
    }
    if (rc == 0 && !lost) {
      fprintf(stderr, "0352-hull: attack-1 vs defense-30 never lost in 40 seeds\n");
      rc = 1;
    }
    const ColonizeUnit* h = rc == 0 ? units_get(&pool, hull) : NULL;
    if (rc == 0 && (!h || !h->active)) {
      fprintf(stderr, "#473: berthed Caravel was destroyed by an attacker-loss sweep\n");
      rc = 1;
    } else if (rc == 0 && (h->col1_flags15 & 0x80u) != 0) {
      fprintf(stderr, "#473: attacker-loss must not damage a hull on the origin tile\n");
      rc = 1;
    } else if (rc == 0 && h->hold_goods_amount[0] != 100) {
      fprintf(stderr, "#473: attacker-loss must not touch the berthed hull's holds\n");
      rc = 1;
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: #473 attacker loss leaves berthed hull alone ok\n");
    }
    units_despawn(&pool, hull);
  }

  /* Hull arm on the DEFENDER side (raw 100720-100722): a winner whose attack
   * byte is 0 sweeps the beaten defender's stack, and the berthed hull there
   * is always damaged (bit7, repair timer, holds lost). */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Colonists", 1, 0, 30, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[1], "Soldiers", 1, 1, 1, COLONIZE_UNIT_DOMAIN_LAND);
    audit_type(&pool.types[2], "Caravel", 4, 0, 2, COLONIZE_UNIT_DOMAIN_SEA);
    pool.types[2].guns = 0;
    pool.types[2].hull = 4;
    pool.type_count = 3;

    const int hull = units_spawn_allow_stack(&pool, 2, 5, 4);
    units_set_nation(units_get(&pool, hull), 1);
    units_get(&pool, hull)->hold_goods_type[0] = 3;
    units_get(&pool, hull)->hold_goods_amount[0] = 100;

    int won = 0;
    for (int seed = 1; seed <= 40 && !won && rc == 0; ++seed) {
      const int atk = units_spawn_allow_stack(&pool, 0, 4, 4);
      const int def = units_spawn_allow_stack(&pool, 1, 5, 4);
      if (atk < 0 || def < 0) {
        rc = 1;
        break;
      }
      units_set_nation(units_get(&pool, atk), 0);
      units_set_nation(units_get(&pool, def), 1);
      ColonizeDosRng rng;
      dos_rng_seed(&rng, (unsigned)(seed * 9973 + 4271));
      if (units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&pool), .col1=(ColonizeCol1Save*)(&col1), .col1_ok=true, .rng=(ColonizeDosRng*)(&rng)}, atk, def)) {
        won = 1;
      }
      if (units_get(&pool, atk) && units_get(&pool, atk)->active) {
        units_despawn(&pool, atk);
      }
      if (units_get(&pool, def) && units_get(&pool, def)->active) {
        units_despawn(&pool, def);
      }
    }
    if (rc == 0 && won) {
      const ColonizeUnit* h = units_get(&pool, hull);
      if (!h || !h->active) {
        fprintf(stderr, "0352-hull: berthed Caravel was destroyed by the land sweep\n");
        rc = 1;
      } else if ((h->col1_flags15 & 0x80u) == 0) {
        fprintf(stderr, "0352-hull: damaged bit7 (+0x3148|0x80) not set\n");
        rc = 1;
      } else if (h->repair_pending == 0) {
        fprintf(stderr, "0352-hull: repair timer not armed\n");
        rc = 1;
      } else if (h->hold_goods_amount[0] != 0) {
        fprintf(stderr, "0352-hull: damage tail must zero the holds (+0x3150)\n");
        rc = 1;
      }
      if (rc == 0) {
        fprintf(stderr, "unit_units: 0352 hull arm damages berthed ships in a land sweep ok\n");
      }
    } else if (rc == 0) {
      fprintf(stderr, "unit_units: 0352 hull arm: attack-0 winner never won (skipped)\n");
    }
    units_despawn(&pool, hull);
  }

  /*
   * #6 / #7 — moves holds REMAINING for Euro units but the DOS SPENT
   * byte for natives, so park/restore must go through the spent-aware
   * helpers. A raw `= 0` park hands a Brave a FULL allotment.
   */
  if (rc == 0) {
    ColonizeUnitPool pool;
    memset(&pool, 0, sizeof(pool));
    audit_type(&pool.types[0], "Braves", 1, 1, 1, COLONIZE_UNIT_DOMAIN_LAND);
    pool.type_count = 1;

    const int bid = units_spawn_allow_stack(&pool, 0, 4, 4);
    ColonizeUnit* b = units_get(&pool, bid);
    b->nation_id = 4;
    b->moves = 0; /* native: nothing spent = full allotment */
    const int full = units_remaining_mp(&pool, bid);
    if (full <= 0) {
      fprintf(stderr, "audit#6: fresh Brave should read a full allotment, got %d\n", full);
      rc = 1;
    }
    if (rc == 0 && !units_set_orders(&pool, bid, UNITS_ORDER_SENTRY)) {
      fprintf(stderr, "audit#6: Sentry refused\n");
      rc = 1;
    }
    if (rc == 0 && units_remaining_mp(&pool, bid) != 0) {
      fprintf(stderr, "audit#6: parked Brave still has %d MP (raw moves=%d)\n",
              units_remaining_mp(&pool, bid), units_get(&pool, bid)->moves);
      rc = 1;
    }
    if (rc == 0) {
      units_get(&pool, bid)->park_nights = 1; /* stood there overnight */
      (void)units_wake(&pool, bid);
      if (units_remaining_mp(&pool, bid) != full) {
        fprintf(stderr, "audit#6: woken Brave has %d MP, want %d\n",
                units_remaining_mp(&pool, bid), full);
        rc = 1;
      }
    }
    if (rc == 0) {
      /* #7: the village phantom is spawned native and must not be able to
       * act — under spent semantics that is moves = max, not 0. */
      ColonizeCol1Tribe tribe;
      memset(&tribe, 0, sizeof(tribe));
      tribe.x = 6;
      tribe.y = 6;
      tribe.nation_id = 4;
      ColonizeCol1Save vcol1;
      memset(&vcol1, 0, sizeof(vcol1));
      memset(vcol1.head.founding_father, 0xff, sizeof(vcol1.head.founding_father));
      vcol1.head.tribe_count = 1;
      vcol1.tribe = &tribe;
      const int atk = units_spawn_allow_stack(&pool, 0, 5, 6);
      units_get(&pool, atk)->nation_id = 0;
      const int ph = units_spawn_village_temp_defender(&pool, &vcol1, 6, 6, 4, atk);
      if (ph < 0) {
        fprintf(stderr, "audit#7: village temp defender not spawned\n");
        rc = 1;
      } else if (units_remaining_mp(&pool, ph) != 0) {
        fprintf(stderr, "audit#7: phantom has %d MP left (raw moves=%d)\n",
                units_remaining_mp(&pool, ph), units_get(&pool, ph)->moves);
        rc = 1;
      }
      if (ph >= 0) {
        units_despawn(&pool, ph);
      }
      units_despawn(&pool, atk);
      units_set_ff_col1(&col1);
    }
    if (rc == 0) {
      fprintf(stderr, "unit_units: audit#6/#7 native park/restore are spent-aware ok\n");
    }
    units_despawn(&pool, bid);
  }

  units_set_occupancy_map(NULL);
  units_set_ff_col1(NULL);
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  return rc;
}

/*
 * bugs.md #503/#504/#507/#509 — FUN_157e_004a veteran peel, FUN_5fef_172c
 * promotion gates, FUN_49dd_0386 profession line, FUN_479b_0158 tools-out.
 */
static int unit_promote_and_label_504(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "promote504: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "promote504: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  int rc = 0;

  /* --- #507: FUN_49dd_0386 profession line. ------------------------------ */
  {
    /* @JOB column 0 is singular. */
    const char* farmer = units_profession_line(&names, 0, 0 /* Farmer */, false);
    if (!farmer || strcmp(farmer, "Farmer") != 0) {
      fprintf(stderr, "507: want singular \"Farmer\", got [%s]\n", farmer ? farmer : "(null)");
      rc = 1;
    }
    /* type 1 (Soldiers) / 4 (Dragoons) + 0x15 -> @MISC[65]. */
    const char* v1 = units_profession_line(&names, 1, UNITS_JOB_SOLDIER, false);
    const char* v4 = units_profession_line(&names, 4, UNITS_JOB_SOLDIER, false);
    if (!v1 || strcmp(v1, "Veteran") != 0 || !v4 || strcmp(v4, "Veteran") != 0) {
      fprintf(stderr, "507: veteran override missing [%s]/[%s]\n",
              v1 ? v1 : "(null)", v4 ? v4 : "(null)");
      rc = 1;
    }
    /* type 5 + 0x16 and type 3 + 0x18 -> @MISC[4]. */
    const char* e5 = units_profession_line(&names, 5, UNITS_JOB_SCOUT, false);
    const char* e3 = units_profession_line(&names, 3, UNITS_JOB_MISSIONARY, false);
    if (!e5 || strcmp(e5, "Expert") != 0 || !e3 || strcmp(e3, "Expert") != 0) {
      fprintf(stderr, "507: expert override missing [%s]/[%s]\n",
              e5 ? e5 : "(null)", e3 ? e3 : "(null)");
      rc = 1;
    }
    /* Suppression: type != 0 + flag 0 + unskilled -> nothing; flag 1 prints. */
    if (units_profession_line(&names, 1, UNITS_JOB_CRIMINAL, false) != NULL) {
      fprintf(stderr, "507: unskilled line should be suppressed at param_3=0\n");
      rc = 1;
    }
    const char* crim = units_profession_line(&names, 1, UNITS_JOB_CRIMINAL, true);
    if (!crim || strcmp(crim, "Criminal") != 0) {
      fprintf(stderr, "507: param_3=1 should print [%s]\n", crim ? crim : "(null)");
      rc = 1;
    }
    /* type 0 is never suppressed; 0x1c folds to 0x13. */
    const char* c0 = units_profession_line(&names, 0, UNITS_JOB_NONE, false);
    if (!c0 || strcmp(c0, "Colonist") != 0) {
      fprintf(stderr, "507: type0 NONE should fold to Colonist [%s]\n", c0 ? c0 : "(null)");
      rc = 1;
    }
  }

  /* --- #504: units_promote_on_win gates via a won engagement. ------------- */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[128];
    if (!map_alloc(&map, 10, 10, err, sizeof(err))) {
      fprintf(stderr, "promote504: map_alloc: %s\n", err);
      assets_msg_free(&names);
      return 1;
    }
    for (int i = 0; i < 10 * 10; ++i) {
      map.terrain[i] = 2;
      map.layer3[i] = 1;
    }
    const int soldiers = units_find_type(&pool, "Soldiers");
    const int brave = units_find_type(&pool, "Braves");
    const int cont_army = units_find_type(&pool, "Cont. Army");
    if (soldiers < 0 || brave < 0 || cont_army < 0) {
      fprintf(stderr, "promote504: missing types\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    pool.types[soldiers].attack = 60; /* attacker wins on every seed */
    pool.types[brave].defense = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.game_options.woi = 1;
    col1.player[0].control = 0;
    col1.player[1].control = 1;
    /* Washington: FUN_5fef_172c skips the 04d4 roll entirely. */
    col1.nation[0].founding_fathers[FF_GEORGE_WASHINGTON / 8] |=
      (uint8_t)(1u << (FF_GEORGE_WASHINGTON % 8));
    col1.nation[1].founding_fathers[FF_GEORGE_WASHINGTON / 8] |=
      (uint8_t)(1u << (FF_GEORGE_WASHINGTON % 8));

    /* Case table: (mobilized nation, human_player, winner nation, want type). */
    struct {
      int mob_nation;
      int human_player;
      int winner_nation;
      int want_continental;
      const char* what;
    } cases[] = {
      {0, 0, 0, 1, "human mobilized -> Continental"},
      {1, 0, 0, 0, "flag on the wrong nation record -> no promote"},
      {0, 0, 1, 0, "AI-controlled winner keeps Veteran (raw 100112)"},
    };
    for (size_t ci = 0; ci < sizeof(cases) / sizeof(cases[0]) && rc == 0; ++ci) {
      col1.nation[0].nation_flags = 0;
      col1.nation[1].nation_flags = 0;
      col1.nation[cases[ci].mob_nation].nation_flags |= 0x08u;
      col1.head.human_player = (uint16_t)cases[ci].human_player;

      const int aid = units_spawn(&pool, soldiers, 3, 3);
      const int did = units_spawn_allow_stack(&pool, brave, 4, 3);
      ColonizeUnit* a = units_get(&pool, aid);
      ColonizeUnit* d = units_get(&pool, did);
      if (!a || !d) {
        fprintf(stderr, "promote504: spawn failed\n");
        rc = 1;
        break;
      }
      a->nation_id = cases[ci].winner_nation;
      a->profession = UNITS_JOB_SOLDIER;
      a->muskets = 50;
      a->moves = 3 * UNITS_MP_PER_TILE;
      d->nation_id = 4;
      d->moves = UNITS_MP_PER_TILE;
      ColonizeDosRng rng;
      dos_rng_seed(&rng, 12345u * (uint32_t)(ci + 1));
      ColonizeWorld w;
      memset(&w, 0, sizeof(w));
      w.units = &pool;
      w.map = &map;
      w.rng = &rng;
      w.col1 = &col1;
      w.col1_ok = true;
      units_set_ff_col1(&col1); /* units_try_move_w routes combat through the global */
      (void)units_try_move_w(&w, aid, 4, 3);
      a = units_get(&pool, aid);
      if (!a || units_last_combat_outcome() <= 0) {
        fprintf(stderr, "promote504[%s]: attacker should win (outcome=%d)\n",
                cases[ci].what, units_last_combat_outcome());
        rc = 1;
      } else {
        const int got_cont = (a->type_index == cont_army);
        if (got_cont != cases[ci].want_continental) {
          fprintf(stderr, "promote504[%s]: type_index=%d want_continental=%d\n",
                  cases[ci].what, a->type_index, cases[ci].want_continental);
          rc = 1;
        }
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        if (pool.units[i].active) {
          (void)units_despawn(&pool, pool.units[i].id);
        }
      }
      units_set_occupancy_map(NULL);
      units_set_ff_col1(NULL);
    }
    map_free(&map);
  }

  assets_msg_free(&names);
  if (rc == 0) {
    fprintf(stderr, "unit_units: #503/#504/#507 promote + profession line ok\n");
  }
  return rc;
}

/*
 * bugs.md #598: an armed Indian Convert (@JOB 0x1b) is NOT short-circuited by
 * the FUN_281f_0c9a eligibility gate (FUN_15eb_0002 raw 9298-9307 returns 0
 * for 0x1b), so FUN_5fef_172c raw 100100-100104 reaches the Washington test
 * and, absent Washington, draws FUN_281f_04d4(1, local_6) before the ladder
 * FUN_5fef_16ea (raw 100040-100059) maps 0x1b -> 0x1b and the promotion
 * no-ops. The port must burn that one draw; the profession must not change.
 */
static int unit_promote_convert_rng_598(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "convert598: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "convert598: units_load_types failed\n");
    assets_msg_free(&names);
    return 1;
  }
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 10, 10, err, sizeof(err))) {
    fprintf(stderr, "convert598: map_alloc: %s\n", err);
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 10 * 10; ++i) {
    map.terrain[i] = 2;
    map.layer3[i] = 1;
  }
  const int soldiers = units_find_type(&pool, "Soldiers");
  const int brave = units_find_type(&pool, "Braves");
  int rc = 0;
  if (soldiers < 0 || brave < 0) {
    fprintf(stderr, "convert598: missing types\n");
    map_free(&map);
    assets_msg_free(&names);
    return 1;
  }
  pool.types[soldiers].attack = 60; /* attacker wins on every seed */
  pool.types[brave].defense = 1;

  uint32_t state_after[2] = {0, 0};
  /* pass 0 = no Washington (DOS draws); pass 1 = Washington (DOS skips). */
  for (int pass = 0; pass < 2 && rc == 0; ++pass) {
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.player[0].control = 0;
    col1.head.human_player = 0;
    if (pass == 1) {
      col1.nation[0].founding_fathers[FF_GEORGE_WASHINGTON / 8] |=
        (uint8_t)(1u << (FF_GEORGE_WASHINGTON % 8));
    }
    const int aid = units_spawn(&pool, soldiers, 3, 3);
    const int did = units_spawn_allow_stack(&pool, brave, 4, 3);
    ColonizeUnit* a = units_get(&pool, aid);
    ColonizeUnit* d = units_get(&pool, did);
    if (!a || !d) {
      fprintf(stderr, "convert598: spawn failed\n");
      rc = 1;
      break;
    }
    a->nation_id = 0;
    a->profession = UNITS_JOB_CONVERT;
    a->muskets = 50;
    a->moves = 3 * UNITS_MP_PER_TILE;
    d->nation_id = 4;
    d->moves = UNITS_MP_PER_TILE;
    ColonizeDosRng rng;
    dos_rng_seed(&rng, 987654u);
    ColonizeWorld w;
    memset(&w, 0, sizeof(w));
    w.units = &pool;
    w.map = &map;
    w.rng = &rng;
    w.col1 = &col1;
    w.col1_ok = true;
    units_set_ff_col1(&col1);
    (void)units_try_move_w(&w, aid, 4, 3);
    a = units_get(&pool, aid);
    if (!a || units_last_combat_outcome() <= 0) {
      fprintf(stderr, "convert598[pass %d]: attacker should win\n", pass);
      rc = 1;
    } else if (a->profession != UNITS_JOB_CONVERT || a->type_index != soldiers) {
      fprintf(stderr, "convert598[pass %d]: Convert promoted (prof=%d type=%d)\n",
              pass, a->profession, a->type_index);
      rc = 1;
    }
    state_after[pass] = rng.state;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (pool.units[i].active) {
        (void)units_despawn(&pool, pool.units[i].id);
      }
    }
    units_set_occupancy_map(NULL);
    units_set_ff_col1(NULL);
  }
  if (rc == 0) {
    ColonizeDosRng probe;
    probe.state = state_after[1];
    (void)dos_rng_next(&probe); /* the one 04d4 draw the no-Washington pass makes */
    if (probe.state != state_after[0]) {
      fprintf(stderr,
              "convert598: expected exactly one extra rng draw "
              "(no-FF state %u, Washington state +1 draw %u)\n",
              state_after[0], probe.state);
      rc = 1;
    }
  }
  map_free(&map);
  assets_msg_free(&names);
  if (rc == 0) {
    fprintf(stderr, "unit_units: #598 Convert falls through and burns one draw ok\n");
  }
  return rc;
}

/*
 * bugs.md #655: NAMES.TXT @UNIT column 12 (DOS `DS:0x523d + type*0xe`, loader
 * raw 121132-121134) is an 8-character MSB-first bit-string, so row 4
 * Dragoons "00111100" = 0x3c and row 7 Cont. Cav. "00011100" = 0x1c — the two
 * values the AI capability tables hardcoded.
 */
static int unit_cap_bits_column12(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "cap_bits: NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  int rc = 0;
  if (!units_load_types(&pool, &names)) {
    fprintf(stderr, "cap_bits: units_load_types failed\n");
    rc = 1;
  } else if (pool.type_count < 8) {
    fprintf(stderr, "cap_bits: only %d @UNIT rows\n", pool.type_count);
    rc = 1;
  } else {
    if (pool.types[4].cap_bits != 0x3c) {
      fprintf(stderr, "cap_bits: row 4 = 0x%02x, want 0x3c\n", pool.types[4].cap_bits);
      rc = 1;
    }
    if (pool.types[7].cap_bits != 0x1c) {
      fprintf(stderr, "cap_bits: row 7 = 0x%02x, want 0x1c\n", pool.types[7].cap_bits);
      rc = 1;
    }
    if (pool.types[10].cap_bits != 0x00) { /* Treasure "00000000" */
      fprintf(stderr, "cap_bits: row 10 = 0x%02x, want 0x00\n", pool.types[10].cap_bits);
      rc = 1;
    }
    if (pool.types[13].cap_bits != 0xa2) { /* Caravel "10100010" */
      fprintf(stderr, "cap_bits: row 13 = 0x%02x, want 0xa2\n", pool.types[13].cap_bits);
      rc = 1;
    }
  }
  assets_msg_free(&names);
  if (rc == 0) {
    fprintf(stderr, "unit_units: #655 @UNIT column 12 cap_bits ok\n");
  }
  return rc;
}
int main(void) {
  diag_init(0, NULL);
  if (unit_cap_bits_column12() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_smell_audit_2026_09_09() != 0) {
    return 1;
  }
  if (unit_continental_equip_tier() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_display_name_free_colonist() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_kind_without_name_resolver() != 0) {
    diag_shutdown();
    return 1;
  }
  if (unit_promote_convert_rng_598() != 0) {
    return 1;
  }
  if (unit_promote_and_label_504() != 0) {
    diag_shutdown();
    return 1;
  }
  diag_shutdown();
  return 0;
}
