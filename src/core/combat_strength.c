#include "core/combat_strength.h"

#include <string.h>

#include "core/founding_fathers.h"

/*
 * DOS `bVar28` (FUN_5fef_1b0e, raw 100387): the defender this engagement is
 * resolving was auto-spawned by 1b0e itself because no live unit stood on the
 * attacked settlement — the colony militia / Paul Revere stand-in (raw
 * 100417-100432) or the empty-dwelling Brave (raw 100405-100416). Both arms
 * set it. The port builds the same two phantoms in units.c, which raises this
 * latch around its combat_land_engage call; nothing else may set it.
 *
 * It lives here, not in ColonizeCombatStrengthCtx, because ~20 sites across
 * ai_*.c build that struct field-by-field — a new field would read
 * uninitialized there.
 */
static bool g_combat_auto_defender = false;

void combat_set_auto_defender(bool on) {
  g_combat_auto_defender = on;
}

bool combat_auto_defender(void) {
  return g_combat_auto_defender;
}

void combat_side_flags_clear(ColonizeCombatSideFlags* f) {
  if (!f) {
    return;
  }
  memset(f, 0, sizeof(*f));
  /* Row-chrome slots are "absent", not "sprite 0". */
  f->colony_icon = -1;
  f->colony_nation = -1;
  f->terrain_sprite = -1;
  f->bombard_icon = -1;
}

/*
 * DOS unit +0x3150, read raw by the FUN_157e_004a cargo peel (viceroy
 * 8957-8959: `0xc < type < 0x13` → `local_4 -= +0x3150`). That byte is the
 * GOODS hold count only — written just by FUN_15eb_30b8 / FUN_15eb_317c
 * (viceroy 13301/13339); boarding parks passengers off-map (FUN_1427_10be)
 * and never bumps it. So goods slots only here: a troop-laden ship takes no
 * strength penalty in DOS, and the naval-evasion peel in units.c
 * (FUN_5bfb_312e, viceroy 98448) reads the same byte the same way.
 */
static int combat_ship_holds_occupied(const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (u->hold_goods_amount[i] > 0 && u->hold_goods_amount[i] < 255) {
      ++n;
    }
  }
  return n;
}

/*
 * FUN_157e_004a veteran gate (viceroy_unpacked.c 8942-8944), verbatim:
 *
 *   if (((*(char *)(param_1 * 0x1c + 0x3146) == '\x01') ||
 *       (*(char *)(param_1 * 0x1c + 0x3146) == '\x04')) &&
 *      (*(char *)(param_1 * 0x1c + 0x315b) == '\x15')) { ... +50% ... }
 *
 * +0x3146 is the raw @UNIT type id, and NAMES.TXT @UNIT order makes 1 =
 * Soldiers, 4 = Dragoons. The promoted tiers — 6 Regulars, 7 Cont. Cav.,
 * 8 Cavalry, 9 Cont. Army — are deliberately NOT in the gate: their @UNIT
 * rows already carry the promoted attack/defense (Cont. Army 4, Cont. Cav.
 * 5), so DOS never layers the veteran +50% on top. The old test also matched
 * "Continental"/"Cont. Army" (paying the bonus twice for the Continental
 * Army) while missing "Cont. Cav." entirely — the asymmetry this closes.
 *
 * Matched by name, not by pool index, because a Linux pool index is not a DOS
 * @UNIT id (synthetic test fixtures place Soldier/Dragoon at arbitrary slots);
 * that is the same mapping idiom as ai_euro.c's ai_euro_5d04_dos_type_of.
 * On the stock roster "Soldier"/"Dragoon" hit exactly types 1 and 4.
 */
static int combat_type_is_soldier_or_dragoon(const ColonizeUnitType* t) {
  if (!t || !t->name[0]) {
    return 0;
  }
  return strstr(t->name, "Soldier") != NULL || strstr(t->name, "Dragoon") != NULL;
}

static int combat_type_is_privateer(const ColonizeUnitType* t) {
  return t && t->name[0] && strstr(t->name, "Privateer") != NULL;
}

static int combat_type_is_ship(const ColonizeUnitPool* pool, int unit_id) {
  return pool && units_is_sea(pool, unit_id);
}

static int combat_nation_is_ai(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id > 3) {
    return 1; /* natives / unknown → treat as AI for terrain gates */
  }
  return col1->player[nation_id].control != 0;
}

static const ColonizeCol1Tribe* combat_tribe_at(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return NULL;
  }
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    if ((int)col1->tribe[i].x == x && (int)col1->tribe[i].y == y) {
      return &col1->tribe[i];
    }
  }
  return NULL;
}

static int combat_colony_local_1a(
  const ColonizeColonyPool* colonies,
  const ColonizeColony* col,
  ColonizeCombatSideFlags* flags
) {
  const int stockade = colonies_find_building(colonies, "Stockade");
  const int fort = colonies_find_building(colonies, "Fort");
  const int fortress = colonies_find_building(colonies, "Fortress");
  const int has_stockade =
    stockade >= 0 && stockade < COLONIZE_BUILDING_TYPES_MAX && col->has_building[stockade];
  const int has_fort = fort >= 0 && fort < COLONIZE_BUILDING_TYPES_MAX && col->has_building[fort];
  const int has_fortress =
    fortress >= 0 && fortress < COLONIZE_BUILDING_TYPES_MAX && col->has_building[fortress];
  /*
   * FUN_157e_0008 (viceroy_unpacked.c 8893-8908): how many of FUN_15eb_038e(0),
   * (1), (2) the colony has. Those three ARE the fortification chain — the
   * building table at DS:0x8f82 (stride 0xc) links each record to its next
   * tier at +4, and records 0/1/2 chain 0 → 1 → 2 → -1 with name ids
   * 0x4b/0x4c/0x4d, i.e. Stockade → Fort → Fortress. That closes the
   * combat.md "Open question 2026-09-03": the colony arm of FUN_157e_015e is
   * literally `local_1a = (FUN_157e_0008() + 1) * 2` (viceroy 9009-9011), so
   * the ladder is 2 / 4 / 6 / 8 — a Fort is ×2.5 (+150%, matching the
   * manual), not the Stockade's ×2. The port previously collapsed Fort into
   * Stockade's 4, which is also why its analysis row had no Fort tier.
   */
  const int tier = (has_stockade ? 1 : 0) + (has_fort ? 1 : 0) + (has_fortress ? 1 : 0);
  const int local_1a = (tier + 1) * 2;
  if (flags) {
    flags->flags |= COMBAT_FLAG_COLONY;
    if (tier > 0) {
      flags->flags |= COMBAT_FLAG_STOCKADE; /* "Stockade or better" (8d02|0x10 shape) */
    }
    if (has_fortress) {
      flags->flags |= COMBAT_FLAG_FORTRESS;
    }
    /*
     * FUN_636c_0000's colony row prints (tier + 1) * 50% and labels itself
     * with the topmost built tier (FUN_281f_0bdc = FUN_15eb_0434(0)).
     */
    flags->fort_tier = tier;
    /* 636c draws the settlement marker beside the row (FUN_281f_02a8 → 112b_0c64). */
    flags->colony_icon = colonies_settlement_icon(colonies, col);
    flags->colony_nation = col->nation_id;
  }
  return local_1a;
}

int combat_unit_base_x8(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int mode,
  ColonizeCombatSideFlags* out_flags
) {
  if (out_flags) {
    combat_side_flags_clear(out_flags);
  }
  if (!ctx || !ctx->units) {
    return 0;
  }
  const ColonizeUnit* u = units_get_const(ctx->units, unit_id);
  if (!u || !u->active) {
    return 0;
  }
  const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
  if (!t) {
    return 0;
  }

  if (out_flags && mode != 0) {
    out_flags->flags |= COMBAT_FLAG_MODE_ATK;
  }

  int local_8 = (mode == 0) ? t->defense : t->attack;
  if (local_8 < 0) {
    local_8 = 0;
  }
  /*
   * FUN_157e_004a 8936-8938: type 0x0b + damaged bit7 → −2. DOS tests
   * exactly type 0x0b (Artillery); no ship takes this peel — the old
   * Privateer arm was invented (smell #4).
   */
  if (combat_type_is_artillery_name(t->name) &&
      (u->col1_unknown15 & 0x80u) != 0) {
    local_8 -= 2;
    if (local_8 < 0) {
      local_8 = 0;
    }
  }
  if (out_flags) {
    out_flags->base_combat = local_8;
  }

  int local_4 = local_8 * 8;

  /*
   * Veteran Soldier/Dragoon: veteran profession → +50%. DOS grants the peel
   * to both veteran professions — 0x15 Veteran Soldiers and 0x17 Veteran
   * Dragoons (a Veteran Dragoon carries profession 0x17, never 0x15), so the
   * old 0x15-only test silently dropped the bonus for every veteran dragoon.
   */
  if (combat_type_is_soldier_or_dragoon(t) &&
      (u->profession == UNITS_JOB_SOLDIER || u->profession == UNITS_JOB_DRAGOON)) {
    local_4 = local_4 + (local_4 >> 1);
    if (out_flags) {
      out_flags->flags |= COMBAT_FLAG_VETERAN;
    }
  }

  /* Drake Privateer → +50%. */
  if (combat_type_is_privateer(t) && ctx->col1 &&
      founding_fathers_nation_has(ctx->col1, u->nation_id, FF_FRANCIS_DRAKE)) {
    local_4 = local_4 + (local_4 >> 1);
    if (out_flags) {
      out_flags->flags_hi |= COMBAT_FLAG_DRAKE;
    }
  }

  /* Ship holds occupied subtract after ×8. */
  if (combat_type_is_ship(ctx->units, unit_id)) {
    const int holds = combat_ship_holds_occupied(u);
    if (holds > 0) {
      local_4 -= holds;
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_HOLDS;
        out_flags->holds_occupied = holds;
      }
    }
  }

  if (local_4 < 0) {
    local_4 = 0;
  }
  return local_4;
}

static int combat_woi_active(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  /*
   * game_options.woi (0x5382&1) alone — unknown46[0] aliases DOS
   * price_group_state word 0 (col1_save.h) and is near-always nonzero on a
   * real DOS-authored save, which made this OR misfire "at war" on every
   * such save regardless of actual WoI state. See ai_king_independence_declared.
   */
  return col1->head.game_options.woi != 0;
}

static int combat_ref_present(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  /* Same unknown46[1]/price_group_state-word-0 collision as combat_woi_active. */
  return col1->head.game_options.ref_present != 0;
}

/*
 * Crown / REF nation id = DOS DS:0x53d2 = head.crown_nation_id (the slot the
 * succession merger vacated, FUN_43f7_0218 — king_ref.md). Real saves carry
 * 0..3 there (dutch-reports.SAV = 2); the old "peer of the human Euro slot
 * (0↔1)" re-derivation could only answer 0 or 1, so the crown open-field peel
 * below graded the wrong nation's units on any save with the crown in slot
 * 2/3. ai_king_crown_nation_col1 keeps the 0↔1 formula as the no-save
 * fallback.
 */
static int combat_crown_nation(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 1;
  }
  int human = -1;
  for (int i = 0; i < 4; ++i) {
    if (col1->player[i].control == 0) {
      human = i;
      break;
    }
  }
  const int crown = (int)col1->head.crown_nation_id;
  if (crown >= 0 && crown < 4 && crown != human) {
    return crown;
  }
  if (human < 0) {
    return 1; /* no human slot at all — prior fallback */
  }
  return (human == 0) ? 1 : 0;
}

static int combat_unit_on_colony(
  const ColonizeCombatStrengthCtx* ctx,
  const ColonizeUnit* u
) {
  if (!ctx || !ctx->colonies || !u) {
    return 0;
  }
  return colonies_id_at(ctx->colonies, u->x, u->y) >= 0;
}

int combat_engagement_strength(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int foe_id,
  ColonizeCombatSideFlags* out_flags
) {
  if (out_flags) {
    combat_side_flags_clear(out_flags);
  }
  if (!ctx || !ctx->units) {
    return 0;
  }
  const ColonizeUnit* u = units_get_const(ctx->units, unit_id);
  const ColonizeUnit* foe = units_get_const(ctx->units, foe_id);
  if (!u || !u->active) {
    return 0;
  }

  const int base = combat_unit_base_x8(ctx, unit_id, 0, out_flags);
  int local_1a = 0;

  /*
   * A. Euro colony on the unit's tile — DOS FUN_157e_015e (viceroy_unpacked.c
   * 9004-9012):
   *
   *   iVar6 = FUN_137f_0358(uVar1,uVar2);      // uVar1/uVar2 = THIS unit's x/y
   *   if (-1 < iVar6) { ...bind colony...; local_1a = (FUN_157e_0008()+1)*2; }
   *
   * FUN_137f_0358 = euro_settlement_owner (viceroy_unpacked.c 6793-6810,
   * SYMBOL_MAP.md): layer2 bit 2 on the tile → owner byte, and −1 when that
   * owner is ≥ 4. So the arm's only conditions are "a settlement stands here"
   * and "its owner is a European nation". DOS never compares the owner to the
   * unit's nation and never tests the unit's own nation — the port's
   * `col->nation_id == u->nation_id` and `u->nation_id <= 3` guards were both
   * invented, and they also disagreed with combat_unit_on_colony above, which
   * is (correctly) a bare tile probe.
   */
  if (ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    const ColonizeColony* col = colonies_get(ctx->colonies, cid);
    if (col && col->active && col->nation_id >= 0 && col->nation_id <= 3) {
      local_1a = combat_colony_local_1a(ctx->colonies, col, out_flags);
      goto fortify;
    }
  }

  /*
   * B. Native village — DOS FUN_157e_015e village arm (viceroy 8989-9002):
   * local_1a = 2; tribe tech > 1 (Advanced/Civilized) → 4 (8d02|0x10);
   * capital (tribe record +3 bit4) → local_1a <<= 1 (8d02|0x20). The old
   * (probe+1)*2 formula here belonged to the COLONY arm (FUN_157e_0008
   * probes the bound colony's fort tier) — it mislabeled an Aztec village
   * +150% (bugs: "Aztec bonus").
   */
  if (ctx->col1) {
    const ColonizeCol1Tribe* tribe = combat_tribe_at(ctx->col1, u->x, u->y);
    if (tribe) {
      const int nation = (int)tribe->nation_id;
      int tech = 0;
      if (nation >= 4 && nation <= 11) {
        tech = (int)ctx->col1->indian[nation - 4].tech;
      }
      local_1a = 2;
      if (tech > 1) {
        local_1a = 4;
      }
      if (tribe->state.capital) {
        local_1a <<= 1;
      }
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_VILLAGE;
        out_flags->village_n = tech;
        if (tribe->state.capital) {
          out_flags->flags2 |= COMBAT_FLAG_VILLAGE_CAPITAL;
        }
      }
      goto fortify;
    }
  }

  /* C. Open terrain (FUN_157e_015e gates → local_1a or 0x8d04 stash). */
  if (ctx->map) {
    const int terr_class = map_dos_terr_class_at(ctx->map, u->x, u->y);
    const int terr_byte = map_dos_terr_found_score_byte(terr_class);
    const int unit_nat = u->nation_id & 0xf;
    /*
     * No foe (foe_id < 0) is a PORT-ONLY case: DOS FUN_157e_015e is reached
     * only through FUN_281f_09dc, whose two xrefs are 5fef:00dc
     * (FUN_5fef_0000 defender pick) and 5fef:1e5b (FUN_5fef_1b0e), and both
     * always hand it a live attacker slot — `param_2 * 0x1c + 0x3147` is read
     * unconditionally at viceroy_unpacked.c 8982. Only the AI's own
     * "how tough is this unit" probes (ai_euro_land_foe_toughness) pass -1.
     *
     * The old default `foe_nat = 0xf` made a missing foe read as a NATIVE
     * attacker, which is the ONE reading that denies a European defender its
     * terrain bonus outright (the 9015 gate needs `uVar4 < 4`), so AI scoring
     * rated every Euro unit as if it stood on Desert. Default instead to the
     * arm DOS takes for every ordinary engagement — an AI European foe — so
     * the probe returns the same number the real fight will.
     */
    const int foe_nat = foe ? (foe->nation_id & 0xf) : 0;
    const int foe_is_ai = foe ? combat_nation_is_ai(ctx->col1, foe_nat) : 1;
    const int woi = combat_woi_active(ctx->col1);

    /*
     * Immediate apply (8d02|0x80): native defender, or Euro foe that is AI /
     * pre-WoI (so normal defender terrain). Cite: viceroy 9015–9021.
     */
    if (unit_nat > 3 || (foe_nat < 4 && (!woi || foe_is_ai))) {
      local_1a = terr_byte;
      /* DOS 1b0e: `if (local_1a == 0) 8d02 &= 0x7f` — a 0-value terrain
       * never shows a "+0%" analysis row (bugs.md 405). */
      if (out_flags && terr_byte != 0) {
        out_flags->flags |= COMBAT_FLAG_TERRAIN;
        out_flags->terrain_byte = terr_byte;
        /* 636c draws the engagement tile beside the row (FUN_281f_033a → 1baa_0006). */
        out_flags->terrain_sprite = map_terrain_sprite_at(ctx->map, u->x, u->y);
      }
      goto fortify;
    }

    /*
     * Else: Euro defender vs native, or vs human Euro under WoI.
     *
     * FUN_157e_015e 9023-9037, verbatim shape:
     *
     *   bVar3 = true;
     *   if ((uVar4 < 4) && (*(char *)(uVar4 * 0x34 + 0x543f) == '\0')) {
     *     iVar8 = FUN_137f_0358(uVar1,uVar2);            // colony on defender tile
     *     if (iVar8 < 0) {
     *       iVar8 = FUN_137f_0358(param_2 x, param_2 y); // colony on ATTACKER tile
     *       if (iVar8 < 0) goto LAB_157e_0304;           // → stash (ambush)
     *     }
     *     bVar3 = false; local_1a = terrain; 8d02 |= 0x80;
     *   }
     *   else if (*(char *)(param_1 * 0x1c + 0x314c) == '\x06') { bVar3 = false; }
     *
     * FUN_137f_0358 is euro_settlement_owner (SYMBOL_MAP.md; owner < 4), i.e.
     * a COLONY probe on either tile — not a village probe: FUN_137f_0392, the
     * Indian-settlement probe, already claimed the defender tile at 8988 and
     * a village on the attacker's tile is never consulted. The old
     * "village on either tile" test here was invented.
     *
     * No WoI guard either: reaching this arm with a human Euro attacker
     * already implies the 9017 `0x5382 & 1` latch was set (otherwise the
     * 9015 gate applied terrain outright), so DOS re-tests nothing.
     */
    int apply_now = 0;
    int skip_stash = 0;
    if (foe_nat < 4 && !foe_is_ai) {
      if (ctx->colonies) {
        const int colony_here = colonies_id_at(ctx->colonies, u->x, u->y) >= 0;
        const int colony_foe =
          foe && colonies_id_at(ctx->colonies, foe->x, foe->y) >= 0;
        if (colony_here || colony_foe) {
          apply_now = 1;
          skip_stash = 1;
        }
      }
    } else if (u->orders == UNITS_ORDER_FORTIFIED) {
      /* FUN_157e_015e 9035: orders byte == 6 only — Fortify(5, still digging
       * in) does not deny terrain (smell #5). */
      skip_stash = 1;
    }

    if (apply_now) {
      local_1a = terr_byte;
      if (out_flags && terr_byte != 0) { /* bugs.md 405, same 8d02 &= 0x7f rule */
        out_flags->flags |= COMBAT_FLAG_TERRAIN;
        out_flags->terrain_byte = terr_byte;
        out_flags->terrain_sprite = map_terrain_sprite_at(ctx->map, u->x, u->y);
      }
    } else if (!skip_stash) {
      /*
       * Stash only — do not set COMBAT_FLAG_TERRAIN on defender (DOS writes
       * 8d00|0x80 for the attacker analysis column). land_engage applies stash.
       */
      if (out_flags) {
        out_flags->terrain_stash = terr_byte;
        out_flags->terrain_byte = terr_byte;
        /* Carried to the attacker column with the stash (land_engage below). */
        out_flags->terrain_sprite = map_terrain_sprite_at(ctx->map, u->x, u->y);
      }
    }
  }

fortify:
  /* D. Fortify: orders==6 ONLY (FUN_157e_015e 9045 — Fortify(5) gets nothing
   * until it flips to Fortified), land unit, local_1a < 5 → +2. */
  if (u->orders == UNITS_ORDER_FORTIFIED &&
      !combat_type_is_ship(ctx->units, unit_id) && local_1a < 5) {
    local_1a += 2;
    if (out_flags) {
      out_flags->flags_hi |= 0x20u;
      out_flags->flags |= COMBAT_FLAG_FORTIFY;
    }
  }

  if (out_flags) {
    out_flags->local_1a = local_1a;
  }
  return (int)(((local_1a + 4) * base) >> 2);
}

int combat_unit_toughness(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int foe_id
) {
  /* Always use 015e so colony/village/fortify apply; foe_id may be -1. */
  return combat_engagement_strength(ctx, unit_id, foe_id, NULL);
}

int combat_type_is_artillery_name(const char* name) {
  return name && name[0] &&
         (strstr(name, "Artillery") != NULL || strstr(name, "Cannon") != NULL);
}

int combat_type_is_scout_name(const char* name) {
  return name && name[0] && strstr(name, "Scout") != NULL;
}

/*
 * "Does this unit's @UNIT TYPE row carry the combat flag?" — the literal
 * DS:0x5236 column read (`type[type_index * 0xe + 0x5236] != 0`, spelled
 * `t->attack > 0` here), nothing else. This is the ONE question DOS asks of
 * the type table:
 *
 *   FUN_5fef_0000 (viceroy_unpacked.c 99190):
 *     if (*(char *)(unit[i].type * 0xe + 0x5236) != '\0') break;   // pick it
 *   FUN_5fef_0352 capture gate (99380): a WINNER may flip a Colonist/Wagon
 *     only when its own 0x5236 byte is non-zero.
 *
 * NOT the same question as units.c `units_is_combat_role(pool, u)` — "can
 * this BODY fight?" — which also counts carried muskets/horses because this
 * port stores a colony-armed colonist as a Colonists-type unit with kit,
 * where DOS stores it as @UNIT type 1 "Soldiers". The two agree on every
 * stock typed military unit and diverge only on armed colonists. Use THIS
 * one wherever DOS reads the type table; use the units.c one for gameplay
 * "is this a combatant" gates. Smell audit 2026-09-09 #12.
 */
int combat_unit_is_combat_role(const ColonizeUnitPool* pool, int unit_id) {
  if (!pool) {
    return 0;
  }
  const ColonizeUnitType* t = NULL;
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active) {
    return 0;
  }
  t = units_type(pool, u->type_index);
  if (!t) {
    return 0;
  }
  /* FUN_5fef_0000: skip when type.attack (5236) == 0. */
  return t->attack > 0;
}

/*
 * FUN_636c_0000 bit-0x8000 (Bombard) row icon — asm 636c:0637-0681.
 * DOS: `local_14 = 1; if (colony_at(x,y) >= 0 && (colony[+0x1c] & 0x40) == 0)
 * local_14 = 0;` then blits DS:0x532e when local_14 else DS:0x52cc. Those two
 * DS bytes are the @UNIT icon fields of type 18 (Man-O-War) and type 11
 * (Artillery) — the unit table lives at DS:0x5230 stride 0xe, icon at +2.
 * Colony flag 0x40 is "coastal", so a landlocked siege reads as artillery.
 * Returns an ICONS.SS 0-based index, or -1 when the type table is missing.
 */
static int combat_bombard_row_icon(const ColonizeCombatStrengthCtx* ctx, int x, int y) {
  if (!ctx || !ctx->units) {
    return -1;
  }
  int coastal = 1;
  if (ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, x, y);
    const ColonizeColony* c = colonies_get(ctx->colonies, cid);
    if (c && (c->colony_flags & 0x40u) == 0) {
      coastal = 0;
    }
  }
  const int type_index = coastal ? 18 : 11; /* DS:0x532e / DS:0x52cc */
  if (type_index >= ctx->units->type_count) {
    return -1;
  }
  return ctx->units->types[type_index].icon_sprite;
}

/*
 * SoL % of the colony record on (x,y), or -1 when no record exists.
 *
 * DOS: FUN_5fef_1b0e reaches this only inside `if (-1 < iVar18)` — iVar18 is
 * FUN_281f_07be, the colony INDEX on the defended tile — and then binds it
 * (FUN_281f_09e6) before calling FUN_281f_0c86 → FUN_15eb_0274
 * (viceroy_unpacked.c 9471-9500), which reads only the BOUND colony record:
 *
 *   local_c = 0;
 *   if ((-1 < divisor.hi) && ((0 < divisor.hi) || (divisor.lo != 0)))
 *     local_c = dividend * 100 / divisor;          // 32-bit
 *   local_8 = local_c + (Bolivar && owner < 4 && control == 0 ? 0x14 : 0);
 *   if (100 < local_8) local_8 = 100;
 *
 * So a zero divisor yields 0, not a fallback, and "no colony record" cannot
 * happen at all — 07be already proved one exists. The old
 * `liberty_bells_total / 4` arm here was invented (that divisor appears
 * nowhere in the SoL machinery); -1 now means "port-side desync between the
 * colony pool and the col1 record array" and the caller skips the peel
 * instead of inventing a support number.
 */
static int combat_colony_sol_at(
  const ColonizeCombatStrengthCtx* ctx,
  int x,
  int y
) {
  if (!ctx || !ctx->colonies || !ctx->col1) {
    return -1;
  }
  const int cid = colonies_id_at(ctx->colonies, x, y);
  if (cid < 0) {
    return -1;
  }
  const ColonizeColony* c = colonies_get(ctx->colonies, cid);
  if (!c) {
    return -1;
  }
  /* Inline colony_prod_sol_percent to avoid linking colony_production into smokes. */
  if (ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* cc = &ctx->col1->colony[i];
      if ((int)cc->x != c->x || (int)cc->y != c->y) {
        continue;
      }
      /* FUN_15eb_0274 guard: divisor <= 0 leaves local_c at 0. */
      int sol = 0;
      if (cc->rebel_divisor != 0) {
        sol = (int)((cc->rebel_dividend * 100u) / cc->rebel_divisor);
      }
      if (sol < 0) {
        sol = 0;
      }
      sol += founding_fathers_bolivar_sol_bonus(ctx->col1, c->nation_id);
      if (sol > 100) {
        sol = 100;
      }
      return sol;
    }
  }
  return -1;
}

/*
 * FUN_5fef_1b0e peels after 157e base strengths.
 * Cite: viceroy_unpacked.c ~100459–100576 (artillery/ambush/SoL/diff/Scout).
 */
void combat_apply_1b0e_peels(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* io
) {
  if (!ctx || !ctx->units || !io) {
    return;
  }
  const ColonizeUnit* atk = units_get_const(ctx->units, attacker_id);
  const ColonizeUnit* def = units_get_const(ctx->units, defender_id);
  if (!atk || !def) {
    return;
  }
  const ColonizeUnitType* at = units_type(ctx->units, atk->type_index);
  const ColonizeUnitType* dt = units_type(ctx->units, def->type_index);
  if (!at || !dt) {
    return;
  }

  const int atk_nat = atk->nation_id;
  const int def_nat = def->nation_id;
  const int atk_ship = combat_type_is_ship(ctx->units, attacker_id);
  const int def_ship = combat_type_is_ship(ctx->units, defender_id);
  const int on_colony = combat_unit_on_colony(ctx, def);
  /*
   * DOS 1b0e keys the artillery + Spanish-ambush clauses on
   * FUN_281f_06be(dest) = tile settlement bit (layer2&2 — set on Euro colony
   * AND Indian village tiles; euro_unit_act.md road-pair rule "colony 0x02"),
   * NOT on the colony lookup (07be) the WoI clauses use. So artillery
   * attacking a village is NOT "in the open" (bugs: −75% wrongly applied),
   * and the Spanish +50% fires on village tiles.
   */
  const int on_settlement =
    on_colony || (ctx->col1 && combat_tribe_at(ctx->col1, def->x, def->y) != NULL);
  const int land = !atk_ship && !def_ship;

  /* Difficulty: human Euro side strength -= (difficulty - 4). */
  if (ctx->col1) {
    const int diff = (int)ctx->col1->head.difficulty;
    const int adj = diff - 4;
    if (atk_nat >= 0 && atk_nat <= 3 && !combat_nation_is_ai(ctx->col1, atk_nat)) {
      io->atk_strength -= adj;
    }
    if (def_nat >= 0 && def_nat <= 3 && !combat_nation_is_ai(ctx->col1, def_nat)) {
      io->def_strength -= adj;
    }
  }

  /*
   * Fatigue (DOS 1b0e `if (local_98) local_92 = local_92 * local_98 / 3`,
   * immediately after the difficulty peel): an attacker with less than one
   * whole movement point left fights at remaining/3 strength — the penalty
   * @HALF warns about ("they will fight at {%NUMBER0/3 strength}"). local_98
   * is the remaining thirds when below 3 and 0 otherwise, so a rested
   * attacker is untouched. bugs.md.
   */
  {
    const int rem = units_remaining_mp(ctx->units, attacker_id);
    if (rem > 0 && rem < UNITS_MP_PER_TILE) {
      io->atk_strength = io->atk_strength * rem / UNITS_MP_PER_TILE;
      /* DOS 636c fatigue rows: 2 thirds → 0x8d01 bit0, 1 third → a156 bit3. */
      io->atk_flags.flags |= (rem == 2) ? COMBAT_FLAG_FATIGUE_33 : 0u;
      io->atk_flags.flags2 |= (rem == 1) ? COMBAT_FLAG_FATIGUE_66 : 0u;
    }
  }

  /*
   * DOS 1b0e 1f0b (unported until 2026-09-03): land vs land, attacker type
   * attack (5236, the +6 column) > 1 and defender type defense (5235, +5) < 2 → defender
   * strength halved. No 8d00 flag — DOS 636c has no row for it.
   */
  if (land && at->attack > 1 && dt->defense < 2) {
    io->def_strength >>= 1;
  }

  /* Artillery open-field >>2 when the combat tile has no settlement. */
  if (land) {
    const int atk_arty = combat_type_is_artillery_name(at->name);
    const int def_arty = combat_type_is_artillery_name(dt->name);
    /*
     * DOS 1f4b: BOTH clauses read the DEFENDER's orders (asm IMUL of the
     * defender slot in each) — penalty unless (defender Fortify/Fortified
     * AND defender is Euro). The old port tested each side's own orders.
     */
    const int def_fort =
      def->orders == UNITS_ORDER_FORTIFIED || def->orders == UNITS_ORDER_FORTIFY;
    const int def_shielded = def_fort && def_nat <= 3;
    if (!on_settlement) {
      if (atk_arty && !def_shielded) {
        io->atk_strength >>= 2;
        io->atk_flags.flags |= COMBAT_FLAG_ARTILLERY;
        io->atk_flags.flags_hi |= COMBAT_FLAG_ARTILLERY;
      }
      if (def_arty && !def_shielded) {
        io->def_strength >>= 2;
        io->def_flags.flags |= COMBAT_FLAG_ARTILLERY;
        io->def_flags.flags_hi |= COMBAT_FLAG_ARTILLERY;
      }
    } else if (def_arty && atk_nat > 3) {
      /* Artillery defender vs native attacker on settlement → ×2. */
      io->def_strength <<= 1;
      io->def_flags.flags2 |= COMBAT_FLAG_ARTY_COLONY;
    }
    /* Spanish ambush +50% vs Indians on a settlement tile (villages). */
    if (atk_nat == 2 && def_nat > 3 && on_settlement) {
      io->atk_strength += io->atk_strength >> 1;
      io->atk_flags.flags |= COMBAT_FLAG_AMBUSH;
      io->atk_flags.flags_hi |= COMBAT_FLAG_AMBUSH;
    }
  }

  /*
   * WoI popular-support peels (FUN_5fef_1b0e raw 100494–100528).
   * Gate, verbatim: `if (((*(byte *)0x5382 & 1) != 0) && (uVar16 < 4))` —
   * WoI active AND Euro ATTACKER, and NOTHING else. There is no is-ship test
   * on either side (bVar9 / bVar10 gate the halving clause at raw 100468 and
   * the 0xd..0x12 range checks elsewhere, never this block), so the port's old
   * `&& land` was invented: it silently denied a WoI crown/REF SHIP the +50%
   * bombardment bonus and the Tory/Rebel SoL peel when it attacked a hull
   * berthed in a rebel colony (a colony tile IS the defended tile then, so
   * DOS's iVar18 = FUN_281f_07be is >= 0 and the colony arm runs). Removed
   * 2026-09-09.
   *
   * The open-field arm keeps its own domain test — DOS's
   * `FUN_281f_0768(x,y) == 0` (ocean_or_high_seas) — so a fight in open water
   * still collects nothing; that is the only place DOS looks at the domain.
   * Cite: viceroy_unpacked.c 100494-100528; king_ref.md (0x53d2 = crown).
   */
  if (ctx->col1 && combat_woi_active(ctx->col1) && atk_nat >= 0 && atk_nat <= 3) {
    const int crown = combat_crown_nation(ctx->col1);
    const int atk_is_crown = (atk_nat == crown);
    if (!on_colony) {
      /*
       * Open field: crown attacker on land (not ocean) →
       * atk += difficulty * atk / 20.
       */
      if (atk_is_crown && ctx->map && map_tile_is_land(ctx->map, def->x, def->y)) {
        const int diff = (int)ctx->col1->head.difficulty;
        io->atk_strength += (diff * io->atk_strength) / 20;
      }
    } else {
      /* On colony: +50% if crown attacker OR ref_present (0x8d01|0x80). */
      if (atk_is_crown || combat_ref_present(ctx->col1)) {
        io->atk_strength += io->atk_strength >> 1;
        io->atk_flags.flags |= COMBAT_FLAG_REF;
        io->atk_flags.flags_hi |= COMBAT_FLAG_REF;
        /*
         * 636c bit-0x8000 row icon (asm 636c:0653-0681): the colony record's
         * +0x1c bit 0x40 (coastal) picks Man-O-War (DS:0x532e = @UNIT type 18
         * icon byte) over Artillery (DS:0x52cc = @UNIT type 11) — naval
         * bombardment vs a landed siege train. No colony → Man-O-War.
         */
        io->atk_flags.bombard_icon = combat_bombard_row_icon(ctx, def->x, def->y);
      }
      /*
       * DOS binds the colony record 07be already proved exists, so the peel
       * always has a real SoL. -1 = no col1 record for this tile (port-side
       * desync only): skip rather than invent a support number — with
       * `sol = 0` a crown attacker would silently collect the full +100%
       * Tory bonus. Cite: FUN_5fef_1b0e `if (-1 < iVar18)` / FUN_15eb_0274.
       */
      const int sol = combat_colony_sol_at(ctx, def->x, def->y);
      if (sol >= 0) {
        int support = sol > 100 ? 100 : sol;
        if (atk_is_crown) {
          support = 100 - support; /* Tory share for crown/REF */
          if (support > 0) {
            io->atk_flags.flags2 |= COMBAT_FLAG_TORIES;
          }
        } else if (support > 0) {
          io->atk_flags.flags2 |= COMBAT_FLAG_REBELS;
        }
        if (support > 0) {
          const int add = (support * io->atk_strength) / 100;
          io->atk_strength += add;
          io->atk_flags.sol_percent = support;
        }
      }
    }
  }

  /*
   * The DOS difficulty-handicap group (incl. the Discoverer beginner shield)
   * sits AFTER 1b0e's `param_5 == 0` early return — Combat Analysis odds and
   * AI scoring never see it. It lives in
   * combat_apply_1b0e_resolve_handicaps(), applied by the resolvers only.
   */

  if (io->atk_strength < 0) {
    io->atk_strength = 0;
  }
  if (io->def_strength < 0) {
    io->def_strength = 0;
  }
}

/*
 * FUN_5fef_1b0e difficulty-handicap group — raw 100534-100556. The whole
 * block sits AFTER the `param_5 == 0` early return (raw 100529-100533), so
 * Combat Analysis odds and every AI-scoring call compute WITHOUT it; only
 * the real resolution roll sees it. Decomp, verbatim:
 *
 *   if ((*(byte *)0x53a6 < 2) &&
 *      ((((*(byte *)0x5382 & 1) == 0 || (iVar18 < 0)) ||
 *        ((0xc < uVar19 && (uVar19 < 0x13)))))) {
 *     if ((((uVar15 < 4) && (*(char *)(uVar15 * 0x34 + 0x543f) == '\0')) &&
 *          (*(int *)0x538e < 0x50)) && (-1 < iVar18)) {
 *       if (*(char *)0x53a6 == '\0') local_92 = local_92 - (local_92 >> 2);
 *       else                         local_92 = local_92 >> 1;
 *       if ((bVar28) && (*(char *)0x53a6 == '\0')) local_92 = 0;
 *     }
 *     if (((uVar15 < 4) && (*(char *)(uVar15 * 0x34 + 0x543f) == '\0')) &&
 *        ((uVar16 < 4 || (*(int *)0x538e < 0x50)))) {
 *       local_92 = local_92 >> 1;
 *     }
 *   }
 *   if (((*(char *)0x53a6 == '\0') && (uVar16 < 4)) &&
 *       (*(char *)(uVar16 * 0x34 + 0x543f) == '\0')) {
 *     local_92 = local_92 << 1;
 *   }
 *
 * Reading: uVar15 = DEFENDER nation, uVar16 = ATTACKER nation, local_92 =
 * attacker strength (the roll compares against it), iVar18 = colony index on
 * the defended tile, 0x53a6 = difficulty, 0x538e = turn, 0x543f + n*0x34 =
 * control byte (0 = human), 0x5382&1 = WoI, uVar19 0xd..0x12 = attacker is
 * a hull, bVar28 = auto-spawned defender (combat_set_auto_defender).
 *
 * So on Discoverer/Explorer, outside WoI colony land fights:
 *   - attacker of a human Euro COLONY in the first 80 turns: −25% (diff 0)
 *     or −50% (diff 1); if the defender is the auto-spawned militia/Revere
 *     phantom and diff 0, the attacker is ZEROED (beginner shield);
 *   - attacker of a human Euro (no colony needed): a further −50%, always
 *     for Euro attackers, first 80 turns only for natives;
 * and unconditionally: diff 0 + human Euro ATTACKER → attacker DOUBLED.
 * (The port's old "Discoverer damper" −25% on a human attacker was the
 * mirror image of these bytes and is deleted.)
 *
 * The colony-tile tail that follows it in DOS — raw 100557-100564, verbatim:
 *
 *   if (-1 < iVar18) {
 *     if ((3 < uVar16) && (*(char *)(uVar15 + 0x9298) == '\x01')) {
 *       local_92 = 0;
 *     }
 *     if (((uVar15 < 4) && (*(char *)(uVar15 * 0x34 + 0x543f) == '\0')) &&
 *        ((int)(uint)(*(byte *)(uVar15 + 0x940c) >> 1) <=
 *         (int)*(char *)(iVar18 * 0xca + 0x5d65))) {
 *       local_a8 = local_a8 + (*(byte *)0x53a6 - 4) * -4;
 *     }
 *   }
 *
 * Same variable reading, plus: 0x9298 + n = that nation's colony COUNT byte,
 * 0x940c + n = its Σ colony population, 0x5d65 + idx*0xca = the population of
 * the colony standing on the defended tile, local_a8 = defender strength.
 * Both DS bytes are the FUN_4962_0018 census window the port mirrors as
 * col1->stuff.colony_counts / colony_pop_totals. Note this pair runs for EVERY
 * difficulty (it sits outside the `0x53a6 < 2` gate above):
 *   (a) last-colony shield — a NATIVE attacker (nation > 3) against a colony
 *       tile whose owner is down to its final colony is ZEROED outright;
 *   (b) half-the-nation bonus — a human Euro defender whose colony on this
 *       tile holds at least half the nation's total colonists gets
 *       (4 − difficulty) × 4 added to defence (×8 scale: +16 at Discoverer
 *       down to +0 at Viceroy).
 */
void combat_apply_1b0e_resolve_handicaps(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* io
) {
  if (!ctx || !ctx->units || !ctx->col1 || !io) {
    return;
  }
  const ColonizeUnit* atk = units_get_const(ctx->units, attacker_id);
  const ColonizeUnit* def = units_get_const(ctx->units, defender_id);
  if (!atk || !def) {
    return;
  }
  const int atk_nat = atk->nation_id;
  const int def_nat = def->nation_id;
  const int diff = (int)ctx->col1->head.difficulty;
  const int turn = (int)ctx->col1->head.turn;
  /* iVar18: colony sitting on the defended tile (-1 = none). */
  const int def_colony_id =
    ctx->colonies ? colonies_id_at(ctx->colonies, def->x, def->y) : -1;
  const int on_colony = (def_colony_id >= 0);
  const int atk_ship = combat_type_is_ship(ctx->units, attacker_id);
  const bool atk_euro = (atk_nat >= 0 && atk_nat <= 3);
  const bool def_human_euro =
    def_nat >= 0 && def_nat <= 3 && !combat_nation_is_ai(ctx->col1, def_nat);
  const bool atk_human_euro = atk_euro && !combat_nation_is_ai(ctx->col1, atk_nat);

  if (diff < 2 && (!combat_woi_active(ctx->col1) || !on_colony || atk_ship)) {
    if (def_human_euro && turn < 0x50 && on_colony) {
      if (diff == 0) {
        io->atk_strength -= io->atk_strength >> 2;
      } else {
        io->atk_strength >>= 1;
      }
      if (g_combat_auto_defender && diff == 0) {
        io->atk_strength = 0;
      }
    }
    if (def_human_euro && (atk_euro || turn < 0x50)) {
      io->atk_strength >>= 1;
    }
  }
  if (diff == 0 && atk_human_euro) {
    io->atk_strength <<= 1;
  }

  /* raw 100557-100564 — colony-tile tail, every difficulty. */
  if (def_colony_id >= 0) {
    const int def_euro = (def_nat >= 0 && def_nat <= 3);
    /*
     * (a) `(3 < uVar16) && 0x9298[uVar15] == 1` — native attacker vs the
     * defender nation's last colony: attacker zeroed. DOS indexes 0x9298 by
     * the defender nation with no <4 guard; a colony tile implies a European
     * defender, so the port guards it like the sibling clauses do.
     */
    if (atk_nat > 3 && def_euro && ctx->col1->stuff.colony_counts[def_nat] == 1) {
      io->atk_strength = 0;
    }
    /*
     * (b) `0x940c[uVar15] >> 1 <= 0x5d65[iVar18]` — human Euro defender whose
     * colony on this tile holds half or more of the nation's colonists:
     * defence += (4 − difficulty) * 4. The population read is the tile's
     * colony record (DOS never re-checks its owner), so read it literally.
     */
    if (def_human_euro) {
      const ColonizeColony* c = colonies_get(ctx->colonies, def_colony_id);
      const int col_pop = c ? c->population : 0;
      const int half_nation = (int)(ctx->col1->stuff.colony_pop_totals[def_nat] >> 1);
      if (half_nation <= col_pop) {
        io->def_strength += (4 - diff) * 4;
      }
    }
  }

  if (io->atk_strength < 0) {
    io->atk_strength = 0;
  }
  if (io->def_strength < 0) {
    io->def_strength = 0;
  }
}

void combat_land_engage(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* out
) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  combat_side_flags_clear(&out->atk_flags);
  combat_side_flags_clear(&out->def_flags);
  out->atk_strength = combat_unit_base_x8(ctx, attacker_id, 1, &out->atk_flags);
  out->def_strength =
    combat_engagement_strength(ctx, defender_id, attacker_id, &out->def_flags);
  /*
   * FUN_5fef_1b0e attacker scale: ((0x8d04 + 4) * 004a >> 2) * 3 >> 1.
   * 8d04 is 015e terrain stash (0 when defender absorbed terrain / colony /
   * village). Always includes the ×3/2 attack factor; stash adds ambush terrain
   * for Indian→Euro and human→AI-Euro under WoI (REF).
   */
  {
    const int stash = out->def_flags.terrain_stash;
    out->atk_strength = ((stash + 4) * out->atk_strength >> 2) * 3 >> 1;
    if (stash > 0) {
      out->atk_flags.flags |= COMBAT_FLAG_TERRAIN;
      out->atk_flags.terrain_byte = stash;
      out->atk_flags.terrain_stash = stash;
      /* 636c draws the same engagement tile on both columns. */
      out->atk_flags.terrain_sprite = out->def_flags.terrain_sprite;
    }
  }
  if (out->atk_strength < 0) {
    out->atk_strength = 0;
  }
  if (out->def_strength < 0) {
    out->def_strength = 0;
  }
  combat_apply_1b0e_peels(ctx, attacker_id, defender_id, out);
}

void combat_naval_engage(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* out
) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  combat_side_flags_clear(&out->atk_flags);
  combat_side_flags_clear(&out->def_flags);
  out->atk_strength = combat_unit_base_x8(ctx, attacker_id, 1, &out->atk_flags);
  out->def_strength = combat_unit_base_x8(ctx, defender_id, 0, &out->def_flags);
  /*
   * Attacking ships get the same ×3/2 attack factor land attackers do.
   * FUN_5fef_1b0e is the SINGLE resolver for both domains and its scale line
   * (viceroy_unpacked.c 100457-100458) is unconditional:
   *   iVar24 = FUN_281f_09c8(0x281f,param_1,1);     // → FUN_157e_004a mode 1
   *   local_92 = ((*(int *)0x8d04 + 4) * iVar24 >> 2) * 3 >> 1;
   * The is-ship flags bVar9 / bVar10 (100347, 100451) gate later clauses, not
   * this one. Combat Analysis shows the matching "Attack Bonus +50%" row for
   * ships as well (FUN_636c_0000 viceroy_unpacked.c 101874-101891, walking
   * DS:0x8d00 bit 0, which FUN_157e_004a sets on every mode-1 evaluation).
   */
  out->atk_strength += out->atk_strength >> 1;
  if (out->atk_strength < 0) {
    out->atk_strength = 0;
  }
  if (out->def_strength < 0) {
    out->def_strength = 0;
  }
  /* Naval: difficulty peel only (artillery/ambush/SoL are land). */
  combat_apply_1b0e_peels(ctx, attacker_id, defender_id, out);
}
